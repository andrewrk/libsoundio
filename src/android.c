/*
 * Copyright (c) 2016 libsoundio
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "android.h"
#include "soundio_private.h"

enum SoundIoError soundio_android_init(struct SoundIoPrivate *si) {
    return SoundIoErrorInitAudioBackend;
}
