/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_SOUNDIO_PRIVATE_H
#define SOUNDIO_SOUNDIO_PRIVATE_H

#include "soundio_internal.h"
#include "config.h"
#include "list.h"

#ifdef SOUNDIO_HAVE_JACK
#include "jack.h"
#endif

#ifdef SOUNDIO_HAVE_PULSEAUDIO
#include "pulseaudio.h"
#endif

#ifdef SOUNDIO_HAVE_ALSA
#include "alsa.h"
#endif

#ifdef SOUNDIO_HAVE_COREAUDIO
#include "coreaudio.h"
#endif

#ifdef SOUNDIO_HAVE_WASAPI
#include "wasapi.h"
#endif

#ifdef SOUNDIO_HAVE_ANDROID
#include "android.h"
#endif

#include "dummy.h"

union SoundIoBackendData {
#ifdef SOUNDIO_HAVE_JACK
    struct SoundIoJack jack;
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    struct SoundIoPulseAudio pulseaudio;
#endif
#ifdef SOUNDIO_HAVE_ALSA
    struct SoundIoAlsa alsa;
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    struct SoundIoCoreAudio coreaudio;
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    struct SoundIoWasapi wasapi;
#endif
#ifdef SOUNDIO_HAVE_ANDROID
    struct SoundIoAndroid android;
#endif
    struct SoundIoDummy dummy;
};

union SoundIoDeviceBackendData {
#ifdef SOUNDIO_HAVE_JACK
    struct SoundIoDeviceJack jack;
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    struct SoundIoDevicePulseAudio pulseaudio;
#endif
#ifdef SOUNDIO_HAVE_ALSA
    struct SoundIoDeviceAlsa alsa;
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    struct SoundIoDeviceCoreAudio coreaudio;
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    struct SoundIoDeviceWasapi wasapi;
#endif
    struct SoundIoDeviceDummy dummy;
};

union SoundIoOutStreamBackendData {
#ifdef SOUNDIO_HAVE_JACK
    struct SoundIoOutStreamJack jack;
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    struct SoundIoOutStreamPulseAudio pulseaudio;
#endif
#ifdef SOUNDIO_HAVE_ALSA
    struct SoundIoOutStreamAlsa alsa;
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    struct SoundIoOutStreamCoreAudio coreaudio;
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    struct SoundIoOutStreamWasapi wasapi;
#endif
    struct SoundIoOutStreamDummy dummy;
};

union SoundIoInStreamBackendData {
#ifdef SOUNDIO_HAVE_JACK
    struct SoundIoInStreamJack jack;
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    struct SoundIoInStreamPulseAudio pulseaudio;
#endif
#ifdef SOUNDIO_HAVE_ALSA
    struct SoundIoInStreamAlsa alsa;
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    struct SoundIoInStreamCoreAudio coreaudio;
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    struct SoundIoInStreamWasapi wasapi;
#endif
    struct SoundIoInStreamDummy dummy;
};

SOUNDIO_MAKE_LIST_STRUCT(struct SoundIoDevice*, SoundIoListDevicePtr, SOUNDIO_LIST_NOT_STATIC)
SOUNDIO_MAKE_LIST_PROTO(struct SoundIoDevice*, SoundIoListDevicePtr, SOUNDIO_LIST_NOT_STATIC)

struct SoundIoDevicesInfo {
    struct SoundIoListDevicePtr input_devices;
    struct SoundIoListDevicePtr output_devices;
    // can be -1 when default device is unknown
    int default_output_index;
    int default_input_index;
};

struct SoundIoOutStreamPrivate {
    struct SoundIoOutStream pub;
    union SoundIoOutStreamBackendData backend_data;
};

struct SoundIoInStreamPrivate {
    struct SoundIoInStream pub;
    union SoundIoInStreamBackendData backend_data;
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

    enum SoundIoError (*outstream_open)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    void (*outstream_destroy)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    enum SoundIoError (*outstream_start)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    enum SoundIoError (*outstream_begin_write)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *,
            struct SoundIoChannelArea **out_areas, int *out_frame_count);
    enum SoundIoError (*outstream_end_write)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    enum SoundIoError (*outstream_clear_buffer)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    enum SoundIoError (*outstream_pause)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *, bool pause);
    enum SoundIoError (*outstream_get_latency)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *, double *out_latency);


    enum SoundIoError (*instream_open)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    void (*instream_destroy)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    enum SoundIoError (*instream_start)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    enum SoundIoError (*instream_begin_read)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *,
            struct SoundIoChannelArea **out_areas, int *out_frame_count);
    enum SoundIoError (*instream_end_read)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    enum SoundIoError (*instream_pause)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *, bool pause);
    enum SoundIoError (*instream_get_latency)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *, double *out_latency);

    union SoundIoBackendData backend_data;
};

SOUNDIO_MAKE_LIST_STRUCT(struct SoundIoSampleRateRange, SoundIoListSampleRateRange, SOUNDIO_LIST_NOT_STATIC)
SOUNDIO_MAKE_LIST_PROTO(struct SoundIoSampleRateRange, SoundIoListSampleRateRange, SOUNDIO_LIST_NOT_STATIC)

struct SoundIoDevicePrivate {
    struct SoundIoDevice pub;
    union SoundIoDeviceBackendData backend_data;
    void (*destruct)(struct SoundIoDevicePrivate *);
    struct SoundIoSampleRateRange prealloc_sample_rate_range;
    struct SoundIoListSampleRateRange sample_rates;
    enum SoundIoFormat prealloc_format;
};

void soundio_destroy_devices_info(struct SoundIoDevicesInfo *devices_info);

static const int SOUNDIO_MIN_SAMPLE_RATE = 8000;
static const int SOUNDIO_MAX_SAMPLE_RATE = 5644800;

#endif
