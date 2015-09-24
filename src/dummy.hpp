/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_DUMMY_HPP
#define SOUNDIO_DUMMY_HPP

#include "soundio_private.h"
#include "os.h"
#include "atomics.hpp"

int soundio_dummy_init(struct SoundIoPrivate *si);

struct SoundIoDummy {
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;
    bool devices_emitted;
};

struct SoundIoDeviceDummy { };

struct SoundIoStreamDummy {
    struct SoundIoOsThread *thread;
    struct SoundIoOsCond *cond;
    atomic_flag abort_flag;
    double period_duration;

    int buffer_frame_count;

    int out_frames_left;
    int write_frame_count;
    atomic_long out_buffer_write_offset;
    atomic_long out_buffer_read_offset;

    int in_frames_left;
    int read_frame_count;
    atomic_long in_buffer_write_offset;
    atomic_long in_buffer_read_offset;

    double playback_start_time;
    atomic_flag clear_buffer_flag;
    SoundIoChannelArea in_areas[SOUNDIO_MAX_CHANNELS];
    SoundIoChannelArea out_areas[SOUNDIO_MAX_CHANNELS];
};

#endif
