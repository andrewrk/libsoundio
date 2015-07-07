/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_SOUNDIO_HPP
#define SOUNDIO_SOUNDIO_HPP

#include "soundio.h"
#include "list.hpp"

struct SoundIoDevicesInfo {
    SoundIoList<SoundIoDevice *> input_devices;
    SoundIoList<SoundIoDevice *> output_devices;
    // can be -1 when default device is unknown
    int default_output_index;
    int default_input_index;
};

void soundio_destroy_devices_info(struct SoundIoDevicesInfo *devices_info);

#endif
