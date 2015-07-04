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

static void write_callback(struct SoundIoOutputDevice *device, int frame_count) {
    fprintf(stderr, "write_callback\n");
}

static void underrun_callback(struct SoundIoOutputDevice *device) {
    static int count = 0;
    fprintf(stderr, "underrun %d\n", count++);
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

    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    if (!device)
        panic("could not get output device: out of memory");

    fprintf(stderr, "Output device: %s: %s\n",
            soundio_device_name(device),
            soundio_device_description(device));

    struct SoundIoOutputDevice *output_device;
    soundio_output_device_create(device, SoundIoSampleFormatFloat, 0.1, NULL,
            write_callback, underrun_callback, &output_device);

    if ((err = soundio_output_device_start(output_device)))
        panic("unable to start device: %s", soundio_error_string(err));

    for (;;)
        soundio_wait_events(soundio);

    soundio_output_device_destroy(output_device);
    soundio_device_unref(device);
    soundio_destroy(soundio);
    return 0;
}
