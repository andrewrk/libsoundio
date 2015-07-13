#undef NDEBUG

#include "soundio.hpp"
#include "os.hpp"
#include "util.hpp"
#include "atomics.hpp"

#include <stdio.h>
#include <string.h>
#include <assert.h>

static inline void ok_or_panic(int err) {
    if (err)
        soundio_panic("%s", soundio_strerror(err));
}

static void test_os_get_time(void) {
    double prev_time = soundio_os_get_time();
    for (int i = 0; i < 1000; i += 1) {
        double time = soundio_os_get_time();
        assert(time >= prev_time);
        prev_time = time;
    }
}

static void write_callback(struct SoundIoOutStream *device, int frame_count) { }
static void underrun_callback(struct SoundIoOutStream *device) { }

static void test_create_outstream(void) {
    struct SoundIo *soundio = soundio_create();
    assert(soundio);
    ok_or_panic(soundio_connect(soundio));
    int default_out_device_index = soundio_get_default_output_device_index(soundio);
    assert(default_out_device_index >= 0);
    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    assert(device);
    struct SoundIoOutStream *outstream = soundio_outstream_create(device);
    outstream->format = SoundIoFormatFloat32NE;
    outstream->sample_rate = 48000;
    outstream->layout = device->layouts[0];
    outstream->latency = 0.1;
    outstream->write_callback = write_callback;
    outstream->underrun_callback = underrun_callback;

    ok_or_panic(soundio_outstream_open(outstream));

    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);
}


static void test_ring_buffer_basic(void) {
    struct SoundIo *soundio = soundio_create();
    assert(soundio);
    SoundIoRingBuffer *rb = soundio_ring_buffer_create(soundio, 10);
    assert(rb);

    int page_size = soundio_os_page_size();

    assert(soundio_ring_buffer_capacity(rb) == page_size);

    char *write_ptr = soundio_ring_buffer_write_ptr(rb);
    int amt = sprintf(write_ptr, "hello") + 1;
    soundio_ring_buffer_advance_write_ptr(rb, amt);

    assert(soundio_ring_buffer_fill_count(rb) == amt);
    assert(soundio_ring_buffer_free_count(rb) == page_size - amt);

    char *read_ptr = soundio_ring_buffer_read_ptr(rb);

    assert(strcmp(read_ptr, "hello") == 0);

    soundio_ring_buffer_advance_read_ptr(rb, amt);

    assert(soundio_ring_buffer_fill_count(rb) == 0);
    assert(soundio_ring_buffer_free_count(rb) == soundio_ring_buffer_capacity(rb));

    soundio_ring_buffer_advance_write_ptr(rb, page_size - 2);
    soundio_ring_buffer_advance_read_ptr(rb, page_size - 2);
    amt = sprintf(soundio_ring_buffer_write_ptr(rb), "writing past the end") + 1;
    soundio_ring_buffer_advance_write_ptr(rb, amt);

    assert(soundio_ring_buffer_fill_count(rb) == amt);

    assert(strcmp(soundio_ring_buffer_read_ptr(rb), "writing past the end") == 0);

    soundio_ring_buffer_advance_read_ptr(rb, amt);

    assert(soundio_ring_buffer_fill_count(rb) == 0);
    assert(soundio_ring_buffer_free_count(rb) == soundio_ring_buffer_capacity(rb));
    soundio_ring_buffer_destroy(rb);
    soundio_destroy(soundio);
}

static SoundIoRingBuffer *rb = nullptr;
static const int rb_size = 3528;
static long expected_write_head;
static long expected_read_head;
static atomic_bool rb_done;
static atomic_int rb_write_it;
static atomic_int rb_read_it;

// just for testing purposes; does not need to be high quality random
static double random_double(void) {
    return ((double)rand() / (double)RAND_MAX);
}

static void reader_thread_run(void *) {
    while (!rb_done) {
        rb_read_it += 1;
        int fill_count = soundio_ring_buffer_fill_count(rb);
        assert(fill_count >= 0);
        assert(fill_count <= rb_size);
        int amount_to_read = min((int)(random_double() * 2.0 * fill_count), fill_count);
        soundio_ring_buffer_advance_read_ptr(rb, amount_to_read);
        expected_read_head += amount_to_read;
    }
}

static void writer_thread_run(void *) {
    while (!rb_done) {
        rb_write_it += 1;
        int fill_count = soundio_ring_buffer_fill_count(rb);
        assert(fill_count >= 0);
        assert(fill_count <= rb_size);
        int free_count = rb_size - fill_count;
        assert(free_count >= 0);
        assert(free_count <= rb_size);
        int value = min((int)(random_double() * 2.0 * free_count), free_count);
        soundio_ring_buffer_advance_write_ptr(rb, value);
        expected_write_head += value;
    }
}

static void test_ring_buffer_threaded(void) {
    struct SoundIo *soundio = soundio_create();
    assert(soundio);
    rb = soundio_ring_buffer_create(soundio, rb_size);
    expected_write_head = 0;
    expected_read_head = 0;
    rb_read_it = 0;
    rb_write_it = 0;
    rb_done = false;

    SoundIoOsThread *reader_thread;
    ok_or_panic(soundio_os_thread_create(reader_thread_run, nullptr, false, &reader_thread));

    SoundIoOsThread *writer_thread;
    ok_or_panic(soundio_os_thread_create(writer_thread_run, nullptr, false, &writer_thread));

    while (rb_read_it < 100000 || rb_write_it < 100000) {}
    rb_done = true;

    soundio_os_thread_destroy(reader_thread);
    soundio_os_thread_destroy(writer_thread);

    int fill_count = soundio_ring_buffer_fill_count(rb);
    int expected_fill_count = expected_write_head - expected_read_head;
    assert(fill_count == expected_fill_count);
    soundio_destroy(soundio);
}

struct Test {
    const char *name;
    void (*fn)(void);
};

static struct Test tests[] = {
    {"os_get_time", test_os_get_time},
    {"create output stream", test_create_outstream},
    {"ring buffer basic", test_ring_buffer_basic},
    {"ring buffer threaded", test_ring_buffer_threaded},
    {NULL, NULL},
};

static void exec_test(struct Test *test) {
    fprintf(stderr, "testing %s...", test->name);
    test->fn();
    fprintf(stderr, "OK\n");
}

int main(int argc, char *argv[]) {
    const char *match = nullptr;

    if (argc == 2)
        match = argv[1];

    struct Test *test = &tests[0];

    while (test->name) {
        if (!match || strstr(test->name, match))
            exec_test(test);
        test += 1;
    }

    return 0;
}
