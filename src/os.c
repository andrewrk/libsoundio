/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#undef _POSIX_C_SOURCE
#else
#define _GNU_SOURCE
#endif

#include "os.h"
#include "soundio_internal.h"
#include "util.h"

#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

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
#include <objbase.h>

#else

#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

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

#ifdef __ANDROID__
#include <fcntl.h>
#include <linux/ashmem.h>
#include <sys/ioctl.h>
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
static const uintptr_t notify_ident = 1;
struct SoundIoOsCond {
    int kq_id;
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
static INIT_ONCE win32_init_once = INIT_ONCE_STATIC_INIT;
static double win32_time_resolution;
static SYSTEM_INFO win32_system_info;
#else
static bool initialized = false;
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
#if defined(__MACH__)
static clock_serv_t cclock;
#endif
#endif

static int page_size;

double soundio_os_get_time(void) {
#if defined(SOUNDIO_OS_WINDOWS)
    unsigned __int64 time;
    QueryPerformanceCounter((LARGE_INTEGER*) &time);
    return time * win32_time_resolution;
#elif defined(__MACH__)
    mach_timespec_t mts;

    kern_return_t err = clock_get_time(cclock, &mts);
    assert(!err);

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
    HRESULT err = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    assert(err == S_OK);
    thread->run(thread->arg);
    CoUninitialize();
    return 0;
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
#endif

int soundio_os_thread_create(
        void (*run)(void *arg), void *arg,
        struct SoundIo *soundio,
        struct SoundIoOsThread **out_thread)
{
    *out_thread = NULL;

    struct SoundIoOsThread *thread = ALLOCATE(struct SoundIoOsThread, 1);
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
    if (soundio) {
        if (!SetThreadPriority(thread->handle, THREAD_PRIORITY_TIME_CRITICAL)) {
            soundio->emit_rtprio_warning(soundio);
        }
    }
#else
    int err;
    if ((err = pthread_attr_init(&thread->attr))) {
        soundio_os_thread_destroy(thread);
        return SoundIoErrorNoMem;
    }
    thread->attr_init = true;
    
    if (soundio) {
        int max_priority = sched_get_priority_max(SCHED_FIFO);
        if (max_priority == -1) {
            soundio_os_thread_destroy(thread);
            return SoundIoErrorSystemResources;
        }

        if ((err = pthread_attr_setschedpolicy(&thread->attr, SCHED_FIFO))) {
            soundio_os_thread_destroy(thread);
            return SoundIoErrorSystemResources;
        }

        struct sched_param param;
        param.sched_priority = max_priority;
        if ((err = pthread_attr_setschedparam(&thread->attr, &param))) {
            soundio_os_thread_destroy(thread);
            return SoundIoErrorSystemResources;
        }

    }

    if ((err = pthread_create(&thread->id, &thread->attr, run_pthread, thread))) {
        if (err == EPERM && soundio) {
            soundio->emit_rtprio_warning(soundio);
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

    free(thread);
}

struct SoundIoOsMutex *soundio_os_mutex_create(void) {
    struct SoundIoOsMutex *mutex = ALLOCATE(struct SoundIoOsMutex, 1);
    if (!mutex) {
        soundio_os_mutex_destroy(mutex);
        return NULL;
    }

#if defined(SOUNDIO_OS_WINDOWS)
    InitializeCriticalSection(&mutex->id);
#else
    int err;
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

    free(mutex);
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
    struct SoundIoOsCond *cond = ALLOCATE(struct SoundIoOsCond, 1);

    if (!cond) {
        soundio_os_cond_destroy(cond);
        return NULL;
    }

#if defined(SOUNDIO_OS_WINDOWS)
    InitializeConditionVariable(&cond->id);
    InitializeCriticalSection(&cond->default_cs_id);
#elif defined(SOUNDIO_OS_KQUEUE)
    cond->kq_id = kqueue();
    if (cond->kq_id == -1)
        return NULL;
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
    close(cond->kq_id);
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

    free(cond);
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
    kev.ident = notify_ident;
    kev.filter = EVFILT_USER;
    kev.fflags = NOTE_TRIGGER;

    if (kevent(cond->kq_id, &kev, 1, NULL, 0, &timeout) == -1) {
        if (errno == EINTR)
            return;
        if (errno == ENOENT)
            return;
        assert(0); // kevent signal error
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

    if (locked_mutex)
        assert_no_err(pthread_mutex_unlock(&locked_mutex->id));

    memset(&kev, 0, sizeof(kev));
    kev.ident = notify_ident;
    kev.filter = EVFILT_USER;
    kev.flags = EV_ADD | EV_CLEAR;

    // this time is relative
    struct timespec timeout;
    timeout.tv_nsec = (seconds * 1000000000L);
    timeout.tv_sec  = timeout.tv_nsec / 1000000000L;
    timeout.tv_nsec = timeout.tv_nsec % 1000000000L;

    if (kevent(cond->kq_id, &kev, 1, &out_kev, 1, &timeout) == -1) {
        if (errno == EINTR)
            return;
        assert(0); // kevent wait error
    }
    if (locked_mutex)
        assert_no_err(pthread_mutex_lock(&locked_mutex->id));
#else
    pthread_mutex_t *target_mutex;
    if (locked_mutex) {
        target_mutex = &locked_mutex->id;
    } else {
        target_mutex = &cond->default_mutex_id;
        assert_no_err(pthread_mutex_lock(target_mutex));
    }
    // this time is absolute
    struct timespec tms;
    clock_gettime(CLOCK_MONOTONIC, &tms);
    tms.tv_nsec += (seconds * 1000000000L);
    tms.tv_sec += tms.tv_nsec / 1000000000L;
    tms.tv_nsec = tms.tv_nsec % 1000000000L;
    int err;
    if ((err = pthread_cond_timedwait(&cond->id, target_mutex, &tms))) {
        assert(err != EPERM);
        assert(err != EINVAL);
    }
    if (!locked_mutex)
        assert_no_err(pthread_mutex_unlock(target_mutex));
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

    if (locked_mutex)
        assert_no_err(pthread_mutex_unlock(&locked_mutex->id));

    memset(&kev, 0, sizeof(kev));
    kev.ident = notify_ident;
    kev.filter = EVFILT_USER;
    kev.flags = EV_ADD | EV_CLEAR;

    if (kevent(cond->kq_id, &kev, 1, &out_kev, 1, NULL) == -1) {
        if (errno == EINTR)
            return;
        assert(0); // kevent wait error
    }
    if (locked_mutex)
        assert_no_err(pthread_mutex_lock(&locked_mutex->id));
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

static int internal_init(void) {
#if defined(SOUNDIO_OS_WINDOWS)
    unsigned __int64 frequency;
    if (QueryPerformanceFrequency((LARGE_INTEGER*) &frequency)) {
        win32_time_resolution = 1.0 / (double) frequency;
    } else {
        return SoundIoErrorSystemResources;
    }
    GetSystemInfo(&win32_system_info);
    page_size = win32_system_info.dwAllocationGranularity;
#else
    page_size = sysconf(_SC_PAGESIZE);
#if defined(__MACH__)
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
#endif
#endif
    return 0;
}

int soundio_os_init(void) {
    int err;
#if defined(SOUNDIO_OS_WINDOWS)
    PVOID lpContext;
    BOOL pending;

    if (!InitOnceBeginInitialize(&win32_init_once, INIT_ONCE_ASYNC, &pending, &lpContext))
        return SoundIoErrorSystemResources;

    if (!pending)
        return 0;

    if ((err = internal_init()))
        return err;

    if (!InitOnceComplete(&win32_init_once, INIT_ONCE_ASYNC, NULL))
        return SoundIoErrorSystemResources;
#else
    assert_no_err(pthread_mutex_lock(&init_mutex));
    if (initialized) {
        assert_no_err(pthread_mutex_unlock(&init_mutex));
        return 0;
    }
    initialized = true;
    if ((err = internal_init()))
        return err;
    assert_no_err(pthread_mutex_unlock(&init_mutex));
#endif

    return 0;
}

int soundio_os_page_size(void) {
    return page_size;
}

static inline size_t ceil_dbl_to_size_t(double x) {
    const double truncation = (size_t)x;
    return truncation + (truncation < x);
}

int soundio_os_init_mirrored_memory(struct SoundIoOsMirroredMemory *mem, size_t requested_capacity) {
    size_t actual_capacity = ceil_dbl_to_size_t(requested_capacity / (double)page_size) * page_size;

#if defined(SOUNDIO_OS_WINDOWS)
    BOOL ok;
    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, actual_capacity * 2, NULL);
    if (!hMapFile)
        return SoundIoErrorNoMem;

    for (;;) {
        // find a free address space with the correct size
        char *address = (char*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, actual_capacity * 2);
        if (!address) {
            ok = CloseHandle(hMapFile);
            assert(ok);
            return SoundIoErrorNoMem;
        }

        // found a big enough address space. hopefully it will remain free
        // while we map to it. if not, we'll try again.
        ok = UnmapViewOfFile(address);
        assert(ok);

        char *addr1 = (char*)MapViewOfFileEx(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, actual_capacity, address);
        if (addr1 != address) {
            DWORD err = GetLastError();
            if (err == ERROR_INVALID_ADDRESS) {
                continue;
            } else {
                ok = CloseHandle(hMapFile);
                assert(ok);
                return SoundIoErrorNoMem;
            }
        }

        char *addr2 = (char*)MapViewOfFileEx(hMapFile, FILE_MAP_WRITE, 0, 0,
                actual_capacity, address + actual_capacity);
        if (addr2 != address + actual_capacity) {
            ok = UnmapViewOfFile(addr1);
            assert(ok);

            DWORD err = GetLastError();
            if (err == ERROR_INVALID_ADDRESS) {
                continue;
            } else {
                ok = CloseHandle(hMapFile);
                assert(ok);
                return SoundIoErrorNoMem;
            }
        }

        mem->priv = hMapFile;
        mem->address = address;
        break;
    }
#else

    int fd;
#ifdef __ANDROID__
    fd = open("/dev/ashmem", O_RDWR);
    if (fd < 0)
        return SoundIoErrorSystemResources;

    int ret = ioctl(fd, ASHMEM_SET_SIZE, actual_capacity);
    if (ret < 0) {
        close(fd);
        return SoundIoErrorSystemResources;
    }
#else
    char shm_path[] = "/dev/shm/soundio-XXXXXX";
    char tmp_path[] = "/tmp/soundio-XXXXXX";
    char *chosen_path;

    fd = mkstemp(shm_path);
    if (fd < 0) {
        fd = mkstemp(tmp_path);
        if (fd < 0) {
            return SoundIoErrorSystemResources;
        } else {
            chosen_path = tmp_path;
        }
    } else {
        chosen_path = shm_path;
    }

    if (unlink(chosen_path)) {
        close(fd);
        return SoundIoErrorSystemResources;
    }

    if (ftruncate(fd, actual_capacity)) {
        close(fd);
        return SoundIoErrorSystemResources;
    }
#endif

    char *address = (char*)mmap(NULL, actual_capacity * 2, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (address == MAP_FAILED) {
        close(fd);
        return SoundIoErrorNoMem;
    }

    char *other_address = (char*)mmap(address, actual_capacity, PROT_READ|PROT_WRITE,
            MAP_FIXED|MAP_SHARED, fd, 0);
    if (other_address != address) {
        munmap(address, 2 * actual_capacity);
        close(fd);
        return SoundIoErrorNoMem;
    }

    other_address = (char*)mmap(address + actual_capacity, actual_capacity,
            PROT_READ|PROT_WRITE, MAP_FIXED|MAP_SHARED, fd, 0);
    if (other_address != address + actual_capacity) {
        munmap(address, 2 * actual_capacity);
        close(fd);
        return SoundIoErrorNoMem;
    }

    mem->address = address;

    if (close(fd))
        return SoundIoErrorSystemResources;
#endif

    mem->capacity = actual_capacity;
    return 0;
}

void soundio_os_deinit_mirrored_memory(struct SoundIoOsMirroredMemory *mem) {
    if (!mem->address)
        return;
#if defined(SOUNDIO_OS_WINDOWS)
    BOOL ok;
    ok = UnmapViewOfFile(mem->address);
    assert(ok);
    ok = UnmapViewOfFile(mem->address + mem->capacity);
    assert(ok);
    ok = CloseHandle((HANDLE)mem->priv);
    assert(ok);
#else
    int err = munmap(mem->address, 2 * mem->capacity);
    assert(!err);
#endif
    mem->address = NULL;
}
