#ifndef PLATFORM_PLATFORM_H
#define PLATFORM_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "core/types.h"

uint32_t platform_millis(void);
void platform_wfi(void);

void platform_relay_set(bool on);
bool platform_relay_get(void);

void platform_write(const char *data, size_t len);
void platform_poll(void);

/**
 * Initialize Independent Watchdog.
 * @param timeout_ms Timeout in milliseconds before reset.
 */
void platform_wdg_init(uint32_t timeout_ms);

/**
 * Feed the watchdog. Should be called periodically.
 * In ticket-based systems, call only if all critical tasks completed.
 */
void platform_wdg_feed(void);

/**
 * Чтение причины последнего сброса из аппаратных регистров (например, RCC_CSR).
 */
reset_cause_t platform_reset_cause_get(void);

/**
 * Инициализация программируемого компаратора напряжения (PVD).
 * Генерирует экстренное прерывание, когда напряжение питания падает ниже 2.9V.
 */
void platform_pvd_init(void);

/**
 * Экстренное побайтное сохранение данных в неразрушаемую память (EEPROM или Backup-регистры).
 * Вызывается из сверхбыстрого прерывания Brown-out, когда Flash тереть уже нет времени.
 */
void platform_emergency_backup_save(const void *data, size_t len);

/**
 * Чтение сохраненного аварийного бэкапа при старте.
 */
bool platform_emergency_backup_load(void *out_data, size_t len);

#endif // PLATFORM_PLATFORM_H
