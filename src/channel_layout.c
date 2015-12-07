/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "soundio_private.h"

#include <stdio.h>

static struct SoundIoChannelLayout *make_builtin_channel_layouts()
{
    static struct SoundIoChannelLayout layouts[SoundIoChannelLayoutIdMax];
    layouts[SoundIoChannelLayoutIdMono] = {
        "Mono",
        1,
        {
            SoundIoChannelIdFrontCenter,
        },
    };
    layouts[SoundIoChannelLayoutIdStereo] = {
        "Stereo",
        2,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
        },
    };
    layouts[SoundIoChannelLayoutId2Point1] = {
        "2.1",
        3,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdLfe,
        },
    };
    layouts[SoundIoChannelLayoutId3Point0] = {
        "3.0",
        3,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
        }
    };
    layouts[SoundIoChannelLayoutId3Point0Back] = {
        "3.0 (back)",
        3,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdBackCenter,
        }
    };
    layouts[SoundIoChannelLayoutId3Point1] = {
        "3.1",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdLfe,
        }
    };
    layouts[SoundIoChannelLayoutId4Point0] = {
        "4.0",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackCenter,
        }
    };
    layouts[SoundIoChannelLayoutIdQuad] = {
        "Quad",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
        },
    };
    layouts[SoundIoChannelLayoutIdQuadSide] = {
        "Quad (side)",
        4,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
        }
    };
    layouts[SoundIoChannelLayoutId4Point1] = {
        "4.1",
        5,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackCenter,
            SoundIoChannelIdLfe,
        }
    };
    layouts[SoundIoChannelLayoutId5Point0Back] = {
        "5.0 (back)",
        5,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdBackLeft,
            SoundIoChannelIdBackRight,
        }
    };
    layouts[SoundIoChannelLayoutId5Point0Side] = {
        "5.0 (side)",
        5,
        {
            SoundIoChannelIdFrontLeft,
            SoundIoChannelIdFrontRight,
            SoundIoChannelIdFrontCenter,
            SoundIoChannelIdSideLeft,
            SoundIoChannelIdSideRight,
        }
    };
    layouts[SoundIoChannelLayoutId5Point1] = {
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
    };
    layouts[SoundIoChannelLayoutId5Point1Back] = {
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
    };
    layouts[SoundIoChannelLayoutId6Point0Side] = {
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
    };
    layouts[SoundIoChannelLayoutId6Point0Front] = {
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
    };
    layouts[SoundIoChannelLayoutIdHexagonal] = {
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
    };
    layouts[SoundIoChannelLayoutId6Point1] = {
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
    };
    layouts[SoundIoChannelLayoutId6Point1Back] = {
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
    };
    layouts[SoundIoChannelLayoutId6Point1Front] = {
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
    };
    layouts[SoundIoChannelLayoutId7Point0] = {
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
    };
    layouts[SoundIoChannelLayoutId7Point0Front] = {
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
    };
    layouts[SoundIoChannelLayoutId7Point1] = {
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
    };
    layouts[SoundIoChannelLayoutId7Point1Wide] = {
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
    };
    layouts[SoundIoChannelLayoutId7Point1WideBack] = {
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
    };
    layouts[SoundIoChannelLayoutIdOctagonal] = {
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
    };
    return &layouts[0];
};
static struct SoundIoChannelLayout *builtin_channel_layouts = make_builtin_channel_layouts();

#define CHANNEL_NAME_ALIAS_COUNT 3
typedef const char *channel_names_t[CHANNEL_NAME_ALIAS_COUNT];
static channel_names_t *make_channel_names()
{
    static channel_names_t names[SoundIoChannelIdMax];
    names[SoundIoChannelIdInvalid][0] = "(Invalid Channel)";
    names[SoundIoChannelIdInvalid][1] = NULL;
    names[SoundIoChannelIdInvalid][2] = NULL;
    names[SoundIoChannelIdFrontLeft][0] = "Front Left";
    names[SoundIoChannelIdFrontLeft][1] = "FL";
    names[SoundIoChannelIdFrontLeft][2] = "front-left";
    names[SoundIoChannelIdFrontRight][0] = "Front Right";
    names[SoundIoChannelIdFrontRight][1] = "FR";
    names[SoundIoChannelIdFrontRight][2] = "front-right";
    names[SoundIoChannelIdFrontCenter][0] = "Front Center";
    names[SoundIoChannelIdFrontCenter][1] = "FC";
    names[SoundIoChannelIdFrontCenter][2] = "front-center";
    names[SoundIoChannelIdLfe][0] = "LFE";
    names[SoundIoChannelIdLfe][1] = "LFE";
    names[SoundIoChannelIdLfe][2] = "lfe";
    names[SoundIoChannelIdBackLeft][0] = "Back Left";
    names[SoundIoChannelIdBackLeft][1] = "BL";
    names[SoundIoChannelIdBackLeft][2] = "rear-left";
    names[SoundIoChannelIdBackRight][0] = "Back Right";
    names[SoundIoChannelIdBackRight][1] = "BR";
    names[SoundIoChannelIdBackRight][2] = "rear-right";
    names[SoundIoChannelIdFrontLeftCenter][0] = "Front Left Center";
    names[SoundIoChannelIdFrontLeftCenter][1] = "FLC";
    names[SoundIoChannelIdFrontLeftCenter][2] = "front-left-of-center";
    names[SoundIoChannelIdFrontRightCenter][0] = "Front Right Center";
    names[SoundIoChannelIdFrontRightCenter][1] = "FRC";
    names[SoundIoChannelIdFrontRightCenter][2] = "front-right-of-center";
    names[SoundIoChannelIdBackCenter][0] = "Back Center";
    names[SoundIoChannelIdBackCenter][1] = "BC";
    names[SoundIoChannelIdBackCenter][2] = "rear-center";
    names[SoundIoChannelIdSideLeft][0] = "Side Left";
    names[SoundIoChannelIdSideLeft][1] = "SL";
    names[SoundIoChannelIdSideLeft][2] = "side-left";
    names[SoundIoChannelIdSideRight][0] = "Side Right";
    names[SoundIoChannelIdSideRight][1] = "SR";
    names[SoundIoChannelIdSideRight][2] = "side-right";
    names[SoundIoChannelIdTopCenter][0] = "Top Center";
    names[SoundIoChannelIdTopCenter][1] = "TC";
    names[SoundIoChannelIdTopCenter][2] = "top-center";
    names[SoundIoChannelIdTopFrontLeft][0] = "Top Front Left";
    names[SoundIoChannelIdTopFrontLeft][1] = "TFL";
    names[SoundIoChannelIdTopFrontLeft][2] = "top-front-left";
    names[SoundIoChannelIdTopFrontCenter][0] = "Top Front Center";
    names[SoundIoChannelIdTopFrontCenter][1] = "TFC";
    names[SoundIoChannelIdTopFrontCenter][2] = "top-front-center";
    names[SoundIoChannelIdTopFrontRight][0] = "Top Front Right";
    names[SoundIoChannelIdTopFrontRight][1] = "TFR";
    names[SoundIoChannelIdTopFrontRight][2] = "top-front-right";
    names[SoundIoChannelIdTopBackLeft][0] = "Top Back Left";
    names[SoundIoChannelIdTopBackLeft][1] = "TBL";
    names[SoundIoChannelIdTopBackLeft][2] = "top-rear-left";
    names[SoundIoChannelIdTopBackCenter][0] = "Top Back Center";
    names[SoundIoChannelIdTopBackCenter][1] = "TBC";
    names[SoundIoChannelIdTopBackCenter][2] = "top-rear-center";
    names[SoundIoChannelIdTopBackRight][0] = "Top Back Right";
    names[SoundIoChannelIdTopBackRight][1] = "TBR";
    names[SoundIoChannelIdTopBackRight][2] = "top-rear-right";
    names[SoundIoChannelIdBackLeftCenter][0] = "Back Left Center";
    names[SoundIoChannelIdBackLeftCenter][1] = NULL;
    names[SoundIoChannelIdBackLeftCenter][2] = NULL;
    names[SoundIoChannelIdBackRightCenter][0] = "Back Right Center";
    names[SoundIoChannelIdBackRightCenter][1] = NULL;
    names[SoundIoChannelIdBackRightCenter][2] = NULL;
    names[SoundIoChannelIdFrontLeftWide][0] = "Front Left Wide";
    names[SoundIoChannelIdFrontLeftWide][1] = NULL;
    names[SoundIoChannelIdFrontLeftWide][2] = NULL;
    names[SoundIoChannelIdFrontRightWide][0] = "Front Right Wide";
    names[SoundIoChannelIdFrontRightWide][1] = NULL;
    names[SoundIoChannelIdFrontRightWide][2] = NULL;
    names[SoundIoChannelIdFrontLeftHigh][0] = "Front Left High";
    names[SoundIoChannelIdFrontLeftHigh][1] = NULL;
    names[SoundIoChannelIdFrontLeftHigh][2] = NULL;
    names[SoundIoChannelIdFrontCenterHigh][0] = "Front Center High";
    names[SoundIoChannelIdFrontCenterHigh][1] = NULL;
    names[SoundIoChannelIdFrontCenterHigh][2] = NULL;
    names[SoundIoChannelIdFrontRightHigh][0] = "Front Right High";
    names[SoundIoChannelIdFrontRightHigh][1] = NULL;
    names[SoundIoChannelIdFrontRightHigh][2] = NULL;
    names[SoundIoChannelIdTopFrontLeftCenter][0] = "Top Front Left Center";
    names[SoundIoChannelIdTopFrontLeftCenter][1] = NULL;
    names[SoundIoChannelIdTopFrontLeftCenter][2] = NULL;
    names[SoundIoChannelIdTopFrontRightCenter][0] = "Top Front Right Center";
    names[SoundIoChannelIdTopFrontRightCenter][1] = NULL;
    names[SoundIoChannelIdTopFrontRightCenter][2] = NULL;
    names[SoundIoChannelIdTopSideLeft][0] = "Top Side Left";
    names[SoundIoChannelIdTopSideLeft][1] = NULL;
    names[SoundIoChannelIdTopSideLeft][2] = NULL;
    names[SoundIoChannelIdTopSideRight][0] = "Top Side Right";
    names[SoundIoChannelIdTopSideRight][1] = NULL;
    names[SoundIoChannelIdTopSideRight][2] = NULL;
    names[SoundIoChannelIdLeftLfe][0] = "Left LFE";
    names[SoundIoChannelIdLeftLfe][1] = NULL;
    names[SoundIoChannelIdLeftLfe][2] = NULL;
    names[SoundIoChannelIdRightLfe][0] = "Right LFE";
    names[SoundIoChannelIdRightLfe][1] = NULL;
    names[SoundIoChannelIdRightLfe][2] = NULL;
    names[SoundIoChannelIdLfe2][0] = "LFE 2";
    names[SoundIoChannelIdLfe2][1] = NULL;
    names[SoundIoChannelIdLfe2][2] = NULL;
    names[SoundIoChannelIdBottomCenter][0] = "Bottom Center";
    names[SoundIoChannelIdBottomCenter][1] = NULL;
    names[SoundIoChannelIdBottomCenter][2] = NULL;
    names[SoundIoChannelIdBottomLeftCenter][0] = "Bottom Left Center";
    names[SoundIoChannelIdBottomLeftCenter][1] = NULL;
    names[SoundIoChannelIdBottomLeftCenter][2] = NULL;
    names[SoundIoChannelIdBottomRightCenter][0] = "Bottom Right Center";
    names[SoundIoChannelIdBottomRightCenter][1] = NULL;
    names[SoundIoChannelIdBottomRightCenter][2] = NULL;
    names[SoundIoChannelIdMsMid][0] = "Mid/Side Mid";
    names[SoundIoChannelIdMsMid][1] = NULL;
    names[SoundIoChannelIdMsMid][2] = NULL;
    names[SoundIoChannelIdMsSide][0] = "Mid/Side Side";
    names[SoundIoChannelIdMsSide][1] = NULL;
    names[SoundIoChannelIdMsSide][2] = NULL;
    names[SoundIoChannelIdAmbisonicW][0] = "Ambisonic W";
    names[SoundIoChannelIdAmbisonicW][1] = NULL;
    names[SoundIoChannelIdAmbisonicW][2] = NULL;
    names[SoundIoChannelIdAmbisonicX][0] = "Ambisonic X";
    names[SoundIoChannelIdAmbisonicX][1] = NULL;
    names[SoundIoChannelIdAmbisonicX][2] = NULL;
    names[SoundIoChannelIdAmbisonicY][0] = "Ambisonic Y";
    names[SoundIoChannelIdAmbisonicY][1] = NULL;
    names[SoundIoChannelIdAmbisonicY][2] = NULL;
    names[SoundIoChannelIdAmbisonicZ][0] = "Ambisonic Z";
    names[SoundIoChannelIdAmbisonicZ][1] = NULL;
    names[SoundIoChannelIdAmbisonicZ][2] = NULL;
    names[SoundIoChannelIdXyX][0] = "X-Y X";
    names[SoundIoChannelIdXyX][1] = NULL;
    names[SoundIoChannelIdXyX][2] = NULL;
    names[SoundIoChannelIdXyY][0] = "X-Y Y";
    names[SoundIoChannelIdXyY][1] = NULL;
    names[SoundIoChannelIdXyY][2] = NULL;
    names[SoundIoChannelIdHeadphonesLeft][0] = "Headphones Left";
    names[SoundIoChannelIdHeadphonesLeft][1] = NULL;
    names[SoundIoChannelIdHeadphonesLeft][2] = NULL;
    names[SoundIoChannelIdHeadphonesRight][0] = "Headphones Right";
    names[SoundIoChannelIdHeadphonesRight][1] = NULL;
    names[SoundIoChannelIdHeadphonesRight][2] = NULL;
    names[SoundIoChannelIdClickTrack][0] = "Click Track";
    names[SoundIoChannelIdClickTrack][1] = NULL;
    names[SoundIoChannelIdClickTrack][2] = NULL;
    names[SoundIoChannelIdForeignLanguage][0] = "Foreign Language";
    names[SoundIoChannelIdForeignLanguage][1] = NULL;
    names[SoundIoChannelIdForeignLanguage][2] = NULL;
    names[SoundIoChannelIdHearingImpaired][0] = "Hearing Impaired";
    names[SoundIoChannelIdHearingImpaired][1] = NULL;
    names[SoundIoChannelIdHearingImpaired][2] = NULL;
    names[SoundIoChannelIdNarration][0] = "Narration";
    names[SoundIoChannelIdNarration][1] = NULL;
    names[SoundIoChannelIdNarration][2] = NULL;
    names[SoundIoChannelIdHaptic][0] = "Haptic";
    names[SoundIoChannelIdHaptic][1] = NULL;
    names[SoundIoChannelIdHaptic][2] = NULL;
    names[SoundIoChannelIdDialogCentricMix][0] = "Dialog Centric Mix";
    names[SoundIoChannelIdDialogCentricMix][1] = NULL;
    names[SoundIoChannelIdDialogCentricMix][2] = NULL;
    names[SoundIoChannelIdAux][0] = "Aux";
    names[SoundIoChannelIdAux][1] = NULL;
    names[SoundIoChannelIdAux][2] = NULL;
    names[SoundIoChannelIdAux0][0] = "Aux 0";
    names[SoundIoChannelIdAux0][1] = NULL;
    names[SoundIoChannelIdAux0][2] = NULL;
    names[SoundIoChannelIdAux1][0] = "Aux 1";
    names[SoundIoChannelIdAux1][1] = NULL;
    names[SoundIoChannelIdAux1][2] = NULL;
    names[SoundIoChannelIdAux2][0] = "Aux 2";
    names[SoundIoChannelIdAux2][1] = NULL;
    names[SoundIoChannelIdAux2][2] = NULL;
    names[SoundIoChannelIdAux3][0] = "Aux 3";
    names[SoundIoChannelIdAux3][1] = NULL;
    names[SoundIoChannelIdAux3][2] = NULL;
    names[SoundIoChannelIdAux4][0] = "Aux 4";
    names[SoundIoChannelIdAux4][1] = NULL;
    names[SoundIoChannelIdAux4][2] = NULL;
    names[SoundIoChannelIdAux5][0] = "Aux 5";
    names[SoundIoChannelIdAux5][1] = NULL;
    names[SoundIoChannelIdAux5][2] = NULL;
    names[SoundIoChannelIdAux6][0] = "Aux 6";
    names[SoundIoChannelIdAux6][1] = NULL;
    names[SoundIoChannelIdAux6][2] = NULL;
    names[SoundIoChannelIdAux7][0] = "Aux 7";
    names[SoundIoChannelIdAux7][1] = NULL;
    names[SoundIoChannelIdAux7][2] = NULL;
    names[SoundIoChannelIdAux8][0] = "Aux 8";
    names[SoundIoChannelIdAux8][1] = NULL;
    names[SoundIoChannelIdAux8][2] = NULL;
    names[SoundIoChannelIdAux9][0] = "Aux 9";
    names[SoundIoChannelIdAux9][1] = NULL;
    names[SoundIoChannelIdAux9][2] = NULL;
    names[SoundIoChannelIdAux10][0] = "Aux 10";
    names[SoundIoChannelIdAux10][1] = NULL;
    names[SoundIoChannelIdAux10][2] = NULL;
    names[SoundIoChannelIdAux11][0] = "Aux 11";
    names[SoundIoChannelIdAux11][1] = NULL;
    names[SoundIoChannelIdAux11][2] = NULL;
    names[SoundIoChannelIdAux12][0] = "Aux 12";
    names[SoundIoChannelIdAux12][1] = NULL;
    names[SoundIoChannelIdAux12][2] = NULL;
    names[SoundIoChannelIdAux13][0] = "Aux 13";
    names[SoundIoChannelIdAux13][1] = NULL;
    names[SoundIoChannelIdAux13][2] = NULL;
    names[SoundIoChannelIdAux14][0] = "Aux 14";
    names[SoundIoChannelIdAux14][1] = NULL;
    names[SoundIoChannelIdAux14][2] = NULL;
    names[SoundIoChannelIdAux15][0] = "Aux 15";
    names[SoundIoChannelIdAux15][1] = NULL;
    names[SoundIoChannelIdAux15][2] = NULL;
    return &names[0];
};
static channel_names_t *channel_names = make_channel_names();

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
            int alias_len = strlen(alias);
            if (soundio_streql(alias, alias_len, str, str_len))
                return (enum SoundIoChannelId)id;
        }
    }
    return SoundIoChannelIdInvalid;
}
