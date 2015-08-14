/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_WASAPI_HPP
#define SOUNDIO_WASAPI_HPP

#include "soundio/soundio.h"
#include "soundio/os.h"
#include "atomics.hpp"
#include "list.hpp"

#define INITGUID
#define CINTERFACE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmreg.h>
#include <audioclient.h>
#include <audiosessiontypes.h>

int soundio_wasapi_init(struct SoundIoPrivate *si);

struct SoundIoDeviceWasapi {
    SoundIoList<SoundIoSampleRateRange> sample_rates;
    double period_duration;
    IMMDevice *mm_device;
};

struct SoundIoWasapi {
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;
    SoundIoOsCond *scan_devices_cond;
    struct SoundIoOsThread *thread;
    atomic_flag abort_flag;
    // this one is ready to be read with flush_events. protected by mutex
    struct SoundIoDevicesInfo *ready_devices_info;
    atomic_bool have_devices_flag;
    atomic_bool device_scan_queued;
    int shutdown_err;
    bool emitted_shutdown_cb;

    IMMDeviceEnumerator* device_enumerator;
    IMMNotificationClient device_events;
    LONG device_events_refs;
};

struct SoundIoOutStreamWasapi {
    IAudioClient *audio_client;
    IAudioClockAdjustment *audio_clock_adjustment;
    bool need_resample;
    SoundIoOsThread *thread;
    atomic_flag thread_exit_flag;
    bool is_raw;
    UINT32 buffer_frame_count;
};

struct SoundIoInStreamWasapi {
};

#endif
