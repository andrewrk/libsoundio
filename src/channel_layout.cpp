/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio.hpp"

#include <stdio.h>

static struct SoundIoChannelLayout builtin_channel_layouts[] = {
    [SoundIoChannelLayoutIdMono] = {
        "Mono",
        1,
        {
            SoundIoChannelIdFrontCenter,
        },
    },
    [SoundIoChannelLayoutIdStereo] = {
        "Stereo",
        2,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
        },
    },
    [SoundIoChannelLayoutId2Point1] = {
        "2.1",
        3,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdLfe,
        },
    },
    [SoundIoChannelLayoutId3Point0] = {
        "3.0",
        3,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
        }
    },
    [SoundIoChannelLayoutId3Point0Back] = {
        "3.0 (back)",
        3,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdBackCenter,
        }
    },
    [SoundIoChannelLayoutId3Point1] = {
        "3.1",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdLfe,
        }
    },
    [SoundIoChannelLayoutId4Point0] = {
        "4.0",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackCenter,
        }
    },
    [SoundIoChannelLayoutIdQuad] = {
        "Quad",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
        },
    },
    [SoundIoChannelLayoutIdQuadSide] = {
        "Quad (side)",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
        }
    },
    [SoundIoChannelLayoutId4Point1] = {
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
    [SoundIoChannelLayoutId5Point0Back] = {
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
    [SoundIoChannelLayoutId5Point0Side] = {
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
    [SoundIoChannelLayoutId5Point1] = {
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
    [SoundIoChannelLayoutId5Point1Back] = {
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
    [SoundIoChannelLayoutId6Point0Side] = {
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
    [SoundIoChannelLayoutId6Point0Front] = {
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
    [SoundIoChannelLayoutIdHexagonal] = {
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
    [SoundIoChannelLayoutId6Point1] = {
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
    [SoundIoChannelLayoutId6Point1Back] = {
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
    [SoundIoChannelLayoutId6Point1Front] = {
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
    [SoundIoChannelLayoutId7Point0] = {
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
    [SoundIoChannelLayoutId7Point0Front] = {
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
    [SoundIoChannelLayoutId7Point1] = {
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
    [SoundIoChannelLayoutId7Point1Wide] = {
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
    [SoundIoChannelLayoutId7Point1WideBack] = {
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
    [SoundIoChannelLayoutIdOctagonal] = {
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

static const int channel_name_alias_count = 3;
static const char *channel_names[][channel_name_alias_count] = {
    [SoundIoChannelIdInvalid] = {
        "(Invalid Channel)",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdFrontLeft] = {
        "Front Left",
        "FL",
        "front-left",
    },
    [SoundIoChannelIdFrontRight] = {
        "Front Right",
        "FR",
        "front-right",
    },
    [SoundIoChannelIdFrontCenter] = {
        "Front Center",
        "FC",
        "front-center",
    },
    [SoundIoChannelIdLfe] = {
        "LFE",
        "LFE",
        "lfe",
    },
    [SoundIoChannelIdBackLeft] = {
        "Back Left",
        "BL",
        "rear-left",
    },
    [SoundIoChannelIdBackRight] = {
        "Back Right",
        "BR",
        "rear-right",
    },
    [SoundIoChannelIdFrontLeftCenter] = {
        "Front Left Center",
        "FLC",
        "front-left-of-center",
    },
    [SoundIoChannelIdFrontRightCenter] = {
        "Front Right Center",
        "FRC",
        "front-right-of-center",
    },
    [SoundIoChannelIdBackCenter] = {
        "Back Center",
        "BC",
        "rear-center",
    },
    [SoundIoChannelIdSideLeft] = {
        "Side Left",
        "SL",
        "side-left",
    },
    [SoundIoChannelIdSideRight] = {
        "Side Right",
        "SR",
        "side-right",
    },
    [SoundIoChannelIdTopCenter] = {
        "Top Center",
        "TC",
        "top-center",
    },
    [SoundIoChannelIdTopFrontLeft] = {
        "Top Front Left",
        "TFL",
        "top-front-left",
    },
    [SoundIoChannelIdTopFrontCenter] = {
        "Top Front Center",
        "TFC",
        "top-front-center",
    },
    [SoundIoChannelIdTopFrontRight] = {
        "Top Front Right",
        "TFR",
        "top-front-right",
    },
    [SoundIoChannelIdTopBackLeft] = {
        "Top Back Left",
        "TBL",
        "top-rear-left",
    },
    [SoundIoChannelIdTopBackCenter] = {
        "Top Back Center",
        "TBC",
        "top-rear-center",
    },
    [SoundIoChannelIdTopBackRight] = {
        "Top Back Right",
        "TBR",
        "top-rear-right",
    },
    [SoundIoChannelIdBackLeftCenter] = {
        "Back Left Center",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdBackRightCenter] = {
        "Back Right Center",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdFrontLeftWide] = {
        "Front Left Wide",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdFrontRightWide] = {
        "Front Right Wide",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdFrontLeftHigh] = {
        "Front Left High",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdFrontCenterHigh] = {
        "Front Center High",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdFrontRightHigh] = {
        "Front Right High",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdTopFrontLeftCenter] = {
        "Top Front Left Center",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdTopFrontRightCenter] = {
        "Top Front Right Center",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdTopSideLeft] = {
        "Top Side Left",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdTopSideRight] = {
        "Top Side Right",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdLeftLfe] = {
        "Left LFE",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdRightLfe] = {
        "Right LFE",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdLfe2] = {
        "LFE 2",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdBottomCenter] = {
        "Bottom Center",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdBottomLeftCenter] = {
        "Bottom Left Center",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdBottomRightCenter] = {
        "Bottom Right Center",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdMsMid] = {
        "Mid/Side Mid",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdMsSide] = {
        "Mid/Side Side",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAmbisonicW] = {
        "Ambisonic W",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAmbisonicX] = {
        "Ambisonic X",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAmbisonicY] = {
        "Ambisonic Y",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAmbisonicZ] = {
        "Ambisonic Z",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdXyX] = {
        "X-Y X",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdXyY] = {
        "X-Y Y",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdHeadphonesLeft] = {
        "Headphones Left",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdHeadphonesRight] = {
        "Headphones Right",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdClickTrack] = {
        "Click Track",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdForeignLanguage] = {
        "Foreign Language",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdHearingImpaired] = {
        "Hearing Impaired",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdNarration] = {
        "Narration",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdHaptic] = {
        "Haptic",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdDialogCentricMix] = {
        "Dialog Centric Mix",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux] = {
        "Aux",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux0] = {
        "Aux 0",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux1] = {
        "Aux 1",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux2] = {
        "Aux 2",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux3] = {
        "Aux 3",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux4] = {
        "Aux 4",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux5] = {
        "Aux 5",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux6] = {
        "Aux 6",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux7] = {
        "Aux 7",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux8] = {
        "Aux 8",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux9] = {
        "Aux 9",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux10] = {
        "Aux 10",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux11] = {
        "Aux 11",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux12] = {
        "Aux 12",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux13] = {
        "Aux 13",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux14] = {
        "Aux 14",
        nullptr,
        nullptr,
    },
    [SoundIoChannelIdAux15] = {
        "Aux 15",
        nullptr,
        nullptr,
    },
};

const char *soundio_get_channel_name(enum SoundIoChannelId id) {
    if (id < 0 || id > array_length(channel_names))
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
    return array_length(builtin_channel_layouts);
}

const struct SoundIoChannelLayout *soundio_channel_layout_get_builtin(int index) {
    assert(index >= 0);
    assert(index <= array_length(builtin_channel_layouts));
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
    for (int i = 0; i < array_length(builtin_channel_layouts); i += 1) {
        const struct SoundIoChannelLayout *builtin_layout = &builtin_channel_layouts[i];
        if (soundio_channel_layout_equal(builtin_layout, layout)) {
            layout->name = builtin_layout->name;
            return true;
        }
    }
    layout->name = nullptr;
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
    return nullptr;
}

enum SoundIoChannelId soundio_parse_channel_id(const char *str, int str_len) {
    for (int id = 0; id < array_length(channel_names); id += 1) {
        for (int i = 0; i < channel_name_alias_count; i += 1) {
            const char *alias = channel_names[id][i];
            if (!alias)
                break;
            int alias_len = strlen(alias);
            if (soundio_streql(alias, alias_len, str, str_len))
                return (SoundIoChannelId)id;
        }
    }
    return SoundIoChannelIdInvalid;
}
