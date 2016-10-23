/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_JACK_H
#define SOUNDIO_JACK_H

#include "soundio_internal.h"
#include "os.h"
#include "atomics.h"

// jack.h does not properly put `void` in function prototypes with no
// arguments, so we're forced to temporarily disable -Werror=strict-prototypes
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <jack/jack.h>
#pragma GCC diagnostic pop

struct SoundIoPrivate;
enum SoundIoError soundio_jack_init(struct SoundIoPrivate *si);

struct SoundIoDeviceJackPort {
    char *full_name;
    int full_name_len;
    enum SoundIoChannelId channel_id;
    jack_latency_range_t latency_range;
};

struct SoundIoDeviceJack {
    int port_count;
    struct SoundIoDeviceJackPort *ports;
};

struct SoundIoJack {
    jack_client_t *client;
    struct SoundIoOsMutex *mutex;
    struct SoundIoOsCond *cond;
    struct SoundIoAtomicFlag refresh_devices_flag;
    int sample_rate;
    int period_size;
    bool is_shutdown;
    bool emitted_shutdown_cb;
};

struct SoundIoOutStreamJackPort {
    jack_port_t *source_port;
    const char *dest_port_name;
    int dest_port_name_len;
};

struct SoundIoOutStreamJack {
    jack_client_t *client;
    int period_size;
    int frames_left;
    double hardware_latency;
    struct SoundIoOutStreamJackPort ports[SOUNDIO_MAX_CHANNELS];
    struct SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

struct SoundIoInStreamJackPort {
    jack_port_t *dest_port;
    const char *source_port_name;
    int source_port_name_len;
};

struct SoundIoInStreamJack {
    jack_client_t *client;
    int period_size;
    int frames_left;
    double hardware_latency;
    struct SoundIoInStreamJackPort ports[SOUNDIO_MAX_CHANNELS];
    struct SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
    char *buf_ptrs[SOUNDIO_MAX_CHANNELS];
};

#endif
