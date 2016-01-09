/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_ALSA_H
#define SOUNDIO_ALSA_H

#include "soundio_internal.h"
#include "os.h"
#include "list.h"
#include "atomics.h"

#include <alsa/asoundlib.h>

struct SoundIoPrivate;
int soundio_alsa_init(struct SoundIoPrivate *si);

struct SoundIoDeviceAlsa { int make_the_struct_not_empty; };

#define SOUNDIO_MAX_ALSA_SND_FILE_LEN 16
struct SoundIoAlsaPendingFile {
    char name[SOUNDIO_MAX_ALSA_SND_FILE_LEN];
};

SOUNDIO_MAKE_LIST_STRUCT(struct SoundIoAlsaPendingFile, SoundIoListAlsaPendingFile, SOUNDIO_LIST_STATIC)

struct SoundIoAlsa {
    struct SoundIoOsMutex *mutex;
    struct SoundIoOsCond *cond;

    struct SoundIoOsThread *thread;
    struct SoundIoAtomicFlag abort_flag;
    int notify_fd;
    int notify_wd;
    bool have_devices_flag;
    int notify_pipe_fd[2];
    struct SoundIoListAlsaPendingFile pending_files;

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
    snd_pcm_uframes_t buffer_size_frames;
    int sample_buffer_size;
    char *sample_buffer;
    int poll_fd_count;
    int poll_fd_count_with_extra;
    struct pollfd *poll_fds;
    int poll_exit_pipe_fd[2];
    struct SoundIoOsThread *thread;
    struct SoundIoAtomicFlag thread_exit_flag;
    snd_pcm_uframes_t period_size;
    int write_frame_count;
    bool is_paused;
    struct SoundIoAtomicFlag clear_buffer_flag;
    struct SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
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
    struct SoundIoOsThread *thread;
    struct SoundIoAtomicFlag thread_exit_flag;
    int period_size;
    int read_frame_count;
    bool is_paused;
    struct SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

#endif
