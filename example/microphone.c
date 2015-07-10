/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include <soundio.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

__attribute__ ((cold))
__attribute__ ((noreturn))
__attribute__ ((format (printf, 1, 2)))
static void panic(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}

static void read_callback(struct SoundIoInputDevice *input_device) {
    fprintf(stderr, "read_callback\n");
}

static void write_callback(struct SoundIoOutputDevice *output_device, int requested_frame_count) {
    fprintf(stderr, "write_callback\n");
}

static void underrun_callback(struct SoundIoOutputDevice *output_device) {
    static int count = 0;
    fprintf(stderr, "underrun %d\n", count++);
    soundio_output_device_fill_with_silence(output_device);
}

int main(int argc, char **argv) {
    struct SoundIo *soundio = soundio_create();
    if (!soundio)
        panic("out of memory");

    int err;
    if ((err = soundio_connect(soundio)))
        panic("error connecting: %s", soundio_error_string(err));

    int default_out_device_index = soundio_get_default_output_device_index(soundio);
    if (default_out_device_index < 0)
        panic("no output device found");

    int default_in_device_index = soundio_get_default_input_device_index(soundio);
    if (default_in_device_index < 0)
        panic("no output device found");

    struct SoundIoDevice *out_device = soundio_get_output_device(soundio, default_out_device_index);
    if (!out_device)
        panic("could not get output device: out of memory");

    struct SoundIoDevice *in_device = soundio_get_input_device(soundio, default_in_device_index);
    if (!in_device)
        panic("could not get input device: out of memory");

    fprintf(stderr, "Input device: %s\n", soundio_device_description(in_device));
    fprintf(stderr, "Output device: %s\n", soundio_device_description(out_device));

    const struct SoundIoChannelLayout *in_layout = soundio_device_channel_layout(in_device);
    const struct SoundIoChannelLayout *out_layout = soundio_device_channel_layout(out_device);

    if (!soundio_channel_layout_equal(in_layout, out_layout))
        panic("channel layouts not compatible");

    if (soundio_device_sample_rate(in_device) != soundio_device_sample_rate(out_device))
        panic("sample rates not compatible");

    double latency = 0.1;

    struct SoundIoInputDevice *input_device;
    soundio_input_device_create(in_device, SoundIoSampleFormatFloat32NE, 48000, latency, NULL,
            read_callback, &input_device);

    struct SoundIoOutputDevice *output_device;
    soundio_output_device_create(out_device, SoundIoSampleFormatFloat32NE, 48000, latency, NULL,
            write_callback, underrun_callback, &output_device);

    if ((err = soundio_input_device_start(input_device)))
        panic("unable to start input device: %s", soundio_error_string(err));

    if ((err = soundio_output_device_start(output_device)))
        panic("unable to start output device: %s", soundio_error_string(err));

    for (;;)
        soundio_wait_events(soundio);

    soundio_output_device_destroy(output_device);
    soundio_input_device_destroy(input_device);
    soundio_device_unref(in_device);
    soundio_device_unref(out_device);
    soundio_destroy(soundio);
    return 0;
}
