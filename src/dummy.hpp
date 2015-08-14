/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_DUMMY_HPP
#define SOUNDIO_DUMMY_HPP

#include "soundio/soundio.h"
#include "soundio/os.h"
#include "atomics.hpp"
#include "ring_buffer.hpp"

int soundio_dummy_init(struct SoundIoPrivate *si);

struct SoundIoDummy {
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;
    bool devices_emitted;
};

struct SoundIoDeviceDummy { };

struct SoundIoOutStreamDummy {
    struct SoundIoOsThread *thread;
    struct SoundIoOsCond *cond;
    atomic_flag abort_flag;
    double period_duration;
    int buffer_frame_count;
    int frames_left;
    int write_frame_count;
    struct SoundIoRingBuffer ring_buffer;
    double playback_start_time;
    SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

struct SoundIoInStreamDummy {
    struct SoundIoOsThread *thread;
    struct SoundIoOsCond *cond;
    atomic_flag abort_flag;
    double period_duration;
    int frames_left;
    int read_frame_count;
    int buffer_frame_count;
    struct SoundIoRingBuffer ring_buffer;
    SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

#endif
