/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_COREAUDIO_HPP
#define SOUNDIO_COREAUDIO_HPP

#include "soundio/soundio.h"
#include "soundio/os.h"
#include "atomics.hpp"

#include <CoreAudio.h>

int soundio_coreaudio_init(struct SoundIoPrivate *si);

struct SoundIoDeviceCoreAudio { };

struct SoundIoCoreAudio {
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;
    struct SoundIoOsThread *thread;
    atomic_flag abort_flag;

    // this one is ready to be read with flush_events. protected by mutex
    struct SoundIoDevicesInfo *ready_devices_info;
    atomic_bool have_devices_flag;
    SoundIoOsCond *have_devices_cond;

    atomic_bool device_scan_queued;
    atomic_bool service_restarted;
    int shutdown_err;
    bool emitted_shutdown_cb;
};

struct SoundIoOutStreamCoreAudioPort {
};

struct SoundIoOutStreamCoreAudio {
};

struct SoundIoInStreamCoreAudioPort {
};

struct SoundIoInStreamCoreAudio {
};

#endif
