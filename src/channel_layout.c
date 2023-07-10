/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio_private.h"

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
            SoundIoChannelIdLfe,
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
            SoundIoChannelIdLfe,
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
        "4.1",
        5,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackCenter,
            SoundIoChannelIdLfe,
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
        "5.0 (side)",
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
        "5.1",
        6,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
            SoundIoChannelIdLfe,
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
            SoundIoChannelIdLfe,
        }
    },
    {
        "6.0 (side)",
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
            SoundIoChannelIdFrontLeftCenter,
            SoundIoChannelIdFrontRightCenter,
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
            SoundIoChannelIdLfe,
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
            SoundIoChannelIdLfe,
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
            SoundIoChannelIdFrontLeftCenter,
            SoundIoChannelIdFrontRightCenter,
            SoundIoChannelIdLfe,
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
            SoundIoChannelIdFrontLeftCenter,
            SoundIoChannelIdFrontRightCenter,
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
            SoundIoChannelIdLfe,
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
            SoundIoChannelIdFrontLeftCenter,
            SoundIoChannelIdFrontRightCenter,
            SoundIoChannelIdLfe,
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
            SoundIoChannelIdFrontLeftCenter,
            SoundIoChannelIdFrontRightCenter,
            SoundIoChannelIdLfe,
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

#define CHANNEL_NAME_ALIAS_COUNT 3
typedef const char *channel_names_t[CHANNEL_NAME_ALIAS_COUNT];
static channel_names_t channel_names[] = {
    {"(Invalid Channel)", NULL, NULL},
    {"Front Left", "FL", "front-left"},
    {"Front Right", "FR", "front-right"},
    {"Front Center", "FC", "front-center"},
    {"LFE", "LFE", "lfe"},
    {"Back Left", "BL", "rear-left"},
    {"Back Right", "BR", "rear-right"},
    {"Front Left Center", "FLC", "front-left-of-center"},
    {"Front Right Center", "FRC", "front-right-of-center"},
    {"Back Center", "BC", "rear-center"},
    {"Side Left", "SL", "side-left"},
    {"Side Right", "SR", "side-right"},
    {"Top Center", "TC", "top-center"},
    {"Top Front Left", "TFL", "top-front-left"},
    {"Top Front Center", "TFC", "top-front-center"},
    {"Top Front Right", "TFR", "top-front-right"},
    {"Top Back Left", "TBL", "top-rear-left"},
    {"Top Back Center", "TBC", "top-rear-center"},
    {"Top Back Right", "TBR", "top-rear-right"},
    {"Back Left Center", NULL, NULL},
    {"Back Right Center", NULL, NULL},
    {"Front Left Wide", NULL, NULL},
    {"Front Right Wide", NULL, NULL},
    {"Front Left High", NULL, NULL},
    {"Front Center High", NULL, NULL},
    {"Front Right High", NULL, NULL},
    {"Top Front Left Center", NULL, NULL},
    {"Top Front Right Center", NULL, NULL},
    {"Top Side Left", NULL, NULL},
    {"Top Side Right", NULL, NULL},
    {"Left LFE", NULL, NULL},
    {"Right LFE", NULL, NULL},
    {"LFE 2", NULL, NULL},
    {"Bottom Center", NULL, NULL},
    {"Bottom Left Center", NULL, NULL},
    {"Bottom Right Center", NULL, NULL},
    {"Mid/Side Mid", NULL, NULL},
    {"Mid/Side Side", NULL, NULL},
    {"Ambisonic W", NULL, NULL},
    {"Ambisonic X", NULL, NULL},
    {"Ambisonic Y", NULL, NULL},
    {"Ambisonic Z", NULL, NULL},
    {"X-Y X", NULL, NULL},
    {"X-Y Y", NULL, NULL},
    {"Headphones Left", NULL, NULL},
    {"Headphones Right", NULL, NULL},
    {"Click Track", NULL, NULL},
    {"Foreign Language", NULL, NULL},
    {"Hearing Impaired", NULL, NULL},
    {"Narration", NULL, NULL},
    {"Haptic", NULL, NULL},
    {"Dialog Centric Mix", NULL, NULL},
    {"Aux", NULL, NULL},
    {"Aux 0", NULL, NULL},
    {"Aux 1", NULL, NULL},
    {"Aux 2", NULL, NULL},
    {"Aux 3", NULL, NULL},
    {"Aux 4", NULL, NULL},
    {"Aux 5", NULL, NULL},
    {"Aux 6", NULL, NULL},
    {"Aux 7", NULL, NULL},
    {"Aux 8", NULL, NULL},
    {"Aux 9", NULL, NULL},
    {"Aux 10", NULL, NULL},
    {"Aux 11", NULL, NULL},
    {"Aux 12", NULL, NULL},
    {"Aux 13", NULL, NULL},
    {"Aux 14", NULL, NULL},
    {"Aux 15", NULL, NULL},
};

const char *soundio_get_channel_name(enum SoundIoChannelId id) {
    if (id >= ARRAY_LENGTH(channel_names))
        return "(Invalid Channel)";
    else
        return channel_names[id][0];
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

int soundio_channel_layout_builtin_count(void) {
    return ARRAY_LENGTH(builtin_channel_layouts);
}

const struct SoundIoChannelLayout *soundio_channel_layout_get_builtin(int index) {
    assert(index >= 0);
    assert(index <= ARRAY_LENGTH(builtin_channel_layouts));
    return &builtin_channel_layouts[index];
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

bool soundio_channel_layout_detect_builtin(struct SoundIoChannelLayout *layout) {
    for (int i = 0; i < ARRAY_LENGTH(builtin_channel_layouts); i += 1) {
        const struct SoundIoChannelLayout *builtin_layout = &builtin_channel_layouts[i];
        if (soundio_channel_layout_equal(builtin_layout, layout)) {
            layout->name = builtin_layout->name;
            return true;
        }
    }
    layout->name = NULL;
    return false;
}

const struct SoundIoChannelLayout *soundio_channel_layout_get_default(int channel_count) {
    switch (channel_count) {
        case 1: return soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);
        case 2: return soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
        case 3: return soundio_channel_layout_get_builtin(SoundIoChannelLayoutId3Point0);
        case 4: return soundio_channel_layout_get_builtin(SoundIoChannelLayoutId4Point0);
        case 5: return soundio_channel_layout_get_builtin(SoundIoChannelLayoutId5Point0Back);
        case 6: return soundio_channel_layout_get_builtin(SoundIoChannelLayoutId5Point1Back);
        case 7: return soundio_channel_layout_get_builtin(SoundIoChannelLayoutId6Point1);
        case 8: return soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point1);
    }
    return NULL;
}

enum SoundIoChannelId soundio_parse_channel_id(const char *str, int str_len) {
    for (int id = 0; id < ARRAY_LENGTH(channel_names); id += 1) {
        for (int i = 0; i < CHANNEL_NAME_ALIAS_COUNT; i += 1) {
            const char *alias = channel_names[id][i];
            if (!alias)
                break;
            int alias_len = (int) strlen(alias);
            if (soundio_streql(alias, alias_len, str, str_len))
                return (enum SoundIoChannelId)id;
        }
    }
    return SoundIoChannelIdInvalid;
}
