/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio_private.h"
#include "util.h"
#include "os.h"
#include "config.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

static const enum SoundIoBackend available_backends[] = {
#ifdef SOUNDIO_HAVE_JACK
    SoundIoBackendJack,
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    SoundIoBackendPulseAudio,
#endif
#ifdef SOUNDIO_HAVE_ALSA
    SoundIoBackendAlsa,
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    SoundIoBackendCoreAudio,
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    SoundIoBackendWasapi,
#endif
    SoundIoBackendDummy,
};

static int (*backend_init_fns[])(struct SoundIoPrivate *) = {
    [SoundIoBackendNone] = NULL,
#ifdef SOUNDIO_HAVE_JACK
    [SoundIoBackendJack] = soundio_jack_init,
#else
    [SoundIoBackendJack] = NULL,
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    [SoundIoBackendPulseAudio] = soundio_pulseaudio_init,
#else
    [SoundIoBackendPulseAudio] = NULL,
#endif
#ifdef SOUNDIO_HAVE_ALSA
    [SoundIoBackendAlsa] = soundio_alsa_init,
#else
    [SoundIoBackendAlsa] = NULL,
#endif
#ifdef SOUNDIO_HAVE_COREAUDIO
    [SoundIoBackendCoreAudio] = soundio_coreaudio_init,
#else
    [SoundIoBackendCoreAudio] = NULL,
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    [SoundIoBackendWasapi] = soundio_wasapi_init,
#else
    [SoundIoBackendWasapi] = NULL,
#endif
    [SoundIoBackendDummy] = soundio_dummy_init,
};


SOUNDIO_MAKE_LIST_DEF(struct SoundIoDevice*, SoundIoListDevicePtr, SOUNDIO_LIST_NOT_STATIC)
SOUNDIO_MAKE_LIST_DEF(struct SoundIoSampleRateRange, SoundIoListSampleRateRange, SOUNDIO_LIST_NOT_STATIC)

const char *soundio_strerror(int error) {
    switch ((enum SoundIoError)error) {
        case SoundIoErrorNone: return "(no error)";
        case SoundIoErrorNoMem: return "out of memory";
        case SoundIoErrorInitAudioBackend: return "unable to initialize audio backend";
        case SoundIoErrorSystemResources: return "system resource not available";
        case SoundIoErrorOpeningDevice: return "unable to open device";
        case SoundIoErrorNoSuchDevice: return "no such device";
        case SoundIoErrorInvalid: return "invalid value";
        case SoundIoErrorBackendUnavailable: return "backend unavailable";
        case SoundIoErrorStreaming: return "unrecoverable streaming failure";
        case SoundIoErrorIncompatibleDevice: return "incompatible device";
        case SoundIoErrorNoSuchClient: return "no such client";
        case SoundIoErrorIncompatibleBackend: return "incompatible backend";
        case SoundIoErrorBackendDisconnected: return "backend disconnected";
        case SoundIoErrorInterrupted: return "interrupted; try again";
        case SoundIoErrorUnderflow: return "buffer underflow";
        case SoundIoErrorEncodingString: return "failed to encode string";
    }
    return "(invalid error)";
}

int soundio_get_bytes_per_sample(enum SoundIoFormat format) {
    switch (format) {
    case SoundIoFormatU8:         return 1;
    case SoundIoFormatS8:         return 1;
    case SoundIoFormatS16LE:      return 2;
    case SoundIoFormatS16BE:      return 2;
    case SoundIoFormatU16LE:      return 2;
    case SoundIoFormatU16BE:      return 2;
    case SoundIoFormatS24LE:      return 4;
    case SoundIoFormatS24BE:      return 4;
    case SoundIoFormatU24LE:      return 4;
    case SoundIoFormatU24BE:      return 4;
    case SoundIoFormatS32LE:      return 4;
    case SoundIoFormatS32BE:      return 4;
    case SoundIoFormatU32LE:      return 4;
    case SoundIoFormatU32BE:      return 4;
    case SoundIoFormatFloat32LE:  return 4;
    case SoundIoFormatFloat32BE:  return 4;
    case SoundIoFormatFloat64LE:  return 8;
    case SoundIoFormatFloat64BE:  return 8;

    case SoundIoFormatInvalid:    return -1;
    }
    return -1;
}

const char * soundio_format_string(enum SoundIoFormat format) {
    switch (format) {
    case SoundIoFormatS8:         return "signed 8-bit";
    case SoundIoFormatU8:         return "unsigned 8-bit";
    case SoundIoFormatS16LE:      return "signed 16-bit LE";
    case SoundIoFormatS16BE:      return "signed 16-bit BE";
    case SoundIoFormatU16LE:      return "unsigned 16-bit LE";
    case SoundIoFormatU16BE:      return "unsigned 16-bit LE";
    case SoundIoFormatS24LE:      return "signed 24-bit LE";
    case SoundIoFormatS24BE:      return "signed 24-bit BE";
    case SoundIoFormatU24LE:      return "unsigned 24-bit LE";
    case SoundIoFormatU24BE:      return "unsigned 24-bit BE";
    case SoundIoFormatS32LE:      return "signed 32-bit LE";
    case SoundIoFormatS32BE:      return "signed 32-bit BE";
    case SoundIoFormatU32LE:      return "unsigned 32-bit LE";
    case SoundIoFormatU32BE:      return "unsigned 32-bit BE";
    case SoundIoFormatFloat32LE:  return "float 32-bit LE";
    case SoundIoFormatFloat32BE:  return "float 32-bit BE";
    case SoundIoFormatFloat64LE:  return "float 64-bit LE";
    case SoundIoFormatFloat64BE:  return "float 64-bit BE";

    case SoundIoFormatInvalid:
        return "(invalid sample format)";
    }
    return "(invalid sample format)";
}


const char *soundio_backend_name(enum SoundIoBackend backend) {
    switch (backend) {
        case SoundIoBackendNone: return "(none)";
        case SoundIoBackendJack: return "JACK";
        case SoundIoBackendPulseAudio: return "PulseAudio";
        case SoundIoBackendAlsa: return "ALSA";
        case SoundIoBackendCoreAudio: return "CoreAudio";
        case SoundIoBackendWasapi: return "WASAPI";
        case SoundIoBackendDummy: return "Dummy";
    }
    return "(invalid backend)";
}

void soundio_destroy(struct SoundIo *soundio) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    soundio_disconnect(soundio);

    free(si);
}

static void do_nothing_cb(struct SoundIo *soundio) { }
static void default_msg_callback(const char *msg) { }

static void default_backend_disconnect_cb(struct SoundIo *soundio, int err) {
    soundio_panic("libsoundio: backend disconnected: %s", soundio_strerror(err));
}

static atomic_flag rtprio_seen = ATOMIC_FLAG_INIT;
static void default_emit_rtprio_warning(void) {
    if (!atomic_flag_test_and_set(&rtprio_seen)) {
        fprintf(stderr, "warning: unable to set high priority thread: Operation not permitted\n");
        fprintf(stderr, "See "
            "https://github.com/andrewrk/genesis/wiki/warning:-unable-to-set-high-priority-thread:-Operation-not-permitted\n");
    }
}

struct SoundIo *soundio_create(void) {
    int err;
    if ((err = soundio_os_init()))
        return NULL;
    struct SoundIoPrivate *si = ALLOCATE(struct SoundIoPrivate, 1);
    if (!si)
        return NULL;
    struct SoundIo *soundio = &si->pub;
    soundio->on_devices_change = do_nothing_cb;
    soundio->on_backend_disconnect = default_backend_disconnect_cb;
    soundio->on_events_signal = do_nothing_cb;
    soundio->app_name = "SoundIo";
    soundio->emit_rtprio_warning = default_emit_rtprio_warning;
    soundio->jack_info_callback = default_msg_callback;
    soundio->jack_error_callback = default_msg_callback;
    return soundio;
}

int soundio_connect(struct SoundIo *soundio) {
    int err = 0;

    for (int i = 0; i < ARRAY_LENGTH(available_backends); i += 1) {
        enum SoundIoBackend backend = available_backends[i];
        err = soundio_connect_backend(soundio, backend);
        if (!err)
            return 0;
        if (err != SoundIoErrorInitAudioBackend)
            return err;
    }

    return err;
}

int soundio_connect_backend(struct SoundIo *soundio, enum SoundIoBackend backend) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    if (soundio->current_backend)
        return SoundIoErrorInvalid;

    if (backend <= 0 || backend > SoundIoBackendDummy)
        return SoundIoErrorInvalid;

    int (*fn)(struct SoundIoPrivate *) = backend_init_fns[backend];

    if (!fn)
        return SoundIoErrorBackendUnavailable;

    int err;
    if ((err = backend_init_fns[backend](si))) {
        soundio_disconnect(soundio);
        return err;
    }
    soundio->current_backend = backend;

    return 0;
}

void soundio_disconnect(struct SoundIo *soundio) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    if (!si)
        return;

    if (si->destroy)
        si->destroy(si);
    memset(&si->backend_data, 0, sizeof(union SoundIoBackendData));

    soundio->current_backend = SoundIoBackendNone;

    soundio_destroy_devices_info(si->safe_devices_info);
    si->safe_devices_info = NULL;

    si->destroy = NULL;
    si->flush_events = NULL;
    si->wait_events = NULL;
    si->wakeup = NULL;
    si->force_device_scan = NULL;

    si->outstream_open = NULL;
    si->outstream_destroy = NULL;
    si->outstream_start = NULL;
    si->outstream_begin_write = NULL;
    si->outstream_end_write = NULL;
    si->outstream_clear_buffer = NULL;
    si->outstream_pause = NULL;
    si->outstream_get_latency = NULL;

    si->instream_open = NULL;
    si->instream_destroy = NULL;
    si->instream_start = NULL;
    si->instream_begin_read = NULL;
    si->instream_end_read = NULL;
    si->instream_pause = NULL;
    si->instream_get_latency = NULL;
}

void soundio_flush_events(struct SoundIo *soundio) {
    assert(soundio->current_backend != SoundIoBackendNone);
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    si->flush_events(si);
}

int soundio_input_device_count(struct SoundIo *soundio) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return -1;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return -1;

    return si->safe_devices_info->input_devices.length;
}

int soundio_output_device_count(struct SoundIo *soundio) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return -1;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return -1;

    return si->safe_devices_info->output_devices.length;
}

int soundio_default_input_device_index(struct SoundIo *soundio) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return -1;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return -1;

    return si->safe_devices_info->default_input_index;
}

int soundio_default_output_device_index(struct SoundIo *soundio) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return -1;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return -1;

    return si->safe_devices_info->default_output_index;
}

struct SoundIoDevice *soundio_get_input_device(struct SoundIo *soundio, int index) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return NULL;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return NULL;

    assert(index >= 0);
    assert(index < si->safe_devices_info->input_devices.length);
    if (index < 0 || index >= si->safe_devices_info->input_devices.length)
        return NULL;

    struct SoundIoDevice *device = SoundIoListDevicePtr_val_at(&si->safe_devices_info->input_devices, index);
    soundio_device_ref(device);
    return device;
}

struct SoundIoDevice *soundio_get_output_device(struct SoundIo *soundio, int index) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return NULL;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return NULL;

    assert(index >= 0);
    assert(index < si->safe_devices_info->output_devices.length);
    if (index < 0 || index >= si->safe_devices_info->output_devices.length)
        return NULL;

    struct SoundIoDevice *device = SoundIoListDevicePtr_val_at(&si->safe_devices_info->output_devices, index);
    soundio_device_ref(device);
    return device;
}

void soundio_device_unref(struct SoundIoDevice *device) {
    if (!device)
        return;

    device->ref_count -= 1;
    assert(device->ref_count >= 0);

    if (device->ref_count == 0) {
        struct SoundIoDevicePrivate *dev = (struct SoundIoDevicePrivate *)device;
        if (dev->destruct)
            dev->destruct(dev);

        free(device->id);
        free(device->name);

        if (device->sample_rates != &dev->prealloc_sample_rate_range &&
            device->sample_rates != dev->sample_rates.items)
        {
            free(device->sample_rates);
        }
        SoundIoListSampleRateRange_deinit(&dev->sample_rates);

        if (device->formats != &dev->prealloc_format)
            free(device->formats);

        if (device->layouts != &device->current_layout)
            free(device->layouts);

        free(dev);
    }
}

void soundio_device_ref(struct SoundIoDevice *device) {
    assert(device);
    device->ref_count += 1;
}

void soundio_wait_events(struct SoundIo *soundio) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    si->wait_events(si);
}

void soundio_wakeup(struct SoundIo *soundio) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    si->wakeup(si);
}

void soundio_force_device_scan(struct SoundIo *soundio) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    si->force_device_scan(si);
}

int soundio_outstream_begin_write(struct SoundIoOutStream *outstream,
        struct SoundIoChannelArea **areas, int *frame_count)
{
    struct SoundIo *soundio = outstream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *)outstream;
    if (*frame_count <= 0)
        return SoundIoErrorInvalid;
    return si->outstream_begin_write(si, os, areas, frame_count);
}

int soundio_outstream_end_write(struct SoundIoOutStream *outstream) {
    struct SoundIo *soundio = outstream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *)outstream;
    return si->outstream_end_write(si, os);
}

static void default_outstream_error_callback(struct SoundIoOutStream *os, int err) {
    soundio_panic("libsoundio: %s", soundio_strerror(err));
}

static void default_underflow_callback(struct SoundIoOutStream *outstream) { }

struct SoundIoOutStream *soundio_outstream_create(struct SoundIoDevice *device) {
    struct SoundIoOutStreamPrivate *os = ALLOCATE(struct SoundIoOutStreamPrivate, 1);
    struct SoundIoOutStream *outstream = &os->pub;

    if (!os)
        return NULL;
    if (!device)
        return NULL;

    outstream->device = device;
    soundio_device_ref(device);

    outstream->error_callback = default_outstream_error_callback;
    outstream->underflow_callback = default_underflow_callback;

    return outstream;
}

int soundio_outstream_open(struct SoundIoOutStream *outstream) {
    struct SoundIoDevice *device = outstream->device;

    if (device->aim != SoundIoDeviceAimOutput)
        return SoundIoErrorInvalid;

    if (device->probe_error)
        return device->probe_error;

    if (outstream->layout.channel_count > SOUNDIO_MAX_CHANNELS)
        return SoundIoErrorInvalid;

    if (outstream->format == SoundIoFormatInvalid) {
        outstream->format = soundio_device_supports_format(device, SoundIoFormatFloat32NE) ?
            SoundIoFormatFloat32NE : device->formats[0];
    }

    if (outstream->format <= SoundIoFormatInvalid)
        return SoundIoErrorInvalid;

    if (!outstream->layout.channel_count) {
        const struct SoundIoChannelLayout *stereo = soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
        outstream->layout = soundio_device_supports_layout(device, stereo) ? *stereo : device->layouts[0];
    }

    if (!outstream->sample_rate)
        outstream->sample_rate = soundio_device_nearest_sample_rate(device, 48000);

    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *)outstream;
    outstream->bytes_per_frame = soundio_get_bytes_per_frame(outstream->format, outstream->layout.channel_count);
    outstream->bytes_per_sample = soundio_get_bytes_per_sample(outstream->format);

    struct SoundIo *soundio = device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    return si->outstream_open(si, os);
}

void soundio_outstream_destroy(struct SoundIoOutStream *outstream) {
    if (!outstream)
        return;

    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *)outstream;
    struct SoundIo *soundio = outstream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    if (si->outstream_destroy)
        si->outstream_destroy(si, os);

    soundio_device_unref(outstream->device);
    free(os);
}

int soundio_outstream_start(struct SoundIoOutStream *outstream) {
    struct SoundIo *soundio = outstream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *)outstream;
    return si->outstream_start(si, os);
}

int soundio_outstream_pause(struct SoundIoOutStream *outstream, bool pause) {
    struct SoundIo *soundio = outstream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *)outstream;
    return si->outstream_pause(si, os, pause);
}

int soundio_outstream_clear_buffer(struct SoundIoOutStream *outstream) {
    struct SoundIo *soundio = outstream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *)outstream;
    return si->outstream_clear_buffer(si, os);
}

int soundio_outstream_get_latency(struct SoundIoOutStream *outstream, double *out_latency) {
    struct SoundIo *soundio = outstream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *)outstream;
    return si->outstream_get_latency(si, os, out_latency);
}

static void default_instream_error_callback(struct SoundIoInStream *is, int err) {
    soundio_panic("libsoundio: %s", soundio_strerror(err));
}

static void default_overflow_callback(struct SoundIoInStream *instream) { }

struct SoundIoInStream *soundio_instream_create(struct SoundIoDevice *device) {
    struct SoundIoInStreamPrivate *is = ALLOCATE(struct SoundIoInStreamPrivate, 1);
    struct SoundIoInStream *instream = &is->pub;

    if (!is)
        return NULL;
    if (!device)
        return NULL;

    instream->device = device;
    soundio_device_ref(device);

    instream->error_callback = default_instream_error_callback;
    instream->overflow_callback = default_overflow_callback;

    return instream;
}

int soundio_instream_open(struct SoundIoInStream *instream) {
    struct SoundIoDevice *device = instream->device;
    if (device->aim != SoundIoDeviceAimInput)
        return SoundIoErrorInvalid;

    if (instream->format <= SoundIoFormatInvalid)
        return SoundIoErrorInvalid;

    if (instream->layout.channel_count > SOUNDIO_MAX_CHANNELS)
        return SoundIoErrorInvalid;

    if (device->probe_error)
        return device->probe_error;

    if (instream->format == SoundIoFormatInvalid) {
        instream->format = soundio_device_supports_format(device, SoundIoFormatFloat32NE) ?
            SoundIoFormatFloat32NE : device->formats[0];
    }

    if (!instream->layout.channel_count) {
        const struct SoundIoChannelLayout *stereo = soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
        instream->layout = soundio_device_supports_layout(device, stereo) ? *stereo : device->layouts[0];
    }

    if (!instream->sample_rate)
        instream->sample_rate = soundio_device_nearest_sample_rate(device, 48000);


    instream->bytes_per_frame = soundio_get_bytes_per_frame(instream->format, instream->layout.channel_count);
    instream->bytes_per_sample = soundio_get_bytes_per_sample(instream->format);
    struct SoundIo *soundio = device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoInStreamPrivate *is = (struct SoundIoInStreamPrivate *)instream;
    return si->instream_open(si, is);
}

int soundio_instream_start(struct SoundIoInStream *instream) {
    struct SoundIo *soundio = instream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoInStreamPrivate *is = (struct SoundIoInStreamPrivate *)instream;
    return si->instream_start(si, is);
}

void soundio_instream_destroy(struct SoundIoInStream *instream) {
    if (!instream)
        return;

    struct SoundIoInStreamPrivate *is = (struct SoundIoInStreamPrivate *)instream;
    struct SoundIo *soundio = instream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;

    if (si->instream_destroy)
        si->instream_destroy(si, is);

    soundio_device_unref(instream->device);
    free(is);
}

int soundio_instream_pause(struct SoundIoInStream *instream, bool pause) {
    struct SoundIo *soundio = instream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoInStreamPrivate *is = (struct SoundIoInStreamPrivate *)instream;
    return si->instream_pause(si, is, pause);
}

int soundio_instream_begin_read(struct SoundIoInStream *instream,
        struct SoundIoChannelArea **areas, int *frame_count)
{
    struct SoundIo *soundio = instream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoInStreamPrivate *is = (struct SoundIoInStreamPrivate *)instream;
    return si->instream_begin_read(si, is, areas, frame_count);
}

int soundio_instream_end_read(struct SoundIoInStream *instream) {
    struct SoundIo *soundio = instream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoInStreamPrivate *is = (struct SoundIoInStreamPrivate *)instream;
    return si->instream_end_read(si, is);
}

int soundio_instream_get_latency(struct SoundIoInStream *instream, double *out_latency) {
    struct SoundIo *soundio = instream->device->soundio;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)soundio;
    struct SoundIoInStreamPrivate *is = (struct SoundIoInStreamPrivate *)instream;
    return si->instream_get_latency(si, is, out_latency);
}

void soundio_destroy_devices_info(struct SoundIoDevicesInfo *devices_info) {
    if (!devices_info)
        return;

    for (int i = 0; i < devices_info->input_devices.length; i += 1)
        soundio_device_unref(SoundIoListDevicePtr_val_at(&devices_info->input_devices, i));
    for (int i = 0; i < devices_info->output_devices.length; i += 1)
        soundio_device_unref(SoundIoListDevicePtr_val_at(&devices_info->output_devices, i));

    SoundIoListDevicePtr_deinit(&devices_info->input_devices);
    SoundIoListDevicePtr_deinit(&devices_info->output_devices);

    free(devices_info);
}

bool soundio_have_backend(enum SoundIoBackend backend) {
    assert(backend > 0);
    assert(backend <= SoundIoBackendDummy);
    return backend_init_fns[backend];
}

int soundio_backend_count(struct SoundIo *soundio) {
    return ARRAY_LENGTH(available_backends);
}

enum SoundIoBackend soundio_get_backend(struct SoundIo *soundio, int index) {
    return available_backends[index];
}

static bool layout_contains(const struct SoundIoChannelLayout *available_layouts, int available_layouts_count,
        const struct SoundIoChannelLayout *target_layout)
{
    for (int i = 0; i < available_layouts_count; i += 1) {
        const struct SoundIoChannelLayout *available_layout = &available_layouts[i];
        if (soundio_channel_layout_equal(target_layout, available_layout))
            return true;
    }
    return false;
}

const struct SoundIoChannelLayout *soundio_best_matching_channel_layout(
        const struct SoundIoChannelLayout *preferred_layouts, int preferred_layouts_count,
        const struct SoundIoChannelLayout *available_layouts, int available_layouts_count)
{
    for (int i = 0; i < preferred_layouts_count; i += 1) {
        const struct SoundIoChannelLayout *preferred_layout = &preferred_layouts[i];
        if (layout_contains(available_layouts, available_layouts_count, preferred_layout))
            return preferred_layout;
    }
    return NULL;
}

static int compare_layouts(const void *a, const void *b) {
    const struct SoundIoChannelLayout *layout_a = *((struct SoundIoChannelLayout **)a);
    const struct SoundIoChannelLayout *layout_b = *((struct SoundIoChannelLayout **)b);
    if (layout_a->channel_count > layout_b->channel_count)
        return -1;
    else if (layout_a->channel_count < layout_b->channel_count)
        return 1;
    else
        return 0;
}

void soundio_sort_channel_layouts(struct SoundIoChannelLayout *layouts, int layouts_count) {
    if (!layouts)
        return;

    qsort(layouts, layouts_count, sizeof(struct SoundIoChannelLayout), compare_layouts);
}

void soundio_device_sort_channel_layouts(struct SoundIoDevice *device) {
    soundio_sort_channel_layouts(device->layouts, device->layout_count);
}

bool soundio_device_supports_format(struct SoundIoDevice *device, enum SoundIoFormat format) {
    for (int i = 0; i < device->format_count; i += 1) {
        if (device->formats[i] == format)
            return true;
    }
    return false;
}

bool soundio_device_supports_layout(struct SoundIoDevice *device,
        const struct SoundIoChannelLayout *layout)
{
    for (int i = 0; i < device->layout_count; i += 1) {
        if (soundio_channel_layout_equal(&device->layouts[i], layout))
            return true;
    }
    return false;
}

bool soundio_device_supports_sample_rate(struct SoundIoDevice *device, int sample_rate) {
    for (int i = 0; i < device->sample_rate_count; i += 1) {
        struct SoundIoSampleRateRange *range = &device->sample_rates[i];
        if (sample_rate >= range->min && sample_rate <= range->max)
            return true;
    }
    return false;
}

static int abs_diff_int(int a, int b) {
    int x = a - b;
    return (x >= 0) ? x : -x;
}

int soundio_device_nearest_sample_rate(struct SoundIoDevice *device, int sample_rate) {
    int best_rate = -1;
    int best_delta = -1;
    for (int i = 0; i < device->sample_rate_count; i += 1) {
        struct SoundIoSampleRateRange *range = &device->sample_rates[i];
        int candidate_rate = soundio_int_clamp(range->min, sample_rate, range->max);
        if (candidate_rate == sample_rate)
            return candidate_rate;

        int delta = abs_diff_int(candidate_rate, sample_rate);
        bool best_rate_too_small = best_rate < sample_rate;
        bool candidate_rate_too_small = candidate_rate < sample_rate;
        if (best_rate == -1 ||
            (best_rate_too_small && !candidate_rate_too_small) ||
            ((best_rate_too_small || !candidate_rate_too_small) && delta < best_delta))
        {
            best_rate = candidate_rate;
            best_delta = delta;
        }
    }
    return best_rate;
}

bool soundio_device_equal(
        const struct SoundIoDevice *a,
        const struct SoundIoDevice *b)
{
    return a->is_raw == b->is_raw && a->aim == b->aim && strcmp(a->id, b->id) == 0;
}

const char *soundio_version_string(void) {
    return SOUNDIO_VERSION_STRING;
}

int soundio_version_major(void) {
    return SOUNDIO_VERSION_MAJOR;
}

int soundio_version_minor(void) {
    return SOUNDIO_VERSION_MINOR;
}

int soundio_version_patch(void) {
    return SOUNDIO_VERSION_PATCH;
}
