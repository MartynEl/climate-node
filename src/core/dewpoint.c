#include "core/dewpoint.h"

#include <stddef.h>

#include "dewpoint_lut.h"

/*
 * Magnus constants:
 *   a = 17.27
 *   b = 237.7 degC
 *
 * Q10 scale:
 *   1.0 = 1024
 *
 * a_q10 = round(17.27 * 1024) = 17684
 * b_cd  = round(237.7 * 100)  = 23770
 */
static const int32_t A_Q10 = 17684;
static const int32_t B_CD = 23770;

static int32_t div_round_pos_den(int32_t numerator, int32_t denominator)
{
    /*
     * denominator is expected to be positive in valid physical range.
     */
    if (numerator >= 0) {
        return (numerator + denominator / 2) / denominator;
    }

    return -((-numerator + denominator / 2) / denominator);
}

static int32_t ln_rh_q10(rh_cp_t rh_cp)
{
    /*
     * Clamp humidity to a safe finite range.
     * 0% humidity leads to log(0), so we clamp to 1.00%.
     * For climate control this is acceptable and prevents numeric faults.
     */
    if (rh_cp < 100U) {
        rh_cp = 100U;
    }

    if (rh_cp > RH_CP_MAX) {
        rh_cp = RH_CP_MAX;
    }

    /*
     * rh_cp is 0.01%, so:
     *   100  -> 1%
     *   10000 -> 100%
     *
     * Table index is integer percent 0..100.
     */
    uint32_t percent = rh_cp / 100U;
    uint32_t frac = rh_cp % 100U;

    int32_t y0 = dewpoint_ln_rh_q10[percent];
    int32_t y1 = (percent < 100U) ? dewpoint_ln_rh_q10[percent + 1U] : y0;

    return y0 + ((y1 - y0) * (int32_t)frac) / 100;
}

err_t dewpoint_calc_cd(temp_cd_t temp_cd, rh_cp_t rh_cp, temp_cd_t *out_dew_cd)
{
    if (out_dew_cd == NULL) {
        return ERR_INVALID_ARG;
    }

    if (temp_cd < TEMP_CD_MIN || temp_cd > TEMP_CD_MAX) {
        return ERR_OUT_OF_RANGE;
    }

    int32_t denom_t = B_CD + temp_cd;
    if (denom_t <= 0) {
        return ERR_MATH;
    }

    int32_t term1_q10 = div_round_pos_den(A_Q10 * temp_cd, denom_t);
    int32_t ln_q10 = ln_rh_q10(rh_cp);
    int32_t alpha_q10 = term1_q10 + ln_q10;

    int32_t denom_q10 = A_Q10 - alpha_q10;
    if (denom_q10 <= 0) {
        return ERR_MATH;
    }

    /*
     * Dew point in 0.01 degC:
     *
     * Td = b * alpha / (a - alpha)
     *
     * Since alpha is Q10 and a is Q10, ratio is unchanged.
     * b is represented in 0.01 degC.
     */
    int32_t dew_cd = div_round_pos_den(B_CD * alpha_q10, denom_q10);

    /*
     * Physical sanity:
     * dew point cannot be higher than air temperature.
     * It can be lower than sensor range in extremely dry conditions.
     */
    if (dew_cd > temp_cd) {
        dew_cd = temp_cd;
    }

    if (dew_cd < TEMP_CD_MIN) {
        dew_cd = TEMP_CD_MIN;
    }

    if (dew_cd > TEMP_CD_MAX) {
        dew_cd = TEMP_CD_MAX;
    }

    *out_dew_cd = dew_cd;
    return ERR_OK;
}