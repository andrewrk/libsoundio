/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include <soundio.h>

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// list or keep a watch on audio devices

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s [--watch]\n", exe);
    return 1;
}

static int list_devices(struct SoundIo *soundio) {
    int output_count = soundio_get_output_device_count(soundio);
    int input_count = soundio_get_input_device_count(soundio);

    int default_output = soundio_get_default_output_device_index(soundio);
    int default_input = soundio_get_default_input_device_index(soundio);

    for (int i = 0; i < input_count; i += 1) {
        struct SoundIoDevice *device = soundio_get_input_device(soundio, i);
        const char *purpose_str = "input";
        const char *default_str = (i == default_input) ? " (default)" : "";
        const char *description = soundio_audio_device_description(device);
        int sample_rate = soundio_audio_device_sample_rate(device);
        fprintf(stderr, "%s device: %d Hz %s%s\n", purpose_str, sample_rate, description, default_str);
        soundio_audio_device_unref(device);
    }
    for (int i = 0; i < output_count; i += 1) {
        struct SoundIoDevice *device = soundio_get_output_device(soundio, i);

        const char *purpose_str = "output";
        const char *default_str = (i == default_output) ? " (default)" : "";
        const char *description = soundio_audio_device_description(device);
        int sample_rate = soundio_audio_device_sample_rate(device);
        fprintf(stderr, "%s device: %d Hz %s%s\n", purpose_str, sample_rate, description, default_str);
        soundio_audio_device_unref(device);
    }

    fprintf(stderr, "%d devices found\n", input_count + output_count);
    return 0;
}

static void on_devices_change(struct SoundIo *soundio) {
    fprintf(stderr, "devices changed\n");
    list_devices(soundio);
}

int main(int argc, char **argv) {
    char *exe = argv[0];
    bool watch = false;

    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (strcmp("--watch", arg) == 0) {
            watch = true;
        } else {
            return usage(exe);
        }
    }

    int err;
    struct SoundIo *soundio;
    if ((err = soundio_create(&soundio))) {
        fprintf(stderr, "%s\n", soundio_error_string(err));
        return err;
    }

    if (watch) {
        soundio->on_devices_change = on_devices_change;
        for (;;) {
            soundio_wait_events(soundio);
        }
    } else {
        int err = list_devices(soundio);
        soundio_destroy(soundio);
        return err;
    }
}
