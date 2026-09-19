#ifndef CORE_DEWPOINT_H
#define CORE_DEWPOINT_H

#include "core/errors.h"
#include "core/types.h"

/*
 * Calculates dew point using Magnus-Tetens approximation.
 *
 * Input:
 *   temp_cd - temperature in 0.01 degC
 *   rh_cp   - relative humidity in 0.01 %
 *
 * Output:
 *   out_dew_cd - dew point in 0.01 degC
 *
 * The implementation uses integer Q10 arithmetic and a small lookup table
 * for ln(RH / 100), avoiding floating point on Cortex-M3-class MCUs.
 */
err_t dewpoint_calc_cd(temp_cd_t temp_cd, rh_cp_t rh_cp, temp_cd_t *out_dew_cd);

#endif // CORE_DEWPOINT_H