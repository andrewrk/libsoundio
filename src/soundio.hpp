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

union SoundIoOutStreamBackendData {
#ifdef SOUNDIO_HAVE_JACK
    SoundIoOutStreamJack jack;
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    SoundIoOutStreamPulseAudio pulseaudio;
#endif
#ifdef SOUNDIO_HAVE_ALSA
    SoundIoOutStreamAlsa alsa;
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    SoundIoOutStreamCoreAudio coreaudio;
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    SoundIoOutStreamWasapi wasapi;
#endif
    SoundIoOutStreamDummy dummy;
};

union SoundIoInStreamBackendData {
#ifdef SOUNDIO_HAVE_JACK
    SoundIoInStreamJack jack;
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    SoundIoInStreamPulseAudio pulseaudio;
#endif
#ifdef SOUNDIO_HAVE_ALSA
    SoundIoInStreamAlsa alsa;
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    SoundIoInStreamCoreAudio coreaudio;
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    SoundIoInStreamWasapi wasapi;
#endif
    SoundIoInStreamDummy dummy;
};

struct SoundIoDevicesInfo {
    SoundIoList<SoundIoDevice *> input_devices;
    SoundIoList<SoundIoDevice *> output_devices;
    // can be -1 when default device is unknown
    int default_output_index;
    int default_input_index;
};

struct SoundIoOutStreamPrivate {
    SoundIoOutStream pub;
    SoundIoOutStreamBackendData backend_data;
};

struct SoundIoInStreamPrivate {
    SoundIoInStream pub;
    SoundIoInStreamBackendData backend_data;
};

struct SoundIoPrivate {
    struct SoundIo pub;

    // Safe to read from a single thread without a mutex.
    struct SoundIoDevicesInfo *safe_devices_info;

    void (*destroy)(struct SoundIoPrivate *);
    void (*flush_events)(struct SoundIoPrivate *);
    void (*wait_events)(struct SoundIoPrivate *);
    void (*wakeup)(struct SoundIoPrivate *);

    int (*outstream_open)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    void (*outstream_destroy)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    int (*outstream_start)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    int (*outstream_begin_write)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *,
            SoundIoChannelArea **out_areas, int *out_frame_count);
    int (*outstream_end_write)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    int (*outstream_clear_buffer)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    int (*outstream_pause)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *, bool pause);


    int (*instream_open)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    void (*instream_destroy)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    int (*instream_start)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    int (*instream_begin_read)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *,
            SoundIoChannelArea **out_areas, int *out_frame_count);
    int (*instream_end_read)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    int (*instream_pause)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *, bool pause);

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
