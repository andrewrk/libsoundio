/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include <soundio/soundio.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define __attribute__(x)
#define sleep(x) Sleep(1000*(x))
#else
#include <unistd.h>
#endif


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

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s [options]\n"
            "Options:\n"
            "  [--backend dummy|alsa|pulseaudio|jack|coreaudio|wasapi]\n"
            "  [--device id]\n"
            "  [--raw]\n", exe);
    return 1;
}

static struct SoundIo *soundio = NULL;
static float seconds_offset = 0.0f;
static float seconds_end = 9.0f;
static bool caused_underflow = false;
static int overflow_count = 0;

static void read_callback(struct SoundIoInStream *instream, int frame_count_min, int frame_count_max) {
    struct SoundIoChannelArea *areas;
    float float_sample_rate = instream->sample_rate;
    float seconds_per_frame = 1.0f / float_sample_rate;
    int err;

    if (!caused_underflow && seconds_offset >= 3.0f) {
        fprintf(stderr, "OK sleeping...\n");
        caused_underflow = true;
        sleep(3);
    }

    if (seconds_offset >= seconds_end) {
        soundio_wakeup(soundio);
        return;
    }

    int frames_left = frame_count_max;

    for (;;) {
        int frame_count = frames_left;

        if ((err = soundio_instream_begin_read(instream, &areas, &frame_count)))
            panic("begin read error: %s", soundio_strerror(err));

        if (!frame_count)
            break;

        seconds_offset += seconds_per_frame * frame_count;

        if ((err = soundio_instream_end_read(instream)))
            panic("end read error: %s", soundio_strerror(err));

        frames_left -= frame_count;
        if (frames_left <= 0)
            break;
    }

    fprintf(stderr, "OK received %d frames\n", frame_count_max);
}

static void overflow_callback(struct SoundIoInStream *instream) {
    fprintf(stderr, "OK overflow %d\n", overflow_count++);
}

int main(int argc, char **argv) {
    char *exe = argv[0];
    enum SoundIoBackend backend = SoundIoBackendNone;
    bool is_raw = false;
    char *device_id = NULL;
    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            if (strcmp(arg, "--raw") == 0) {
                is_raw = true;
            } else if (++i >= argc) {
                return usage(exe);
            } else if (strcmp(arg, "--device") == 0) {
                device_id = argv[i];
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
            } else {
                return usage(exe);
            }
        } else {
            return usage(exe);
        }
    }

    fprintf(stderr,
            "Records for 3 seconds, sleeps for 3 seconds, then you should see at least\n"
            "one buffer overflow message, then records for 3 seconds.\n"
            "PulseAudio is not expected to pass this test.\n"
            "CoreAudio is not expected to pass this test.\n"
            "WASAPI is not expected to pass this test.\n");

    if (!(soundio = soundio_create()))
        panic("out of memory");

    int err = (backend == SoundIoBackendNone) ?
        soundio_connect(soundio) : soundio_connect_backend(soundio, backend);

    if (err)
        panic("error connecting: %s", soundio_strerror(err));

    soundio_flush_events(soundio);

    int selected_device_index = -1;
    if (device_id) {
        int device_count = soundio_input_device_count(soundio);
        for (int i = 0; i < device_count; i += 1) {
            struct SoundIoDevice *device = soundio_get_input_device(soundio, i);
            if (strcmp(device->id, device_id) == 0 && device->is_raw == is_raw) {
                selected_device_index = i;
                break;
            }
        }
    } else {
        selected_device_index = soundio_default_input_device_index(soundio);
    }

    if (selected_device_index < 0) {
        fprintf(stderr, "input device not found\n");
        return 1;
    }

    struct SoundIoDevice *device = soundio_get_input_device(soundio, selected_device_index);
    if (!device) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    fprintf(stderr, "Input device: %s\n", device->name);

    enum SoundIoFormat *fmt;
    for (fmt = prioritized_formats; *fmt != SoundIoFormatInvalid; fmt += 1) {
        if (soundio_device_supports_format(device, *fmt))
            break;
    }
    if (*fmt == SoundIoFormatInvalid)
        panic("incompatible sample format");

    struct SoundIoInStream *instream = soundio_instream_create(device);
    instream->format = *fmt;
    instream->read_callback = read_callback;
    instream->overflow_callback = overflow_callback;

    if ((err = soundio_instream_open(instream)))
        panic("unable to open device: %s", soundio_strerror(err));

    fprintf(stderr, "OK format: %s\n", soundio_format_string(instream->format));

    if ((err = soundio_instream_start(instream)))
        panic("unable to start device: %s", soundio_strerror(err));

    while (seconds_offset < seconds_end)
        soundio_wait_events(soundio);

    soundio_instream_destroy(instream);
    soundio_device_unref(device);
    soundio_destroy(soundio);

    if (overflow_count > 0) {
        fprintf(stderr, "OK test passed with %d overflow callbacks\n", overflow_count);
        return 0;
    } else {
        fprintf(stderr, "FAIL no overflow callbacks received\n");
        return 1;
    }
}
