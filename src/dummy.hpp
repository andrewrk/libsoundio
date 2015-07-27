/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_DUMMY_HPP
#define SOUNDIO_DUMMY_HPP

#include "os.hpp"

int soundio_dummy_init(struct SoundIoPrivate *si);

struct SoundIoDummy {
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;
    bool devices_emitted;
};

struct SoundIoDeviceDummy {

};

#endif
