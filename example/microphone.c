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

static void read_callback(struct SoundIoInStream *in_stream) {
    fprintf(stderr, "read_callback\n");
}

static void write_callback(struct SoundIoOutStream *out_stream, int requested_frame_count) {
    fprintf(stderr, "write_callback\n");
}

static void underrun_callback(struct SoundIoOutStream *out_stream) {
    static int count = 0;
    fprintf(stderr, "underrun %d\n", count++);
    soundio_out_stream_fill_with_silence(out_stream);
}

int main(int argc, char **argv) {
    struct SoundIo *soundio = soundio_create();
    if (!soundio)
        panic("out of memory");

    int err;
    if ((err = soundio_connect(soundio)))
        panic("error connecting: %s", soundio_strerror(err));

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

    fprintf(stderr, "Input device: %s\n", in_device->description);
    fprintf(stderr, "Output device: %s\n", out_device->description);

    if (!soundio_channel_layout_equal(&in_device->channel_layout, &out_device->channel_layout))
        panic("channel layouts not compatible");

    double latency = 0.1;

    struct SoundIoInStream *in_stream;
    soundio_in_stream_create(in_device, SoundIoFormatFloat32NE, 48000, latency, NULL,
            read_callback, &in_stream);

    struct SoundIoOutStream *out_stream;
    soundio_out_stream_create(out_device, SoundIoFormatFloat32NE, 48000, latency, NULL,
            write_callback, underrun_callback, &out_stream);

    if ((err = soundio_in_stream_start(in_stream)))
        panic("unable to start input device: %s", soundio_strerror(err));

    if ((err = soundio_out_stream_start(out_stream)))
        panic("unable to start output device: %s", soundio_strerror(err));

    for (;;)
        soundio_wait_events(soundio);

    soundio_out_stream_destroy(out_stream);
    soundio_in_stream_destroy(in_stream);
    soundio_device_unref(in_device);
    soundio_device_unref(out_device);
    soundio_destroy(soundio);
    return 0;
}
