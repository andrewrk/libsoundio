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

static enum SoundIoFormat prioritized_formats[] = {
    SoundIoFormatFloat32NE,
    SoundIoFormatFloat32FE,
    SoundIoFormatS32NE,
    SoundIoFormatS32FE,
    SoundIoFormatS24NE,
    SoundIoFormatS24FE,
    SoundIoFormatS16NE,
    SoundIoFormatS16FE,
    SoundIoFormatFloat64NE,
    SoundIoFormatFloat64FE,
    SoundIoFormatU32NE,
    SoundIoFormatU32FE,
    SoundIoFormatU24NE,
    SoundIoFormatU24FE,
    SoundIoFormatU16NE,
    SoundIoFormatU16FE,
    SoundIoFormatS8,
    SoundIoFormatU8,
    SoundIoFormatInvalid,
};

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

static void read_callback(struct SoundIoInStream *instream) {
    fprintf(stderr, "read_callback\n");
}

static void write_callback(struct SoundIoOutStream *outstream, int requested_frame_count) {
    fprintf(stderr, "write_callback\n");
}

static void underrun_callback(struct SoundIoOutStream *outstream) {
    static int count = 0;
    fprintf(stderr, "underrun %d\n", count++);
    soundio_outstream_fill_with_silence(outstream);
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

    soundio_device_sort_channel_layouts(out_device);
    const struct SoundIoChannelLayout *layout = soundio_best_matching_channel_layout(
            out_device->layouts, out_device->layout_count,
            in_device->layouts, in_device->layout_count);

    if (!layout)
        panic("channel layouts not compatible");


    int sample_rate = 48000;
    if (in_device->sample_rate_max < sample_rate) sample_rate = in_device->sample_rate_max;
    if (out_device->sample_rate_max < sample_rate) sample_rate = out_device->sample_rate_max;
    if (in_device->sample_rate_min > sample_rate || out_device->sample_rate_min > sample_rate)
        panic("incompatible sample rates");

    enum SoundIoFormat *fmt;
    for (fmt = prioritized_formats; *fmt != SoundIoFormatInvalid; fmt += 1) {
        if (soundio_device_supports_format(in_device, *fmt) &&
            soundio_device_supports_format(out_device, *fmt))
        {
            break;
        }
    }
    if (*fmt == SoundIoFormatInvalid)
        panic("incompatible sample formats");

    struct SoundIoInStream *instream = soundio_instream_create(in_device);
    if (!instream)
        panic("out of memory");
    instream->format = *fmt;
    instream->sample_rate = sample_rate;
    instream->layout = *layout;
    instream->latency = 0.1;
    instream->read_callback = read_callback;

    if ((err = soundio_instream_open(instream)))
        panic("unable to open input stream: %s", soundio_strerror(err));

    struct SoundIoOutStream *outstream = soundio_outstream_create(out_device);
    if (!outstream)
        panic("out of memory");
    outstream->format = *fmt;
    outstream->sample_rate = sample_rate;
    outstream->layout = *layout;
    outstream->latency = 0.1;
    outstream->write_callback = write_callback;
    outstream->underrun_callback = underrun_callback;

    if ((err = soundio_outstream_open(outstream)))
        panic("unable to open output stream: %s", soundio_strerror(err));

    if ((err = soundio_instream_start(instream)))
        panic("unable to start input device: %s", soundio_strerror(err));

    if ((err = soundio_outstream_start(outstream)))
        panic("unable to start output device: %s", soundio_strerror(err));

    for (;;)
        soundio_wait_events(soundio);

    soundio_outstream_destroy(outstream);
    soundio_instream_destroy(instream);
    soundio_device_unref(in_device);
    soundio_device_unref(out_device);
    soundio_destroy(soundio);
    return 0;
}
