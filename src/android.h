/*
 * Copyright (c) 2016 libsoundio
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_ANDROID_H
#define SOUNDIO_ANDROID_H

#include "soundio_internal.h"

struct SoundIoPrivate;
enum SoundIoError soundio_android_init(struct SoundIoPrivate *si);

#endif
