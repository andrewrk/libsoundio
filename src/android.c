/*
 * Copyright (c) 2016 libsoundio
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "android.h"
#include "soundio_private.h"

static void destroy_android(struct SoundIoPrivate *si) {
    struct SoundIoAndroid *sia = &si->backend_data.android;

    if (sia->cond)
        soundio_os_cond_destroy(sia->cond);

}

static void flush_events_android(struct SoundIoPrivate *si) {
    struct SoundIo *soundio = &si->pub;
    struct SoundIoAndroid *sia = &si->backend_data.android;
    if (sia->devices_emitted)
        return;
    sia->devices_emitted = true;
    soundio->on_devices_change(soundio);
}

static void wait_events_android(struct SoundIoPrivate *si) {
    struct SoundIoAndroid *sia = &si->backend_data.android;
    flush_events_android(si);
    soundio_os_cond_wait(sia->cond, NULL);
}

static void wakeup_android(struct SoundIoPrivate *si) {
    struct SoundIoAndroid *sia = &si->backend_data.android;
    soundio_os_cond_signal(sia->cond, NULL);
}

static void force_device_scan_android(struct SoundIoPrivate *si) {
    // nothing to do; Android devices never change
}

enum SoundIoError soundio_android_init(struct SoundIoPrivate *si) {
    struct SoundIo *soundio = &si->pub;
    struct SoundIoAndroid *sia = &si->backend_data.android;

    sia->cond = soundio_os_cond_create();
    if (!sia->cond) {
        destroy_android(si);
        return SoundIoErrorNoMem;
    }

    si->safe_devices_info = ALLOCATE(struct SoundIoDevicesInfo, 1);
    if (!si->safe_devices_info) {
        destroy_android(si);
        return SoundIoErrorNoMem;
    }

    // TODO: no input devices yet
    si->safe_devices_info->default_input_index = -1;
    si->safe_devices_info->default_output_index = 0;
    si->destroy = destroy_android;
    si->flush_events = flush_events_android;
    si->wait_events = wait_events_android;
    si->wakeup = wakeup_android;
    si->force_device_scan = force_device_scan_android;

    // create output device
    {
        struct SoundIoDevicePrivate *dev = ALLOCATE(struct SoundIoDevicePrivate, 1);
        if (!dev) {
            destroy_android(si);
            return SoundIoErrorNoMem;
        }
        struct SoundIoDevice *device = &dev->pub;

        device->ref_count = 1;
        device->soundio = soundio;
        device->id = strdup("android-out");
        device->name = strdup("Android Output");
        if (!device->id || !device->name) {
            soundio_device_unref(device);
            destroy_android(si);
            return SoundIoErrorNoMem;
        }

        // The following information is according to:
        // https://developer.android.com/ndk/guides/audio/opensl-for-android.html

        // TODO: could be prealloc'd?
        device->layout_count = 2;
        device->layouts = ALLOCATE(struct SoundIoChannelLayout, device->layout_count);
        if (!device->layouts) {
            soundio_device_unref(device);
            destroy_android(si);
            return SoundIoErrorNoMem;
        }
        device->layouts[0] = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);
        device->layouts[1] = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);

        // TODO: Android 5.0 (API level 21) and above support floating-point data.
        // TODO: Could be prealloc'd?
        device->format_count = 2;
        device->formats = ALLOCATE(enum SoundIoFormat, device->format_count);
        if (!device->formats) {
            soundio_device_unref(device);
            destroy_android(si);
            return SoundIoErrorNoMem;
        }
        device->formats[0] = SoundIoFormatU8;
        device->formats[1] = SoundIoFormatS16LE;

        // TODO: Could be prealloc'd?
        device->sample_rate_count = 9;
        device->sample_rates = ALLOCATE(struct SoundIoSampleRateRange, device->sample_rate_count);
        if (!device->sample_rates) {
            soundio_device_unref(device);
            destroy_android(si);
            return SoundIoErrorNoMem;
        }
        device->sample_rates[0].min = device->sample_rates[0].max = 8000;
        device->sample_rates[1].min = device->sample_rates[1].max = 11025;
        device->sample_rates[2].min = device->sample_rates[2].max = 12000;
        device->sample_rates[3].min = device->sample_rates[3].max = 16000;
        device->sample_rates[4].min = device->sample_rates[4].max = 22050;
        device->sample_rates[5].min = device->sample_rates[5].max = 24000;
        device->sample_rates[6].min = device->sample_rates[6].max = 32000;
        device->sample_rates[7].min = device->sample_rates[7].max = 44100;
        device->sample_rates[8].min = device->sample_rates[8].max = 48000;

        // TODO: Not sure what to do with these right now
        device->software_latency_current = 0.1;
        device->software_latency_min = 0.01;
        device->software_latency_max = 4.0;

        // TODO: Any way to get device optimal sample rate here?
        device->sample_rate_current = 48000;
        device->aim = SoundIoDeviceAimOutput;

        if (SoundIoListDevicePtr_append(&si->safe_devices_info->output_devices, device)) {
            soundio_device_unref(device);
            destroy_android(si);
            return SoundIoErrorNoMem;
        }
    }

    return SoundIoErrorNone;
}
