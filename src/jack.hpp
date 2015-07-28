/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_JACK_HPP
#define SOUNDIO_JACK_HPP

#include "soundio.h"
#include "os.hpp"

#include <jack/jack.h>

int soundio_jack_init(struct SoundIoPrivate *si);

struct SoundIoDeviceJack {

};

struct SoundIoJack {
    jack_client_t *client;
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;
    // this one is ready to be read with flush_events. protected by mutex
    struct SoundIoDevicesInfo *ready_devices_info;
    bool initialized;
    int sample_rate;
    int buffer_size;
};

struct SoundIoOutStreamJack {
    jack_client_t *client;
    jack_port_t *ports[SOUNDIO_MAX_CHANNELS];
};

struct SoundIoInStreamJack {

};

#endif
