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
#include <new>

template<typename T>
__attribute__((malloc)) static inline T *allocate_nonzero(size_t count) {
    T *ptr = reinterpret_cast<T*>(malloc(count * sizeof(T)));
    if (ptr) {
        for (size_t i = 0; i < count; i++)
            new (&ptr[i]) T;
    }
    return ptr;
}

template<typename T>
__attribute__((malloc)) static inline T *allocate(size_t count) {
    T *ptr = reinterpret_cast<T*>(calloc(count, sizeof(T)));
    if (ptr) {
        for (size_t i = 0; i < count; i++)
            new (&ptr[i]) T;
    }
    return ptr;
}

template<typename T>
static inline T * reallocate_nonzero(T * old, size_t old_count, size_t new_count) {
    assert(old_count <= new_count);
    T * new_ptr = reinterpret_cast<T*>(realloc(old, new_count * sizeof(T)));
    if (new_ptr) {
        for (size_t i = old_count; i < new_count; i += 1)
            new (&new_ptr[i]) T;
    }
    return new_ptr;
}

template<typename T>
static inline void deallocate(T * ptr, size_t count) {
    if (ptr) {
        for (size_t i = 0; i < count; i += 1)
            ptr[i].~T();
    }
    // keep this outside the if so that the if statement can be optimized out
    // completely if T has no destructor
    free(ptr);
}

template<typename T, typename... Args>
__attribute__((malloc)) static inline T * create_nonzero(Args... args) {
    T * ptr = reinterpret_cast<T*>(malloc(sizeof(T)));
    if (ptr)
        new (ptr) T(args...);
    return ptr;
}

template<typename T, typename... Args>
__attribute__((malloc)) static inline T * create(Args... args) {
    T * ptr = reinterpret_cast<T*>(calloc(1, sizeof(T)));
    if (ptr)
        new (ptr) T(args...);
    return ptr;
}

template<typename T>
static inline void destroy(T * ptr) {
    if (ptr)
        ptr[0].~T();
    // keep this outside the if so that the if statement can be optimized out
    // completely if T has no destructor
    free(ptr);
}

void soundio_panic(const char *format, ...)
    __attribute__((cold))
    __attribute__ ((noreturn))
    __attribute__ ((format (printf, 1, 2)));

char *soundio_alloc_sprintf(int *len, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));


template <typename T, long n>
constexpr long array_length(const T (&)[n]) {
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
#endif
