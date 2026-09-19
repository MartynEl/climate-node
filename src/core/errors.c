#include "core/errors.h"

const char *err_str(err_t e)
{
    switch (e) {
    case ERR_OK:
        return "OK";
    case ERR_INVALID_ARG:
        return "INVALID_ARG";
    case ERR_OUT_OF_RANGE:
        return "OUT_OF_RANGE";
    case ERR_NOT_READY:
        return "NOT_READY";
    case ERR_WARMUP:
        return "WARMUP";
    case ERR_SENSOR:
        return "SENSOR";
    case ERR_MATH:
        return "MATH";
    case ERR_STATE:
        return "STATE";
    case ERR_COMM:
        return "COMM";
    case ERR_STORAGE:
        return "STORAGE";
    case ERR_CONFIG:
        return "CONFIG";
    case ERR_HW:
        return "HW";
    default:
        return "UNKNOWN";
    }
}