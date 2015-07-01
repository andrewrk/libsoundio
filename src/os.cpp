/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "os.hpp"
#include "soundio.h"
#include "util.hpp"

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

struct SoundIoOsThread {
    pthread_attr_t attr;
    bool attr_init;

    pthread_t id;
    bool running;

    void *arg;
    void (*run)(void *arg);
};

struct SoundIoOsMutex {
    pthread_mutex_t id;
    bool id_init;
};

struct SoundIoOsCond {
    pthread_cond_t id;
    bool id_init;

    pthread_condattr_t attr;
    bool attr_init;
};

double soundio_os_get_time(void) {
    struct timespec tms;
    clock_gettime(CLOCK_MONOTONIC, &tms);
    double seconds = (double)tms.tv_sec;
    seconds += ((double)tms.tv_nsec) / 1000000000.0;
    return seconds;
}

static void assert_no_err(int err) {
    assert(!err);
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

int soundio_os_concurrency(void) {
    long cpu_core_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_core_count <= 0)
        cpu_core_count = 1;
    return cpu_core_count;
}

static void *run_thread(void *userdata) {
    struct SoundIoOsThread *thread = (struct SoundIoOsThread *)userdata;
    thread->run(thread->arg);
    return NULL;
}

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

    if ((err = pthread_create(&thread->id, &thread->attr, run_thread, thread))) {
        if (err == EPERM) {
            emit_rtprio_warning();
            err = pthread_create(&thread->id, NULL, run_thread, thread);
        }
        if (err) {
            soundio_os_thread_destroy(thread);
            return SoundIoErrorNoMem;
        }
    }
    thread->running = true;

    *out_thread = thread;
    return 0;
}

void soundio_os_thread_destroy(struct SoundIoOsThread *thread) {
    if (!thread)
        return;

    if (thread->running) {
        assert_no_err(pthread_join(thread->id, NULL));
    }

    if (thread->attr_init) {
        assert_no_err(pthread_attr_destroy(&thread->attr));
    }

    destroy(thread);
}

struct SoundIoOsMutex *soundio_os_mutex_create(void) {
    int err;

    struct SoundIoOsMutex *mutex = create<SoundIoOsMutex>();
    if (!mutex) {
        soundio_os_mutex_destroy(mutex);
        return NULL;
    }

    if ((err = pthread_mutex_init(&mutex->id, NULL))) {
        soundio_os_mutex_destroy(mutex);
        return NULL;
    }
    mutex->id_init = true;

    return mutex;
}

void soundio_os_mutex_destroy(struct SoundIoOsMutex *mutex) {
    if (!mutex)
        return;

    if (mutex->id_init) {
        assert_no_err(pthread_mutex_destroy(&mutex->id));
    }

    destroy(mutex);
}

void soundio_os_mutex_lock(struct SoundIoOsMutex *mutex) {
    assert_no_err(pthread_mutex_lock(&mutex->id));
}

void soundio_os_mutex_unlock(struct SoundIoOsMutex *mutex) {
    assert_no_err(pthread_mutex_unlock(&mutex->id));
}

struct SoundIoOsCond * soundio_os_cond_create(void) {
    struct SoundIoOsCond *cond = create<SoundIoOsCond>();

    if (!cond) {
        soundio_os_cond_destroy(cond);
        return NULL;
    }

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

    return cond;
}

void soundio_os_cond_destroy(struct SoundIoOsCond *cond) {
    if (!cond)
        return;

    if (cond->id_init) {
        assert_no_err(pthread_cond_destroy(&cond->id));
    }

    if (cond->attr_init) {
        assert_no_err(pthread_condattr_destroy(&cond->attr));
    }

    destroy(cond);
}

void soundio_os_cond_signal(struct SoundIoOsCond *cond) {
    assert_no_err(pthread_cond_signal(&cond->id));
}

void soundio_os_cond_broadcast(struct SoundIoOsCond *cond) {
    assert_no_err(pthread_cond_broadcast(&cond->id));
}

void soundio_os_cond_timed_wait(struct SoundIoOsCond *cond,
        struct SoundIoOsMutex *mutex, double seconds)
{
    struct timespec tms;
    clock_gettime(CLOCK_MONOTONIC, &tms);
    tms.tv_nsec += (seconds * 1000000000L);
    int err;
    if ((err = pthread_cond_timedwait(&cond->id, &mutex->id, &tms))) {
        assert(err != EPERM);
        assert(err != EINVAL);
    }
}

void soundio_os_cond_wait(struct SoundIoOsCond *cond,
        struct SoundIoOsMutex *mutex)
{
    int err;
    if ((err = pthread_cond_wait(&cond->id, &mutex->id))) {
        assert(err != EPERM);
        assert(err != EINVAL);
    }
}
