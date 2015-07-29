/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_ATOMICS_HPP
#define SOUNDIO_ATOMICS_HPP

#include <atomic>
using std::atomic_flag;
using std::atomic_int;
using std::atomic_long;
using std::atomic_bool;
using std::atomic_uintptr_t;

#if ATOMIC_INT_LOCK_FREE != 2
#error "require atomic_int to be lock free"
#endif

#if ATOMIC_LONG_LOCK_FREE != 2
#error "require atomic_long to be lock free"
#endif

#if ATOMIC_BOOL_LOCK_FREE != 2
#error "require atomic_bool to be lock free"
#endif

#if ATOMIC_POINTER_LOCK_FREE != 2
#error "require atomic pointers to be lock free"
#endif

#endif
