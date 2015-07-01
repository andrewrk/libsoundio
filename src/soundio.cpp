/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio.h"
#include "util.hpp"
#include "dummy.hpp"
#include "pulseaudio.hpp"

#include <assert.h>
#include <stdio.h>

static struct SoundIoChannelLayout builtin_channel_layouts[] = {
    {
        "Mono",
        1,
        {
            SoundIoChannelIdFrontCenter,
        },
    },
    {
        "Stereo",
        2,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
        },
    },
    {
        "2.1",
        3,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdLowFrequency,
        },
    },
    {
        "3.0",
        3,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
        }
    },
    {
        "3.0 (back)",
        3,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdBackCenter,
        }
    },
    {
        "3.1",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdLowFrequency,
        }
    },
    {
        "4.0",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackCenter,
        }
    },
    {
        "4.1",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdLowFrequency,
        }
    },
    {
        "Quad",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
        },
    },
    {
        "Quad (side)",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
        }
    },
    {
        "5.0",
        5,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
        }
    },
    {
        "5.0 (back)",
        5,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
        }
    },
    {
        "5.1",
        6,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdLowFrequency,
        }
    },
    {
        "5.1 (back)",
        6,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
            SoundIoChannelIdLowFrequency,
        }
    },
    {
        "6.0",
        6,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdBackCenter,
        }
    },
    {
        "6.0 (front)",
        6,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdFrontLeftOfCenter,
            SoundIoChannelIdFrontRightOfCenter,
        }
    },
    {
        "Hexagonal",
        6,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
            SoundIoChannelIdBackCenter,
        }
    },
    {
        "6.1",
        7,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdBackCenter,
            SoundIoChannelIdLowFrequency,
        }
    },
    {
        "6.1 (back)",
        7,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
            SoundIoChannelIdBackCenter,
            SoundIoChannelIdLowFrequency,
        }
    },
    {
        "6.1 (front)",
        7,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdFrontLeftOfCenter,
            SoundIoChannelIdFrontRightOfCenter,
            SoundIoChannelIdLowFrequency,
        }
    },
    {
        "7.0",
        7,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
        }
    },
    {
        "7.0 (front)",
        7,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdFrontLeftOfCenter,
            SoundIoChannelIdFrontRightOfCenter,
        }
    },
    {
        "7.1",
        8,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
            SoundIoChannelIdLowFrequency,
        }
    },
    {
        "7.1 (wide)",
        8,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdFrontLeftOfCenter,
            SoundIoChannelIdFrontRightOfCenter,
            SoundIoChannelIdLowFrequency,
        }
    },
    {
        "7.1 (wide) (back)",
        8,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
            SoundIoChannelIdFrontLeftOfCenter,
            SoundIoChannelIdFrontRightOfCenter,
            SoundIoChannelIdLowFrequency,
        }
    },
    {
        "Octagonal",
        8,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
            SoundIoChannelIdBackCenter,
        }
    },
};

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


bool soundio_channel_layout_equal(
        const struct SoundIoChannelLayout *a,
        const struct SoundIoChannelLayout *b)
{
    if (a->channel_count != b->channel_count)
        return false;

    for (int i = 0; i < a->channel_count; i += 1) {
        if (a->channels[i] != b->channels[i])
            return false;
    }

    return true;
}

const char *soundio_get_channel_name(enum SoundIoChannelId id) {
    switch (id) {
    case SoundIoChannelIdInvalid: return "(Invalid Channel)";
    case SoundIoChannelIdCount: return "(Invalid Channel)";

    case SoundIoChannelIdFrontLeft: return "Front Left";
    case SoundIoChannelIdFrontRight: return "Front Right";
    case SoundIoChannelIdFrontCenter: return "Front Center";
    case SoundIoChannelIdLowFrequency: return "Low Frequency";
    case SoundIoChannelIdBackLeft:  return "Back Left";
    case SoundIoChannelIdBackRight: return "Back Right";
    case SoundIoChannelIdFrontLeftOfCenter: return "Front Left of Center";
    case SoundIoChannelIdFrontRightOfCenter: return "Front Right of Center";
    case SoundIoChannelIdBackCenter: return "Back Center";
    case SoundIoChannelIdSideLeft: return "Side Left";
    case SoundIoChannelIdSideRight: return "Side Right";
    case SoundIoChannelIdTopCenter: return "Top Center";
    case SoundIoChannelIdTopFrontLeft: return "Top Front Left";
    case SoundIoChannelIdTopFrontCenter: return "Top Front Center";
    case SoundIoChannelIdTopFrontRight: return "Top Front Right";
    case SoundIoChannelIdTopBackLeft: return "Top Back Left";
    case SoundIoChannelIdTopBackCenter: return "Top Back Center";
    case SoundIoChannelIdTopBackRight: return "Top Back Right";
    }
    return "(Invalid Channel)";
}

int soundio_channel_layout_builtin_count(void) {
    return array_length(builtin_channel_layouts);
}

const struct SoundIoChannelLayout *soundio_channel_layout_get_builtin(int index) {
    assert(index >= 0);
    assert(index <= array_length(builtin_channel_layouts));
    return &builtin_channel_layouts[index];
}

void soundio_debug_print_channel_layout(const struct SoundIoChannelLayout *layout) {
    if (layout->name) {
        fprintf(stderr, "%s\n", layout->name);
    } else {
        fprintf(stderr, "%s", soundio_get_channel_name(layout->channels[0]));
        for (int i = 1; i < layout->channel_count; i += 1) {
            fprintf(stderr, ", %s", soundio_get_channel_name(layout->channels[i]));
        }
        fprintf(stderr, "\n");
    }
}

int soundio_channel_layout_find_channel(
        const struct SoundIoChannelLayout *layout, enum SoundIoChannelId channel)
{
    for (int i = 0; i < layout->channel_count; i += 1) {
        if (layout->channels[i] == channel)
            return i;
    }
    return -1;
}

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
