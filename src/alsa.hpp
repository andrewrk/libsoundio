/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_ALSA_HPP
#define SOUNDIO_ALSA_HPP

#include "soundio_private.h"
#include "os.h"
#include "atomics.hpp"

#include <alsa/asoundlib.h>

int soundio_alsa_init(struct SoundIoPrivate *si);

struct SoundIoDeviceAlsa { };

struct SoundIoAlsa {
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;

    struct SoundIoOsThread *thread;
    atomic_flag abort_flag;
    int notify_fd;
    int notify_wd;
    atomic_bool have_devices_flag;
    int notify_pipe_fd[2];

    // this one is ready to be read with flush_events. protected by mutex
    struct SoundIoDevicesInfo *ready_devices_info;

    int shutdown_err;
    bool emitted_shutdown_cb;
};

struct SoundIoOutStreamAlsa {
    snd_pcm_t *handle;
    snd_pcm_chmap_t *chmap;
    int chmap_size;
    snd_pcm_uframes_t offset;
    snd_pcm_access_t access;
    int sample_buffer_size;
    char *sample_buffer;
    int poll_fd_count;
    struct pollfd *poll_fds;
    SoundIoOsThread *thread;
    atomic_flag thread_exit_flag;
    int period_size;
    int write_frame_count;
    SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

struct SoundIoInStreamAlsa {
    snd_pcm_t *handle;
    snd_pcm_chmap_t *chmap;
    int chmap_size;
    snd_pcm_uframes_t offset;
    snd_pcm_access_t access;
    int sample_buffer_size;
    char *sample_buffer;
    int poll_fd_count;
    struct pollfd *poll_fds;
    SoundIoOsThread *thread;
    atomic_flag thread_exit_flag;
    int period_size;
    int read_frame_count;
    SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

#endif
