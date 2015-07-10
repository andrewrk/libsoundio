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

    soundio->output_device_init = nullptr;
    soundio->output_device_destroy = nullptr;
    soundio->output_device_start = nullptr;
    soundio->output_device_free_count = nullptr;
    soundio->output_device_begin_write = nullptr;
    soundio->output_device_write = nullptr;
    soundio->output_device_clear_buffer = nullptr;

    soundio->input_device_init = nullptr;
    soundio->input_device_destroy = nullptr;
    soundio->input_device_start = nullptr;
    soundio->input_device_peek = nullptr;
    soundio->input_device_drop = nullptr;
    soundio->input_device_clear_buffer = nullptr;
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

void soundio_output_device_fill_with_silence(struct SoundIoOutputDevice *output_device) {
    char *buffer;
    int requested_frame_count = soundio_output_device_free_count(output_device);
    while (requested_frame_count > 0) {
        int frame_count = requested_frame_count;
        soundio_output_device_begin_write(output_device, &buffer, &frame_count);
        memset(buffer, 0, frame_count * output_device->bytes_per_frame);
        soundio_output_device_write(output_device, buffer, frame_count);
        requested_frame_count -= frame_count;
    }
}

int soundio_output_device_free_count(struct SoundIoOutputDevice *output_device) {
    SoundIo *soundio = output_device->device->soundio;
    return soundio->output_device_free_count(soundio, output_device);
}

void soundio_output_device_begin_write(struct SoundIoOutputDevice *output_device,
        char **data, int *frame_count)
{
    SoundIo *soundio = output_device->device->soundio;
    soundio->output_device_begin_write(soundio, output_device, data, frame_count);
}

void soundio_output_device_write(struct SoundIoOutputDevice *output_device,
        char *data, int frame_count)
{
    SoundIo *soundio = output_device->device->soundio;
    soundio->output_device_write(soundio, output_device, data, frame_count);
}


int soundio_output_device_create(struct SoundIoDevice *device,
        enum SoundIoFormat format, int sample_rate,
        double latency, void *userdata,
        void (*write_callback)(struct SoundIoOutputDevice *, int frame_count),
        void (*underrun_callback)(struct SoundIoOutputDevice *),
        struct SoundIoOutputDevice **out_output_device)
{
    *out_output_device = nullptr;

    SoundIoOutputDevice *output_device = create<SoundIoOutputDevice>();
    if (!output_device) {
        soundio_output_device_destroy(output_device);
        return SoundIoErrorNoMem;
    }

    soundio_device_ref(device);
    output_device->device = device;
    output_device->userdata = userdata;
    output_device->write_callback = write_callback;
    output_device->underrun_callback = underrun_callback;
    output_device->format = format;
    output_device->sample_rate = sample_rate;
    output_device->latency = latency;
    output_device->bytes_per_frame = soundio_get_bytes_per_frame(format,
            device->channel_layout.channel_count);

    SoundIo *soundio = device->soundio;
    int err = soundio->output_device_init(soundio, output_device);
    if (err) {
        soundio_output_device_destroy(output_device);
        return err;
    }

    *out_output_device = output_device;
    return 0;
}

void soundio_output_device_destroy(SoundIoOutputDevice *output_device) {
    if (!output_device)
        return;

    SoundIo *soundio = output_device->device->soundio;

    if (soundio->output_device_destroy)
        soundio->output_device_destroy(soundio, output_device);

    soundio_device_unref(output_device->device);
    destroy(output_device);
}

int soundio_output_device_start(struct SoundIoOutputDevice *output_device) {
    SoundIo *soundio = output_device->device->soundio;
    return soundio->output_device_start(soundio, output_device);
}

int soundio_input_device_create(struct SoundIoDevice *device,
        enum SoundIoFormat format, int sample_rate,
        double latency, void *userdata,
        void (*read_callback)(struct SoundIoInputDevice *),
        struct SoundIoInputDevice **out_input_device)
{
    *out_input_device = nullptr;

    SoundIoInputDevice *sid = create<SoundIoInputDevice>();
    if (!sid) {
        soundio_input_device_destroy(sid);
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
    int err = soundio->input_device_init(soundio, sid);
    if (err) {
        soundio_input_device_destroy(sid);
        return err;
    }

    *out_input_device = sid;
    return 0;
}

int soundio_input_device_start(struct SoundIoInputDevice *input_device) {
    SoundIo *soundio = input_device->device->soundio;
    return soundio->input_device_start(soundio, input_device);
}

void soundio_input_device_destroy(struct SoundIoInputDevice *input_device) {
    if (!input_device)
        return;

    SoundIo *soundio = input_device->device->soundio;

    if (soundio->input_device_destroy)
        soundio->input_device_destroy(soundio, input_device);

    soundio_device_unref(input_device->device);
    destroy(input_device);
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
