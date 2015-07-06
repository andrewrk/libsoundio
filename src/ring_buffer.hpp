/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_RING_BUFFER_HPP
#define SOUNDIO_RING_BUFFER_HPP

#include "atomics.hpp"

struct SoundIoRingBuffer {
    char *address;
    int capacity;
    atomic_long write_offset;
    atomic_long read_offset;
};

int soundio_ring_buffer_init(struct SoundIoRingBuffer *rb, int requested_capacity);
void soundio_ring_buffer_deinit(struct SoundIoRingBuffer *rb);

#endif
