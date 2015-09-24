/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio.hpp"
#include "util.hpp"
#include "os.h"
#include "config.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

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
#ifdef SOUNDIO_HAVE_COREAUDIO
    SoundIoBackendCoreAudio,
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    SoundIoBackendWasapi,
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
#ifdef SOUNDIO_HAVE_COREAUDIO
    [SoundIoBackendCoreAudio] = soundio_coreaudio_init,
#else
    [SoundIoBackendCoreAudio] = nullptr,
#endif
#ifdef SOUNDIO_HAVE_WASAPI
    [SoundIoBackendWasapi] = soundio_wasapi_init,
#else
    [SoundIoBackendWasapi] = nullptr,
#endif
    [SoundIoBackendDummy] = soundio_dummy_init,
};

static const SoundIoFormat prioritized_formats[] = {
    SoundIoFormatFloat32NE,
    SoundIoFormatFloat32FE,
    SoundIoFormatS32NE,
    SoundIoFormatS32FE,
    SoundIoFormatU32NE,
    SoundIoFormatU32FE,
    SoundIoFormatFloat64NE,
    SoundIoFormatFloat64FE,
    SoundIoFormatS24NE,
    SoundIoFormatS24FE,
    SoundIoFormatU24NE,
    SoundIoFormatU24FE,
    SoundIoFormatS16NE,
    SoundIoFormatS16FE,
    SoundIoFormatU16NE,
    SoundIoFormatU16FE,
    SoundIoFormatS8,
    SoundIoFormatU8,
};

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
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    soundio_disconnect(soundio);

    free(si);
}

static void do_nothing_cb(struct SoundIo *) { }
static void default_msg_callback(const char *msg) { }

static void default_backend_disconnect_cb(struct SoundIo *, int err) {
    soundio_panic("libsoundio: backend disconnected: %s", soundio_strerror(err));
}

static atomic_flag rtprio_seen = ATOMIC_FLAG_INIT;
static void default_emit_rtprio_warning(void) {
    if (!rtprio_seen.test_and_set()) {
        fprintf(stderr, "warning: unable to set high priority thread: Operation not permitted\n");
        fprintf(stderr, "See "
            "https://github.com/andrewrk/genesis/wiki/warning:-unable-to-set-high-priority-thread:-Operation-not-permitted\n");
    }
}

struct SoundIo *soundio_create(void) {
    int err;
    if ((err = soundio_os_init()))
        return nullptr;
    struct SoundIoPrivate *si = allocate<SoundIoPrivate>(1);
    if (!si)
        return nullptr;
    SoundIo *soundio = &si->pub;
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
    soundio->current_backend = backend;

    return 0;
}

void soundio_disconnect(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    if (!si)
        return;

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
    si->force_device_scan = nullptr;

    si->stream_open = nullptr;
    si->stream_destroy = nullptr;
    si->stream_start = nullptr;
    si->stream_begin_write = nullptr;
    si->stream_end_write = nullptr;
    si->stream_begin_read = nullptr;
    si->stream_end_read = nullptr;
    si->stream_clear_buffer = nullptr;
    si->stream_pause = nullptr;
    si->stream_get_latency = nullptr;
}

void soundio_flush_events(struct SoundIo *soundio) {
    assert(soundio->current_backend != SoundIoBackendNone);
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    si->flush_events(si);
}

int soundio_input_device_count(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return -1;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return -1;

    return si->safe_devices_info->input_devices.length;
}

int soundio_output_device_count(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return -1;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return -1;

    return si->safe_devices_info->output_devices.length;
}

int soundio_default_input_device_index(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return -1;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return -1;

    return si->safe_devices_info->default_input_index;
}

int soundio_default_output_device_index(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return -1;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return -1;

    return si->safe_devices_info->default_output_index;
}

struct SoundIoDevice *soundio_get_input_device(struct SoundIo *soundio, int index) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return nullptr;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return nullptr;

    assert(index >= 0);
    assert(index < si->safe_devices_info->input_devices.length);
    if (index < 0 || index >= si->safe_devices_info->input_devices.length)
        return nullptr;

    SoundIoDevice *device = si->safe_devices_info->input_devices.at(index);
    soundio_device_ref(device);
    return device;
}

struct SoundIoDevice *soundio_get_output_device(struct SoundIo *soundio, int index) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    assert(soundio->current_backend != SoundIoBackendNone);
    if (soundio->current_backend == SoundIoBackendNone)
        return nullptr;

    assert(si->safe_devices_info);
    if (!si->safe_devices_info)
        return nullptr;

    assert(index >= 0);
    assert(index < si->safe_devices_info->output_devices.length);
    if (index < 0 || index >= si->safe_devices_info->output_devices.length)
        return nullptr;

    SoundIoDevice *device = si->safe_devices_info->output_devices.at(index);
    soundio_device_ref(device);
    return device;
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

        free(device->id);
        free(device->name);

        if (device->sample_rates != &dev->prealloc_sample_rate_range &&
            device->sample_rates != dev->sample_rates.items)
        {
            free(device->sample_rates);
        }
        dev->sample_rates.deinit();

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
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    si->wait_events(si);
}

void soundio_wakeup(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    si->wakeup(si);
}

void soundio_force_device_scan(struct SoundIo *soundio) {
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    si->force_device_scan(si);
}

int soundio_stream_begin_write(struct SoundIoStream *stream,
        SoundIoChannelArea **areas, int *frame_count)
{
    SoundIo *soundio = stream->output_device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoStreamPrivate *st = (SoundIoStreamPrivate *)stream;
    if (*frame_count <= 0)
        return SoundIoErrorInvalid;
    return si->stream_begin_write(si, st, areas, frame_count);
}

int soundio_stream_end_write(struct SoundIoStream *stream) {
    SoundIo *soundio = stream->output_device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoStreamPrivate *st = (SoundIoStreamPrivate *)stream;
    return si->stream_end_write(si, st);
}

static void default_stream_error_callback(struct SoundIoStream *s, int err) {
    soundio_panic("libsoundio: %s", soundio_strerror(err));
}

static void default_underflow_callback(struct SoundIoStream *stream) { }
static void default_overflow_callback(struct SoundIoStream *stream) { }


struct SoundIoStream *soundio_stream_create(
        struct SoundIoDevice *input_device, struct SoundIoDevice *output_device)
{
    SoundIoStreamPrivate *st = allocate<SoundIoStreamPrivate>(1);
    SoundIoStream *stream = &st->pub;

    if (!st)
        return nullptr;

    if (!input_device && !output_device)
        return nullptr;

    stream->input_device = input_device;
    stream->output_device = output_device;

    soundio_device_ref(input_device);
    soundio_device_ref(output_device);

    stream->error_callback = default_stream_error_callback;
    stream->underflow_callback = default_underflow_callback;
    stream->overflow_callback = default_overflow_callback;

    return stream;
}

static enum SoundIoFormat get_default_stream_format(struct SoundIoDevice *device) {
    for (int i = 0; i < array_length(prioritized_formats); i += 1) {
        SoundIoFormat format = prioritized_formats[i];
        if (soundio_device_supports_format(device, format))
            return format;
    }
    return SoundIoFormatInvalid;
}

static int choose_default_stream_sample_rate(struct SoundIoStream *stream) {
    static const int target_sample_rate = 44100;
    if (stream->input_device && stream->output_device) {
        int nearest_input = soundio_device_nearest_sample_rate(stream->input_device, target_sample_rate);
        int nearest_output = soundio_device_nearest_sample_rate(stream->output_device, nearest_input);

        if (nearest_input == nearest_output) {
            stream->sample_rate = nearest_input;
            return 0;
        }

        return SoundIoErrorIncompatibleDevice;
    } else if (stream->input_device) {
        return soundio_device_nearest_sample_rate(stream->input_device, target_sample_rate);
    } else if (stream->output_device) {
        return soundio_device_nearest_sample_rate(stream->output_device, target_sample_rate);
    }

    return SoundIoErrorInvalid;
}

static SoundIo *stream_soundio(SoundIoStream *stream) {
    return stream->input_device ? stream->input_device->soundio : stream->output_device->soundio;
}

int soundio_stream_open(struct SoundIoStream *stream) {
    SoundIoDevice *input_device = stream->input_device;
    SoundIoDevice *output_device = stream->output_device;
    int err;

    if (input_device && output_device && strcmp(input_device->id, output_device->id) != 0)
        return SoundIoErrorInvalid;

    if (input_device) {
        if (input_device->aim != SoundIoDeviceAimInput)
            return SoundIoErrorInvalid;
        if (stream->input_layout.channel_count > SOUNDIO_MAX_CHANNELS)
            return SoundIoErrorInvalid;
        if (stream->input_format < SoundIoFormatInvalid)
            return SoundIoErrorInvalid;
        if (input_device->probe_error)
            return input_device->probe_error;
        if (stream->input_format == SoundIoFormatInvalid) {
            stream->input_format = get_default_stream_format(input_device);
            if (stream->input_format == SoundIoFormatInvalid)
                return SoundIoErrorIncompatibleDevice;
        }
        if (!stream->input_layout.channel_count) {
            soundio_sort_channel_layouts(stream->input_device->layouts, stream->input_device->layout_count);
            stream->input_layout = stream->input_device->layouts[0];
        }
    }

    if (output_device) {
        if (output_device->aim != SoundIoDeviceAimOutput)
            return SoundIoErrorInvalid;
        if (stream->output_layout.channel_count > SOUNDIO_MAX_CHANNELS)
            return SoundIoErrorInvalid;
        if (stream->output_format < SoundIoFormatInvalid)
            return SoundIoErrorInvalid;
        if (output_device->probe_error)
            return output_device->probe_error;
        if (stream->output_format == SoundIoFormatInvalid) {
            stream->output_format = get_default_stream_format(output_device);
            if (stream->output_format == SoundIoFormatInvalid)
                return SoundIoErrorIncompatibleDevice;
        }
        if (!stream->output_layout.channel_count) {
            soundio_sort_channel_layouts(stream->output_device->layouts, stream->output_device->layout_count);
            stream->output_layout = stream->output_device->layouts[0];
        }
    }

    if (!stream->sample_rate) {
        if ((err = choose_default_stream_sample_rate(stream))) {
            return err;
        }
    }

    SoundIoStreamPrivate *st = (SoundIoStreamPrivate *)stream;
    stream->input_bytes_per_frame = soundio_get_bytes_per_frame(stream->input_format,
            stream->input_layout.channel_count);
    stream->input_bytes_per_sample = soundio_get_bytes_per_sample(stream->input_format);
    stream->output_bytes_per_frame = soundio_get_bytes_per_frame(stream->output_format,
            stream->output_layout.channel_count);
    stream->output_bytes_per_sample = soundio_get_bytes_per_sample(stream->output_format);

    SoundIo *soundio = stream_soundio(stream);
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    return si->stream_open(si, st);
}

void soundio_stream_destroy(SoundIoStream *stream) {
    if (!stream)
        return;

    SoundIoStreamPrivate *st = (SoundIoStreamPrivate *)stream;
    SoundIo *soundio = stream->input_device ? stream->input_device->soundio : stream->output_device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;

    if (si->stream_destroy)
        si->stream_destroy(si, st);

    soundio_device_unref(stream->input_device);
    soundio_device_unref(stream->output_device);
    free(st);
}

int soundio_stream_start(struct SoundIoStream *stream) {
    SoundIo *soundio = stream_soundio(stream);
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoStreamPrivate *st = (SoundIoStreamPrivate *)stream;
    return si->stream_start(si, st);
}

int soundio_stream_pause(struct SoundIoStream *stream, bool pause) {
    SoundIo *soundio = stream_soundio(stream);
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoStreamPrivate *st = (SoundIoStreamPrivate *)stream;
    return si->stream_pause(si, st, pause);
}

int soundio_stream_clear_buffer(struct SoundIoStream *stream) {
    SoundIo *soundio = stream_soundio(stream);
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoStreamPrivate *st = (SoundIoStreamPrivate *)stream;
    return si->stream_clear_buffer(si, st);
}

int soundio_stream_get_latency(struct SoundIoStream *stream, double *out_latency) {
    SoundIo *soundio = stream_soundio(stream);
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoStreamPrivate *st = (SoundIoStreamPrivate *)stream;
    return si->stream_get_latency(si, st, out_latency);
}

int soundio_stream_begin_read(struct SoundIoStream *stream,
        struct SoundIoChannelArea **areas, int *frame_count)
{
    SoundIo *soundio = stream->input_device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoStreamPrivate *st = (SoundIoStreamPrivate *)stream;
    return si->stream_begin_read(si, st, areas, frame_count);
}

int soundio_stream_end_read(struct SoundIoStream *stream) {
    SoundIo *soundio = stream->input_device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoStreamPrivate *st = (SoundIoStreamPrivate *)stream;
    return si->stream_end_read(si, st);
}

void soundio_destroy_devices_info(SoundIoDevicesInfo *devices_info) {
    if (!devices_info)
        return;

    for (int i = 0; i < devices_info->input_devices.length; i += 1)
        soundio_device_unref(devices_info->input_devices.at(i));
    for (int i = 0; i < devices_info->output_devices.length; i += 1)
        soundio_device_unref(devices_info->output_devices.at(i));

    devices_info->input_devices.deinit();
    devices_info->output_devices.deinit();

    free(devices_info);
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

static bool layout_contains(const SoundIoChannelLayout *available_layouts, int available_layout_count,
        const SoundIoChannelLayout *target_layout)
{
    for (int i = 0; i < available_layout_count; i += 1) {
        const SoundIoChannelLayout *available_layout = &available_layouts[i];
        if (soundio_channel_layout_equal(target_layout, available_layout))
            return true;
    }
    return false;
}

const struct SoundIoChannelLayout *soundio_best_matching_channel_layout(
        const struct SoundIoChannelLayout *preferred_layouts, int preferred_layout_count,
        const struct SoundIoChannelLayout *available_layouts, int available_layout_count)
{
    for (int i = 0; i < preferred_layout_count; i += 1) {
        const SoundIoChannelLayout *preferred_layout = &preferred_layouts[i];
        if (layout_contains(available_layouts, available_layout_count, preferred_layout))
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

void soundio_sort_channel_layouts(struct SoundIoChannelLayout *layouts, int layout_count) {
    if (!layouts)
        return;

    qsort(layouts, layout_count, sizeof(SoundIoChannelLayout), compare_layouts);
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
        SoundIoSampleRateRange *range = &device->sample_rates[i];
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
        SoundIoSampleRateRange *range = &device->sample_rates[i];
        int candidate_rate = clamp(range->min, sample_rate, range->max);
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
