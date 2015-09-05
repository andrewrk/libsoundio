/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_UTIL_HPP
#define SOUNDIO_UTIL_HPP

#include <stdlib.h>
#include <string.h>
#include <assert.h>

template<typename T>
__attribute__((malloc)) static inline T *allocate_nonzero(size_t count) {
    return reinterpret_cast<T*>(malloc(count * sizeof(T)));
}

template<typename T>
__attribute__((malloc)) static inline T *allocate(size_t count) {
    return reinterpret_cast<T*>(calloc(count, sizeof(T)));
}

template<typename T>
static inline T *reallocate_nonzero(T * old, size_t new_count) {
    return reinterpret_cast<T*>(realloc(old, new_count * sizeof(T)));
}

void soundio_panic(const char *format, ...)
    __attribute__((cold))
    __attribute__ ((noreturn))
    __attribute__ ((format (printf, 1, 2)));

char *soundio_alloc_sprintf(int *len, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

static inline char *soundio_str_dupe(const char *str, int str_len) {
    char *out = allocate_nonzero<char>(str_len + 1);
    if (!out)
        return nullptr;
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


template <typename T, long n>
static constexpr long array_length(const T (&)[n]) {
    return n;
}

template <typename T>
static inline T max(T a, T b) {
    return (a >= b) ? a : b;
}

template <typename T>
static inline T min(T a, T b) {
    return (a <= b) ? a : b;
}

template<typename T>
static inline T clamp(T min_value, T value, T max_value) {
    return max(min(value, max_value), min_value);
}
#endif
