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

struct SoundIoRingBuffer *ring_buffer = NULL;

static void read_callback(struct SoundIoInStream *instream, int available_frame_count) {
    int err;
    struct SoundIoChannelArea *areas;
    char *write_ptr = soundio_ring_buffer_write_ptr(ring_buffer);
    int frames_left = available_frame_count;

    for (;;) {
        int frame_count = frames_left;

        if ((err = soundio_instream_begin_read(instream, &areas, &frame_count)))
            panic("begin read error: %s", soundio_strerror(err));

        if (frame_count <= 0)
            break;

        if (!areas) {
            // Due to an overflow there is a hole. Fill the ring buffer with
            // silence for the size of the hole.
            memset(write_ptr, 0, frame_count * instream->bytes_per_frame);
        } else {
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
                    memcpy(write_ptr, areas[ch].ptr, instream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                    write_ptr += instream->bytes_per_sample;
                }
            }
        }

        if ((err = soundio_instream_end_read(instream)))
            panic("end read error: %s", soundio_strerror(err));

        frames_left -= frame_count;

        if (frames_left <= 0)
            break;
    }

    int advance_frames = available_frame_count * instream->bytes_per_frame;
    soundio_ring_buffer_advance_write_ptr(ring_buffer, advance_frames);
}

static void write_callback(struct SoundIoOutStream *outstream, int requested_frame_count) {
    int err;
    struct SoundIoChannelArea *areas;
    char *read_ptr = soundio_ring_buffer_read_ptr(ring_buffer);

    int fill_count = soundio_ring_buffer_fill_count(ring_buffer) / outstream->bytes_per_frame;
    int frames_left = requested_frame_count;
    int silence_count = frames_left - fill_count;
    if (silence_count < 0)
        silence_count = 0;
    int total_read_count = requested_frame_count - silence_count;

    for (;;) {
        int frame_count = frames_left;

        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count)))
            panic("begin write error: %s", soundio_strerror(err));

        if (frame_count <= 0)
            break;

        int silence_frame_count = (silence_count < frame_count) ? silence_count : frame_count;
        int read_count = frame_count - silence_frame_count;

        for (int frame = 0; frame < silence_frame_count; frame += 1) {
            for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
                memset(areas[ch].ptr, 0, outstream->bytes_per_sample);
                areas[ch].ptr += areas[ch].step;
            }
        }

        for (int frame = 0; frame < read_count; frame += 1) {
            for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
                memcpy(areas[ch].ptr, read_ptr, outstream->bytes_per_sample);
                areas[ch].ptr += areas[ch].step;
                read_ptr += outstream->bytes_per_sample;
            }
        }

        if ((err = soundio_outstream_end_write(outstream, frame_count)))
            panic("end write error: %s", soundio_strerror(err));

        frames_left -= frame_count;

        if (frames_left <= 0)
            break;
    }

    soundio_ring_buffer_advance_read_ptr(ring_buffer, total_read_count * outstream->bytes_per_frame);
}

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s [--dummy] [--alsa] [--pulseaudio] [--in-device name] [--out-device name]\n", exe);
    return 1;
}

int main(int argc, char **argv) {
    char *exe = argv[0];
    enum SoundIoBackend backend = SoundIoBackendNone;
    char *in_device_name = NULL;
    char *out_device_name = NULL;
    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (strcmp("--dummy", arg) == 0) {
            backend = SoundIoBackendDummy;
        } else if (strcmp("--alsa", arg) == 0) {
            backend = SoundIoBackendAlsa;
        } else if (strcmp("--pulseaudio", arg) == 0) {
            backend = SoundIoBackendPulseAudio;
        } else if (strcmp("--in-device", arg) == 0) {
            if (++i >= argc) {
                return usage(exe);
            } else {
                in_device_name = argv[i];
            }
        } else if (strcmp("--out-device", arg) == 0) {
            if (++i >= argc) {
                return usage(exe);
            } else {
                out_device_name = argv[i];
            }
        } else {
            return usage(exe);
        }
    }
    struct SoundIo *soundio = soundio_create();
    if (!soundio)
        panic("out of memory");

    int err = (backend == SoundIoBackendNone) ?
        soundio_connect(soundio) : soundio_connect_backend(soundio, backend);

    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0)
        panic("no output device found");

    int default_in_device_index = soundio_default_input_device_index(soundio);
    if (default_in_device_index < 0)
        panic("no output device found");

    int in_device_index = default_in_device_index;
    if (in_device_name) {
        bool found = false;
        for (int i = 0; i < soundio_input_device_count(soundio); i += 1) {
            struct SoundIoDevice *device = soundio_get_input_device(soundio, i);
            if (strcmp(device->name, in_device_name) == 0) {
                in_device_index = i;
                found = true;
                soundio_device_unref(device);
                break;
            }
            soundio_device_unref(device);
        }
        if (!found)
            panic("invalid input device name: %s", in_device_name);
    }

    int out_device_index = default_out_device_index;
    if (out_device_name) {
        bool found = false;
        for (int i = 0; i < soundio_output_device_count(soundio); i += 1) {
            struct SoundIoDevice *device = soundio_get_output_device(soundio, i);
            if (strcmp(device->name, out_device_name) == 0) {
                out_device_index = i;
                found = true;
                soundio_device_unref(device);
                break;
            }
            soundio_device_unref(device);
        }
        if (!found)
            panic("invalid output device name: %s", out_device_name);
    }

    struct SoundIoDevice *out_device = soundio_get_output_device(soundio, out_device_index);
    if (!out_device)
        panic("could not get output device: out of memory");

    struct SoundIoDevice *in_device = soundio_get_input_device(soundio, in_device_index);
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
    instream->period_duration = 0.1;
    instream->read_callback = read_callback;

    if ((err = soundio_instream_open(instream)))
        panic("unable to open input stream: %s", soundio_strerror(err));

    struct SoundIoOutStream *outstream = soundio_outstream_create(out_device);
    if (!outstream)
        panic("out of memory");
    outstream->format = *fmt;
    outstream->sample_rate = sample_rate;
    outstream->layout = *layout;
    outstream->buffer_duration = 0.2;
    outstream->period_duration = 0.1;
    outstream->write_callback = write_callback;

    if ((err = soundio_outstream_open(outstream)))
        panic("unable to open output stream: %s", soundio_strerror(err));

    int capacity = 0.4 * instream->sample_rate * instream->bytes_per_frame;
    ring_buffer = soundio_ring_buffer_create(soundio, capacity);
    if (!ring_buffer)
        panic("unable to create ring buffer: out of memory");

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
