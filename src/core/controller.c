#include "core/controller.h"
#include <stddef.h>

#include "core/dewpoint.h"

const controller_config_t CONTROLLER_DEFAULTS = {
    .dew_margin_on_cd = 500,            /* 5.00 degC */
    .dew_margin_off_cd = 700,           /* 7.00 degC */
    .sample_period_ms = 1000,
    .fault_threshold = 3,
    .recovery_threshold = 3,
    .relay_safe_state = false
};

/* Объявление внешних контрактов слоя абстракции, которые используются в ядре */
extern void platform_relay_set(bool on);
extern void platform_emergency_backup_save(const void *data, size_t len);
extern bool platform_emergency_backup_load(void *out_data, size_t len);

static void ctrl_set_relay(controller_t *c, bool on)
{
    c->relay_on = on;
}

static bool sample_is_valid(const sample_t *s)
{
    if (s == NULL) {
        return false;
    }

    if (s->quality != QUALITY_VALID) {
        return false;
    }

    if (s->temp_cd < TEMP_CD_MIN || s->temp_cd > TEMP_CD_MAX) {
        return false;
    }

    if (s->rh_cp > RH_CP_MAX) {
        return false;
    }

    return true;
}

/* Базовая инициализация полей ядра */
void ctrl_init(controller_t *c, const controller_config_t *cfg)
{
    if (c == NULL) {
        return;
    }

    c->cfg = (cfg != NULL) ? *cfg : CONTROLLER_DEFAULTS;

    if (c->cfg.dew_margin_on_cd >= c->cfg.dew_margin_off_cd) {
        c->cfg.dew_margin_on_cd = CONTROLLER_DEFAULTS.dew_margin_on_cd;
        c->cfg.dew_margin_off_cd = CONTROLLER_DEFAULTS.dew_margin_off_cd;
    }

    if (c->cfg.fault_threshold == 0U) {
        c->cfg.fault_threshold = 1U;
    }

    if (c->cfg.recovery_threshold == 0U) {
        c->cfg.recovery_threshold = 1U;
    }

    if (c->cfg.sample_period_ms == 0U) {
        c->cfg.sample_period_ms = 1000U;
    }

    ma_init(&c->filter);

    c->filtered_temp_cd = 0;
    c->humidity_cp = 0;
    c->dew_point_cd = 0;

    c->sensor_error_streak = 0;
    c->sensor_ok_streak = 0;

    c->relay_on = c->cfg.relay_safe_state;

    c->state = CTRL_STATE_INIT;
    c->last_error = ERR_NOT_READY;

    c->sample_count = 0;
    c->fault_count = 0;
    
    c->watchdog_crash_counter = 0u;
    c->power_is_dying = false;
}

/* Расширенная серийная инициализация с логикой защиты Fault Isolation */
void ctrl_init_extended(controller_t *c, const controller_config_t *cfg, reset_cause_t cause)
{
    if (c == NULL) {
        return;
    }

    // 1. Запускаем базовую чистку полей
    ctrl_init(c, cfg);
    c->power_is_dying = false;

    // 2. Восстанавливаем счетчик падений из защищенной Backup RAM платы
    uint8_t saved_crashes = 0u;
    if (platform_emergency_backup_load(&saved_crashes, sizeof(saved_crashes))) {
        c->watchdog_crash_counter = saved_crashes;
    }

    // 3. Анализируем причину старта процессора
    if (cause == RESET_CAUSE_WATCHDOG) {
        c->watchdog_crash_counter++;
        c->fault_count++;
        c->last_error = ERR_HW;
        
        // Фиксируем инкремент в энергонезависимой памяти
        platform_emergency_backup_save(&c->watchdog_crash_counter, sizeof(c->watchdog_crash_counter));

        // Жесткая изоляция: если падаем по WDT 3 раза подряд — блокируем контур
        if (c->watchdog_crash_counter >= 3u) {
            c->state = CTRL_STATE_SAFE; 
            c->relay_on = c->cfg.relay_safe_state;
            return;
        }
        
        c->state = CTRL_STATE_WARMUP;
    } else if (cause == RESET_CAUSE_POWER_ON) {
        // Штатный запуск по питанию — полностью обнуляем кредит аварий
        c->watchdog_crash_counter = 0u;
        platform_emergency_backup_save(&c->watchdog_crash_counter, sizeof(c->watchdog_crash_counter));
        c->state = CTRL_STATE_INIT;
    } else {
        /* MISRA C:2012 Rule 15.7 - Финальный защитный блок else.
         * Для всех остальных сценариев (External Reset, Software или Unknown)
         * оставляем базовую безопасную инициализацию INIT */
        c->watchdog_crash_counter = 0u;
        c->state = CTRL_STATE_INIT;
    }
}

/* Экстренное выключение при Brown-out с жестким таймингом исполнения */
void ctrl_force_emergency_shutdown(controller_t *c)
{
    if (c == NULL) {
        return;
    }
    c->power_is_dying = true;
    c->state = CTRL_STATE_SAFE;
    
    // Мгновенная изоляция силового выхода на уровне кремния
    c->relay_on = c->cfg.relay_safe_state;
    platform_relay_set(c->relay_on);
}

/* Основной шаг автомата с защитой серийного уровня */
err_t ctrl_update(controller_t *c, const sample_t *s)
{
    if (c == NULL || s == NULL) {
        return ERR_INVALID_ARG;
    }

    // СЕРИЙНАЯ ЗАЩИТА: Блокировка автомата при критических авариях и сбое питания
    if (c->state == CTRL_STATE_SAFE || c->power_is_dying) {
        ctrl_set_relay(c, c->cfg.relay_safe_state);
        return ERR_HW;
    }

    c->sample_count++;

    if (!sample_is_valid(s)) {
        c->sensor_error_streak++;
        c->sensor_ok_streak = 0;
        c->last_error = ERR_SENSOR;

        if (c->sensor_error_streak >= c->cfg.fault_threshold) {
            c->state = CTRL_STATE_SENSOR_FAULT;
            c->fault_count++;
            ctrl_set_relay(c, c->cfg.relay_safe_state);
        }

        return ERR_SENSOR;
    }

    c->sensor_error_streak = 0;

    if (c->state == CTRL_STATE_SENSOR_FAULT || c->state == CTRL_STATE_MATH_FAULT) {
        c->sensor_ok_streak++;

        if (c->sensor_ok_streak < c->cfg.recovery_threshold) {
            c->last_error = ERR_NOT_READY;
            return ERR_NOT_READY;
        }
    }

    c->sensor_ok_streak = 0;
    c->humidity_cp = s->rh_cp;

    err_t ferr = ma_update(&c->filter, s->temp_cd, &c->filtered_temp_cd, NULL);

    if (ferr == ERR_WARMUP) {
        c->state = CTRL_STATE_WARMUP;
        c->last_error = ERR_WARMUP;
        ctrl_set_relay(c, c->cfg.relay_safe_state);
        return ERR_WARMUP;
    }

    if (ferr != ERR_OK) {
        c->state = CTRL_STATE_MATH_FAULT;
        c->last_error = ferr;
        c->fault_count++;
        ctrl_set_relay(c, c->cfg.relay_safe_state);
        return ferr;
    }

    err_t derr = dewpoint_calc_cd(c->filtered_temp_cd, c->humidity_cp, &c->dew_point_cd);

    if (derr != ERR_OK) {
        c->state = CTRL_STATE_MATH_FAULT;
        c->last_error = derr;
        c->fault_count++;
        ctrl_set_relay(c, c->cfg.relay_safe_state);
        return derr;
    }

    temp_cd_t margin_cd = c->filtered_temp_cd - c->dew_point_cd;

    if (!c->relay_on) {
        if (margin_cd < c->cfg.dew_margin_on_cd) {
            ctrl_set_relay(c, true);
        }
    } else {
        if (margin_cd > c->cfg.dew_margin_off_cd) {
            ctrl_set_relay(c, false);
        }
    }

    // Если успешно дошли до стабильного RUN, сбрасываем счетчик аварийных перезапусков WDT
    if (c->watchdog_crash_counter > 0u) {
        c->watchdog_crash_counter = 0u;
        platform_emergency_backup_save(&c->watchdog_crash_counter, sizeof(c->watchdog_crash_counter));
    }

    c->state = CTRL_STATE_RUN;
    c->last_error = ERR_OK;

    return ERR_OK;
}

bool ctrl_relay_on(const controller_t *c)
{
    if (c == NULL) {
        return false;
    }
    return c->relay_on;
}
