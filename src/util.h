/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_UTIL_H
#define SOUNDIO_UTIL_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#define ALLOCATE_NONZERO(Type, count) malloc((count) * sizeof(Type))

#define ALLOCATE(Type, count) calloc(count, sizeof(Type))

#define REALLOCATE_NONZERO(Type, old, new_count) realloc(old, (new_count) * sizeof(Type))

#define ARRAY_LENGTH(array) (sizeof(array)/sizeof((array)[0]))

static inline int soundio_int_min(int a, int b) {
    return (a <= b) ? a : b;
}

static inline int soundio_int_max(int a, int b) {
    return (a >= b) ? a : b;
}

static inline int soundio_int_clamp(int min_value, int value, int max_value) {
    return soundio_int_max(soundio_int_min(value, max_value), min_value);
}

static inline double soundio_double_min(double a, double b) {
    return (a <= b) ? a : b;
}

static inline double soundio_double_max(double a, double b) {
    return (a >= b) ? a : b;
}

static inline double soundio_double_clamp(double min_value, double value, double max_value) {
    return soundio_double_max(soundio_double_min(value, max_value), min_value);
}

void soundio_panic(const char *format, ...)
    __attribute__((cold))
    __attribute__ ((noreturn))
    __attribute__ ((format (printf, 1, 2)));

char *soundio_alloc_sprintf(int *len, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

static inline char *soundio_str_dupe(const char *str, int str_len) {
    char *out = ALLOCATE_NONZERO(char, str_len + 1);
    if (!out)
        return NULL;
    memcpy(out, str, str_len);
    out[str_len] = 0;
    return out;
}

static inline bool soundio_streql(const char *str1, int str1_len, const char *str2, int str2_len) {
    if (str1_len != str2_len)
        return false;
    return memcmp(str1, str2, str1_len) == 0;
}

static inline int ceil_dbl_to_int(double x) {
    const double truncation = (int)x;
    return truncation + (truncation < x);
}

static inline double ceil_dbl(double x) {
    const double truncation = (long long) x;
    const double ceiling = truncation + (truncation < x);
    return ceiling;
}

#endif
