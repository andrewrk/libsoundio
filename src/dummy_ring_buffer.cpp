#include "dummy_ring_buffer.hpp"
#include "soundio.hpp"
#include <math.h>

int soundio_dummy_ring_buffer_create(int requested_capacity, struct SoundIoDummyRingBuffer **out) {
    *out = nullptr;
    SoundIoDummyRingBuffer *rb = create<SoundIoDummyRingBuffer>();

    if (!rb) {
        soundio_dummy_ring_buffer_destroy(rb);
        return SoundIoErrorNoMem;
    }

    int err;
    if ((err = soundio_dummy_ring_buffer_init(rb, requested_capacity))) {
        soundio_dummy_ring_buffer_destroy(rb);
        return SoundIoErrorNoMem;
    }

    *out = rb;
    return 0;
}

int soundio_dummy_ring_buffer_init(struct SoundIoDummyRingBuffer *rb, int requested_capacity) {
    // round size up to the nearest power of two
    rb->capacity = powf(2, ceilf(log2(requested_capacity)));

    rb->address = allocate_nonzero<char>(rb->capacity);
    if (!rb->address) {
        soundio_dummy_ring_buffer_deinit(rb);
        return SoundIoErrorNoMem;
    }

    return 0;
}

void soundio_dummy_ring_buffer_destroy(struct SoundIoDummyRingBuffer *rb) {
    if (!rb)
        return;

    soundio_dummy_ring_buffer_deinit(rb);

    destroy(rb);
}

void soundio_dummy_ring_buffer_deinit(struct SoundIoDummyRingBuffer *rb) {
    deallocate(rb, rb->capacity);
    rb->address = nullptr;
}

void soundio_dummy_ring_buffer_clear(struct SoundIoDummyRingBuffer *rb) {
    rb->write_offset.store(rb->read_offset.load());
}

int soundio_dummy_ring_buffer_free_count(struct SoundIoDummyRingBuffer *rb) {
    return rb->capacity - soundio_dummy_ring_buffer_fill_count(rb);
}

int soundio_dummy_ring_buffer_fill_count(struct SoundIoDummyRingBuffer *rb) {
    int count = rb->write_offset - rb->read_offset;
    assert(count >= 0);
    assert(count <= rb->capacity);
    return count;
}

void soundio_dummy_ring_buffer_advance_write_ptr(struct SoundIoDummyRingBuffer *rb, int count) {
    rb->write_offset += count;
    assert(soundio_dummy_ring_buffer_fill_count(rb) >= 0);
}

void soundio_dummy_ring_buffer_advance_read_ptr(struct SoundIoDummyRingBuffer *rb, int count) {
    rb->read_offset += count;
    assert(soundio_dummy_ring_buffer_fill_count(rb) >= 0);
}

