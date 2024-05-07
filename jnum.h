/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2019-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* URL: https://github.com/lengjingzju/json *
*******************************************/
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JNUM_NULL,
    JNUM_BOOL,
    JNUM_INT,
    JNUM_HEX,
    JNUM_LINT,
    JNUM_LHEX,
    JNUM_DOUBLE,
} jnum_type_t;

typedef union {
    bool vbool;
    int32_t vint;
    uint32_t vhex;
    int64_t vlint;
    uint64_t vlhex;
    double vdbl;
} jnum_value_t;

int jnum_itoa(int32_t num, char *buffer);
int jnum_ltoa(int64_t num, char *buffer);
int jnum_htoa(uint32_t num, char *buffer);
int jnum_lhtoa(uint64_t num, char *buffer);
int jnum_dtoa(double num, char *buffer);

int32_t jnum_atoi(const char *str);
int64_t jnum_atol(const char *str);
uint32_t jnum_atoh(const char *str);
uint64_t jnum_atolh(const char *str);
double jnum_atod(const char *str);
int jnum_parse(const char *str, jnum_type_t *type, jnum_value_t *value);

#ifdef __cplusplus
}
#endif
