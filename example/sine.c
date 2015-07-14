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

static const float PI = 3.1415926535f;
static float seconds_offset = 0.0f;
static void write_callback(struct SoundIoOutStream *outstream, int requested_frame_count) {
    float float_sample_rate = outstream->sample_rate;
    float seconds_per_frame = 1.0f / float_sample_rate;

    while (requested_frame_count > 0) {
        char *data;
        int frame_count = requested_frame_count;
        soundio_outstream_begin_write(outstream, &data, &frame_count);

        // clear everything to 0
        memset(data, 0, frame_count * outstream->bytes_per_frame);

        const struct SoundIoChannelLayout *layout = &outstream->layout;

        float *ptr = (float *)data;

        float pitch = 440.0f;
        float radians_per_second = pitch * 2.0f * PI;
        for (int frame = 0; frame < frame_count; frame += 1) {
            float sample = sinf((seconds_offset + frame * seconds_per_frame) * radians_per_second);
            for (int channel = 0; channel < layout->channel_count; channel += 1) {
                *ptr += sample;
                ptr += 1;
            }
        }
        seconds_offset += seconds_per_frame * frame_count;

        soundio_outstream_write(outstream, data, frame_count);
        requested_frame_count -= frame_count;
    }

}

static void underrun_callback(struct SoundIoOutStream *device) {
    static int count = 0;
    fprintf(stderr, "underrun %d\n", count++);
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

    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    if (!device)
        panic("out of memory");

    fprintf(stderr, "Output device: %s: %s\n", device->name, device->description);

    struct SoundIoOutStream *outstream = soundio_outstream_create(device);
    outstream->format = SoundIoFormatFloat32NE;
    outstream->write_callback = write_callback;
    outstream->underrun_callback = underrun_callback;

    if ((err = soundio_outstream_open(outstream)))
        panic("unable to open device: %s", soundio_strerror(err));

    if ((err = soundio_outstream_start(outstream)))
        panic("unable to start device: %s", soundio_strerror(err));

    for (;;)
        soundio_wait_events(soundio);

    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);
    return 0;
}
