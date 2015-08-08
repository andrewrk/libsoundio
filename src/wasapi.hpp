/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_WASAPI_HPP
#define SOUNDIO_WASAPI_HPP

#include "soundio/soundio.h"

int soundio_wasapi_init(struct SoundIoPrivate *si);

struct SoundIoDeviceWasapi {
};

struct SoundIoWasapi {
};

struct SoundIoOutStreamWasapi {
};

struct SoundIoInStreamWasapi {
};

#endif
