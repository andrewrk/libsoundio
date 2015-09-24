/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_SOUNDIO_HPP
#define SOUNDIO_SOUNDIO_HPP

#include "soundio_private.h"
#include "list.hpp"

#ifdef SOUNDIO_HAVE_JACK
#include "jack.hpp"
#endif

#ifdef SOUNDIO_HAVE_PULSEAUDIO
#include "pulseaudio.hpp"
#endif

#ifdef SOUNDIO_HAVE_ALSA
#include "alsa.hpp"
#endif

#ifdef SOUNDIO_HAVE_COREAUDIO
#include "coreaudio.hpp"
#endif

#ifdef SOUNDIO_HAVE_WASAPI
#include "wasapi.hpp"
#endif

#include "dummy.hpp"

union SoundIoBackendData {
#ifdef SOUNDIO_HAVE_JACK
    SoundIoJack jack;
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    SoundIoPulseAudio pulseaudio;
#endif
#ifdef SOUNDIO_HAVE_ALSA
    SoundIoAlsa alsa;
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    SoundIoCoreAudio coreaudio;
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    SoundIoWasapi wasapi;
#endif
    SoundIoDummy dummy;
};

union SoundIoDeviceBackendData {
#ifdef SOUNDIO_HAVE_JACK
    SoundIoDeviceJack jack;
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    SoundIoDevicePulseAudio pulseaudio;
#endif
#ifdef SOUNDIO_HAVE_ALSA
    SoundIoDeviceAlsa alsa;
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    SoundIoDeviceCoreAudio coreaudio;
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    SoundIoDeviceWasapi wasapi;
#endif
    SoundIoDeviceDummy dummy;
};

union SoundIoStreamBackendData {
#ifdef SOUNDIO_HAVE_JACK
    SoundIoStreamJack jack;
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    SoundIoStreamPulseAudio pulseaudio;
#endif
#ifdef SOUNDIO_HAVE_ALSA
    SoundIoStreamAlsa alsa;
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    SoundIoStreamCoreAudio coreaudio;
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    SoundIoStreamWasapi wasapi;
#endif
    SoundIoStreamDummy dummy;
};

struct SoundIoDevicesInfo {
    SoundIoList<SoundIoDevice *> input_devices;
    SoundIoList<SoundIoDevice *> output_devices;
    // can be -1 when default device is unknown
    int default_output_index;
    int default_input_index;
};

struct SoundIoStreamPrivate {
    SoundIoStream pub;
    SoundIoStreamBackendData backend_data;
};

struct SoundIoPrivate {
    struct SoundIo pub;

    // Safe to read from a single thread without a mutex.
    struct SoundIoDevicesInfo *safe_devices_info;

    void (*destroy)(struct SoundIoPrivate *);
    void (*flush_events)(struct SoundIoPrivate *);
    void (*wait_events)(struct SoundIoPrivate *);
    void (*wakeup)(struct SoundIoPrivate *);
    void (*force_device_scan)(struct SoundIoPrivate *);

    int (*stream_open)(struct SoundIoPrivate *, struct SoundIoStreamPrivate *);
    void (*stream_destroy)(struct SoundIoPrivate *, struct SoundIoStreamPrivate *);
    int (*stream_start)(struct SoundIoPrivate *, struct SoundIoStreamPrivate *);
    int (*stream_begin_write)(struct SoundIoPrivate *, struct SoundIoStreamPrivate *,
            SoundIoChannelArea **out_areas, int *out_frame_count);
    int (*stream_end_write)(struct SoundIoPrivate *, struct SoundIoStreamPrivate *);
    int (*stream_begin_read)(struct SoundIoPrivate *, struct SoundIoStreamPrivate *,
            SoundIoChannelArea **out_areas, int *out_frame_count);
    int (*stream_end_read)(struct SoundIoPrivate *, struct SoundIoStreamPrivate *);
    int (*stream_clear_buffer)(struct SoundIoPrivate *, struct SoundIoStreamPrivate *);
    int (*stream_pause)(struct SoundIoPrivate *, struct SoundIoStreamPrivate *, bool pause);
    int (*stream_get_latency)(struct SoundIoPrivate *, struct SoundIoStreamPrivate *, double *out_latency);

    SoundIoBackendData backend_data;
};

struct SoundIoDevicePrivate {
    SoundIoDevice pub;
    SoundIoDeviceBackendData backend_data;
    void (*destruct)(SoundIoDevicePrivate *);
    SoundIoSampleRateRange prealloc_sample_rate_range;
    SoundIoList<SoundIoSampleRateRange> sample_rates;
    SoundIoFormat prealloc_format;
};

void soundio_destroy_devices_info(struct SoundIoDevicesInfo *devices_info);

static const int SOUNDIO_MIN_SAMPLE_RATE = 8000;
static const int SOUNDIO_MAX_SAMPLE_RATE = 5644800;

#endif
