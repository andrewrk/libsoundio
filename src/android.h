/*
 * Copyright (c) 2016 libsoundio
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_ANDROID_H
#define SOUNDIO_ANDROID_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "soundio_internal.h"
#include "os.h"
#include "soundio_internal.h"

struct SoundIoPrivate;
enum SoundIoError soundio_android_init(struct SoundIoPrivate *si);

struct SoundIoAndroid {
    struct SoundIoOsCond *cond;
    bool devices_emitted;

    SLObjectItf engineObject;
    SLEngineItf engineEngine;
};

struct SoundIoOutStreamAndroid {
    SLObjectItf outputMixObject;
    SLObjectItf playerObject;
    SLPlayItf playerPlay;
    SLAndroidSimpleBufferQueueItf playerBufferQueue;

    int curBuffer;
    size_t write_frame_count;
    size_t bytes_per_buffer;
    char *buffers[2];
    struct SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

struct SoundIoInStreamAndroid {
    SLObjectItf audioRecorderObject;
    SLRecordItf recorderRecord;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue;

    int curBuffer;
    size_t bytes_per_buffer;
    char *buffers[2];
    struct SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

#endif
