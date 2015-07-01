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

static void print_channel_layout(const struct SoundIoChannelLayout *layout) {
    if (layout->name) {
        fprintf(stderr, "%s", layout->name);
    } else {
        fprintf(stderr, "%s", soundio_get_channel_name(layout->channels[0]));
        for (int i = 1; i < layout->channel_count; i += 1) {
            fprintf(stderr, ", %s", soundio_get_channel_name(layout->channels[i]));
        }
    }

}

static void print_device(struct SoundIoDevice *device, bool is_default) {
    const char *purpose_str;
    const char *default_str;
    if (soundio_device_purpose(device) == SoundIoDevicePurposeOutput) {
        purpose_str = "playback";
        default_str = is_default ? " (default)" : "";
    } else {
        purpose_str = "recording";
        default_str = is_default ? " (default)" : "";
    }
    const char *description = soundio_device_description(device);
    int sample_rate = soundio_device_sample_rate(device);
    fprintf(stderr, "%s device: ", purpose_str);
    print_channel_layout(soundio_device_channel_layout(device));
    fprintf(stderr, " %d Hz %s%s\n", sample_rate, description, default_str);
}

static int list_devices(struct SoundIo *soundio) {
    int output_count = soundio_get_output_device_count(soundio);
    int input_count = soundio_get_input_device_count(soundio);

    int default_output = soundio_get_default_output_device_index(soundio);
    int default_input = soundio_get_default_input_device_index(soundio);

    for (int i = 0; i < input_count; i += 1) {
        struct SoundIoDevice *device = soundio_get_input_device(soundio, i);
        print_device(device, default_input == i);
        soundio_device_unref(device);
    }
    for (int i = 0; i < output_count; i += 1) {
        struct SoundIoDevice *device = soundio_get_output_device(soundio, i);
        print_device(device, default_output == i);
        soundio_device_unref(device);
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

    struct SoundIo *soundio = soundio_create();
    if (!soundio) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    int err;
    if ((err = soundio_connect(soundio))) {
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
