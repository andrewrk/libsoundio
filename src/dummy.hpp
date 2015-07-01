/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_DUMMY_HPP
#define SOUNDIO_DUMMY_HPP

#include "os.hpp"
#include "dummy_ring_buffer.hpp"
#include <atomic>
using std::atomic_flag;

struct SoundIoOutputDeviceDummy {
    struct SoundIoOsThread *thread;
    struct SoundIoOsMutex *mutex;
    struct SoundIoOsCond *cond;
    atomic_flag abort_flag;
    int buffer_size;
    double period;
    struct SoundIoDummyRingBuffer ring_buffer;
};

struct SoundIoInputDeviceDummy {

};

int soundio_dummy_init(struct SoundIo *soundio);

#endif


