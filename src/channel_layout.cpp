/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio.hpp"

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
        "4.1",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdLfe,
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

const char *soundio_get_channel_name(enum SoundIoChannelId id) {
    switch (id) {
    case SoundIoChannelIdInvalid: return "(Invalid Channel)";
    case SoundIoChannelIdCount: return "(Invalid Channel)";

    case SoundIoChannelIdFrontLeft: return "Front Left";
    case SoundIoChannelIdFrontRight: return "Front Right";
    case SoundIoChannelIdFrontCenter: return "Front Center";
    case SoundIoChannelIdLfe: return "LFE";
    case SoundIoChannelIdBackLeft:  return "Back Left";
    case SoundIoChannelIdBackRight: return "Back Right";
    case SoundIoChannelIdFrontLeftCenter: return "Front Left Center";
    case SoundIoChannelIdFrontRightCenter: return "Front Right Center";
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
    case SoundIoChannelIdBackLeftCenter: return "Back Left Center";
    case SoundIoChannelIdBackRightCenter: return "Back Right Center";
    case SoundIoChannelIdFrontLeftWide: return "Front Left Wide";
    case SoundIoChannelIdFrontRightWide: return "Front Right Wide";
    case SoundIoChannelIdFrontLeftHigh: return "Front Left High";
    case SoundIoChannelIdFrontCenterHigh: return "Front Center High";
    case SoundIoChannelIdFrontRightHigh: return "Front Right High";
    case SoundIoChannelIdTopFrontLeftCenter: return "Top Front Left Center";
    case SoundIoChannelIdTopFrontRightCenter: return "Top Front Right Center";
    case SoundIoChannelIdTopSideLeft: return "Top Side Left";
    case SoundIoChannelIdTopSideRight: return "Top Side Right";
    case SoundIoChannelIdLeftLfe: return "Left LFE";
    case SoundIoChannelIdRightLfe: return "Right LFE";
    case SoundIoChannelIdBottomCenter: return "Bottom Center";
    case SoundIoChannelIdBottomLeftCenter: return "Bottom Left Center";
    case SoundIoChannelIdBottomRightCenter: return "Bottom Right Center";
    }
    return "(Invalid Channel)";
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

bool soundio_channel_layout_detect_builtin(struct SoundIoChannelLayout *layout) {
    for (int i = 0; i < array_length(builtin_channel_layouts); i += 1) {
        const struct SoundIoChannelLayout *builtin_layout = &builtin_channel_layouts[i];
        if (soundio_channel_layout_equal(builtin_layout, layout)) {
            layout->name = builtin_layout->name;
            return true;
        }
    }
    return false;
}
