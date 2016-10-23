#undef NDEBUG

#include "soundio_private.h"
#include "os.h"
#include "util.h"
#include "atomics.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

static inline void ok_or_panic(int err) {
    if (err)
        soundio_panic("%s", soundio_error_name(err));
}

static void test_os_get_time(void) {
    ok_or_panic(soundio_os_init());
    double prev_time = soundio_os_get_time();
    for (int i = 0; i < 1000; i += 1) {
        double time = soundio_os_get_time();
        assert(time >= prev_time);
        prev_time = time;
    }
}

static void write_callback(struct SoundIoOutStream *device, int frame_count_min, int frame_count_max) { }
static void error_callback(struct SoundIoOutStream *device, enum SoundIoError err) { }

static void test_create_outstream(void) {
    struct SoundIo *soundio = soundio_create();
    assert(soundio);
    ok_or_panic(soundio_connect(soundio));
    soundio_flush_events(soundio);
    int default_out_device_index = soundio_default_output_device_index(soundio);
    assert(default_out_device_index >= 0);
    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    assert(device);
    struct SoundIoOutStream *outstream = soundio_outstream_create(device);
    outstream->format = SoundIoFormatFloat32NE;
    outstream->sample_rate = 48000;
    outstream->layout = device->layouts[0];
    outstream->software_latency = 0.1;
    outstream->write_callback = write_callback;
    outstream->error_callback = error_callback;

    ok_or_panic(soundio_outstream_open(outstream));

    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);
    soundio = NULL;
    soundio_destroy(soundio);
}


static void test_ring_buffer_basic(void) {
    struct SoundIo *soundio = soundio_create();
    assert(soundio);
    struct SoundIoRingBuffer *rb = soundio_ring_buffer_create(soundio, 10);
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

static struct SoundIoRingBuffer *rb = NULL;
static const int rb_size = 3528;
static long expected_write_head;
static long expected_read_head;
static struct SoundIoAtomicBool rb_done;
static struct SoundIoAtomicInt rb_write_it;
static struct SoundIoAtomicInt rb_read_it;

// just for testing purposes; does not need to be high quality random
static double random_double(void) {
    return ((double)rand() / (double)RAND_MAX);
}

static void reader_thread_run(void *arg) {
    while (!SOUNDIO_ATOMIC_LOAD(rb_done)) {
        SOUNDIO_ATOMIC_FETCH_ADD(rb_read_it, 1);
        int fill_count = soundio_ring_buffer_fill_count(rb);
        assert(fill_count >= 0);
        assert(fill_count <= rb_size);
        int amount_to_read = soundio_int_min(random_double() * 2.0 * fill_count, fill_count);
        soundio_ring_buffer_advance_read_ptr(rb, amount_to_read);
        expected_read_head += amount_to_read;
    }
}

static void writer_thread_run(void *arg) {
    while (!SOUNDIO_ATOMIC_LOAD(rb_done)) {
        SOUNDIO_ATOMIC_FETCH_ADD(rb_write_it, 1);
        int fill_count = soundio_ring_buffer_fill_count(rb);
        assert(fill_count >= 0);
        assert(fill_count <= rb_size);
        int free_count = rb_size - fill_count;
        assert(free_count >= 0);
        assert(free_count <= rb_size);
        int value = soundio_int_min(random_double() * 2.0 * free_count, free_count);
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
    SOUNDIO_ATOMIC_STORE(rb_read_it, 0);
    SOUNDIO_ATOMIC_STORE(rb_write_it, 0);
    SOUNDIO_ATOMIC_STORE(rb_done, false);

    struct SoundIoOsThread *reader_thread;
    ok_or_panic(soundio_os_thread_create(reader_thread_run, NULL, NULL, &reader_thread));

    struct SoundIoOsThread *writer_thread;
    ok_or_panic(soundio_os_thread_create(writer_thread_run, NULL, NULL, &writer_thread));

    while (SOUNDIO_ATOMIC_LOAD(rb_read_it) < 100000 || SOUNDIO_ATOMIC_LOAD(rb_write_it) < 100000) {}
    SOUNDIO_ATOMIC_STORE(rb_done, true);

    soundio_os_thread_destroy(reader_thread);
    soundio_os_thread_destroy(writer_thread);

    int fill_count = soundio_ring_buffer_fill_count(rb);
    int expected_fill_count = expected_write_head - expected_read_head;
    assert(fill_count == expected_fill_count);
    soundio_destroy(soundio);
}

static void test_mirrored_memory(void) {
    struct SoundIoOsMirroredMemory mem;
    ok_or_panic(soundio_os_init());

    static const int requested_bytes = 1024;
    ok_or_panic(soundio_os_init_mirrored_memory(&mem, requested_bytes));
    const int size_bytes = mem.capacity;

    for (int i = 0; i < size_bytes; i += 1) {
        mem.address[i] = rand() % CHAR_MAX;
    }
    for (int i = 0; i < size_bytes; i += 1) {
        assert(mem.address[i] == mem.address[size_bytes+i]);
    }

    soundio_os_deinit_mirrored_memory(&mem);
}

static void test_nearest_sample_rate(void) {
    struct SoundIoDevice device;
    struct SoundIoSampleRateRange sample_rates[2] = {
        {
            44100,
            48000
        },
        {
            96000,
            96000,
        },
    };

    device.sample_rate_count = 2;
    device.sample_rates = sample_rates;

    assert(soundio_device_nearest_sample_rate(&device, 100) == 44100);
    assert(soundio_device_nearest_sample_rate(&device, 44099) == 44100);
    assert(soundio_device_nearest_sample_rate(&device, 44100) == 44100);
    assert(soundio_device_nearest_sample_rate(&device, 45000) == 45000);
    assert(soundio_device_nearest_sample_rate(&device, 48000) == 48000);
    assert(soundio_device_nearest_sample_rate(&device, 48001) == 96000);
    assert(soundio_device_nearest_sample_rate(&device, 90000) == 96000);
    assert(soundio_device_nearest_sample_rate(&device, 96001) == 96000);
    assert(soundio_device_nearest_sample_rate(&device, 9999999) == 96000);
}

struct Test {
    const char *name;
    void (*fn)(void);
};

static struct Test tests[] = {
    {"os_get_time", test_os_get_time},
    {"create output stream", test_create_outstream},
    {"mirrored memory", test_mirrored_memory},
    {"soundio_device_nearest_sample_rate", test_nearest_sample_rate},
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
    const char *match = NULL;

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
