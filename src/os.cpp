/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "os.hpp"
#include "soundio.h"
#include "util.hpp"
#include "atomics.hpp"

#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#if defined(_WIN32)
#define SOUNDIO_OS_WINDOWS

#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#if !defined(VC_EXTRALEAN)
#define VC_EXTRALEAN
#endif

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#if !defined(UNICODE)
#define UNICODE
#endif

// require Windows 7 or later
#if WINVER < 0x0601
#undef WINVER
#define WINVER 0x0601
#endif
#if _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <mmsystem.h>

#else
#include <pthread.h>
#include <unistd.h>
#endif

#if defined(__FreeBSD__) || defined(__MACH__)
#define SOUNDIO_OS_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif

#if defined(__MACH__)
#include <mach/clock.h>
#include <mach/mach.h>
#endif

struct SoundIoOsThread {
#if defined(SOUNDIO_OS_WINDOWS)
    HANDLE handle;
    DWORD id;
#else
    pthread_attr_t attr;
    bool attr_init;

    pthread_t id;
    bool running;
#endif
    void *arg;
    void (*run)(void *arg);
};

struct SoundIoOsMutex {
#if defined(SOUNDIO_OS_WINDOWS)
    CRITICAL_SECTION id;
#else
    pthread_mutex_t id;
    bool id_init;
#endif
};

#if defined(SOUNDIO_OS_KQUEUE)
static int kq_id;
static atomic_uintptr_t next_notify_ident;
struct SoundIoOsCond {
    int notify_ident;
};
#elif defined(SOUNDIO_OS_WINDOWS)
struct SoundIoOsCond {
    CONDITION_VARIABLE id;
    CRITICAL_SECTION default_cs_id;
};
#else
struct SoundIoOsCond {
    pthread_cond_t id;
    bool id_init;

    pthread_condattr_t attr;
    bool attr_init;

    pthread_mutex_t default_mutex_id;
    bool default_mutex_init;
};
#endif

#if defined(SOUNDIO_OS_WINDOWS)
INIT_ONCE win32_init_once = INIT_ONCE_STATIC_INIT;
#else
atomic_bool initialized = ATOMIC_VAR_INIT(false);
pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#if defined(SOUNDIO_OS_WINDOWS)
static double win32_time_resolution;
#endif

double soundio_os_get_time(void) {
#if defined(SOUNDIO_OS_WINDOWS)
    unsigned __int64 time;
    QueryPerformanceCounter((LARGE_INTEGER*) &time);
    return time * win32_time_resolution;
#elif defined(__MACH__)
    clock_serv_t cclock;
    mach_timespec_t mts;

    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
    kern_return_t err = clock_get_time(cclock, &mts);
    assert(!err);
    mach_port_deallocate(mach_task_self(), cclock);

    double seconds = (double)mts.tv_sec;
    seconds += ((double)mts.tv_nsec) / 1000000000.0;

    return seconds;
#else
    struct timespec tms;
    clock_gettime(CLOCK_MONOTONIC, &tms);
    double seconds = (double)tms.tv_sec;
    seconds += ((double)tms.tv_nsec) / 1000000000.0;
    return seconds;
#endif
}

#if defined(SOUNDIO_OS_WINDOWS)
static DWORD WINAPI run_win32_thread(LPVOID userdata) {
    struct SoundIoOsThread *thread = (struct SoundIoOsThread *)userdata;
    thread->run(thread->arg);
    return 0;
}

static void win32_panic(const char *str) {
    DWORD err = GetLastError();
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    soundio_panic(str, messageBuffer);
    LocalFree(messageBuffer);
}
#else
static void assert_no_err(int err) {
    assert(!err);
}

static void *run_pthread(void *userdata) {
    struct SoundIoOsThread *thread = (struct SoundIoOsThread *)userdata;
    thread->run(thread->arg);
    return NULL;
}
static void emit_rtprio_warning(void) {
    static bool seen = false;
    if (seen)
        return;
    seen = true;
    fprintf(stderr, "warning: unable to set high priority thread: Operation not permitted\n");
    fprintf(stderr, "See https://github.com/andrewrk/genesis/wiki/"
            "warning:-unable-to-set-high-priority-thread:-Operation-not-permitted\n");
}
#endif

int soundio_os_thread_create(
        void (*run)(void *arg), void *arg,
        bool high_priority, struct SoundIoOsThread ** out_thread)
{
    *out_thread = NULL;

    struct SoundIoOsThread *thread = create<SoundIoOsThread>();
    if (!thread) {
        soundio_os_thread_destroy(thread);
        return SoundIoErrorNoMem;
    }

    thread->run = run;
    thread->arg = arg;

#if defined(SOUNDIO_OS_WINDOWS)
    thread->handle = CreateThread(NULL, 0, run_win32_thread, thread, 0, &thread->id);
    if (!thread->handle) {
        soundio_os_thread_destroy(thread);
        return SoundIoErrorSystemResources;
    }
    if (high_priority) {
        if (!SetThreadPriority(thread->handle, THREAD_PRIORITY_TIME_CRITICAL)) {
            win32_panic("unable to set high priority thread: %s"); // TODO don't panic
        }
    }
#else
    int err;
    if ((err = pthread_attr_init(&thread->attr))) {
        soundio_os_thread_destroy(thread);
        return SoundIoErrorNoMem;
    }
    thread->attr_init = true;
    
    if (high_priority) {
        int max_priority = sched_get_priority_max(SCHED_FIFO);
        if (max_priority == -1) {
            soundio_os_thread_destroy(thread);
            return SoundIoErrorSystemResources;
        }
        struct sched_param param;
        param.sched_priority = max_priority;
        if ((err = pthread_attr_setschedparam(&thread->attr, &param))) {
            soundio_os_thread_destroy(thread);
            return SoundIoErrorSystemResources;
        }

        if ((err = pthread_attr_setschedpolicy(&thread->attr, SCHED_FIFO))) {
            soundio_os_thread_destroy(thread);
            return SoundIoErrorSystemResources;
        }
    }

    if ((err = pthread_create(&thread->id, &thread->attr, run_pthread, thread))) {
        if (err == EPERM) {
            emit_rtprio_warning();
            err = pthread_create(&thread->id, NULL, run_pthread, thread);
        }
        if (err) {
            soundio_os_thread_destroy(thread);
            return SoundIoErrorNoMem;
        }
    }
    thread->running = true;
#endif

    *out_thread = thread;
    return 0;
}

void soundio_os_thread_destroy(struct SoundIoOsThread *thread) {
    if (!thread)
        return;

#if defined(SOUNDIO_OS_WINDOWS)
    if (thread->handle) {
        DWORD err = WaitForSingleObject(thread->handle, INFINITE);
        assert(err != WAIT_FAILED);
        BOOL ok = CloseHandle(thread->handle);
        assert(ok);
    }
#else

    if (thread->running) {
        assert_no_err(pthread_join(thread->id, NULL));
    }

    if (thread->attr_init) {
        assert_no_err(pthread_attr_destroy(&thread->attr));
    }
#endif

    destroy(thread);
}

struct SoundIoOsMutex *soundio_os_mutex_create(void) {
    int err;

    struct SoundIoOsMutex *mutex = create<SoundIoOsMutex>();
    if (!mutex) {
        soundio_os_mutex_destroy(mutex);
        return NULL;
    }

#if defined(SOUNDIO_OS_WINDOWS)
    InitializeCriticalSection(&mutex->id);
#else
    if ((err = pthread_mutex_init(&mutex->id, NULL))) {
        soundio_os_mutex_destroy(mutex);
        return NULL;
    }
    mutex->id_init = true;
#endif

    return mutex;
}

void soundio_os_mutex_destroy(struct SoundIoOsMutex *mutex) {
    if (!mutex)
        return;

#if defined(SOUNDIO_OS_WINDOWS)
    DeleteCriticalSection(&mutex->id);
#else
    if (mutex->id_init) {
        assert_no_err(pthread_mutex_destroy(&mutex->id));
    }
#endif

    destroy(mutex);
}

void soundio_os_mutex_lock(struct SoundIoOsMutex *mutex) {
#if defined(SOUNDIO_OS_WINDOWS)
    EnterCriticalSection(&mutex->id);
#else
    assert_no_err(pthread_mutex_lock(&mutex->id));
#endif
}

void soundio_os_mutex_unlock(struct SoundIoOsMutex *mutex) {
#if defined(SOUNDIO_OS_WINDOWS)
    LeaveCriticalSection(&mutex->id);
#else
    assert_no_err(pthread_mutex_unlock(&mutex->id));
#endif
}

struct SoundIoOsCond * soundio_os_cond_create(void) {
    struct SoundIoOsCond *cond = create<SoundIoOsCond>();

    if (!cond) {
        soundio_os_cond_destroy(cond);
        return NULL;
    }

#if defined(SOUNDIO_OS_WINDOWS)
    InitializeConditionVariable(&cond->id);
    InitializeCriticalSection(&cond->default_cs_id);
#elif defined(SOUNDIO_OS_KQUEUE)
    cond->notify_ident = next_notify_ident.fetch_add(1);
#else
    if (pthread_condattr_init(&cond->attr)) {
        soundio_os_cond_destroy(cond);
        return NULL;
    }
    cond->attr_init = true;

    if (pthread_condattr_setclock(&cond->attr, CLOCK_MONOTONIC)) {
        soundio_os_cond_destroy(cond);
        return NULL;
    }

    if (pthread_cond_init(&cond->id, &cond->attr)) {
        soundio_os_cond_destroy(cond);
        return NULL;
    }
    cond->id_init = true;

    if ((pthread_mutex_init(&cond->default_mutex_id, NULL))) {
        soundio_os_cond_destroy(cond);
        return NULL;
    }
    cond->default_mutex_init = true;
#endif

    return cond;
}

void soundio_os_cond_destroy(struct SoundIoOsCond *cond) {
    if (!cond)
        return;

#if defined(SOUNDIO_OS_WINDOWS)
    DeleteCriticalSection(&cond->default_cs_id);
#elif defined(SOUNDIO_OS_KQUEUE)
    // nothing to do
#else
    if (cond->id_init) {
        assert_no_err(pthread_cond_destroy(&cond->id));
    }

    if (cond->attr_init) {
        assert_no_err(pthread_condattr_destroy(&cond->attr));
    }
    if (cond->default_mutex_init) {
        assert_no_err(pthread_mutex_destroy(&cond->default_mutex_id));
    }
#endif

    destroy(cond);
}

void soundio_os_cond_signal(struct SoundIoOsCond *cond,
        struct SoundIoOsMutex *locked_mutex)
{
#if defined(SOUNDIO_OS_WINDOWS)
    if (locked_mutex) {
        WakeConditionVariable(&cond->id);
    } else {
        EnterCriticalSection(&cond->default_cs_id);
        WakeConditionVariable(&cond->id);
        LeaveCriticalSection(&cond->default_cs_id);
    }
#elif defined(SOUNDIO_OS_KQUEUE)
    struct kevent kev;
    struct timespec timeout = { 0, 0 };

    memset(&kev, 0, sizeof(kev));
    kev.ident = cond->notify_ident;
    kev.filter = EVFILT_USER;
    kev.fflags = NOTE_TRIGGER;

    if (kevent(kq_id, &kev, 1, NULL, 0, &timeout) == -1) {
        if (errno == EINTR)
            return;
        soundio_panic("kevent signal error: %s", strerror(errno));
    }
#else
    if (locked_mutex) {
        assert_no_err(pthread_cond_signal(&cond->id));
    } else {
        assert_no_err(pthread_mutex_lock(&cond->default_mutex_id));
        assert_no_err(pthread_cond_signal(&cond->id));
        assert_no_err(pthread_mutex_unlock(&cond->default_mutex_id));
    }
#endif
}

void soundio_os_cond_timed_wait(struct SoundIoOsCond *cond,
        struct SoundIoOsMutex *locked_mutex, double seconds)
{
#if defined(SOUNDIO_OS_WINDOWS)
    CRITICAL_SECTION *target_cs;
    if (locked_mutex) {
        target_cs = &locked_mutex->id;
    } else {
        target_cs = &cond->default_cs_id;
        EnterCriticalSection(&cond->default_cs_id);
    }
    DWORD ms = seconds * 1000.0;
    SleepConditionVariableCS(&cond->id, target_cs, ms);
    if (!locked_mutex)
        LeaveCriticalSection(&cond->default_cs_id);
#elif defined(SOUNDIO_OS_KQUEUE)
    struct kevent kev;
    struct kevent out_kev;

    memset(&kev, 0, sizeof(kev));
    kev.ident = cond->notify_ident;
    kev.filter = EVFILT_USER;
    kev.flags = EV_ADD | EV_CLEAR;

    // this time is relative
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = (seconds * 1000000000L);

    if (kevent(kq_id, &kev, 1, &out_kev, 1, &timeout) == -1) {
        if (errno == EINTR)
            return;
        soundio_panic("kevent wait error: %s", strerror(errno));
    }
#else
    pthread_mutex_t *target_mutex;
    if (locked_mutex) {
        target_mutex = &locked_mutex->id;
    } else {
        target_mutex = &cond->default_mutex_id;
        assert_no_err(pthread_mutex_lock(&cond->default_mutex_id));
    }
    // this time is absolute
    struct timespec tms;
    clock_gettime(CLOCK_MONOTONIC, &tms);
    tms.tv_nsec += (seconds * 1000000000L);
    int err;
    if ((err = pthread_cond_timedwait(&cond->id, target_mutex, &tms))) {
        assert(err != EPERM);
        assert(err != EINVAL);
    }
    if (!locked_mutex)
        assert_no_err(pthread_mutex_unlock(&cond->default_mutex_id));
#endif
}

void soundio_os_cond_wait(struct SoundIoOsCond *cond,
        struct SoundIoOsMutex *locked_mutex)
{
#if defined(SOUNDIO_OS_WINDOWS)
    CRITICAL_SECTION *target_cs;
    if (locked_mutex) {
        target_cs = &locked_mutex->id;
    } else {
        target_cs = &cond->default_cs_id;
        EnterCriticalSection(&cond->default_cs_id);
    }
    SleepConditionVariableCS(&cond->id, target_cs, INFINITE);
    if (!locked_mutex)
        LeaveCriticalSection(&cond->default_cs_id);
#elif defined(SOUNDIO_OS_KQUEUE)
    struct kevent kev;
    struct kevent out_kev;

    memset(&kev, 0, sizeof(kev));
    kev.ident = cond->notify_ident;
    kev.filter = EVFILT_USER;
    kev.flags = EV_ADD | EV_CLEAR;

    if (kevent(kq_id, &kev, 1, &out_kev, 1, NULL) == -1) {
        if (errno == EINTR)
            return;
        soundio_panic("kevent wait error: %s", strerror(errno));
    }
#else
    pthread_mutex_t *target_mutex;
    if (locked_mutex) {
        target_mutex = &locked_mutex->id;
    } else {
        target_mutex = &cond->default_mutex_id;
        assert_no_err(pthread_mutex_lock(&cond->default_mutex_id));
    }
    int err;
    if ((err = pthread_cond_wait(&cond->id, target_mutex))) {
        assert(err != EPERM);
        assert(err != EINVAL);
    }
    if (!locked_mutex)
        assert_no_err(pthread_mutex_unlock(&cond->default_mutex_id));
#endif
}

static void internal_init(void) {
#if defined(SOUNDIO_OS_KQUEUE)
    kq_id = kqueue();
    if (kq_id == -1)
        soundio_panic("unable to create kqueue: %s", strerror(errno));
    next_notify_ident.store(1);
#endif
#if defined(SOUNDIO_OS_WINDOWS)
    unsigned __int64 frequency;
    if (QueryPerformanceFrequency((LARGE_INTEGER*) &frequency)) {
        win32_time_resolution = 1.0 / (double) frequency;
    } else {
        win32_panic("unable to initialize high precision timer: %s");
    }
#endif
}

void soundio_os_init(void) {
#if defined(SOUNDIO_OS_WINDOWS)
    PVOID lpContext;
    BOOL pending;

    if (!InitOnceBeginInitialize(&win32_init_once, INIT_ONCE_ASYNC, &pending, &lpContext))
      win32_panic("InitOnceBeginInitialize failed: %s");

    if (!pending)
        return;

    internal_init();

    if (!InitOnceComplete(&win32_init_once, INIT_ONCE_ASYNC, nullptr))
        win32_panic("InitOnceComplete failed: %s");
#else
    if (initialized.load())
        return;
    assert_no_err(pthread_mutex_lock(&init_mutex));
    if (initialized.load()) {
        assert_no_err(pthread_mutex_unlock(&init_mutex));
        return;
    }
    initialized.store(true);
    internal_init();
    assert_no_err(pthread_mutex_unlock(&init_mutex));
#endif
}
