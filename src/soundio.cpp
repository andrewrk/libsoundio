/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio.hpp"
#include "util.hpp"
#include "os.hpp"
#include "config.h"

#include <string.h>
#include <assert.h>

static const SoundIoBackend available_backends[] = {
#ifdef SOUNDIO_HAVE_JACK
    SoundIoBackendJack,
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    SoundIoBackendPulseAudio,
#endif
#ifdef SOUNDIO_HAVE_ALSA
    SoundIoBackendAlsa,
#endif
    SoundIoBackendDummy,
};

static int (*backend_init_fns[])(SoundIoPrivate *) = {
    [SoundIoBackendNone] = nullptr,
#ifdef SOUNDIO_HAVE_JACK
    [SoundIoBackendJack] = soundio_jack_init,
#else
    [SoundIoBackendJack] = nullptr,
#endif
#ifdef SOUNDIO_HAVE_PULSEAUDIO
    [SoundIoBackendPulseAudio] = soundio_pulseaudio_init,
#else
    [SoundIoBackendPulseAudio] = nullptr,
#endif
#ifdef SOUNDIO_HAVE_ALSA
    [SoundIoBackendAlsa] = soundio_alsa_init,
#else
    [SoundIoBackendAlsa] = nullptr,
#endif
    [SoundIoBackendDummy] = soundio_dummy_init,
};

const char *soundio_strerror(int error) {
    switch ((enum SoundIoError)error) {
        case SoundIoErrorNone: return "(no error)";
        case SoundIoErrorNoMem: return "out of memory";
        case SoundIoErrorInitAudioBackend: return "unable to initialize audio backend";
        case SoundIoErrorSystemResources: return "system resource not available";
        case SoundIoErrorOpeningDevice: return "unable to open device";
        case SoundIoErrorInvalid: return "invalid value";
        case SoundIoErrorBackendUnavailable: return "backend unavailable";
        case SoundIoErrorStreaming: return "unrecoverable streaming failure";
        case SoundIoErrorIncompatibleDevice: return "incompatible device";
        case SoundIoErrorNoSuchClient: return "no such client";
        case SoundIoErrorIncompatibleBackend: return "incompatible backend";
        case SoundIoErrorBackendDisconnected: return "backend disconnected";
        case SoundIoErrorInterrupted: return "interrupted; try again";
    }
    soundio_panic("invalid error enum value: %d", error);
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

    case SoundIoFormatInvalid:
        soundio_panic("invalid sample format");
    }
    soundio_panic("invalid sample format");
}

const char * soundio_format_string(enum SoundIoFormat format) {
    switch (format) {
    case SoundIoFormatU8:         return "signed 8-bit";
    case SoundIoFormatS8:         return "unsigned 8-bit";
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
        case SoundIoBackendDummy: return "Dummy";
    }
    soundio_panic("invalid backend enum value: %d", (int)backend);
}

void soundio_destroy(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    if (!si)
        return;

    soundio_disconnect(soundio);

    destroy(si);
}

static void do_nothing_cb(struct SoundIo *) { }
static void default_msg_callback(const char *msg) { }

struct SoundIo * soundio_create(void) {
    soundio_os_init();
    struct SoundIoPrivate *si = create<SoundIoPrivate>();
    if (!si)
        return NULL;
    SoundIo *soundio = &si->pub;
    soundio->on_devices_change = do_nothing_cb;
    soundio->on_backend_disconnect = do_nothing_cb;
    soundio->on_events_signal = do_nothing_cb;
    soundio->app_name = "SoundIo";
    soundio->jack_info_callback = default_msg_callback;
    soundio->jack_error_callback = default_msg_callback;
    return soundio;
}

int soundio_connect(struct SoundIo *soundio) {
    int err = 0;

    for (int i = 0; i < array_length(available_backends); i += 1) {
        SoundIoBackend backend = available_backends[i];
        err = soundio_connect_backend(soundio, backend);
        if (!err)
            return 0;
        if (err != SoundIoErrorInitAudioBackend)
            return err;
    }

    return err;
}

int soundio_connect_backend(SoundIo *soundio, SoundIoBackend backend) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    if (soundio->current_backend)
        return SoundIoErrorInvalid;

    if (backend <= 0 || backend > SoundIoBackendDummy)
        return SoundIoErrorInvalid;

    int (*fn)(SoundIoPrivate *) = backend_init_fns[backend];

    if (!fn)
        return SoundIoErrorBackendUnavailable;

    int err;
    if ((err = backend_init_fns[backend](si))) {
        soundio_disconnect(soundio);
        return err;
    }
    return 0;
}

void soundio_disconnect(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    if (si->destroy)
        si->destroy(si);
    memset(&si->backend_data, 0, sizeof(SoundIoBackendData));

    soundio->current_backend = SoundIoBackendNone;

    soundio_destroy_devices_info(si->safe_devices_info);

    si->safe_devices_info = nullptr;

    si->destroy = nullptr;
    si->flush_events = nullptr;
    si->wait_events = nullptr;
    si->wakeup = nullptr;

    si->outstream_open = nullptr;
    si->outstream_destroy = nullptr;
    si->outstream_start = nullptr;
    si->outstream_begin_write = nullptr;
    si->outstream_end_write = nullptr;
    si->outstream_clear_buffer = nullptr;

    si->instream_open = nullptr;
    si->instream_destroy = nullptr;
    si->instream_start = nullptr;
    si->instream_begin_read = nullptr;
    si->instream_end_read = nullptr;
}

void soundio_flush_events(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    si->flush_events(si);
}

int soundio_input_device_count(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    if (!si->safe_devices_info)
        soundio_flush_events(soundio);
    assert(si->safe_devices_info);
    return si->safe_devices_info->input_devices.length;
}

int soundio_output_device_count(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    if (!si->safe_devices_info)
        soundio_flush_events(soundio);
    assert(si->safe_devices_info);
    return si->safe_devices_info->output_devices.length;
}

int soundio_default_input_device_index(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    if (!si->safe_devices_info)
        soundio_flush_events(soundio);
    assert(si->safe_devices_info);
    return si->safe_devices_info->default_input_index;
}

int soundio_default_output_device_index(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    if (!si->safe_devices_info)
        soundio_flush_events(soundio);
    assert(si->safe_devices_info);
    return si->safe_devices_info->default_output_index;
}

struct SoundIoDevice *soundio_get_input_device(struct SoundIo *soundio, int index) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    assert(si->safe_devices_info);
    assert(index >= 0);
    assert(index < si->safe_devices_info->input_devices.length);
    SoundIoDevice *device = si->safe_devices_info->input_devices.at(index);
    soundio_device_ref(device);
    return device;
}

struct SoundIoDevice *soundio_get_output_device(struct SoundIo *soundio, int index) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    assert(si->safe_devices_info);
    assert(index >= 0);
    assert(index < si->safe_devices_info->output_devices.length);
    SoundIoDevice *device = si->safe_devices_info->output_devices.at(index);
    soundio_device_ref(device);
    return device;
}

enum SoundIoDevicePurpose soundio_device_purpose(const struct SoundIoDevice *device) {
    return device->purpose;
}

void soundio_device_unref(struct SoundIoDevice *device) {
    if (!device)
        return;

    device->ref_count -= 1;
    assert(device->ref_count >= 0);

    if (device->ref_count == 0) {
        SoundIoDevicePrivate *dev = (SoundIoDevicePrivate *)device;
        if (dev->destruct)
            dev->destruct(dev);

        free(device->name);
        free(device->description);
        deallocate(device->formats, device->format_count);
        deallocate(device->layouts, device->layout_count);

        destroy(dev);
    }
}

void soundio_device_ref(struct SoundIoDevice *device) {
    device->ref_count += 1;
}

void soundio_wait_events(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    si->wait_events(si);
}

void soundio_wakeup(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    si->wakeup(si);
}

int soundio_outstream_begin_write(struct SoundIoOutStream *outstream,
        SoundIoChannelArea **areas, int *frame_count)
{
    SoundIo *soundio = outstream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate *)outstream;
    assert(*frame_count > 0);
    return si->outstream_begin_write(si, os, areas, frame_count);
}

int soundio_outstream_end_write(struct SoundIoOutStream *outstream, int frame_count) {
    SoundIo *soundio = outstream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate *)outstream;
    return si->outstream_end_write(si, os, frame_count);
}

static void default_outstream_error_callback(struct SoundIoOutStream *os, int err) {
    soundio_panic("libsoundio: %s", soundio_strerror(err));
}

static void default_underflow_callback(struct SoundIoOutStream *outstream) { }

struct SoundIoOutStream *soundio_outstream_create(struct SoundIoDevice *device) {
    SoundIoOutStreamPrivate *os = create<SoundIoOutStreamPrivate>();
    if (!os)
        return nullptr;
    SoundIoOutStream *outstream = &os->pub;

    outstream->device = device;
    soundio_device_ref(device);

    outstream->error_callback = default_outstream_error_callback;
    outstream->underflow_callback = default_underflow_callback;

    outstream->prebuf_duration = -1.0;

    return outstream;
}

int soundio_outstream_open(struct SoundIoOutStream *outstream) {
    if (outstream->format <= SoundIoFormatInvalid)
        return SoundIoErrorInvalid;

    SoundIoDevice *device = outstream->device;
    if (device->probe_error)
        return device->probe_error;

    if (outstream->format == SoundIoFormatInvalid) {
        outstream->format = soundio_device_supports_format(device, SoundIoFormatFloat32NE) ?
            SoundIoFormatFloat32NE : device->formats[0];
    }

    if (!outstream->layout.channel_count) {
        const SoundIoChannelLayout *stereo = soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
        outstream->layout = soundio_device_supports_layout(device, stereo) ? *stereo : device->layouts[0];
    }

    if (!outstream->sample_rate)
        outstream->sample_rate = clamp(device->sample_rate_min, 48000, device->sample_rate_max);

    if (!outstream->name)
        outstream->name = "SoundIoOutStream";

    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate *)outstream;
    outstream->bytes_per_frame = soundio_get_bytes_per_frame(outstream->format, outstream->layout.channel_count);
    outstream->bytes_per_sample = soundio_get_bytes_per_sample(outstream->format);

    SoundIo *soundio = device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    return si->outstream_open(si, os);
}

void soundio_outstream_destroy(SoundIoOutStream *outstream) {
    if (!outstream)
        return;

    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate *)outstream;
    SoundIo *soundio = outstream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    if (si->outstream_destroy)
        si->outstream_destroy(si, os);

    soundio_device_unref(outstream->device);
    destroy(os);
}

int soundio_outstream_start(struct SoundIoOutStream *outstream) {
    SoundIo *soundio = outstream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate *)outstream;
    return si->outstream_start(si, os);
}

int soundio_outstream_pause(struct SoundIoOutStream *outstream, bool pause) {
    SoundIo *soundio = outstream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate *)outstream;
    return si->outstream_pause(si, os, pause);
}

static void default_instream_error_callback(struct SoundIoInStream *is, int err) {
    soundio_panic("libsoundio: %s", soundio_strerror(err));
}

struct SoundIoInStream *soundio_instream_create(struct SoundIoDevice *device) {
    SoundIoInStreamPrivate *is = create<SoundIoInStreamPrivate>();
    if (!is)
        return nullptr;
    SoundIoInStream *instream = &is->pub;

    instream->device = device;
    soundio_device_ref(device);

    instream->error_callback = default_instream_error_callback;

    return instream;
}

int soundio_instream_open(struct SoundIoInStream *instream) {
    if (instream->format <= SoundIoFormatInvalid)
        return SoundIoErrorInvalid;

    SoundIoDevice *device = instream->device;

    if (device->probe_error)
        return device->probe_error;

    if (instream->format == SoundIoFormatInvalid) {
        instream->format = soundio_device_supports_format(device, SoundIoFormatFloat32NE) ?
            SoundIoFormatFloat32NE : device->formats[0];
    }

    if (!instream->layout.channel_count) {
        const SoundIoChannelLayout *stereo = soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
        instream->layout = soundio_device_supports_layout(device, stereo) ? *stereo : device->layouts[0];
    }

    if (!instream->sample_rate)
        instream->sample_rate = clamp(device->sample_rate_min, 48000, device->sample_rate_max);

    if (!instream->name)
        instream->name = "SoundIoInStream";


    instream->bytes_per_frame = soundio_get_bytes_per_frame(instream->format, instream->layout.channel_count);
    instream->bytes_per_sample = soundio_get_bytes_per_sample(instream->format);
    SoundIo *soundio = device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoInStreamPrivate *is = (SoundIoInStreamPrivate *)instream;
    return si->instream_open(si, is);
}

int soundio_instream_start(struct SoundIoInStream *instream) {
    SoundIo *soundio = instream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoInStreamPrivate *is = (SoundIoInStreamPrivate *)instream;
    return si->instream_start(si, is);
}

void soundio_instream_destroy(struct SoundIoInStream *instream) {
    if (!instream)
        return;

    SoundIoInStreamPrivate *is = (SoundIoInStreamPrivate *)instream;
    SoundIo *soundio = instream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    if (si->instream_destroy)
        si->instream_destroy(si, is);

    soundio_device_unref(instream->device);
    destroy(is);
}

int soundio_instream_pause(struct SoundIoInStream *instream, bool pause) {
    SoundIo *soundio = instream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoInStreamPrivate *is = (SoundIoInStreamPrivate *)instream;
    return si->instream_pause(si, is, pause);
}

int soundio_instream_begin_read(struct SoundIoInStream *instream,
        struct SoundIoChannelArea **areas, int *frame_count)
{
    SoundIo *soundio = instream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoInStreamPrivate *is = (SoundIoInStreamPrivate *)instream;
    return si->instream_begin_read(si, is, areas, frame_count);
}

int soundio_instream_end_read(struct SoundIoInStream *instream) {
    SoundIo *soundio = instream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoInStreamPrivate *is = (SoundIoInStreamPrivate *)instream;
    return si->instream_end_read(si, is);
}

void soundio_destroy_devices_info(SoundIoDevicesInfo *devices_info) {
    if (!devices_info)
        return;

    for (int i = 0; i < devices_info->input_devices.length; i += 1)
        soundio_device_unref(devices_info->input_devices.at(i));
    for (int i = 0; i < devices_info->output_devices.length; i += 1)
        soundio_device_unref(devices_info->output_devices.at(i));

    destroy(devices_info);
}

bool soundio_have_backend(SoundIoBackend backend) {
    assert(backend > 0);
    assert(backend <= SoundIoBackendDummy);
    return backend_init_fns[backend];
}

int soundio_backend_count(struct SoundIo *soundio) {
    return array_length(available_backends);
}

SoundIoBackend soundio_get_backend(struct SoundIo *soundio, int index) {
    return available_backends[index];
}

static bool layout_contains(const SoundIoChannelLayout *available_layouts, int available_layouts_count,
        const SoundIoChannelLayout *target_layout)
{
    for (int i = 0; i < available_layouts_count; i += 1) {
        const SoundIoChannelLayout *available_layout = &available_layouts[i];
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
        const SoundIoChannelLayout *preferred_layout = &preferred_layouts[i];
        if (layout_contains(available_layouts, available_layouts_count, preferred_layout))
            return preferred_layout;
    }
    return nullptr;
}

static int compare_layouts(const void *a, const void *b) {
    const SoundIoChannelLayout *layout_a = *((SoundIoChannelLayout **)a);
    const SoundIoChannelLayout *layout_b = *((SoundIoChannelLayout **)b);
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

    qsort(layouts, layouts_count, sizeof(SoundIoChannelLayout), compare_layouts);
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
