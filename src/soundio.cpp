/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio.hpp"
#include "util.hpp"
#include "dummy.hpp"
#include "pulseaudio.hpp"

#include <assert.h>

const char *soundio_error_string(int error) {
    switch ((enum SoundIoError)error) {
        case SoundIoErrorNone: return "(no error)";
        case SoundIoErrorNoMem: return "out of memory";
        case SoundIoErrorInitAudioBackend: return "unable to initialize audio backend";
        case SoundIoErrorSystemResources: return "system resource not available";
        case SoundIoErrorOpeningDevice: return "unable to open device";
    }
    panic("invalid error enum value: %d", error);
}

int soundio_get_bytes_per_sample(enum SoundIoSampleFormat sample_format) {
    switch (sample_format) {
    case SoundIoSampleFormatUInt8: return 1;
    case SoundIoSampleFormatInt16: return 2;
    case SoundIoSampleFormatInt24: return 3;
    case SoundIoSampleFormatInt32: return 4;
    case SoundIoSampleFormatFloat: return 4;
    case SoundIoSampleFormatDouble: return 8;
    case SoundIoSampleFormatInvalid: panic("invalid sample format");
    }
    panic("invalid sample format");
}

const char * soundio_sample_format_string(enum SoundIoSampleFormat sample_format) {
    switch (sample_format) {
    case SoundIoSampleFormatUInt8: return "unsigned 8-bit integer";
    case SoundIoSampleFormatInt16: return "signed 16-bit integer";
    case SoundIoSampleFormatInt24: return "signed 24-bit integer";
    case SoundIoSampleFormatInt32: return "signed 32-bit integer";
    case SoundIoSampleFormatFloat: return "32-bit float";
    case SoundIoSampleFormatDouble: return "64-bit float";
    case SoundIoSampleFormatInvalid: return "invalid sample format";
    }
    panic("invalid sample format");
}



const char *soundio_backend_name(enum SoundIoBackend backend) {
    switch (backend) {
        case SoundIoBackendPulseAudio: return "PulseAudio";
        case SoundIoBackendDummy: return "Dummy";
    }
    panic("invalid backend enum value: %d", (int)backend);
}

void soundio_destroy(struct SoundIo *soundio) {
    if (!soundio)
        return;

    if (soundio->destroy)
        soundio->destroy(soundio);

    destroy(soundio);
}

int soundio_create(struct SoundIo **out_soundio) {
    *out_soundio = NULL;

    struct SoundIo *soundio = create<SoundIo>();
    if (!soundio) {
        soundio_destroy(soundio);
        return SoundIoErrorNoMem;
    }

    int err;

    err = soundio_pulseaudio_init(soundio);
    if (err != SoundIoErrorInitAudioBackend) {
        soundio_destroy(soundio);
        return err;
    }

    err = soundio_dummy_init(soundio);
    if (err) {
        soundio_destroy(soundio);
        return err;
    }

    *out_soundio = soundio;
    return 0;
}

void soundio_flush_events(struct SoundIo *soundio) {
    if (soundio->flush_events)
        soundio->flush_events(soundio);
}

int soundio_get_input_device_count(struct SoundIo *soundio) {
    assert(soundio->safe_devices_info);
    return soundio->safe_devices_info->input_devices.length;
}

int soundio_get_output_device_count(struct SoundIo *soundio) {
    assert(soundio->safe_devices_info);
    return soundio->safe_devices_info->output_devices.length;
}

int soundio_get_default_input_device_index(struct SoundIo *soundio) {
    assert(soundio->safe_devices_info);
    return soundio->safe_devices_info->default_input_index;
}

int soundio_get_default_output_device_index(struct SoundIo *soundio) {
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
