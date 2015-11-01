/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio.hpp"
#include "os.h"
#include "util.hpp"
#include "atomics.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s [--backend dummy|alsa|pulseaudio|jack|coreaudio|wasapi] [--latency seconds]\n", exe);
    return 1;
}

static void write_sample_s16ne(char *ptr, double sample) {
    int16_t *buf = (int16_t *)ptr;
    double range = (double)INT16_MAX - (double)INT16_MIN;
    double val = sample * range / 2.0;
    *buf = val;
}

static void write_sample_s32ne(char *ptr, double sample) {
    int32_t *buf = (int32_t *)ptr;
    double range = (double)INT32_MAX - (double)INT32_MIN;
    double val = sample * range / 2.0;
    *buf = val;
}

static void write_sample_float32ne(char *ptr, double sample) {
    float *buf = (float *)ptr;
    *buf = sample;
}

static void write_sample_float64ne(char *ptr, double sample) {
    double *buf = (double *)ptr;
    *buf = sample;
}

static void (*write_sample)(char *ptr, double sample);

static int frames_until_pulse = 0;
static int pulse_frames_left = -1;
static const double PI = 3.14159265358979323846264338328;
static double seconds_offset = 0.0;

static SoundIoRingBuffer pulse_rb;

static void write_time(SoundIoOutStream *outstream, double extra) {
    double latency;
    int err;
    if ((err = soundio_outstream_get_latency(outstream, &latency))) {
        soundio_panic("getting latency: %s", soundio_strerror(err));
    }
    double now = soundio_os_get_time();
    double audible_time = now + latency + extra;
    double *write_ptr = (double *)soundio_ring_buffer_write_ptr(&pulse_rb);
    *write_ptr = audible_time;
    soundio_ring_buffer_advance_write_ptr(&pulse_rb, sizeof(double));
}

static void write_callback(struct SoundIoOutStream *outstream, int frame_count_min, int frame_count_max) {
    double float_sample_rate = outstream->sample_rate;
    double seconds_per_frame = 1.0f / float_sample_rate;
    struct SoundIoChannelArea *areas;
    int err;

    int frames_left = frame_count_max;

    while (frames_left > 0) {
        int frame_count = frames_left;

        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count)))
            soundio_panic("begin write: %s", soundio_strerror(err));

        if (!frame_count)
            break;

        const struct SoundIoChannelLayout *layout = &outstream->layout;

        double pitch = 440.0;
        double radians_per_second = pitch * 2.0 * PI;
        for (int frame = 0; frame < frame_count; frame += 1) {
            double sample;
            if (frames_until_pulse <= 0) {
                if (pulse_frames_left == -1) {
                    pulse_frames_left = 0.25 * float_sample_rate;
                    write_time(outstream, seconds_per_frame * frame); // announce beep start
                }
                if (pulse_frames_left > 0) {
                    pulse_frames_left -= 1;
                    sample = sinf((seconds_offset + frame * seconds_per_frame) * radians_per_second);
                } else {
                    frames_until_pulse = (0.5 + (rand() / (double)RAND_MAX) * 2.0) * float_sample_rate;
                    pulse_frames_left = -1;
                    sample = 0.0;
                    write_time(outstream, seconds_per_frame * frame); // announce beep end
                }
            } else {
                frames_until_pulse -= 1;
                sample = 0.0;
            }
            for (int channel = 0; channel < layout->channel_count; channel += 1) {
                write_sample(areas[channel].ptr, sample);
                areas[channel].ptr += areas[channel].step;
            }
        }

        seconds_offset += seconds_per_frame * frame_count;

        if ((err = soundio_outstream_end_write(outstream)))
            soundio_panic("end write: %s", soundio_strerror(err));

        frames_left -= frame_count;
    }
}

static void underflow_callback(struct SoundIoOutStream *outstream) {
    soundio_panic("underflow\n");
}

int main(int argc, char **argv) {
    char *exe = argv[0];
    enum SoundIoBackend backend = SoundIoBackendNone;
    double software_latency = 0.0;
    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            i += 1;
            if (i >= argc) {
                return usage(exe);
            } else if (strcmp(arg, "--backend") == 0) {
                if (strcmp("dummy", argv[i]) == 0) {
                    backend = SoundIoBackendDummy;
                } else if (strcmp("alsa", argv[i]) == 0) {
                    backend = SoundIoBackendAlsa;
                } else if (strcmp("pulseaudio", argv[i]) == 0) {
                    backend = SoundIoBackendPulseAudio;
                } else if (strcmp("jack", argv[i]) == 0) {
                    backend = SoundIoBackendJack;
                } else if (strcmp("coreaudio", argv[i]) == 0) {
                    backend = SoundIoBackendCoreAudio;
                } else if (strcmp("wasapi", argv[i]) == 0) {
                    backend = SoundIoBackendWasapi;
                } else {
                    fprintf(stderr, "Invalid backend: %s\n", argv[i]);
                    return 1;
                }
            } else if (strcmp(arg, "--latency") == 0) {
                software_latency = atof(argv[i]);
            } else {
                return usage(exe);
            }
        } else {
            return usage(exe);
        }
    }

    struct SoundIo *soundio;
    if (!(soundio = soundio_create()))
        soundio_panic("out of memory");

    int err = (backend == SoundIoBackendNone) ?
        soundio_connect(soundio) : soundio_connect_backend(soundio, backend);

    if (err)
        soundio_panic("error connecting: %s", soundio_strerror(err));

    soundio_flush_events(soundio);

    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0)
        soundio_panic("no output device found");

    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    if (!device)
        soundio_panic("out of memory");

    fprintf(stderr, "Output device: %s\n", device->name);

    struct SoundIoOutStream *outstream = soundio_outstream_create(device);
    outstream->format = SoundIoFormatFloat32NE;
    outstream->write_callback = write_callback;
    outstream->underflow_callback = underflow_callback;
    outstream->software_latency = software_latency;

    if (soundio_device_supports_format(device, SoundIoFormatFloat32NE)) {
        outstream->format = SoundIoFormatFloat32NE;
        write_sample = write_sample_float32ne;
    } else if (soundio_device_supports_format(device, SoundIoFormatFloat64NE)) {
        outstream->format = SoundIoFormatFloat64NE;
        write_sample = write_sample_float64ne;
    } else if (soundio_device_supports_format(device, SoundIoFormatS32NE)) {
        outstream->format = SoundIoFormatS32NE;
        write_sample = write_sample_s32ne;
    } else if (soundio_device_supports_format(device, SoundIoFormatS16NE)) {
        outstream->format = SoundIoFormatS16NE;
        write_sample = write_sample_s16ne;
    } else {
        soundio_panic("No suitable device format available.\n");
    }

    if ((err = soundio_ring_buffer_init(&pulse_rb, 1024)))
        soundio_panic("ring buffer init: %s", soundio_strerror(err));

    if ((err = soundio_outstream_open(outstream)))
        soundio_panic("unable to open device: %s", soundio_strerror(err));

    if (outstream->layout_error)
        fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

    if ((err = soundio_outstream_start(outstream)))
        soundio_panic("unable to start device: %s", soundio_strerror(err));

    bool beep_on = true;
    int count = 0;
    for (;;) {
        int fill_count = soundio_ring_buffer_fill_count(&pulse_rb);
        if (fill_count >= (int)sizeof(double)) {
            double *read_ptr = (double *)soundio_ring_buffer_read_ptr(&pulse_rb);
            double audible_time = *read_ptr;
            while (audible_time > soundio_os_get_time()) {
                // Burn the CPU while we wait for our precisely timed event.
            }
            if (beep_on) {
                fprintf(stderr, "BEEP %d start\n", count);
            } else {
                fprintf(stderr, "BEEP %d end\n", count++);
            }
            fflush(stderr);
            double off_by = soundio_os_get_time() - audible_time;
            if (off_by > 0.0001)
                fprintf(stderr, "off by %f\n", off_by);
            beep_on = !beep_on;
            soundio_ring_buffer_advance_read_ptr(&pulse_rb, sizeof(double));
        }
    }

    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);
    return 0;
}

