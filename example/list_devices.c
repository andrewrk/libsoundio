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
    fprintf(stderr, "Usage: %s [--watch] [--dummy] [--alsa] [--pulseaudio]\n", exe);
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
    const char *default_str = is_default ? " (default)" : "";
    const char *raw_str = device->is_raw ? " (raw)" : "";
    fprintf(stderr, "%s%s%s\n", device->description, default_str, raw_str);
    fprintf(stderr, "  name: %s\n", device->name);

    if (device->probe_error) {
        fprintf(stderr, "  probe error: %s\n", soundio_strerror(device->probe_error));
    } else {
        fprintf(stderr, "  channel layouts:\n");
        for (int i = 0; i < device->layout_count; i += 1) {
            fprintf(stderr, "    ");
            print_channel_layout(&device->layouts[i]);
            fprintf(stderr, "\n");
        }
        if (device->current_layout.channel_count > 0) {
            fprintf(stderr, "  current layout: ");
            print_channel_layout(&device->current_layout);
            fprintf(stderr, "\n");
        }

        fprintf(stderr, "  min sample rate: %d\n", device->sample_rate_min);
        fprintf(stderr, "  max sample rate: %d\n", device->sample_rate_max);
        if (device->sample_rate_current)
            fprintf(stderr, "  current sample rate: %d\n", device->sample_rate_current);
        fprintf(stderr, "  formats: ");
        for (int i = 0; i < device->format_count; i += 1) {
            const char *comma = (i == device->format_count - 1) ? "" : ", ";
            fprintf(stderr, "%s%s", soundio_format_string(device->formats[i]), comma);
        }
        fprintf(stderr, "\n");
        if (device->current_format != SoundIoFormatInvalid)
            fprintf(stderr, "  current format: %s\n", soundio_format_string(device->current_format));

        fprintf(stderr, "  min buffer duration: %0.8f sec\n", device->buffer_duration_min);
        fprintf(stderr, "  max buffer duration: %0.8f sec\n", device->buffer_duration_max);
        if (device->buffer_duration_current != 0.0)
            fprintf(stderr, "  current buffer duration: %0.8f sec\n", device->buffer_duration_current);

        fprintf(stderr, "  min period duration: %0.8f sec\n", device->period_duration_min);
        fprintf(stderr, "  max period duration: %0.8f sec\n", device->period_duration_max);
        if (device->period_duration_current != 0.0)
            fprintf(stderr, "  current period duration: %0.8f sec\n", device->period_duration_current);
    }
    fprintf(stderr, "\n");
}

static int list_devices(struct SoundIo *soundio) {
    int output_count = soundio_output_device_count(soundio);
    int input_count = soundio_input_device_count(soundio);

    int default_output = soundio_default_output_device_index(soundio);
    int default_input = soundio_default_input_device_index(soundio);

    fprintf(stderr, "--------Input Devices--------\n\n");
    for (int i = 0; i < input_count; i += 1) {
        struct SoundIoDevice *device = soundio_get_input_device(soundio, i);
        print_device(device, default_input == i);
        soundio_device_unref(device);
    }
    fprintf(stderr, "\n--------Output Devices--------\n\n");
    for (int i = 0; i < output_count; i += 1) {
        struct SoundIoDevice *device = soundio_get_output_device(soundio, i);
        print_device(device, default_output == i);
        soundio_device_unref(device);
    }

    fprintf(stderr, "\n%d devices found\n", input_count + output_count);
    return 0;
}

static void on_devices_change(struct SoundIo *soundio) {
    fprintf(stderr, "devices changed\n");
    list_devices(soundio);
}

int main(int argc, char **argv) {
    char *exe = argv[0];
    bool watch = false;
    enum SoundIoBackend backend = SoundIoBackendNone;

    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (strcmp("--watch", arg) == 0) {
            watch = true;
        } else if (strcmp("--dummy", arg) == 0) {
            backend = SoundIoBackendDummy;
        } else if (strcmp("--alsa", arg) == 0) {
            backend = SoundIoBackendAlsa;
        } else if (strcmp("--pulseaudio", arg) == 0) {
            backend = SoundIoBackendPulseAudio;
        } else {
            return usage(exe);
        }
    }

    struct SoundIo *soundio = soundio_create();
    if (!soundio) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    int err = (backend == SoundIoBackendNone) ?
        soundio_connect(soundio) : soundio_connect_backend(soundio, backend);

    if (err) {
        fprintf(stderr, "%s\n", soundio_strerror(err));
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
