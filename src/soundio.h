/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_SOUNDIO_H
#define SOUNDIO_SOUNDIO_H

#include "config.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

enum SoundIoBackend {
    SoundIoBackendPulseAudio,
    SoundIoBackendDummy,
};


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
