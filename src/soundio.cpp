/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio.hpp"
#include "util.hpp"
#include "dummy.hpp"
#include "os.hpp"
#include "config.h"

#ifdef SOUNDIO_HAVE_PULSEAUDIO
#include "pulseaudio.hpp"
#endif

#ifdef SOUNDIO_HAVE_ALSA
#include "alsa.hpp"
#endif

#include <string.h>
#include <assert.h>

const char *soundio_strerror(int error) {
    switch ((enum SoundIoError)error) {
        case SoundIoErrorNone: return "(no error)";
        case SoundIoErrorNoMem: return "out of memory";
        case SoundIoErrorInitAudioBackend: return "unable to initialize audio backend";
        case SoundIoErrorSystemResources: return "system resource not available";
        case SoundIoErrorOpeningDevice: return "unable to open device";
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
        case SoundIoBackendPulseAudio: return "PulseAudio";
        case SoundIoBackendAlsa: return "ALSA";
        case SoundIoBackendDummy: return "Dummy";
    }
    soundio_panic("invalid backend enum value: %d", (int)backend);
}

void soundio_destroy(struct SoundIo *soundio) {
    if (!soundio)
        return;

    soundio_disconnect(soundio);

    destroy(soundio);
}

static void default_on_devices_change(struct SoundIo *) { }
static void default_on_events_signal(struct SoundIo *) { }

struct SoundIo * soundio_create(void) {
    soundio_os_init();
    struct SoundIo *soundio = create<SoundIo>();
    if (!soundio) {
        soundio_destroy(soundio);
        return NULL;
    }
    soundio->on_devices_change = default_on_devices_change;
    soundio->on_events_signal = default_on_events_signal;
    return soundio;
}

int soundio_connect(struct SoundIo *soundio) {
    int err;

#ifdef SOUNDIO_HAVE_PULSEAUDIO
    soundio->current_backend = SoundIoBackendPulseAudio;
    err = soundio_pulseaudio_init(soundio);
    if (!err)
        return 0;
    if (err != SoundIoErrorInitAudioBackend) {
        soundio_disconnect(soundio);
        return err;
    }
#endif

#ifdef SOUNDIO_HAVE_ALSA
    soundio->current_backend = SoundIoBackendAlsa;
    err = soundio_alsa_init(soundio);
    if (!err)
        return 0;
    if (err != SoundIoErrorInitAudioBackend) {
        soundio_disconnect(soundio);
        return err;
    }
#endif

    soundio->current_backend = SoundIoBackendDummy;
    err = soundio_dummy_init(soundio);
    if (err) {
        soundio_disconnect(soundio);
        return err;
    }

    return 0;
}

void soundio_disconnect(struct SoundIo *soundio) {
    if (soundio->destroy)
        soundio->destroy(soundio);
    assert(!soundio->backend_data);

    soundio->current_backend = SoundIoBackendNone;

    soundio_destroy_devices_info(soundio->safe_devices_info);
    soundio->safe_devices_info = nullptr;

    soundio->destroy = nullptr;
    soundio->flush_events = nullptr;
    soundio->wait_events = nullptr;
    soundio->wakeup = nullptr;

    soundio->out_stream_init = nullptr;
    soundio->out_stream_destroy = nullptr;
    soundio->out_stream_start = nullptr;
    soundio->out_stream_free_count = nullptr;
    soundio->out_stream_begin_write = nullptr;
    soundio->out_stream_write = nullptr;
    soundio->out_stream_clear_buffer = nullptr;

    soundio->in_stream_init = nullptr;
    soundio->in_stream_destroy = nullptr;
    soundio->in_stream_start = nullptr;
    soundio->in_stream_peek = nullptr;
    soundio->in_stream_drop = nullptr;
    soundio->in_stream_clear_buffer = nullptr;
}

void soundio_flush_events(struct SoundIo *soundio) {
    assert(soundio->flush_events);
    if (soundio->flush_events)
        soundio->flush_events(soundio);
}

int soundio_get_input_device_count(struct SoundIo *soundio) {
    soundio_flush_events(soundio);
    assert(soundio->safe_devices_info);
    return soundio->safe_devices_info->input_devices.length;
}

int soundio_get_output_device_count(struct SoundIo *soundio) {
    soundio_flush_events(soundio);
    assert(soundio->safe_devices_info);
    return soundio->safe_devices_info->output_devices.length;
}

int soundio_get_default_input_device_index(struct SoundIo *soundio) {
    soundio_flush_events(soundio);
    assert(soundio->safe_devices_info);
    return soundio->safe_devices_info->default_input_index;
}

int soundio_get_default_output_device_index(struct SoundIo *soundio) {
    soundio_flush_events(soundio);
    assert(soundio->safe_devices_info);
    return soundio->safe_devices_info->default_output_index;
}

struct SoundIoDevice *soundio_get_input_device(struct SoundIo *soundio, int index) {
    assert(soundio->safe_devices_info);
    assert(index >= 0);
    assert(index < soundio->safe_devices_info->input_devices.length);
    SoundIoDevice *device = soundio->safe_devices_info->input_devices.at(index);
    soundio_device_ref(device);
    return device;
}

struct SoundIoDevice *soundio_get_output_device(struct SoundIo *soundio, int index) {
    assert(soundio->safe_devices_info);
    assert(index >= 0);
    assert(index < soundio->safe_devices_info->output_devices.length);
    SoundIoDevice *device = soundio->safe_devices_info->output_devices.at(index);
    soundio_device_ref(device);
    return device;
}

// the name is the identifier for the device. UTF-8 encoded
const char *soundio_device_name(const struct SoundIoDevice *device) {
    return device->name;
}

// UTF-8 encoded
const char *soundio_device_description(const struct SoundIoDevice *device) {
    return device->description;
}

enum SoundIoDevicePurpose soundio_device_purpose(const struct SoundIoDevice *device) {
    return device->purpose;
}

const struct SoundIoChannelLayout *soundio_device_channel_layout(const struct SoundIoDevice *device) {
    return &device->channel_layout;
}

void soundio_device_unref(struct SoundIoDevice *device) {
    if (!device)
        return;

    device->ref_count -= 1;
    assert(device->ref_count >= 0);

    if (device->ref_count == 0) {
        free(device->name);
        free(device->description);
        deallocate(device->formats, device->format_count);
        destroy(device);
    }
}

void soundio_device_ref(struct SoundIoDevice *device) {
    device->ref_count += 1;
}

void soundio_wait_events(struct SoundIo *soundio) {
    soundio->wait_events(soundio);
}

void soundio_wakeup(struct SoundIo *soundio) {
    soundio->wakeup(soundio);
}

void soundio_out_stream_fill_with_silence(struct SoundIoOutStream *out_stream) {
    char *buffer;
    int requested_frame_count = soundio_out_stream_free_count(out_stream);
    while (requested_frame_count > 0) {
        int frame_count = requested_frame_count;
        soundio_out_stream_begin_write(out_stream, &buffer, &frame_count);
        memset(buffer, 0, frame_count * out_stream->bytes_per_frame);
        soundio_out_stream_write(out_stream, buffer, frame_count);
        requested_frame_count -= frame_count;
    }
}

int soundio_out_stream_free_count(struct SoundIoOutStream *out_stream) {
    SoundIo *soundio = out_stream->device->soundio;
    return soundio->out_stream_free_count(soundio, out_stream);
}

void soundio_out_stream_begin_write(struct SoundIoOutStream *out_stream,
        char **data, int *frame_count)
{
    SoundIo *soundio = out_stream->device->soundio;
    soundio->out_stream_begin_write(soundio, out_stream, data, frame_count);
}

void soundio_out_stream_write(struct SoundIoOutStream *out_stream,
        char *data, int frame_count)
{
    SoundIo *soundio = out_stream->device->soundio;
    soundio->out_stream_write(soundio, out_stream, data, frame_count);
}


int soundio_out_stream_create(struct SoundIoDevice *device,
        enum SoundIoFormat format, int sample_rate,
        double latency, void *userdata,
        void (*write_callback)(struct SoundIoOutStream *, int frame_count),
        void (*underrun_callback)(struct SoundIoOutStream *),
        struct SoundIoOutStream **out_out_stream)
{
    *out_out_stream = nullptr;

    SoundIoOutStream *out_stream = create<SoundIoOutStream>();
    if (!out_stream) {
        soundio_out_stream_destroy(out_stream);
        return SoundIoErrorNoMem;
    }

    soundio_device_ref(device);
    out_stream->device = device;
    out_stream->userdata = userdata;
    out_stream->write_callback = write_callback;
    out_stream->underrun_callback = underrun_callback;
    out_stream->format = format;
    out_stream->sample_rate = sample_rate;
    out_stream->latency = latency;
    out_stream->bytes_per_frame = soundio_get_bytes_per_frame(format,
            device->channel_layout.channel_count);

    SoundIo *soundio = device->soundio;
    int err = soundio->out_stream_init(soundio, out_stream);
    if (err) {
        soundio_out_stream_destroy(out_stream);
        return err;
    }

    *out_out_stream = out_stream;
    return 0;
}

void soundio_out_stream_destroy(SoundIoOutStream *out_stream) {
    if (!out_stream)
        return;

    SoundIo *soundio = out_stream->device->soundio;

    if (soundio->out_stream_destroy)
        soundio->out_stream_destroy(soundio, out_stream);

    soundio_device_unref(out_stream->device);
    destroy(out_stream);
}

int soundio_out_stream_start(struct SoundIoOutStream *out_stream) {
    SoundIo *soundio = out_stream->device->soundio;
    return soundio->out_stream_start(soundio, out_stream);
}

int soundio_in_stream_create(struct SoundIoDevice *device,
        enum SoundIoFormat format, int sample_rate,
        double latency, void *userdata,
        void (*read_callback)(struct SoundIoInStream *),
        struct SoundIoInStream **out_in_stream)
{
    *out_in_stream = nullptr;

    SoundIoInStream *sid = create<SoundIoInStream>();
    if (!sid) {
        soundio_in_stream_destroy(sid);
        return SoundIoErrorNoMem;
    }

    soundio_device_ref(device);
    sid->device = device;
    sid->userdata = userdata;
    sid->read_callback = read_callback;
    sid->format = format;
    sid->latency = latency;
    sid->sample_rate = sample_rate;
    sid->bytes_per_frame = soundio_get_bytes_per_frame(format,
            device->channel_layout.channel_count);

    SoundIo *soundio = device->soundio;
    int err = soundio->in_stream_init(soundio, sid);
    if (err) {
        soundio_in_stream_destroy(sid);
        return err;
    }

    *out_in_stream = sid;
    return 0;
}

int soundio_in_stream_start(struct SoundIoInStream *in_stream) {
    SoundIo *soundio = in_stream->device->soundio;
    return soundio->in_stream_start(soundio, in_stream);
}

void soundio_in_stream_destroy(struct SoundIoInStream *in_stream) {
    if (!in_stream)
        return;

    SoundIo *soundio = in_stream->device->soundio;

    if (soundio->in_stream_destroy)
        soundio->in_stream_destroy(soundio, in_stream);

    soundio_device_unref(in_stream->device);
    destroy(in_stream);
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
