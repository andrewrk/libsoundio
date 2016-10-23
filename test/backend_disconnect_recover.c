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
#include <unistd.h>

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
            "  [--timeout seconds]\n", exe);
    return 1;
}

static enum SoundIoBackend backend = SoundIoBackendNone;

static bool severed = false;

static void on_backend_disconnect(struct SoundIo *soundio, enum SoundIoError err) {
    fprintf(stderr, "OK backend disconnected with '%s'.\n", soundio_error_name(err));
    severed = true;
}

int main(int argc, char **argv) {
    char *exe = argv[0];
    int timeout = 0;
    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            i += 1;
            if (i >= argc) {
                return usage(exe);
            } else if (strcmp(arg, "--timeout") == 0) {
                timeout = atoi(argv[i]);
            } else if (strcmp(arg, "--backend") == 0) {
                if (strcmp("-dummy", argv[i]) == 0) {
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

    struct SoundIo *soundio;
    if (!(soundio = soundio_create()))
        panic("out of memory");

    int err = (backend == SoundIoBackendNone) ?
        soundio_connect(soundio) : soundio_connect_backend(soundio, backend);

    if (err)
        panic("error connecting: %s", soundio_error_name(err));

    soundio->on_backend_disconnect = on_backend_disconnect;

    fprintf(stderr, "OK connected to %s. Now cause the backend to disconnect.\n",
            soundio_backend_name(soundio->current_backend));

    while (!severed)
        soundio_wait_events(soundio);

    soundio_disconnect(soundio);

    if (timeout > 0) {
        fprintf(stderr, "OK sleeping for %d seconds\n", timeout);
        sleep(timeout);
    }

    fprintf(stderr, "OK cleaned up. Reconnecting...\n");

    err = (backend == SoundIoBackendNone) ?
        soundio_connect(soundio) : soundio_connect_backend(soundio, backend);

    if (err)
        panic("error reconnecting: %s", soundio_error_name(err));

    fprintf(stderr, "OK reconnected successfully to %s\n", soundio_backend_name(soundio->current_backend));

    soundio_flush_events(soundio);

    fprintf(stderr, "OK test passed\n");

    return 0;
}
