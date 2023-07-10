/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_ATOMICS_H
#define SOUNDIO_ATOMICS_H

#ifdef __cplusplus
#define SOUNDIO_USE_CPP_ATOMICS
#else // ifdef __cplusplus

#ifdef _MSC_VER
#define SOUNDIO_USE_MSVC_NATIVE_C_ATOMICS
#else
#define SOUNDIO_USE_UNIX_ATOMICS
#endif // ifdef _MSC_VER

#endif // ifdef __cplusplus


// Simple wrappers around atomic values so that the compiler will catch it if
// I accidentally use operators such as +, -, += on them.

#ifdef SOUNDIO_USE_CPP_ATOMICS

#include <atomic>

struct SoundIoAtomicLong {
    std::atomic<long> x;
};

struct SoundIoAtomicInt {
    std::atomic<int> x;
};

struct SoundIoAtomicBool {
    std::atomic<bool> x;
};

struct SoundIoAtomicFlag {
    std::atomic_flag x;
};

struct SoundIoAtomicULong {
    std::atomic<unsigned long> x;
};

#define SOUNDIO_ATOMIC_LOAD_(a) (a.x.load())
#define SOUNDIO_ATOMIC_FETCH_ADD_(a, delta) (a.x.fetch_add(delta))
#define SOUNDIO_ATOMIC_STORE_(a, value) (a.x.store(value))
#define SOUNDIO_ATOMIC_EXCHANGE_(a, value) (a.x.exchange(value))

#define SOUNDIO_ATOMIC_LOAD_BOOL SOUNDIO_ATOMIC_LOAD_
#define SOUNDIO_ATOMIC_STORE_BOOL SOUNDIO_ATOMIC_STORE_
#define SOUNDIO_ATOMIC_FETCH_ADD_BOOL SOUNDIO_ATOMIC_FETCH_ADD_
#define SOUNDIO_ATOMIC_EXCHANGE_BOOL SOUNDIO_ATOMIC_EXCHANGE_

#define SOUNDIO_ATOMIC_LOAD_INT SOUNDIO_ATOMIC_LOAD_
#define SOUNDIO_ATOMIC_STORE_INT SOUNDIO_ATOMIC_STORE_
#define SOUNDIO_ATOMIC_FETCH_ADD_INT SOUNDIO_ATOMIC_FETCH_ADD_
#define SOUNDIO_ATOMIC_EXCHANGE_INT SOUNDIO_ATOMIC_EXCHANGE_

#define SOUNDIO_ATOMIC_LOAD_LONG SOUNDIO_ATOMIC_LOAD_
#define SOUNDIO_ATOMIC_STORE_LONG SOUNDIO_ATOMIC_STORE_
#define SOUNDIO_ATOMIC_FETCH_ADD_LONG SOUNDIO_ATOMIC_FETCH_ADD_
#define SOUNDIO_ATOMIC_EXCHANGE_LONG SOUNDIO_ATOMIC_EXCHANGE_

#define SOUNDIO_ATOMIC_LOAD_ULONG SOUNDIO_ATOMIC_LOAD_
#define SOUNDIO_ATOMIC_STORE_ULONG SOUNDIO_ATOMIC_STORE_
#define SOUNDIO_ATOMIC_FETCH_ADD_ULONG SOUNDIO_ATOMIC_FETCH_ADD_
#define SOUNDIO_ATOMIC_EXCHANGE_ULONG SOUNDIO_ATOMIC_EXCHANGE_

#define SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(a) (a.x.test_and_set())
#define SOUNDIO_ATOMIC_FLAG_CLEAR(a) (a.x.clear())
#define SOUNDIO_ATOMIC_FLAG_INIT ATOMIC_FLAG_INIT

#endif

#ifdef SOUNDIO_USE_UNIX_ATOMICS

#include <stdatomic.h>

struct SoundIoAtomicLong {
    atomic_long x;
};

struct SoundIoAtomicInt {
    atomic_int x;
};

struct SoundIoAtomicBool {
    atomic_bool x;
};

struct SoundIoAtomicFlag {
    atomic_flag x;
};

struct SoundIoAtomicULong {
    atomic_ulong x;
};

#define SOUNDIO_ATOMIC_LOAD_(a) atomic_load(&a.x)
#define SOUNDIO_ATOMIC_FETCH_ADD_(a, delta) atomic_fetch_add(&a.x, delta)
#define SOUNDIO_ATOMIC_STORE_(a, value) atomic_store(&a.x, value)
#define SOUNDIO_ATOMIC_EXCHANGE_(a, value) atomic_exchange(&a.x, value)
#define SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(a) atomic_flag_test_and_set(&a.x)
#define SOUNDIO_ATOMIC_FLAG_CLEAR(a) atomic_flag_clear(&a.x)
#define SOUNDIO_ATOMIC_FLAG_INIT ATOMIC_FLAG_INIT

#define SOUNDIO_ATOMIC_LOAD_BOOL SOUNDIO_ATOMIC_LOAD_
#define SOUNDIO_ATOMIC_STORE_BOOL SOUNDIO_ATOMIC_STORE_
#define SOUNDIO_ATOMIC_FETCH_ADD_BOOL SOUNDIO_ATOMIC_FETCH_ADD_
#define SOUNDIO_ATOMIC_EXCHANGE_BOOL SOUNDIO_ATOMIC_EXCHANGE_

#define SOUNDIO_ATOMIC_LOAD_INT SOUNDIO_ATOMIC_LOAD_
#define SOUNDIO_ATOMIC_STORE_INT SOUNDIO_ATOMIC_STORE_
#define SOUNDIO_ATOMIC_FETCH_ADD_INT SOUNDIO_ATOMIC_FETCH_ADD_
#define SOUNDIO_ATOMIC_EXCHANGE_INT SOUNDIO_ATOMIC_EXCHANGE_

#define SOUNDIO_ATOMIC_LOAD_LONG SOUNDIO_ATOMIC_LOAD_
#define SOUNDIO_ATOMIC_STORE_LONG SOUNDIO_ATOMIC_STORE_
#define SOUNDIO_ATOMIC_FETCH_ADD_LONG SOUNDIO_ATOMIC_FETCH_ADD_
#define SOUNDIO_ATOMIC_EXCHANGE_LONG SOUNDIO_ATOMIC_EXCHANGE_

#define SOUNDIO_ATOMIC_LOAD_ULONG SOUNDIO_ATOMIC_LOAD_
#define SOUNDIO_ATOMIC_STORE_ULONG SOUNDIO_ATOMIC_STORE_
#define SOUNDIO_ATOMIC_FETCH_ADD_ULONG SOUNDIO_ATOMIC_FETCH_ADD_
#define SOUNDIO_ATOMIC_EXCHANGE_ULONG SOUNDIO_ATOMIC_EXCHANGE_

#endif

#ifdef SOUNDIO_USE_MSVC_NATIVE_C_ATOMICS
// When building for native msvc, we can leverage builtin atomics.

#ifdef _WIN64
#define SOUNDIO_MSVC_POINTER_SIZE 8
#else
#define SOUNDIO_MSVC_POINTER_SIZE 4
#endif

struct SoundIoAtomicLong {
    long x;
};

struct SoundIoAtomicInt {
    int x;
};

struct SoundIoAtomicBool {
    char x;
};

struct SoundIoAtomicFlag {
    long x;
};

struct SoundIoAtomicULong { 
    unsigned long x;
};

// When sticking to raw C and msvc, we rely on MSVC compiler intrinsics and are unable to
// Automatically deduce types at compile time. Must use the correct macro for the correct function.
//
// No headers needed. We use _Interlocked* family of compiler intrinsics. 
// Implementation referenced from the fantastic turf library's `atomic_msvc.h`

// How the living hell is it that to this day and age. msvc does not support typeof which both clang 
// and gcc support.
//
// guess this is why c11 in msvc can't implement stdatomic.h right now
#define SOUNDIO_ATOMIC_LOAD_BOOL(a) (char) *((volatile char*) &(a.x))
#define SOUNDIO_ATOMIC_STORE_BOOL(a, value) (*((volatile char*) &(a.x))) = value
#define SOUNDIO_ATOMIC_FETCH_ADD_BOOL(a, delta) _InterlockedExchangeAdd8((volatile char*) (&a.x), delta) 
#define SOUNDIO_ATOMIC_EXCHANGE_BOOL(a, value) _InterlockedExchange8((volatile char*) (&a.x), value)

#define SOUNDIO_ATOMIC_LOAD_INT(a) (int) *((volatile int*) &(a.x))
#define SOUNDIO_ATOMIC_STORE_INT(a, value) *((volatile int*) &(a.x)) = value
#define SOUNDIO_ATOMIC_FETCH_ADD_INT(a, delta) _InterlockedExchangeAdd((volatile LONG*) (&a.x), delta)
#define SOUNDIO_ATOMIC_EXCHANGE_INT(a, value) _InterlockedExchange((volatile LONG*) (&a.x), value)

#define SOUNDIO_ATOMIC_LOAD_LONG(a) SoundIoMSVCLoadLong(&a)
#define SOUNDIO_ATOMIC_STORE_LONG(a, value) SoundIoMSVCStoreLong((struct SoundIoAtomicLong*) &a, value)
#define SOUNDIO_ATOMIC_FETCH_ADD_LONG(a, delta) _InterlockedExchangeAdd64((long*) (&a.x), delta)
#define SOUNDIO_ATOMIC_EXCHANGE_LONG(a, value) _InterlockedExchange64((long*) (&a.x), delta)

#define SOUNDIO_ATOMIC_LOAD_ULONG(a) SoundIoMSVCLoadLong(&a)
#define SOUNDIO_ATOMIC_STORE_ULONG(a, value) SoundIoMSVCStoreLong((struct SoundIoAtomicULong*) &a, value)
#define SOUNDIO_ATOMIC_FETCH_ADD_ULONG(a, delta) _InterlockedExchangeAdd64((volatile long*) (&a.x), delta)
#define SOUNDIO_ATOMIC_EXCHANGE_ULONG(a, value) _InterlockedExchange64((volatile long*) (&a.x), delta)

#define SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(a) _interlockedbittestandset(&a.x, 0)
#define SOUNDIO_ATOMIC_FLAG_CLEAR(a) _interlockedbittestandreset(&a.x, 0)
#define SOUNDIO_ATOMIC_FLAG_INIT {1}

#define SOUNDIO_ATOMIC_LOAD(a) error
#define SOUNDIO_ATOMIC_FETCH_ADD(a, delta) error
#define SOUNDIO_ATOMIC_STORE(a, value) error
#define SOUNDIO_ATOMIC_EXCHANGE(a, value) error

static long SoundIoMSVCLoadLong(struct SoundIoAtomicLong* a)
{
#if SOUNDIO_MSVC_POINTER_SIZE == 8
    return ((volatile struct SoundIoAtomicLong*) a)->x;
#else

   uint64_t result;
    __asm {
        mov esi, object;
        mov ebx, eax;
        mov ecx, edx;
        lock cmpxchg8b [esi];
        mov dword ptr result, eax;
        mov dword ptr result[4], edx;
    }
    return result;

#endif
}

static void SoundIoMSVCStoreLong(struct SoundIoAtomicLong* a, long value)
{
#if SOUNDIO_MSVC_POINTER_SIZE == 8
    ((volatile struct SoundIoAtomicLong*) a)->x = value;
#else
    __asm {
        mov esi, object;
        mov ebx, dword ptr value;
        mov ecx, dword ptr value[4];
    retry:
        cmpxchg8b [esi];
        jne retry;
    }
#endif
}

#endif

#endif //SOUNDIO_ATOMICS_H

