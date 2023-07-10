/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "ring_buffer.h"
#include "soundio_private.h"
#include "util.h"

#include <stdlib.h>

struct SoundIoRingBuffer *soundio_ring_buffer_create(struct SoundIo *soundio, int requested_capacity) {
    (void)soundio;
    (void)requested_capacity;

    struct SoundIoRingBuffer *rb = ALLOCATE(struct SoundIoRingBuffer, 1);

    assert(requested_capacity > 0);

    if (!rb) {
        soundio_ring_buffer_destroy(rb);
        return NULL;
    }

    if (soundio_ring_buffer_init(rb, requested_capacity)) {
        soundio_ring_buffer_destroy(rb);
        return NULL;
    }

    return rb;
}

void soundio_ring_buffer_destroy(struct SoundIoRingBuffer *rb) {
    if (!rb)
        return;

    soundio_ring_buffer_deinit(rb);

    free(rb);
}

int soundio_ring_buffer_capacity(struct SoundIoRingBuffer *rb) {
    return rb->capacity;
}

char *soundio_ring_buffer_write_ptr(struct SoundIoRingBuffer *rb) {
    unsigned long write_offset = SOUNDIO_ATOMIC_LOAD_ULONG(rb->write_offset);
    return rb->mem.address + (write_offset % rb->capacity);
}

void soundio_ring_buffer_advance_write_ptr(struct SoundIoRingBuffer *rb, int count) {
    SOUNDIO_ATOMIC_FETCH_ADD_ULONG(rb->write_offset, count);
    assert(soundio_ring_buffer_fill_count(rb) >= 0);
}

char *soundio_ring_buffer_read_ptr(struct SoundIoRingBuffer *rb) {
    unsigned long read_offset = SOUNDIO_ATOMIC_LOAD_ULONG(rb->read_offset);
    return rb->mem.address + (read_offset % rb->capacity);
}

void soundio_ring_buffer_advance_read_ptr(struct SoundIoRingBuffer *rb, int count) {
    SOUNDIO_ATOMIC_FETCH_ADD_ULONG(rb->read_offset, count);
    assert(soundio_ring_buffer_fill_count(rb) >= 0);
}

int soundio_ring_buffer_fill_count(struct SoundIoRingBuffer *rb) {
    // Whichever offset we load first might have a smaller value. So we load
    // the read_offset first.
    unsigned long read_offset = SOUNDIO_ATOMIC_LOAD_ULONG(rb->read_offset);
    unsigned long write_offset = SOUNDIO_ATOMIC_LOAD_ULONG(rb->write_offset);
    int count = write_offset - read_offset;
    assert(count >= 0);
    assert(count <= rb->capacity);
    return count;
}

int soundio_ring_buffer_free_count(struct SoundIoRingBuffer *rb) {
    return rb->capacity - soundio_ring_buffer_fill_count(rb);
}

void soundio_ring_buffer_clear(struct SoundIoRingBuffer *rb) {
    unsigned long read_offset = SOUNDIO_ATOMIC_LOAD_ULONG(rb->read_offset);
    SOUNDIO_ATOMIC_STORE_ULONG(rb->write_offset, read_offset);
}

int soundio_ring_buffer_init(struct SoundIoRingBuffer *rb, int requested_capacity) {
    int err;
    if ((err = soundio_os_init_mirrored_memory(&rb->mem, requested_capacity)))
        return err;
    SOUNDIO_ATOMIC_STORE_ULONG(rb->write_offset, 0);
    SOUNDIO_ATOMIC_STORE_ULONG(rb->read_offset, 0);
    rb->capacity = rb->mem.capacity;

    return 0;
}

void soundio_ring_buffer_deinit(struct SoundIoRingBuffer *rb) {
    soundio_os_deinit_mirrored_memory(&rb->mem);
}
