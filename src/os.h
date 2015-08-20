/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_OS_H
#define SOUNDIO_OS_H

#include <stdbool.h>
#include <stddef.h>

// safe to call from any thread(s) multiple times, but
// must be called at least once before calling any other os functions
// soundio_create calls this function.
int soundio_os_init(void);

double soundio_os_get_time(void);

struct SoundIoOsThread;
int soundio_os_thread_create(
        void (*run)(void *arg), void *arg,
        bool high_priority, struct SoundIoOsThread ** out_thread);

void soundio_os_thread_destroy(struct SoundIoOsThread *thread);


struct SoundIoOsMutex;
struct SoundIoOsMutex *soundio_os_mutex_create(void);
void soundio_os_mutex_destroy(struct SoundIoOsMutex *mutex);
void soundio_os_mutex_lock(struct SoundIoOsMutex *mutex);
void soundio_os_mutex_unlock(struct SoundIoOsMutex *mutex);

struct SoundIoOsCond;
struct SoundIoOsCond *soundio_os_cond_create(void);
void soundio_os_cond_destroy(struct SoundIoOsCond *cond);

// locked_mutex is optional. On systems that use mutexes for conditions, if you
// pass NULL, a mutex will be created and locked/unlocked for you. On systems
// that do not use mutexes for conditions, no mutex handling is necessary. If
// you already have a locked mutex available, pass it; this will be better on
// systems that use mutexes for conditions.
void soundio_os_cond_signal(struct SoundIoOsCond *cond,
        struct SoundIoOsMutex *locked_mutex);
void soundio_os_cond_timed_wait(struct SoundIoOsCond *cond,
        struct SoundIoOsMutex *locked_mutex, double seconds);
void soundio_os_cond_wait(struct SoundIoOsCond *cond,
        struct SoundIoOsMutex *locked_mutex);


int soundio_os_page_size(void);

// You may rely on the size of this struct as part of the API and ABI.
struct SoundIoOsMirroredMemory {
    size_t capacity;
    char *address;
    void *priv;
};

// returned capacity might be increased from capacity to be a multiple of the
// system page size
int soundio_os_init_mirrored_memory(struct SoundIoOsMirroredMemory *mem, size_t capacity);
void soundio_os_deinit_mirrored_memory(struct SoundIoOsMirroredMemory *mem);

#endif
