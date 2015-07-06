/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "ring_buffer.hpp"
#include "soundio.hpp"
#include "util.hpp"

#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <errno.h>

struct SoundIoRingBuffer *soundio_ring_buffer_create(struct SoundIo *soundio, int requested_capacity) {
    SoundIoRingBuffer *rb = create<SoundIoRingBuffer>();

    if (!rb) {
        soundio_ring_buffer_destroy(rb);
        return nullptr;
    }

    if (soundio_ring_buffer_init(rb, requested_capacity)) {
        soundio_ring_buffer_destroy(rb);
        return nullptr;
    }

    return rb;
}

void soundio_ring_buffer_destroy(struct SoundIoRingBuffer *rb) {
    if (!rb)
        return;

    soundio_ring_buffer_deinit(rb);

    destroy(rb);
}

int soundio_ring_buffer_capacity(struct SoundIoRingBuffer *rb) {
    return rb->capacity;
}

char *soundio_ring_buffer_write_ptr(struct SoundIoRingBuffer *rb) {
    return rb->address + (rb->write_offset % rb->capacity);
}

void soundio_ring_buffer_advance_write_ptr(struct SoundIoRingBuffer *rb, int count) {
    rb->write_offset += count;
    assert(soundio_ring_buffer_fill_count(rb) >= 0);
}

char *soundio_ring_buffer_read_ptr(struct SoundIoRingBuffer *rb) {
    return rb->address + (rb->read_offset % rb->capacity);
}

void soundio_ring_buffer_advance_read_ptr(struct SoundIoRingBuffer *rb, int count) {
    rb->read_offset += count;
    assert(soundio_ring_buffer_fill_count(rb) >= 0);
}

int soundio_ring_buffer_fill_count(struct SoundIoRingBuffer *rb) {
    int count = rb->write_offset - rb->read_offset;
    assert(count >= 0);
    assert(count <= rb->capacity);
    return count;
}

int soundio_ring_buffer_free_count(struct SoundIoRingBuffer *rb) {
    return rb->capacity - soundio_ring_buffer_fill_count(rb);
}

void soundio_ring_buffer_clear(struct SoundIoRingBuffer *rb) {
    return rb->write_offset.store(rb->read_offset.load());
}

int soundio_ring_buffer_init(struct SoundIoRingBuffer *rb, int requested_capacity) {
    // round size up to the nearest power of two
    int pow2_size = powf(2, ceilf(log2(requested_capacity)));
    // at minimum must be page size
    int page_size = getpagesize();
    rb->capacity = max(pow2_size, page_size);

    rb->write_offset = 0;
    rb->read_offset = 0;

    char shm_path[] = "/dev/shm/ring-buffer-XXXXXX";
    int fd = mkstemp(shm_path);
    if (fd < 0)
        return SoundIoErrorSystemResources;

    if (unlink(shm_path))
        return SoundIoErrorSystemResources;

    if (ftruncate(fd, rb->capacity))
        return SoundIoErrorNoMem;

    rb->address = (char*)mmap(NULL, rb->capacity * 2, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (rb->address == MAP_FAILED)
        return SoundIoErrorNoMem;

    char *other_address = (char*)mmap(rb->address, rb->capacity,
            PROT_READ|PROT_WRITE, MAP_FIXED|MAP_SHARED, fd, 0);
    if (other_address != rb->address)
        return SoundIoErrorNoMem;

    other_address = (char*)mmap(rb->address + rb->capacity, rb->capacity,
            PROT_READ|PROT_WRITE, MAP_FIXED|MAP_SHARED, fd, 0);
    if (other_address != rb->address + rb->capacity)
        return SoundIoErrorNoMem;

    if (close(fd))
        return SoundIoErrorSystemResources;

    return 0;
}

void soundio_ring_buffer_deinit(struct SoundIoRingBuffer *rb) {
    if (munmap(rb->address, 2 * rb->capacity))
        soundio_panic("munmap failed: %s", strerror(errno));
}
