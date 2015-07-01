#ifndef SOUNDIO_DUMMY_RING_BUFFER_HPP
#define SOUNDIO_DUMMY_RING_BUFFER_HPP

#include "util.hpp"
#include <atomic>
using std::atomic_long;

#ifndef ATOMIC_LONG_LOCK_FREE
#error "require atomic long to be lock free"
#endif

struct SoundIoDummyRingBuffer {
    char *address;
    long capacity;
    atomic_long write_offset;
    atomic_long read_offset;
};

int soundio_dummy_ring_buffer_create(int requested_capacity, struct SoundIoDummyRingBuffer **out);
void soundio_dummy_ring_buffer_destroy(struct SoundIoDummyRingBuffer *rb);
int soundio_dummy_ring_buffer_init(struct SoundIoDummyRingBuffer *rb, int requested_capacity);
void soundio_dummy_ring_buffer_deinit(struct SoundIoDummyRingBuffer *rb);

// must be called by the writer
void soundio_dummy_ring_buffer_clear(struct SoundIoDummyRingBuffer *rb);


// how much is available, ready for writing
int soundio_dummy_ring_buffer_free_count(struct SoundIoDummyRingBuffer *rb);

// how much of the buffer is used, ready for reading
int soundio_dummy_ring_buffer_fill_count(struct SoundIoDummyRingBuffer *rb);

void soundio_dummy_ring_buffer_advance_write_ptr(struct SoundIoDummyRingBuffer *rb, int count);
void soundio_dummy_ring_buffer_advance_read_ptr(struct SoundIoDummyRingBuffer *rb, int count);

#endif

