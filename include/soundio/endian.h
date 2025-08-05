/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_ENDIAN_H
#define SOUNDIO_ENDIAN_H

#if defined(__BIG_ENDIAN__)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__ARMEB__)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__THUMBEB__)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__AARCH64EB__)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(_MIPSEB)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__MIPSEB)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__MIPSEB__)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(_BIG_ENDIAN)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__sparc)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__sparc__)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(_POWER)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__powerpc__)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__ppc__)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__hpux)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__hppa)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(_POWER)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__s390__)
#define SOUNDIO_OS_BIG_ENDIAN
#elif defined(__LITTLE_ENDIAN__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__ARMEL__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__THUMBEL__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__AARCH64EL__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(_MIPSEL)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__MIPSEL)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__MIPSEL__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(_LITTLE_ENDIAN)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__i386__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__alpha__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__ia64)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__ia64__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(_M_IX86)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(_M_IA64)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(_M_ALPHA)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__amd64)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__amd64__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(_M_AMD64)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__x86_64)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__x86_64__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(_M_X64)
#define SOUNDIO_OS_LITTLE_ENDIAN
#elif defined(__bfin__)
#define SOUNDIO_OS_LITTLE_ENDIAN
#else
#error unable to detect endianness
#endif

#endif
