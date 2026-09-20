#ifndef CORE_ERRORS_H
#define CORE_ERRORS_H

typedef enum {
    ERR_OK = 0,
    ERR_INVALID_ARG,
    ERR_OUT_OF_RANGE,
    ERR_NOT_READY,
    ERR_WARMUP,
    ERR_SENSOR,
    ERR_MATH,
    ERR_STATE,
    ERR_COMM,
    ERR_STORAGE,
    ERR_CONFIG,
    ERR_HW,
    ERR_NOT_FOUND
} err_t;

const char *err_str(err_t e);

#endif // CORE_ERRORS_H