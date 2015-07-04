#undef NDEBUG

#include "soundio.h"
#include "os.hpp"
#include "util.hpp"

#include <stdio.h>
#include <string.h>
#include <assert.h>

static inline void ok_or_panic(int err) {
    if (err)
        soundio_panic("%s", soundio_error_string(err));
}

static void test_os_get_time(void) {
    double prev_time = soundio_os_get_time();
    for (int i = 0; i < 1000; i += 1) {
        double time = soundio_os_get_time();
        assert(time >= prev_time);
        prev_time = time;
    }
}

static void write_callback(struct SoundIoOutputDevice *device, int frame_count) { }
static void underrun_callback(struct SoundIoOutputDevice *device) { }

static void test_create_output_device(void) {
    struct SoundIo *soundio = soundio_create();
    assert(soundio);
    ok_or_panic(soundio_connect(soundio));
    int default_out_device_index = soundio_get_default_output_device_index(soundio);
    assert(default_out_device_index >= 0);
    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    assert(device);
    soundio_device_name(device);
    soundio_device_description(device);
    struct SoundIoOutputDevice *output_device;
    soundio_output_device_create(device, SoundIoSampleFormatFloat, 0.1, NULL,
            write_callback, underrun_callback, &output_device);
    soundio_output_device_destroy(output_device);
    soundio_device_unref(device);
    soundio_destroy(soundio);
}

struct Test {
    const char *name;
    void (*fn)(void);
};

static struct Test tests[] = {
    {"os_get_time", test_os_get_time},
    {"create output device", test_create_output_device},
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
