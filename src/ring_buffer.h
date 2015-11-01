/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_RING_BUFFER_H
#define SOUNDIO_RING_BUFFER_H

#include "os.h"
#include "atomics.h"

struct SoundIoRingBuffer {
    struct SoundIoOsMirroredMemory mem;
    struct SoundIoAtomicLong write_offset;
    struct SoundIoAtomicLong read_offset;
    int capacity;
};

int soundio_ring_buffer_init(struct SoundIoRingBuffer *rb, int requested_capacity);
void soundio_ring_buffer_deinit(struct SoundIoRingBuffer *rb);

#endif
