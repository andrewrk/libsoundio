/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "util.h"

void soundio_panic(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}

char *soundio_alloc_sprintf(int *len, const char *format, ...) {
    va_list ap, ap2;
    va_start(ap, format);
    va_copy(ap2, ap);

    int len1 = vsnprintf(NULL, 0, format, ap);
    assert(len1 >= 0);

    size_t required_size = len1 + 1;
    char *mem = ALLOCATE(char, required_size);
    if (!mem)
        return NULL;

#ifndef NDEBUG
    int len2 =
#endif
		vsnprintf(mem, required_size, format, ap2);
    assert(len2 == len1);

    va_end(ap2);
    va_end(ap);

    if (len)
        *len = len1;
    return mem;
}
