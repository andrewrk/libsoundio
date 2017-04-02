/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include <soundio/soundio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

struct RecordContext {
    struct SoundIoRingBuffer *ring_buffer;
};

static enum SoundIoFormat prioritized_formats[] = {
    SoundIoFormatFloat32NE,
    SoundIoFormatFloat32FE,
    SoundIoFormatS32NE,
    SoundIoFormatS32FE,
    SoundIoFormatS24NE,
    SoundIoFormatS24FE,
    SoundIoFormatS24PackedNE,
    SoundIoFormatS24PackedFE,
    SoundIoFormatS16NE,
    SoundIoFormatS16FE,
    SoundIoFormatFloat64NE,
    SoundIoFormatFloat64FE,
    SoundIoFormatU32NE,
    SoundIoFormatU32FE,
    SoundIoFormatU24NE,
    SoundIoFormatU24FE,
    SoundIoFormatU24PackedNE,
    SoundIoFormatU24PackedFE,
    SoundIoFormatU16NE,
    SoundIoFormatU16FE,
    SoundIoFormatS8,
    SoundIoFormatU8,
    SoundIoFormatInvalid,
};

static int prioritized_sample_rates[] = {
    48000,
    44100,
    96000,
    24000,
    0,
};

static int min_int(int a, int b) {
    return (a < b) ? a : b;
}

static void read_callback(struct SoundIoInStream *instream, int frame_count_min, int frame_count_max) {
    struct RecordContext *rc = instream->userdata;
    struct SoundIoChannelArea *areas;
    int err;

    char *write_ptr = soundio_ring_buffer_write_ptr(rc->ring_buffer);
    int free_bytes = soundio_ring_buffer_free_count(rc->ring_buffer);
    int free_count = free_bytes / instream->bytes_per_frame;

    if (free_count < frame_count_min) {
        fprintf(stderr, "ring buffer overflow\n");
        exit(1);
    }

    int write_frames = min_int(free_count, frame_count_max);
    int frames_left = write_frames;

    for (;;) {
        int frame_count = frames_left;

        if ((err = soundio_instream_begin_read(instream, &areas, &frame_count))) {
            fprintf(stderr, "begin read error: %s", soundio_strerror(err));
            exit(1);
        }

        if (!frame_count)
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

        if ((err = soundio_instream_end_read(instream))) {
            fprintf(stderr, "end read error: %s", soundio_strerror(err));
            exit(1);
        }

        frames_left -= frame_count;
        if (frames_left <= 0)
            break;
    }

    int advance_bytes = write_frames * instream->bytes_per_frame;
    soundio_ring_buffer_advance_write_ptr(rc->ring_buffer, advance_bytes);
}

static void overflow_callback(struct SoundIoInStream *instream) {
    static int count = 0;
    fprintf(stderr, "overflow %d\n", ++count);
}

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s [options] outfile\n"
            "Options:\n"
            "  [--backend dummy|alsa|pulseaudio|jack|coreaudio|wasapi]\n"
            "  [--device id]\n"
            "  [--raw]\n"
            , exe);
    return 1;
}

int main(int argc, char **argv) {
    char *exe = argv[0];
    enum SoundIoBackend backend = SoundIoBackendNone;
    char *device_id = NULL;
    bool is_raw = false;
    char *outfile = NULL;
    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            if (strcmp(arg, "--raw") == 0) {
                is_raw = true;
            } else if (++i >= argc) {
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
            } else if (strcmp(arg, "--device") == 0) {
                device_id = argv[i];
            } else {
                return usage(exe);
            }
        } else if (!outfile) {
            outfile = argv[i];
        } else {
            return usage(exe);
        }
    }

    if (!outfile)
        return usage(exe);

    struct RecordContext rc;

    struct SoundIo *soundio = soundio_create();
    if (!soundio) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    int err = (backend == SoundIoBackendNone) ?
        soundio_connect(soundio) : soundio_connect_backend(soundio, backend);
    if (err) {
        fprintf(stderr, "error connecting: %s", soundio_strerror(err));
        return 1;
    }

    soundio_flush_events(soundio);

    struct SoundIoDevice *selected_device = NULL;

    if (device_id) {
        for (int i = 0; i < soundio_input_device_count(soundio); i += 1) {
            struct SoundIoDevice *device = soundio_get_input_device(soundio, i);
            if (device->is_raw == is_raw && strcmp(device->id, device_id) == 0) {
                selected_device = device;
                break;
            }
            soundio_device_unref(device);
        }
        if (!selected_device) {
            fprintf(stderr, "Invalid device id: %s\n", device_id);
            return 1;
        }
    } else {
        int device_index = soundio_default_input_device_index(soundio);
        selected_device = soundio_get_input_device(soundio, device_index);
        if (!selected_device) {
            fprintf(stderr, "No input devices available.\n");
            return 1;
        }
    }

    fprintf(stderr, "Device: %s\n", selected_device->name);

    if (selected_device->probe_error) {
        fprintf(stderr, "Unable to probe device: %s\n", soundio_strerror(selected_device->probe_error));
        return 1;
    }

    soundio_device_sort_channel_layouts(selected_device);

    int sample_rate = 0;
    int *sample_rate_ptr;
    for (sample_rate_ptr = prioritized_sample_rates; *sample_rate_ptr; sample_rate_ptr += 1) {
        if (soundio_device_supports_sample_rate(selected_device, *sample_rate_ptr)) {
            sample_rate = *sample_rate_ptr;
            break;
        }
    }
    if (!sample_rate)
        sample_rate = selected_device->sample_rates[0].max;

    enum SoundIoFormat fmt = SoundIoFormatInvalid;
    enum SoundIoFormat *fmt_ptr;
    for (fmt_ptr = prioritized_formats; *fmt_ptr != SoundIoFormatInvalid; fmt_ptr += 1) {
        if (soundio_device_supports_format(selected_device, *fmt_ptr)) {
            fmt = *fmt_ptr;
            break;
        }
    }
    if (fmt == SoundIoFormatInvalid)
        fmt = selected_device->formats[0];

    FILE *out_f = fopen(outfile, "wb");
    if (!out_f) {
        fprintf(stderr, "unable to open %s: %s\n", outfile, strerror(errno));
        return 1;
    }
    struct SoundIoInStream *instream = soundio_instream_create(selected_device);
    if (!instream) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    instream->format = fmt;
    instream->sample_rate = sample_rate;
    instream->read_callback = read_callback;
    instream->overflow_callback = overflow_callback;
    instream->userdata = &rc;

    if ((err = soundio_instream_open(instream))) {
        fprintf(stderr, "unable to open input stream: %s", soundio_strerror(err));
        return 1;
    }

    fprintf(stderr, "%s %dHz %s interleaved\n",
            instream->layout.name, sample_rate, soundio_format_string(fmt));

    const int ring_buffer_duration_seconds = 30;
    int capacity = ring_buffer_duration_seconds * instream->sample_rate * instream->bytes_per_frame;
    rc.ring_buffer = soundio_ring_buffer_create(soundio, capacity);
    if (!rc.ring_buffer) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    if ((err = soundio_instream_start(instream))) {
        fprintf(stderr, "unable to start input device: %s", soundio_strerror(err));
        return 1;
    }

    // Note: in this example, if you send SIGINT (by pressing Ctrl+C for example)
    // you will lose up to 1 second of recorded audio data. In non-example code,
    // consider a better shutdown strategy.
    for (;;) {
        soundio_flush_events(soundio);
        sleep(1);
        int fill_bytes = soundio_ring_buffer_fill_count(rc.ring_buffer);
        char *read_buf = soundio_ring_buffer_read_ptr(rc.ring_buffer);
        size_t amt = fwrite(read_buf, 1, fill_bytes, out_f);
        if ((int)amt != fill_bytes) {
            fprintf(stderr, "write error: %s\n", strerror(errno));
            return 1;
        }
        soundio_ring_buffer_advance_read_ptr(rc.ring_buffer, fill_bytes);
    }

    soundio_instream_destroy(instream);
    soundio_device_unref(selected_device);
    soundio_destroy(soundio);
    return 0;
}
