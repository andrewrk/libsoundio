/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_OS_HPP
#define SOUNDIO_OS_HPP

#include <stdbool.h>


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
struct SoundIoOsCond * soundio_os_cond_create(void);
void soundio_os_cond_destroy(struct SoundIoOsCond *cond);
void soundio_os_cond_signal(struct SoundIoOsCond *cond);
void soundio_os_cond_broadcast(struct SoundIoOsCond *cond);

void soundio_os_cond_timed_wait(struct SoundIoOsCond *cond,
        struct SoundIoOsMutex *mutex, double seconds);

#endif
