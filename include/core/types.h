#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Fixed-point units:
 *
 * temperature: 0.01 degC
 *   2250  = 22.50 degC
 *   -550  = -5.50 degC
 *
 * humidity: 0.01 %
 *   6500  = 65.00 %
 *   10000 = 100.00 %
 *
 * time: milliseconds
 */

typedef int32_t temp_cd_t;
typedef uint32_t rh_cp_t;
typedef uint32_t time_ms_t;

#define TEMP_CD_MIN (-4000)
#define TEMP_CD_MAX (8500)

#define RH_CP_MIN (0U)
#define RH_CP_MAX (10000U)

typedef enum {
    QUALITY_INVALID = 0,
    QUALITY_WARMUP,
    QUALITY_VALID,
    QUALITY_FAULT
} quality_t;

typedef struct {
    temp_cd_t temp_cd;
    rh_cp_t rh_cp;
    time_ms_t timestamp_ms;
    quality_t quality;
} sample_t;

#endif // CORE_TYPES_H