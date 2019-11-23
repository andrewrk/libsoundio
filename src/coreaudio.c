/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "coreaudio.h"
#include "soundio_private.h"

#include <assert.h>

#include <AudioToolbox/AudioFormat.h>

static const int OUTPUT_ELEMENT = 0;
static const int INPUT_ELEMENT = 1;

static AudioObjectPropertyAddress device_listen_props[] = {
    {
        kAudioDevicePropertyDeviceHasChanged,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyDeviceUID,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyStreamConfiguration,
        kAudioObjectPropertyScopeInput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyStreamConfiguration,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyPreferredChannelLayout,
        kAudioObjectPropertyScopeInput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyPreferredChannelLayout,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeInput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyAvailableNominalSampleRates,
        kAudioObjectPropertyScopeInput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyAvailableNominalSampleRates,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyBufferFrameSize,
        kAudioObjectPropertyScopeInput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyBufferFrameSize,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyBufferFrameSizeRange,
        kAudioObjectPropertyScopeInput,
        kAudioObjectPropertyElementMaster
    },
    {
        kAudioDevicePropertyBufferFrameSizeRange,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    }
};

static enum SoundIoDeviceAim aims[] = {
    SoundIoDeviceAimInput,
    SoundIoDeviceAimOutput,
};

SOUNDIO_MAKE_LIST_DEF(AudioDeviceID, SoundIoListAudioDeviceID, SOUNDIO_LIST_STATIC)

static OSStatus on_devices_changed(AudioObjectID in_object_id, UInt32 in_number_addresses,
    const AudioObjectPropertyAddress in_addresses[], void *in_client_data)
{
    struct SoundIoPrivate *si = (struct SoundIoPrivate*)in_client_data;
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    SOUNDIO_ATOMIC_STORE(sica->device_scan_queued, true);
    soundio_os_cond_signal(sica->scan_devices_cond, NULL);

    return noErr;
}

static OSStatus on_service_restarted(AudioObjectID in_object_id, UInt32 in_number_addresses,
    const AudioObjectPropertyAddress in_addresses[], void *in_client_data)
{
    struct SoundIoPrivate *si = (struct SoundIoPrivate*)in_client_data;
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    SOUNDIO_ATOMIC_STORE(sica->service_restarted, true);
    soundio_os_cond_signal(sica->scan_devices_cond, NULL);

    return noErr;
}

static void unsubscribe_device_listeners(struct SoundIoPrivate *si) {
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    for (int device_index = 0; device_index < sica->registered_listeners.length; device_index += 1) {
        AudioDeviceID device_id = SoundIoListAudioDeviceID_val_at(&sica->registered_listeners, device_index);
        for (int i = 0; i < ARRAY_LENGTH(device_listen_props); i += 1) {
            AudioObjectRemovePropertyListener(device_id, &device_listen_props[i],
                on_devices_changed, si);
        }
    }
    SoundIoListAudioDeviceID_clear(&sica->registered_listeners);
}

static void destroy_ca(struct SoundIoPrivate *si) {
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    AudioObjectPropertyAddress prop_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &prop_address, on_devices_changed, si);

    prop_address.mSelector = kAudioHardwarePropertyServiceRestarted;
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &prop_address, on_service_restarted, si);

    unsubscribe_device_listeners(si);
    SoundIoListAudioDeviceID_deinit(&sica->registered_listeners);

    if (sica->thread) {
        SOUNDIO_ATOMIC_FLAG_CLEAR(sica->abort_flag);
        soundio_os_cond_signal(sica->scan_devices_cond, NULL);
        soundio_os_thread_destroy(sica->thread);
    }

    if (sica->cond)
        soundio_os_cond_destroy(sica->cond);

    if (sica->have_devices_cond)
        soundio_os_cond_destroy(sica->have_devices_cond);

    if (sica->scan_devices_cond)
        soundio_os_cond_destroy(sica->scan_devices_cond);

    if (sica->mutex)
        soundio_os_mutex_destroy(sica->mutex);

    soundio_destroy_devices_info(sica->ready_devices_info);
}

// Possible errors:
//  * SoundIoErrorNoMem
//  * SoundIoErrorEncodingString
static int from_cf_string(CFStringRef string_ref, char **out_str, int *out_str_len) {
    assert(string_ref);

    CFIndex length = CFStringGetLength(string_ref);
    CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    char *buf = ALLOCATE_NONZERO(char, max_size);
    if (!buf)
        return SoundIoErrorNoMem;

    if (!CFStringGetCString(string_ref, buf, max_size, kCFStringEncodingUTF8)) {
        free(buf);
        return SoundIoErrorEncodingString;
    }

    *out_str = buf;
    *out_str_len = strlen(buf);
    return 0;
}

static int aim_to_scope(enum SoundIoDeviceAim aim) {
    return (aim == SoundIoDeviceAimInput) ?
        kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput;
}

static enum SoundIoChannelId from_channel_descr(const AudioChannelDescription *descr) {
    switch (descr->mChannelLabel) {
        default:                                        return SoundIoChannelIdInvalid;
        case kAudioChannelLabel_Left:                   return SoundIoChannelIdFrontLeft;
        case kAudioChannelLabel_Right:                  return SoundIoChannelIdFrontRight;
        case kAudioChannelLabel_Center:                 return SoundIoChannelIdFrontCenter;
        case kAudioChannelLabel_LFEScreen:              return SoundIoChannelIdLfe;
        case kAudioChannelLabel_LeftSurround:           return SoundIoChannelIdBackLeft;
        case kAudioChannelLabel_RightSurround:          return SoundIoChannelIdBackRight;
        case kAudioChannelLabel_LeftCenter:             return SoundIoChannelIdFrontLeftCenter;
        case kAudioChannelLabel_RightCenter:            return SoundIoChannelIdFrontRightCenter;
        case kAudioChannelLabel_CenterSurround:         return SoundIoChannelIdBackCenter;
        case kAudioChannelLabel_LeftSurroundDirect:     return SoundIoChannelIdSideLeft;
        case kAudioChannelLabel_RightSurroundDirect:    return SoundIoChannelIdSideRight;
        case kAudioChannelLabel_TopCenterSurround:      return SoundIoChannelIdTopCenter;
        case kAudioChannelLabel_VerticalHeightLeft:     return SoundIoChannelIdTopFrontLeft;
        case kAudioChannelLabel_VerticalHeightCenter:   return SoundIoChannelIdTopFrontCenter;
        case kAudioChannelLabel_VerticalHeightRight:    return SoundIoChannelIdTopFrontRight;
        case kAudioChannelLabel_TopBackLeft:            return SoundIoChannelIdTopBackLeft;
        case kAudioChannelLabel_TopBackCenter:          return SoundIoChannelIdTopBackCenter;
        case kAudioChannelLabel_TopBackRight:           return SoundIoChannelIdTopBackRight;
        case kAudioChannelLabel_RearSurroundLeft:       return SoundIoChannelIdBackLeft;
        case kAudioChannelLabel_RearSurroundRight:      return SoundIoChannelIdBackRight;
        case kAudioChannelLabel_LeftWide:               return SoundIoChannelIdFrontLeftWide;
        case kAudioChannelLabel_RightWide:              return SoundIoChannelIdFrontRightWide;
        case kAudioChannelLabel_LFE2:                   return SoundIoChannelIdLfe2;
        case kAudioChannelLabel_LeftTotal:              return SoundIoChannelIdFrontLeft;
        case kAudioChannelLabel_RightTotal:             return SoundIoChannelIdFrontRight;
        case kAudioChannelLabel_HearingImpaired:        return SoundIoChannelIdHearingImpaired;
        case kAudioChannelLabel_Narration:              return SoundIoChannelIdNarration;
        case kAudioChannelLabel_Mono:                   return SoundIoChannelIdFrontCenter;
        case kAudioChannelLabel_DialogCentricMix:       return SoundIoChannelIdDialogCentricMix;
        case kAudioChannelLabel_CenterSurroundDirect:   return SoundIoChannelIdBackCenter;
        case kAudioChannelLabel_Haptic:                 return SoundIoChannelIdHaptic;

        case kAudioChannelLabel_Ambisonic_W:            return SoundIoChannelIdAmbisonicW;
        case kAudioChannelLabel_Ambisonic_X:            return SoundIoChannelIdAmbisonicX;
        case kAudioChannelLabel_Ambisonic_Y:            return SoundIoChannelIdAmbisonicY;
        case kAudioChannelLabel_Ambisonic_Z:            return SoundIoChannelIdAmbisonicZ;

        case kAudioChannelLabel_MS_Mid:                 return SoundIoChannelIdMsMid;
        case kAudioChannelLabel_MS_Side:                return SoundIoChannelIdMsSide;

        case kAudioChannelLabel_XY_X:                   return SoundIoChannelIdXyX;
        case kAudioChannelLabel_XY_Y:                   return SoundIoChannelIdXyY;

        case kAudioChannelLabel_HeadphonesLeft:         return SoundIoChannelIdHeadphonesLeft;
        case kAudioChannelLabel_HeadphonesRight:        return SoundIoChannelIdHeadphonesRight;
        case kAudioChannelLabel_ClickTrack:             return SoundIoChannelIdClickTrack;
        case kAudioChannelLabel_ForeignLanguage:        return SoundIoChannelIdForeignLanguage;

        case kAudioChannelLabel_Discrete:               return SoundIoChannelIdAux;

        case kAudioChannelLabel_Discrete_0:             return SoundIoChannelIdAux0;
        case kAudioChannelLabel_Discrete_1:             return SoundIoChannelIdAux1;
        case kAudioChannelLabel_Discrete_2:             return SoundIoChannelIdAux2;
        case kAudioChannelLabel_Discrete_3:             return SoundIoChannelIdAux3;
        case kAudioChannelLabel_Discrete_4:             return SoundIoChannelIdAux4;
        case kAudioChannelLabel_Discrete_5:             return SoundIoChannelIdAux5;
        case kAudioChannelLabel_Discrete_6:             return SoundIoChannelIdAux6;
        case kAudioChannelLabel_Discrete_7:             return SoundIoChannelIdAux7;
        case kAudioChannelLabel_Discrete_8:             return SoundIoChannelIdAux8;
        case kAudioChannelLabel_Discrete_9:             return SoundIoChannelIdAux9;
        case kAudioChannelLabel_Discrete_10:            return SoundIoChannelIdAux10;
        case kAudioChannelLabel_Discrete_11:            return SoundIoChannelIdAux11;
        case kAudioChannelLabel_Discrete_12:            return SoundIoChannelIdAux12;
        case kAudioChannelLabel_Discrete_13:            return SoundIoChannelIdAux13;
        case kAudioChannelLabel_Discrete_14:            return SoundIoChannelIdAux14;
        case kAudioChannelLabel_Discrete_15:            return SoundIoChannelIdAux15;
    }
}

// See https://developer.apple.com/library/mac/documentation/MusicAudio/Reference/CoreAudioDataTypesRef/#//apple_ref/doc/constant_group/Audio_Channel_Layout_Tags
// Possible Errors:
// * SoundIoErrorIncompatibleDevice
// This does not handle all the possible layout enum values and it does not
// handle channel bitmaps.
static int from_coreaudio_layout(const AudioChannelLayout *ca_layout, struct SoundIoChannelLayout *layout) {
    switch (ca_layout->mChannelLayoutTag) {
    case kAudioChannelLayoutTag_UseChannelDescriptions:
    {
        layout->channel_count = soundio_int_min(
            SOUNDIO_MAX_CHANNELS,
            ca_layout->mNumberChannelDescriptions
        );
        for (int i = 0; i < layout->channel_count; i += 1) {
            layout->channels[i] = from_channel_descr(&ca_layout->mChannelDescriptions[i]);
        }
        break;
    }
    case kAudioChannelLayoutTag_UseChannelBitmap:
        return SoundIoErrorIncompatibleDevice;
    case kAudioChannelLayoutTag_Mono:
        layout->channel_count = 1;
        layout->channels[0] = SoundIoChannelIdFrontCenter;
        break;
    case kAudioChannelLayoutTag_Stereo:
    case kAudioChannelLayoutTag_StereoHeadphones:
    case kAudioChannelLayoutTag_MatrixStereo:
    case kAudioChannelLayoutTag_Binaural:
        layout->channel_count = 2;
        layout->channels[0] = SoundIoChannelIdFrontLeft;
        layout->channels[1] = SoundIoChannelIdFrontRight;
        break;
    case kAudioChannelLayoutTag_XY:
        layout->channel_count = 2;
        layout->channels[0] = SoundIoChannelIdXyX;
        layout->channels[1] = SoundIoChannelIdXyY;
        break;
    case kAudioChannelLayoutTag_MidSide:
        layout->channel_count = 2;
        layout->channels[0] = SoundIoChannelIdMsMid;
        layout->channels[1] = SoundIoChannelIdMsSide;
        break;
    case kAudioChannelLayoutTag_Ambisonic_B_Format:
        layout->channel_count = 4;
        layout->channels[0] = SoundIoChannelIdAmbisonicW;
        layout->channels[1] = SoundIoChannelIdAmbisonicX;
        layout->channels[2] = SoundIoChannelIdAmbisonicY;
        layout->channels[3] = SoundIoChannelIdAmbisonicZ;
        break;
    case kAudioChannelLayoutTag_Quadraphonic:
        layout->channel_count = 4;
        layout->channels[0] = SoundIoChannelIdFrontLeft;
        layout->channels[1] = SoundIoChannelIdFrontRight;
        layout->channels[2] = SoundIoChannelIdBackLeft;
        layout->channels[3] = SoundIoChannelIdBackRight;
        break;
    case kAudioChannelLayoutTag_Pentagonal:
        layout->channel_count = 5;
        layout->channels[0] = SoundIoChannelIdSideLeft;
        layout->channels[1] = SoundIoChannelIdSideRight;
        layout->channels[2] = SoundIoChannelIdBackLeft;
        layout->channels[3] = SoundIoChannelIdBackRight;
        layout->channels[4] = SoundIoChannelIdFrontCenter;
        break;
    case kAudioChannelLayoutTag_Hexagonal:
        layout->channel_count = 6;
        layout->channels[0] = SoundIoChannelIdFrontLeft;
        layout->channels[1] = SoundIoChannelIdFrontRight;
        layout->channels[2] = SoundIoChannelIdBackLeft;
        layout->channels[3] = SoundIoChannelIdBackRight;
        layout->channels[4] = SoundIoChannelIdFrontCenter;
        layout->channels[5] = SoundIoChannelIdBackCenter;
        break;
    case kAudioChannelLayoutTag_Octagonal:
        layout->channel_count = 8;
        layout->channels[0] = SoundIoChannelIdFrontLeft;
        layout->channels[1] = SoundIoChannelIdFrontRight;
        layout->channels[2] = SoundIoChannelIdBackLeft;
        layout->channels[3] = SoundIoChannelIdBackRight;
        layout->channels[4] = SoundIoChannelIdFrontCenter;
        layout->channels[5] = SoundIoChannelIdBackCenter;
        layout->channels[6] = SoundIoChannelIdSideLeft;
        layout->channels[7] = SoundIoChannelIdSideRight;
        break;
    case kAudioChannelLayoutTag_Cube:
        layout->channel_count = 8;
        layout->channels[0] = SoundIoChannelIdFrontLeft;
        layout->channels[1] = SoundIoChannelIdFrontRight;
        layout->channels[2] = SoundIoChannelIdBackLeft;
        layout->channels[3] = SoundIoChannelIdBackRight;
        layout->channels[4] = SoundIoChannelIdTopFrontLeft;
        layout->channels[5] = SoundIoChannelIdTopFrontRight;
        layout->channels[6] = SoundIoChannelIdTopBackLeft;
        layout->channels[7] = SoundIoChannelIdTopBackRight;
        break;
    default:
        return SoundIoErrorIncompatibleDevice;
    }
    soundio_channel_layout_detect_builtin(layout);
    return 0;
}

static bool all_channels_invalid(const struct SoundIoChannelLayout *layout) {
    for (int i = 0; i < layout->channel_count; i += 1) {
        if (layout->channels[i] != SoundIoChannelIdInvalid)
            return false;
    }
    return true;
}

static bool flag_in_format(AudioFormatFlags formatFlags, AudioFormatFlags flag) {
    return ((formatFlags & flag) == flag);
}

static enum SoundIoFormat from_ca_asbd(AudioStreamBasicDescription *desc) {
    uint32_t formatFlags = desc->mFormatFlags;
    if (flag_in_format(formatFlags, kAudioFormatFlagIsFloat)) {
        if (desc->mBitsPerChannel == 32) {
            return SoundIoFormatFloat32LE;
        }
        if (desc->mBitsPerChannel == 64) {
            return SoundIoFormatFloat64LE;
        }
    }
    if (flag_in_format(formatFlags, kAudioFormatFlagIsSignedInteger)) {
        if (desc->mBitsPerChannel == 16) {
            return SoundIoFormatS16LE;
        }
        if (desc->mBitsPerChannel == 24) {
            if (flag_in_format(formatFlags, kAudioFormatFlagIsPacked)) {
                return SoundIoFormatS24PLE;
            } else {
                return SoundIoFormatS24LE;
            }
        }
        if (desc->mBitsPerChannel == 32) {
            return SoundIoFormatS32LE;
        }
    }
    return SoundIoFormatInvalid;
}

static bool asbd_equal(AudioStreamBasicDescription *a, AudioStreamBasicDescription *b)
{
    return (a->mSampleRate == b->mSampleRate &&
    a->mFormatID == b->mFormatID &&
    a->mFormatFlags == b->mFormatFlags &&
    a->mBytesPerPacket == b->mBytesPerPacket &&
    a->mFramesPerPacket == b->mFramesPerPacket &&
    a->mBytesPerFrame == b->mBytesPerFrame &&
    a->mChannelsPerFrame == b->mChannelsPerFrame &&
    a->mBitsPerChannel == b->mBitsPerChannel);
}

struct RefreshDevices {
    struct SoundIoPrivate *si;
    struct SoundIoDevicesInfo *devices_info;
    int devices_size;
    AudioObjectID *devices;
    CFStringRef string_ref;
    char *device_name;
    int device_name_len;
    AudioBufferList *buffer_list;
    struct SoundIoDevice *device_shared;
    struct SoundIoDevice *device_raw;
    AudioChannelLayout *audio_channel_layout;
    char *device_uid;
    int device_uid_len;
    AudioValueRange *avr_array;
    bool ok;
};

static void deinit_refresh_devices(struct RefreshDevices *rd) {
    if (!rd->ok)
        unsubscribe_device_listeners(rd->si);
    soundio_destroy_devices_info(rd->devices_info);
    free(rd->devices);
    if (rd->string_ref)
        CFRelease(rd->string_ref);
    free(rd->device_name);
    free(rd->buffer_list);
    soundio_device_unref(rd->device_shared);
    soundio_device_unref(rd->device_raw);
    free(rd->audio_channel_layout);
    free(rd->device_uid);
    free(rd->avr_array);
}

#define STREAM_FORMAT_MSG(id, sfm) \
fprintf(stderr, "%i: [%i][%4.4s][%i][%i][%i][%i][%i][%i]\n", \
id, (UInt32)sfm.mSampleRate, (char *)&sfm.mFormatID, \
sfm.mFormatFlags, sfm.mBytesPerPacket, \
sfm.mFramesPerPacket, sfm.mBytesPerFrame, \
sfm.mChannelsPerFrame, sfm.mBitsPerChannel)

static int get_hardware_streams_for_device(AudioDeviceID device_id, enum SoundIoDeviceAim aim, AudioStreamID **streams) {
    AudioObjectPropertyAddress prop_address;

    prop_address.mSelector = kAudioDevicePropertyStreams;
    prop_address.mScope = aim_to_scope(aim);
    prop_address.mElement = kAudioObjectPropertyElementMaster;

    OSStatus os_err;
    UInt32 io_size;

    if ((os_err = AudioObjectGetPropertyDataSize(device_id, &prop_address, 0, NULL, &io_size))) {
        return -1;
    }

    AudioStreamID *hardware_stream_ids = (AudioStreamID *)ALLOCATE_NONZERO(char, io_size);

    if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, NULL, &io_size, hardware_stream_ids)))
    {
        free(hardware_stream_ids);
        return -1;
    }
    *streams = hardware_stream_ids;
    int stream_count = io_size / sizeof(AudioStreamID);
    return stream_count;
}

static int get_asrd_count_for_stream(AudioStreamID stream_id, bool virtual) {
    AudioObjectPropertyAddress prop_address;

    prop_address.mSelector = virtual ? kAudioStreamPropertyAvailableVirtualFormats : kAudioStreamPropertyAvailablePhysicalFormats;
    prop_address.mScope = kAudioObjectPropertyScopeGlobal;
    prop_address.mElement = kAudioObjectPropertyElementMaster;

    OSStatus os_err;
    UInt32 io_size;

    if ((os_err = AudioObjectGetPropertyDataSize(stream_id, &prop_address, 0, NULL, &io_size))) {
        return -1;
    }
    int format_count = io_size / sizeof(AudioStreamRangedDescription);
    return format_count;
}

static int get_asrds_for_stream (AudioStreamID stream_id, int format_count, bool virtual, AudioStreamRangedDescription **asrds) {

    AudioObjectPropertyAddress prop_address;

    prop_address.mSelector = virtual ? kAudioStreamPropertyAvailableVirtualFormats : kAudioStreamPropertyAvailablePhysicalFormats;
    prop_address.mScope = kAudioObjectPropertyScopeGlobal;
    prop_address.mElement = kAudioObjectPropertyElementMaster;

    int asrd_count = format_count;
    if (asrd_count <= 0) {
        return 0;
    }

    OSStatus os_err;
    UInt32 io_size = format_count * sizeof(AudioStreamRangedDescription);

    if ((os_err = AudioObjectGetPropertyData(stream_id, &prop_address, 0, NULL, &io_size, *asrds)))
    {
        free(asrds);
        return 0;
    }
    assert(asrd_count == io_size / sizeof(AudioStreamRangedDescription));
    return asrd_count;
}

struct StreamFormat {
    UInt32 stream_id;
    AudioStreamRangedDescription asrd;
    bool physical_integer_match;
};

static int get_stream_formats_for_device (AudioDeviceID device_id, enum SoundIoDeviceAim aim, struct StreamFormat **result) {

    AudioStreamID *stream_ids;
    int stream_count = get_hardware_streams_for_device(device_id, aim, &stream_ids);

    AudioStreamID stream_id;
    int *virtual_asrd_counts = (int *)ALLOCATE_NONZERO(int, stream_count);
    int virtual_asrd_count = 0;

    for (int i = 0; i < stream_count; i++) {
        stream_id = stream_ids[i];
        virtual_asrd_counts[i] = get_asrd_count_for_stream(stream_id, true);
        virtual_asrd_count += virtual_asrd_counts[i];
    }

    int stream_format_count = 0;
    struct StreamFormat *stream_formats = (struct StreamFormat *)ALLOCATE(struct StreamFormat, virtual_asrd_count);

    int stream_asrd_count;
    struct StreamFormat *stream_format;

    for (int i = 0; i < stream_count; i++) {
        stream_id = stream_ids[i];
        stream_asrd_count = virtual_asrd_counts[i];
        AudioStreamRangedDescription *virtual_asrds = (AudioStreamRangedDescription *)ALLOCATE_NONZERO(AudioStreamRangedDescription, stream_asrd_count);

        get_asrds_for_stream(stream_id, stream_asrd_count, true, &virtual_asrds);

        for (int k = 0; k < stream_asrd_count; k++) {
            stream_format = &stream_formats[stream_format_count++];
            stream_format->stream_id = stream_id;
            memcpy(&stream_format->asrd, &virtual_asrds[k], sizeof(AudioStreamRangedDescription));
            stream_format->physical_integer_match = false;
        }

        free(virtual_asrds);
    }
    assert(stream_format_count == virtual_asrd_count);

    int *physical_asrd_counts = (int *)ALLOCATE_NONZERO(int, stream_count);

    for (int i = 0; i < stream_count; i++) {
        stream_id = stream_ids[i];
        physical_asrd_counts[i] = get_asrd_count_for_stream(stream_id, false);
    }

    AudioStreamRangedDescription *physical_asrd;
    for (int i = 0; i < stream_count; i++) {
        stream_id = stream_ids[i];
        stream_asrd_count = physical_asrd_counts[i];
        AudioStreamRangedDescription *physical_asrds = (AudioStreamRangedDescription *)ALLOCATE_NONZERO(AudioStreamRangedDescription, stream_asrd_count);
        get_asrds_for_stream(stream_id, stream_asrd_count, false, &physical_asrds);

        for (int j = 0; j < stream_asrd_count; j++) {
            physical_asrd = &physical_asrds[j];

            for (int k = 0; k < stream_format_count; k++) {
            // check for integer match
            stream_format = &stream_formats[k];
            if (asbd_equal(&stream_format->asrd.mFormat, &physical_asrd->mFormat) &&
                stream_format->asrd.mFormat.mFormatID == kAudioFormatLinearPCM &&
                flag_in_format(stream_format->asrd.mFormat.mFormatFlags, kAudioFormatFlagIsSignedInteger) &&
                !flag_in_format(stream_format->asrd.mFormat.mFormatFlags, kAudioFormatFlagIsFloat))
                {
                    stream_format->physical_integer_match = true;
                }
            }
        }
        free(physical_asrds);
    }

    *result = stream_formats;
    return stream_format_count;
}

static int refresh_devices(struct SoundIoPrivate *si) {
    struct SoundIo *soundio = &si->pub;
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    UInt32 io_size;
    OSStatus os_err;
    int err;

    unsubscribe_device_listeners(si);

    struct RefreshDevices rd = {0};
    rd.si = si;

    if (!(rd.devices_info = ALLOCATE(struct SoundIoDevicesInfo, 1))) {
        deinit_refresh_devices(&rd);
        return SoundIoErrorNoMem;
    }

    AudioObjectPropertyAddress prop_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    if ((os_err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
        &prop_address, 0, NULL, &io_size)))
    {
        deinit_refresh_devices(&rd);
        return SoundIoErrorOpeningDevice;
    }

    AudioObjectID default_input_id;
    AudioObjectID default_output_id;

    int device_count = io_size / (UInt32)sizeof(AudioObjectID);
    if (device_count >= 1) {
        rd.devices_size = io_size;
        rd.devices = (AudioObjectID *)ALLOCATE(char, rd.devices_size);
        if (!rd.devices) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorNoMem;
        }

        if ((os_err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_address, 0, NULL,
            &io_size, rd.devices)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }


        io_size = sizeof(AudioObjectID);
        prop_address.mSelector = kAudioHardwarePropertyDefaultInputDevice;
        if ((os_err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_address,
            0, NULL, &io_size, &default_input_id)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        io_size = sizeof(AudioObjectID);
        prop_address.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        if ((os_err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_address,
            0, NULL, &io_size, &default_output_id)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
    }

    for (int device_i = 0; device_i < device_count; device_i += 1) {
        AudioObjectID device_id = rd.devices[device_i];

        for (int i = 0; i < ARRAY_LENGTH(device_listen_props); i += 1) {
            if ((err = SoundIoListAudioDeviceID_add_one(&sica->registered_listeners))) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }
            if ((os_err = AudioObjectAddPropertyListener(device_id, &device_listen_props[i],
                on_devices_changed, si)))
            {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }
            AudioDeviceID *last_device_id = SoundIoListAudioDeviceID_last_ptr(&sica->registered_listeners);
            *last_device_id = device_id;
        }

        prop_address.mSelector = kAudioObjectPropertyName;
        prop_address.mScope = kAudioObjectPropertyScopeGlobal;
        prop_address.mElement = kAudioObjectPropertyElementMaster;
        io_size = sizeof(CFStringRef);
        if (rd.string_ref) {
            CFRelease(rd.string_ref);
            rd.string_ref = NULL;
        }
        if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address,
            0, NULL, &io_size, &rd.string_ref)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        free(rd.device_name);
        rd.device_name = NULL;
        if ((err = from_cf_string(rd.string_ref, &rd.device_name, &rd.device_name_len))) {
            deinit_refresh_devices(&rd);
            return err;
        }

        prop_address.mSelector = kAudioDevicePropertyDeviceUID;
        prop_address.mScope = kAudioObjectPropertyScopeGlobal;
        prop_address.mElement = kAudioObjectPropertyElementMaster;
        io_size = sizeof(CFStringRef);
        if (rd.string_ref) {
            CFRelease(rd.string_ref);
            rd.string_ref = NULL;
        }
        if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address,
            0, NULL, &io_size, &rd.string_ref)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        free(rd.device_uid);
        rd.device_uid = NULL;
        if ((err = from_cf_string(rd.string_ref, &rd.device_uid, &rd.device_uid_len))) {
            deinit_refresh_devices(&rd);
            return err;
        }


        for (int aim_i = 0; aim_i < ARRAY_LENGTH(aims); aim_i += 1) {
            enum SoundIoDeviceAim aim = aims[aim_i];

            io_size = 0;
            prop_address.mSelector = kAudioDevicePropertyStreamConfiguration;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            if ((os_err = AudioObjectGetPropertyDataSize(device_id, &prop_address, 0, NULL, &io_size))) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }

            free(rd.buffer_list);
            rd.buffer_list = (AudioBufferList*)ALLOCATE_NONZERO(char, io_size);
            if (!rd.buffer_list) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorNoMem;
            }

            if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, NULL,
                &io_size, rd.buffer_list)))
            {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }

            int channel_count = 0;
            for (int i = 0; i < rd.buffer_list->mNumberBuffers; i += 1) {
                channel_count += rd.buffer_list->mBuffers[i].mNumberChannels;
            }

            if (channel_count <= 0)
                continue;

            struct SoundIoDevicePrivate *dev_shared = ALLOCATE(struct SoundIoDevicePrivate, 1);
            if (!dev_shared) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorNoMem;
            }

            struct SoundIoDeviceCoreAudio *dca_shared = &dev_shared->backend_data.coreaudio;
            dca_shared->device_id = device_id;
            assert(!rd.device_shared);
            rd.device_shared = &dev_shared->pub;
            rd.device_shared->ref_count = 1;
            rd.device_shared->soundio = soundio;
            rd.device_shared->is_raw = false;
            rd.device_shared->aim = aim;
            rd.device_shared->id = soundio_str_dupe(rd.device_uid, rd.device_uid_len);
            rd.device_shared->name = soundio_str_dupe(rd.device_name, rd.device_name_len);

            if (!rd.device_shared->id || !rd.device_shared->name) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorNoMem;
            }

            struct SoundIoDevicePrivate *dev_raw = ALLOCATE(struct SoundIoDevicePrivate, 1);
            if (!dev_raw) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorNoMem;
            }

            struct SoundIoDeviceCoreAudio *dca_raw = &dev_raw->backend_data.coreaudio;
            dca_raw->device_id = device_id;
            assert(!rd.device_raw);
            rd.device_raw = &dev_raw->pub;
            rd.device_raw->ref_count = 1;
            rd.device_raw->soundio = soundio;
            rd.device_raw->is_raw = true;
            rd.device_raw->aim = aim;
            rd.device_raw->id = soundio_str_dupe(rd.device_uid, rd.device_uid_len);
            rd.device_raw->name = soundio_str_dupe(rd.device_name, rd.device_name_len);

            if (!rd.device_raw->id || !rd.device_raw->name) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorNoMem;
            }

            prop_address.mSelector = kAudioDevicePropertyPreferredChannelLayout;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            if (!(os_err = AudioObjectGetPropertyDataSize(device_id, &prop_address,
                0, NULL, &io_size)))
            {
                rd.audio_channel_layout = (AudioChannelLayout *)ALLOCATE(char, io_size);
                if (!rd.audio_channel_layout) {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorNoMem;
                }
                if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, NULL,
                    &io_size, rd.audio_channel_layout)))
                {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorOpeningDevice;
                }
                if ((err = from_coreaudio_layout(rd.audio_channel_layout, &rd.device_shared->current_layout))) {
                    rd.device_shared->current_layout.channel_count = channel_count;
                }
                if ((err = from_coreaudio_layout(rd.audio_channel_layout, &rd.device_raw->current_layout))) {
                    rd.device_raw->current_layout.channel_count = channel_count;
                }
            }
            if (all_channels_invalid(&rd.device_shared->current_layout)) {
                const struct SoundIoChannelLayout *guessed_layout =
                    soundio_channel_layout_get_default(channel_count);
                if (guessed_layout)
                    rd.device_shared->current_layout = *guessed_layout;
            }

            if (all_channels_invalid(&rd.device_raw->current_layout)) {
                const struct SoundIoChannelLayout *guessed_layout =
                    soundio_channel_layout_get_default(channel_count);
                if (guessed_layout)
                    rd.device_raw->current_layout = *guessed_layout;
            }

            rd.device_shared->layout_count = 1;
            rd.device_shared->layouts = &rd.device_shared->current_layout;

            rd.device_raw->layout_count = 1;
            rd.device_raw->layouts = &rd.device_raw->current_layout;

            rd.device_shared->format_count = 4;
            rd.device_shared->formats = ALLOCATE(enum SoundIoFormat, rd.device_shared->format_count);
            if (!rd.device_shared->formats)
                return SoundIoErrorNoMem;
            rd.device_shared->formats[0] = SoundIoFormatS16LE;
            rd.device_shared->formats[1] = SoundIoFormatS32LE;
            rd.device_shared->formats[2] = SoundIoFormatFloat32LE;
            rd.device_shared->formats[3] = SoundIoFormatFloat64LE;

            prop_address.mSelector = kAudioDevicePropertyNominalSampleRate;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            io_size = sizeof(double);
            double value;
            if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, NULL,
                &io_size, &value)))
            {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }
            double floored_value = floor(value);
            if (value != floored_value) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorIncompatibleDevice;
            }
            rd.device_shared->sample_rate_current = (int)floored_value;

            // If you try to open an input stream with anything but the current
            // nominal sample rate, AudioUnitRender returns an error.
            if (aim == SoundIoDeviceAimInput) {
                rd.device_shared->sample_rate_count = 1;
                rd.device_shared->sample_rates = &dev_shared->prealloc_sample_rate_range;
                rd.device_shared->sample_rates[0].min = rd.device_shared->sample_rate_current;
                rd.device_shared->sample_rates[0].max = rd.device_shared->sample_rate_current;
            } else {
                prop_address.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
                prop_address.mScope = aim_to_scope(aim);
                prop_address.mElement = kAudioObjectPropertyElementMaster;
                if ((os_err = AudioObjectGetPropertyDataSize(device_id, &prop_address, 0, NULL,
                    &io_size)))
                {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorOpeningDevice;
                }
                int avr_array_len = io_size / sizeof(AudioValueRange);
                rd.avr_array = (AudioValueRange*)ALLOCATE(char, io_size);

                if (!rd.avr_array) {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorNoMem;
                }

                if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, NULL,
                    &io_size, rd.avr_array)))
                {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorOpeningDevice;
                }

                if (avr_array_len == 1) {
                    rd.device_shared->sample_rate_count = 1;
                    rd.device_shared->sample_rates = &dev_shared->prealloc_sample_rate_range;
                    rd.device_shared->sample_rates[0].min = ceil_dbl_to_int(rd.avr_array[0].mMinimum);
                    rd.device_shared->sample_rates[0].max = (int)(rd.avr_array[0].mMaximum);
                } else {
                    rd.device_shared->sample_rate_count = avr_array_len;
                    rd.device_shared->sample_rates = ALLOCATE(struct SoundIoSampleRateRange, avr_array_len);
                    if (!rd.device_shared->sample_rates) {
                        deinit_refresh_devices(&rd);
                        return SoundIoErrorNoMem;
                    }
                    for (int i = 0; i < avr_array_len; i += 1) {
                        AudioValueRange *avr = &rd.avr_array[i];
                        int min_val = ceil_dbl_to_int(avr->mMinimum);
                        int max_val = (int)(avr->mMaximum);
                        rd.device_shared->sample_rates[i].min = min_val;
                        rd.device_shared->sample_rates[i].max = max_val;
                    }
                }
            }

            // raw
            struct StreamFormat *stream_formats;
            UInt32 stream_format_count = get_stream_formats_for_device(device_id, aim, &stream_formats);

            int unique_sample_rates_count = 0;
            AudioValueRange *unique_sample_rates = ALLOCATE(AudioValueRange, stream_format_count);

            int unique_formats_count = 0;
            enum SoundIoFormat *unique_formats = ALLOCATE(enum SoundIoFormat, stream_format_count);

            bool is_unique;

            AudioStreamRangedDescription asrd;

            for (int i = 0; i < stream_format_count; i++) {

                asrd = stream_formats[i].asrd;
                is_unique = true;

                for (int j = 0; j < unique_sample_rates_count; j++) {
                    if (unique_sample_rates[j].mMinimum == asrd.mSampleRateRange.mMinimum &&
                        unique_sample_rates[j].mMaximum == asrd.mSampleRateRange.mMaximum) {
                        is_unique = false;
                        break;
                    }
                }
                if (is_unique) {
                    unique_sample_rates[unique_sample_rates_count++] = asrd.mSampleRateRange;
                }

                enum SoundIoFormat asrd_format = from_ca_asbd(&asrd.mFormat);
                is_unique = true;

                for (int j = 0; j < unique_formats_count; j++) {
                    if (unique_formats[j] == asrd_format) {
                        is_unique = false;
                        break;
                    }
                }

                if (is_unique) {
                    unique_formats[unique_formats_count++] = asrd_format;
                }
            }

            free(stream_formats);

            if (unique_sample_rates_count == 1) {
                rd.device_raw->sample_rate_count = 1;
                rd.device_raw->sample_rates = &dev_raw->prealloc_sample_rate_range;
                rd.device_raw->sample_rates[0].min = ceil_dbl_to_int(unique_sample_rates[0].mMinimum);
                rd.device_raw->sample_rates[0].max = (int)(unique_sample_rates[0].mMaximum);
            } else {
                rd.device_raw->sample_rate_count = unique_sample_rates_count;
                rd.device_raw->sample_rates = ALLOCATE(struct SoundIoSampleRateRange, unique_sample_rates_count);
                if (!rd.device_raw->sample_rates) {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorNoMem;
                }

                for (int i = 0; i < unique_sample_rates_count; i += 1) {
                    AudioValueRange *avr = &unique_sample_rates[i];
                    int min_val = ceil_dbl_to_int(avr->mMinimum);
                    int max_val = (int)(avr->mMaximum);
                    rd.device_raw->sample_rates[i].min = min_val;
                    rd.device_raw->sample_rates[i].max = max_val;
                }
            }
            rd.device_raw->format_count = unique_formats_count;
            rd.device_raw->formats = unique_formats;

            prop_address.mSelector = kAudioDevicePropertyBufferFrameSize;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            io_size = sizeof(UInt32);
            UInt32 buffer_frame_size;
            if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, NULL,
                &io_size, &buffer_frame_size)))
            {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }
            double use_sample_rate = rd.device_shared->sample_rate_current;
            rd.device_shared->software_latency_current = buffer_frame_size / use_sample_rate;
            rd.device_raw->software_latency_current = buffer_frame_size / use_sample_rate;

            prop_address.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            io_size = sizeof(AudioValueRange);
            AudioValueRange avr;
            if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, NULL,
                &io_size, &avr)))
            {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }
            rd.device_shared->software_latency_min = avr.mMinimum / use_sample_rate;
            rd.device_raw->software_latency_min = avr.mMinimum / use_sample_rate;
            rd.device_shared->software_latency_max = avr.mMaximum / use_sample_rate;
            rd.device_raw->software_latency_max = avr.mMaximum / use_sample_rate;

            prop_address.mSelector = kAudioDevicePropertyLatency;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            io_size = sizeof(UInt32);
            if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, NULL,
                &io_size, &dca_shared->latency_frames)))
            {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }
            dca_raw->latency_frames = dca_shared->latency_frames;

            struct SoundIoListDevicePtr *device_list;
            if (rd.device_shared->aim == SoundIoDeviceAimOutput) {
                device_list = &rd.devices_info->output_devices;
                if (device_id == default_output_id)
                    rd.devices_info->default_output_index = device_list->length;
            } else {
                assert(rd.device_shared->aim == SoundIoDeviceAimInput);
                device_list = &rd.devices_info->input_devices;
                if (device_id == default_input_id)
                    rd.devices_info->default_input_index = device_list->length;
            }

            if ((err = SoundIoListDevicePtr_append(device_list, rd.device_shared))) {
                deinit_refresh_devices(&rd);
                return err;
            }

            if ((err = SoundIoListDevicePtr_append(device_list, rd.device_raw))) {
                deinit_refresh_devices(&rd);
                return err;
            }

            rd.device_shared = NULL;
            rd.device_raw = NULL;
        }
    }

    soundio_os_mutex_lock(sica->mutex);
    soundio_destroy_devices_info(sica->ready_devices_info);
    sica->ready_devices_info = rd.devices_info;
    soundio_os_mutex_unlock(sica->mutex);

    rd.devices_info = NULL;
    rd.ok = true;
    deinit_refresh_devices(&rd);

    return 0;
}

static void shutdown_backend(struct SoundIoPrivate *si, int err) {
    struct SoundIo *soundio = &si->pub;
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    soundio_os_mutex_lock(sica->mutex);
    sica->shutdown_err = err;
    SOUNDIO_ATOMIC_STORE(sica->have_devices_flag, true);
    soundio_os_mutex_unlock(sica->mutex);
    soundio_os_cond_signal(sica->cond, NULL);
    soundio_os_cond_signal(sica->have_devices_cond, NULL);
    soundio->on_events_signal(soundio);
}

static void flush_events_ca(struct SoundIoPrivate *si) {
    struct SoundIo *soundio = &si->pub;
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    // block until have devices
    while (!SOUNDIO_ATOMIC_LOAD(sica->have_devices_flag))
        soundio_os_cond_wait(sica->have_devices_cond, NULL);

    bool change = false;
    bool cb_shutdown = false;
    struct SoundIoDevicesInfo *old_devices_info = NULL;

    soundio_os_mutex_lock(sica->mutex);

    if (sica->shutdown_err && !sica->emitted_shutdown_cb) {
        sica->emitted_shutdown_cb = true;
        cb_shutdown = true;
    } else if (sica->ready_devices_info) {
        old_devices_info = si->safe_devices_info;
        si->safe_devices_info = sica->ready_devices_info;
        sica->ready_devices_info = NULL;
        change = true;
    }

    soundio_os_mutex_unlock(sica->mutex);

    if (cb_shutdown)
        soundio->on_backend_disconnect(soundio, sica->shutdown_err);
    else if (change)
        soundio->on_devices_change(soundio);

    soundio_destroy_devices_info(old_devices_info);
}

static void wait_events_ca(struct SoundIoPrivate *si) {
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    flush_events_ca(si);
    soundio_os_cond_wait(sica->cond, NULL);
}

static void wakeup_ca(struct SoundIoPrivate *si) {
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    soundio_os_cond_signal(sica->cond, NULL);
}

static void force_device_scan_ca(struct SoundIoPrivate *si) {
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    SOUNDIO_ATOMIC_STORE(sica->device_scan_queued, true);
    soundio_os_cond_signal(sica->scan_devices_cond, NULL);
}

static void device_thread_run(void *arg) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)arg;
    struct SoundIo *soundio = &si->pub;
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    int err;

    for (;;) {
        if (!SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(sica->abort_flag))
            break;
        if (SOUNDIO_ATOMIC_LOAD(sica->service_restarted)) {
            shutdown_backend(si, SoundIoErrorBackendDisconnected);
            return;
        }
        if (SOUNDIO_ATOMIC_EXCHANGE(sica->device_scan_queued, false)) {
            err = refresh_devices(si);
            if (err) {
                shutdown_backend(si, err);
                return;
            }
            if (!SOUNDIO_ATOMIC_EXCHANGE(sica->have_devices_flag, true))
                soundio_os_cond_signal(sica->have_devices_cond, NULL);
            soundio_os_cond_signal(sica->cond, NULL);
            soundio->on_events_signal(soundio);
        }
        soundio_os_cond_wait(sica->scan_devices_cond, NULL);
    }
}

static void outstream_destroy_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os);

static OSStatus on_physical_format_changed(AudioObjectID in_object_id, UInt32 in_number_addresses,
    const AudioObjectPropertyAddress in_addresses[], void *in_client_data)
{
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *)in_client_data;
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoDevice *device = outstream->device;
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)device->soundio;


    for (int i = 0; i < in_number_addresses; i++)
    {
        if (in_addresses[i].mSelector == kAudioStreamPropertyVirtualFormat)
        {
            // Hardware physical format has changed.
            AudioStreamBasicDescription new_hardware_format;
            UInt32 io_size = sizeof(AudioStreamBasicDescription);

            OSStatus os_err;
            if ((os_err = AudioObjectGetPropertyData(osca->raw_stream_id, &in_addresses[i], 0, NULL, &io_size, &new_hardware_format))) {
                return os_err;
            }

            SOUNDIO_ATOMIC_STORE(osca->output_format_match, asbd_equal(&new_hardware_format, &osca->hardware_format));
            if (SOUNDIO_ATOMIC_LOAD(osca->output_format_match) == false) {
                outstream_destroy_ca(si, os);
                return SoundIoErrorOpeningDevice;
            }
        }
    }
    return noErr;
}

static OSStatus on_outstream_device_overload(AudioObjectID in_object_id, UInt32 in_number_addresses,
    const AudioObjectPropertyAddress in_addresses[], void *in_client_data)
{
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *)in_client_data;
    struct SoundIoOutStream *outstream = &os->pub;
    outstream->underflow_callback(outstream);
    return noErr;
}

static void outstream_destroy_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoDevice *device = outstream->device;
    struct SoundIoDevicePrivate *dev = (struct SoundIoDevicePrivate *)device;
    struct SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;

    AudioObjectPropertyAddress prop_address = {
        kAudioDeviceProcessorOverload,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    AudioObjectRemovePropertyListener(dca->device_id, &prop_address, on_outstream_device_overload, os);

    if (osca->instance) {
        AudioOutputUnitStop(osca->instance);
        AudioComponentInstanceDispose(osca->instance);
        osca->instance = NULL;
    }

    if (osca->io_proc_id) {

        prop_address.mSelector = kAudioStreamPropertyVirtualFormat;
        prop_address.mScope = kAudioObjectPropertyScopeGlobal;
        prop_address.mElement = kAudioObjectPropertyElementMaster;

        AudioObjectRemovePropertyListener(dca->device_id, &prop_address, on_physical_format_changed, os);

        AudioDeviceStop(dca->device_id, osca->io_proc_id);
        AudioDeviceDestroyIOProcID(dca->device_id, osca->io_proc_id);

        uint32_t io_size = sizeof(AudioStreamBasicDescription);

        if (osca->revert_format) {
            AudioObjectSetPropertyData(osca->raw_stream_id, &prop_address, 0, NULL, io_size, &osca->previous_hardware_format);
        }

        osca->io_proc_id = NULL;

        // unhog device
        prop_address.mElement = kAudioObjectPropertyElementMaster;
        prop_address.mScope = kAudioObjectPropertyScopeGlobal;
        prop_address.mSelector = kAudioDevicePropertyHogMode;

        io_size = sizeof(pid_t);

        pid_t hogmode_pid;
        AudioObjectGetPropertyData(dca->device_id, &prop_address, 0, NULL, &io_size, &hogmode_pid);
    }
}

static OSStatus write_callback_ca_shared(void *userdata, AudioUnitRenderActionFlags *io_action_flags,
    const AudioTimeStamp *in_time_stamp, UInt32 in_bus_number, UInt32 in_number_frames,
    AudioBufferList *io_data)
{
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *) userdata;
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;

    osca->io_data = io_data;
    osca->buffer_index = 0;
    osca->frames_left = in_number_frames;
    outstream->write_callback(outstream, osca->frames_left, osca->frames_left);
    osca->io_data = NULL;

    return noErr;
}

static OSStatus write_callback_ca_raw(AudioDeviceID inDevice, const AudioTimeStamp *inNow, const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime, AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime, void *inClientData)
{
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *) inClientData;
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;

    osca->io_data = outOutputData;
    osca->buffer_index = 0;
    osca->frames_left = outOutputData->mBuffers[0].mDataByteSize / osca->hardware_format.mBytesPerFrame;

    outstream->write_callback(outstream, osca->frames_left, osca->frames_left);

    osca->io_data = NULL;
    return noErr;
}

static int set_ca_desc(enum SoundIoFormat fmt, AudioStreamBasicDescription *desc) {
    switch (fmt) {
    case SoundIoFormatFloat32LE:
        desc->mFormatFlags = kAudioFormatFlagIsFloat;
        desc->mBitsPerChannel = 32;
        break;
    case SoundIoFormatFloat64LE:
        desc->mFormatFlags = kAudioFormatFlagIsFloat;
        desc->mBitsPerChannel = 64;
        break;
    case SoundIoFormatS32LE:
        desc->mFormatFlags = kAudioFormatFlagIsSignedInteger;
        desc->mBitsPerChannel = 32;
        break;
    case SoundIoFormatS16LE:
        desc->mFormatFlags = kAudioFormatFlagIsSignedInteger;
        desc->mBitsPerChannel = 16;
        break;
    case SoundIoFormatS24LE:
        desc->mFormatFlags = kAudioFormatFlagIsSignedInteger;
        desc->mBitsPerChannel = 24;
        break;
    case SoundIoFormatS24PLE:
        desc->mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
        desc->mBitsPerChannel = 24;
    default:
        return SoundIoErrorIncompatibleDevice;
    }
    return 0;
}

static int outstream_open_ca_raw(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os)
{
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoDevice *device = outstream->device;
    struct SoundIoDevicePrivate *dev = (struct SoundIoDevicePrivate *)device;
    struct SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;

    OSStatus os_err;
    if ((os_err = AudioDeviceCreateIOProcID(dca->device_id, write_callback_ca_raw, outstream, &osca->io_proc_id)))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    assert(osca->io_proc_id != NULL);

    if (outstream->software_latency == 0.0)
        outstream->software_latency = device->software_latency_current;

    outstream->software_latency = soundio_double_clamp(
            device->software_latency_min,
            outstream->software_latency,
            device->software_latency_max);

    AudioObjectPropertyAddress prop_address;
    UInt32 io_size;

    // hog device
    prop_address.mElement = kAudioObjectPropertyElementMaster;
    prop_address.mScope = kAudioObjectPropertyScopeGlobal;
    prop_address.mSelector = kAudioDevicePropertyHogMode;

    pid_t pid = getpid();
	pid_t hogmode_pid, current_pid = -1;
    io_size = sizeof(pid_t);

    if ((os_err = AudioObjectGetPropertyData(dca->device_id, &prop_address, 0, NULL, &io_size, &hogmode_pid)))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if (hogmode_pid != pid) {
        if (hogmode_pid != -1) {
            // device is exclusively in use by another process
            outstream_destroy_ca(si, os);
            return SoundIoErrorOpeningDevice;
        }
        if ((os_err = AudioObjectSetPropertyData(dca->device_id, &prop_address, 0, NULL, io_size, &hogmode_pid)))
        {
            outstream_destroy_ca(si, os);
            return SoundIoErrorOpeningDevice;
        }
    }

    if ((os_err = AudioObjectGetPropertyData(dca->device_id, &prop_address, 0, NULL, &io_size, &current_pid)))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if (current_pid != pid) {
        // Could not hog device
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    // Get available formats
    prop_address.mSelector = kAudioDevicePropertyStreams;
    prop_address.mScope = kAudioObjectPropertyScopeGlobal;
    prop_address.mElement = kAudioObjectPropertyElementMaster;

    if ((os_err = AudioObjectGetPropertyDataSize(dca->device_id, &prop_address, 0, NULL, &io_size)))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    AudioStreamID *hardware_stream_ids = (AudioStreamID *)ALLOCATE(char, io_size);

    if ((os_err = AudioObjectGetPropertyData(dca->device_id, &prop_address, 0, NULL, &io_size, hardware_stream_ids)))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    int stream_count = io_size / sizeof(AudioStreamID);
    osca->raw_stream_id = -1;

    for (int i = 0; i < stream_count; i++) {

        if (osca->raw_stream_id != -1) {
            break;
        }
        AudioStreamID stream_id = hardware_stream_ids[i];

        prop_address.mSelector = kAudioStreamPropertyDirection;
        prop_address.mScope = kAudioObjectPropertyScopeGlobal;
        prop_address.mElement = kAudioObjectPropertyElementMaster;


        io_size = sizeof(uint32_t);
        uint32_t direction = -1;

        if ((os_err = AudioObjectGetPropertyData(stream_id, &prop_address, 0, NULL, &io_size, &direction)))
        {
            outstream_destroy_ca(si, os);
            return SoundIoErrorOpeningDevice;
        }

        // @constant       kAudioStreamPropertyDirection
        //        A UInt32 where a value of 0 means that this AudioStream is an output stream
        //        and a value of 1 means that it is an input stream.
        if (device->aim == direction) {
            continue;
        }

        prop_address.mSelector = kAudioStreamPropertyAvailableVirtualFormats;
        prop_address.mScope = kAudioObjectPropertyScopeGlobal;
        prop_address.mElement = kAudioObjectPropertyElementMaster;

        if ((os_err = AudioObjectGetPropertyDataSize(stream_id, &prop_address, 0, NULL, &io_size)))
        {
            outstream_destroy_ca(si, os);
            return SoundIoErrorOpeningDevice;
        }

        AudioStreamRangedDescription *asrds = (AudioStreamRangedDescription *)ALLOCATE(char, io_size);
        int asrd_count = io_size / sizeof(AudioStreamRangedDescription);

        if ((os_err = AudioObjectGetPropertyData(stream_id, &prop_address, 0, NULL, &io_size, asrds)))
        {
            outstream_destroy_ca(si, os);
            return SoundIoErrorOpeningDevice;
        }

        // Select best
        for (int j = 0; j < asrd_count; j++)
        {
            AudioStreamRangedDescription asrd = asrds[j];
            enum SoundIoFormat asrd_format = from_ca_asbd(&asrd.mFormat);
            if (asrd_format == outstream->format && asrd.mFormat.mSampleRate == outstream->sample_rate) {
                osca->raw_stream_id = stream_id;
                memcpy(&osca->hardware_format, &asrd.mFormat, sizeof(AudioStreamBasicDescription));
                break;
            }
        }
        free(asrds);
    }
    free(hardware_stream_ids);

    if (osca->raw_stream_id == -1)
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    // check buffer size is acceptable
    prop_address.mSelector = kAudioDevicePropertyBufferFrameSize;
    prop_address.mScope = kAudioObjectPropertyScopeGlobal;
    prop_address.mElement = kAudioObjectPropertyElementMaster;

    io_size = sizeof(UInt32);
    UInt32 buffer_frame_size;
    if ((os_err = AudioObjectGetPropertyData(dca->device_id, &prop_address, 0, NULL, &io_size, &buffer_frame_size)))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if (buffer_frame_size % osca->hardware_format.mBytesPerFrame != 0) {
        buffer_frame_size = (osca->hardware_format.mBytesPerFrame == 24 ? 768 : 512);

        if ((os_err = AudioObjectSetPropertyData(dca->device_id, &prop_address, 0, NULL, io_size, &buffer_frame_size)))
        {
            outstream_destroy_ca(si, os);
            return SoundIoErrorOpeningDevice;
        }
    }

    // get current
    prop_address.mSelector = kAudioStreamPropertyVirtualFormat;
    prop_address.mScope = kAudioDevicePropertyScopeOutput;
    prop_address.mElement = kAudioObjectPropertyElementMaster;


    io_size = sizeof(AudioStreamBasicDescription);

    if ((os_err = AudioObjectGetPropertyData(osca->raw_stream_id, &prop_address, 0, NULL, &io_size, &osca->previous_hardware_format)))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if (asbd_equal(&osca->previous_hardware_format, &osca->hardware_format))
    {
        osca->revert_format = false;
        SOUNDIO_ATOMIC_STORE(osca->output_format_match, true);
    } else {
        osca->revert_format = true;
        SOUNDIO_ATOMIC_STORE(osca->output_format_match, false);
    }

    // Listen to physical format changes
    prop_address.mSelector = kAudioStreamPropertyVirtualFormat;
    prop_address.mScope = kAudioObjectPropertyScopeGlobal;
    prop_address.mElement = kAudioObjectPropertyElementMaster;

    AudioObjectAddPropertyListener(osca->raw_stream_id, &prop_address, on_physical_format_changed, os);

    // Attempt to change
    if (osca->revert_format)
    {
        if ((os_err = AudioObjectSetPropertyData(osca->raw_stream_id, &prop_address, 0, NULL, io_size, &osca->hardware_format)))
        {
            outstream_destroy_ca(si, os);
            return SoundIoErrorOpeningDevice;
        }

        // wait for hardware
        struct timespec delay;
        int second_timer = 0;
        while (!SOUNDIO_ATOMIC_LOAD(osca->output_format_match) && second_timer < 100) {

            delay.tv_sec = 0;
            delay.tv_nsec = 1E7L;  /* 10ms in ns */

            if (nanosleep(&delay, NULL)) {
                outstream_destroy_ca(si, os);
                return SoundIoErrorOpeningDevice;
            }
            second_timer++;
        }

    }

    if (!SOUNDIO_ATOMIC_LOAD(osca->output_format_match))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    prop_address.mSelector = kAudioDeviceProcessorOverload;
    prop_address.mScope = kAudioObjectPropertyScopeGlobal;
    prop_address.mElement = OUTPUT_ELEMENT;
    if ((os_err = AudioObjectAddPropertyListener(dca->device_id, &prop_address,
        on_outstream_device_overload, os)))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    osca->hardware_latency = dca->latency_frames / (double)outstream->sample_rate;

    prop_address.mSelector = kAudioDevicePropertyVolumeScalar;
    prop_address.mScope = aim_to_scope(device->aim);
    prop_address.mElement = kAudioObjectPropertyElementMaster;

    io_size = sizeof(float);
    if ((os_err = AudioObjectGetPropertyData(dca->device_id, &prop_address, 0, NULL, &io_size, &outstream->volume)))
    {
        return SoundIoErrorIncompatibleDevice;
    }

    return 0;
}

static int outstream_open_ca_shared(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoDevice *device = outstream->device;
    struct SoundIoDevicePrivate *dev = (struct SoundIoDevicePrivate *)device;
    struct SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;

    if (outstream->software_latency == 0.0)
        outstream->software_latency = device->software_latency_current;

    outstream->software_latency = soundio_double_clamp(
            device->software_latency_min,
            outstream->software_latency,
            device->software_latency_max);

    AudioComponentDescription desc = {0};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(NULL, &desc);
    if (!component) {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    OSStatus os_err;
    if ((os_err = AudioComponentInstanceNew(component, &osca->instance))) {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if ((os_err = AudioUnitInitialize(osca->instance))) {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    AudioStreamBasicDescription format = {0};
    format.mSampleRate = outstream->sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    int err;
    if ((err = set_ca_desc(outstream->format, &format))) {
        outstream_destroy_ca(si, os);
        return err;
    }
    format.mBytesPerPacket = outstream->bytes_per_frame;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = outstream->bytes_per_frame;
    format.mChannelsPerFrame = outstream->layout.channel_count;

    if ((os_err = AudioUnitSetProperty(osca->instance, kAudioOutputUnitProperty_CurrentDevice,
        kAudioUnitScope_Input, OUTPUT_ELEMENT, &dca->device_id, sizeof(AudioDeviceID))))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if ((os_err = AudioUnitSetProperty(osca->instance, kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Input, OUTPUT_ELEMENT, &format, sizeof(AudioStreamBasicDescription))))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorIncompatibleDevice;
    }

    AURenderCallbackStruct render_callback = {write_callback_ca_shared, os};
    if ((os_err = AudioUnitSetProperty(osca->instance, kAudioUnitProperty_SetRenderCallback,
        kAudioUnitScope_Input, OUTPUT_ELEMENT, &render_callback, sizeof(AURenderCallbackStruct))))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    AudioObjectPropertyAddress prop_address = {
        kAudioDevicePropertyBufferFrameSize,
        kAudioObjectPropertyScopeInput,
        OUTPUT_ELEMENT
    };
    UInt32 buffer_frame_size = outstream->software_latency * outstream->sample_rate;
    if ((os_err = AudioObjectSetPropertyData(dca->device_id, &prop_address,
        0, NULL, sizeof(UInt32), &buffer_frame_size)))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    prop_address.mSelector = kAudioDeviceProcessorOverload;
    prop_address.mScope = kAudioObjectPropertyScopeGlobal;
    prop_address.mElement = OUTPUT_ELEMENT;
    if ((os_err = AudioObjectAddPropertyListener(dca->device_id, &prop_address,
        on_outstream_device_overload, os)))
    {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if ((os_err = AudioUnitGetParameter (osca->instance, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, &outstream->volume))) {
        outstream_destroy_ca(si, os);
        return SoundIoErrorOpeningDevice;
    }

    osca->hardware_latency = dca->latency_frames / (double)outstream->sample_rate;

    return 0;
}

static int outstream_open_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoDevice *device = outstream->device;
    if (device->is_raw) {
        return outstream_open_ca_raw(si, os);
    } else {
        assert(!device->is_raw);
        return outstream_open_ca_shared(si, os);
    }
}

static int outstream_pause_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os, bool pause)
{
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoDevice *device = outstream->device;
    struct SoundIoDevicePrivate *dev = (struct SoundIoDevicePrivate *)device;
    struct SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;

    OSStatus os_err;

    if (device->is_raw) {
        if (pause) {
            if ((os_err = AudioDeviceStop(dca->device_id, osca->io_proc_id))) {
                return SoundIoErrorStreaming;
            }
        } else {
            if ((os_err = AudioDeviceStart(dca->device_id, osca->io_proc_id))) {
                return SoundIoErrorStreaming;
            }
        }
    } else {
        if (pause) {
            if ((os_err = AudioOutputUnitStop(osca->instance))) {
                return SoundIoErrorStreaming;
            }
        } else {
            if ((os_err = AudioOutputUnitStart(osca->instance))) {
                return SoundIoErrorStreaming;
            }
        }
    }
    return 0;
}

static int outstream_start_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    return outstream_pause_ca(si, os, false);
}

static int outstream_begin_write_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os,
        struct SoundIoChannelArea **out_areas, int *frame_count)
{
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;

    if (osca->buffer_index >= osca->io_data->mNumberBuffers)
        return SoundIoErrorInvalid;

    if (*frame_count != osca->frames_left)
        return SoundIoErrorInvalid;

    AudioBuffer *audio_buffer = &osca->io_data->mBuffers[osca->buffer_index];
    assert(audio_buffer->mNumberChannels == outstream->layout.channel_count);
    osca->write_frame_count = audio_buffer->mDataByteSize / outstream->bytes_per_frame;
    *frame_count = osca->write_frame_count;
    assert((audio_buffer->mDataByteSize % outstream->bytes_per_frame) == 0);
    for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
        osca->areas[ch].ptr = ((char*)audio_buffer->mData) + outstream->bytes_per_sample * ch;
        osca->areas[ch].step = outstream->bytes_per_frame;
    }
    *out_areas = osca->areas;
    return 0;
}

static int outstream_end_write_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    osca->buffer_index += 1;
    osca->frames_left -= osca->write_frame_count;
    assert(osca->frames_left >= 0);
    return 0;
}

static int outstream_clear_buffer_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    return SoundIoErrorIncompatibleBackend;
}

static int outstream_get_latency_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os,
        double *out_latency)
{
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    *out_latency = osca->hardware_latency;
    return 0;
}

static int outstream_set_volume_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os, float volume) {
    struct SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    struct SoundIoOutStream *outstream = &os->pub;

    OSStatus os_err;

    if (osca->instance) {
        if ((os_err = AudioUnitSetParameter(osca->instance, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, volume, 0))) {
            return SoundIoErrorIncompatibleDevice;
        }
    }

    else if (osca->io_proc_id) {

        struct SoundIoDevice *device = outstream->device;
        struct SoundIoDevicePrivate *dev = (struct SoundIoDevicePrivate *)device;
        struct SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;

        AudioObjectPropertyAddress prop_address;
        prop_address.mSelector = kAudioDevicePropertyVolumeScalar;
        prop_address.mScope = aim_to_scope(device->aim);
        prop_address.mElement = kAudioObjectPropertyElementMaster;

        if ((os_err = AudioObjectSetPropertyData(dca->device_id, &prop_address, 0, NULL, sizeof(float), &volume))) {
            return SoundIoErrorIncompatibleDevice;
        }
    }

    outstream->volume = volume;
    return 0;
}

static OSStatus on_instream_device_overload(AudioObjectID in_object_id, UInt32 in_number_addresses,
    const AudioObjectPropertyAddress in_addresses[], void *in_client_data)
{
    struct SoundIoInStreamPrivate *os = (struct SoundIoInStreamPrivate *)in_client_data;
    struct SoundIoInStream *instream = &os->pub;
    instream->overflow_callback(instream);
    return noErr;
}

static void instream_destroy_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    struct SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;
    struct SoundIoInStream *instream = &is->pub;
    struct SoundIoDevice *device = instream->device;
    struct SoundIoDevicePrivate *dev = (struct SoundIoDevicePrivate *)device;
    struct SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;

    AudioObjectPropertyAddress prop_address = {
        kAudioDeviceProcessorOverload,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    AudioObjectRemovePropertyListener(dca->device_id, &prop_address, on_instream_device_overload, is);

    if (isca->instance) {
        AudioOutputUnitStop(isca->instance);
        AudioComponentInstanceDispose(isca->instance);
        isca->instance = NULL;
    }

    free(isca->buffer_list);
    isca->buffer_list = NULL;
}

static OSStatus read_callback_ca(void *userdata, AudioUnitRenderActionFlags *io_action_flags,
    const AudioTimeStamp *in_time_stamp, UInt32 in_bus_number, UInt32 in_number_frames,
    AudioBufferList *io_data)
{
    struct SoundIoInStreamPrivate *is = (struct SoundIoInStreamPrivate *) userdata;
    struct SoundIoInStream *instream = &is->pub;
    struct SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;

    for (int i = 0; i < isca->buffer_list->mNumberBuffers; i += 1) {
        isca->buffer_list->mBuffers[i].mData = NULL;
    }

    OSStatus os_err;
    if ((os_err = AudioUnitRender(isca->instance, io_action_flags, in_time_stamp,
        in_bus_number, in_number_frames, isca->buffer_list)))
    {
        instream->error_callback(instream, SoundIoErrorStreaming);
        return noErr;
    }

    if (isca->buffer_list->mNumberBuffers == 1) {
        AudioBuffer *audio_buffer = &isca->buffer_list->mBuffers[0];
        assert(audio_buffer->mNumberChannels == instream->layout.channel_count);
        assert(audio_buffer->mDataByteSize == in_number_frames * instream->bytes_per_frame);
        for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
            isca->areas[ch].ptr = ((char*)audio_buffer->mData) + (instream->bytes_per_sample * ch);
            isca->areas[ch].step = instream->bytes_per_frame;
        }
    } else {
        assert(isca->buffer_list->mNumberBuffers == instream->layout.channel_count);
        for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
            AudioBuffer *audio_buffer = &isca->buffer_list->mBuffers[ch];
            assert(audio_buffer->mDataByteSize == in_number_frames * instream->bytes_per_sample);
            isca->areas[ch].ptr = (char*)audio_buffer->mData;
            isca->areas[ch].step = instream->bytes_per_sample;
        }
    }

    isca->frames_left = in_number_frames;
    instream->read_callback(instream, isca->frames_left, isca->frames_left);

    return noErr;
}

static int instream_open_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    struct SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;
    struct SoundIoInStream *instream = &is->pub;
    struct SoundIoDevice *device = instream->device;
    struct SoundIoDevicePrivate *dev = (struct SoundIoDevicePrivate *)device;
    struct SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;
    UInt32 io_size;
    OSStatus os_err;

    if (instream->software_latency == 0.0)
        instream->software_latency = device->software_latency_current;

    instream->software_latency = soundio_double_clamp(
            device->software_latency_min,
            instream->software_latency,
            device->software_latency_max);


    AudioObjectPropertyAddress prop_address;
    prop_address.mSelector = kAudioDevicePropertyStreamConfiguration;
    prop_address.mScope = kAudioObjectPropertyScopeInput;
    prop_address.mElement = kAudioObjectPropertyElementMaster;
    io_size = 0;
    if ((os_err = AudioObjectGetPropertyDataSize(dca->device_id, &prop_address,
        0, NULL, &io_size)))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }

    isca->buffer_list = (AudioBufferList*)ALLOCATE_NONZERO(char, io_size);
    if (!isca->buffer_list) {
        instream_destroy_ca(si, is);
        return SoundIoErrorNoMem;
    }

    if ((os_err = AudioObjectGetPropertyData(dca->device_id, &prop_address,
        0, NULL, &io_size, isca->buffer_list)))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }


    AudioComponentDescription desc = {0};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(NULL, &desc);
    if (!component) {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }

    if ((os_err = AudioComponentInstanceNew(component, &isca->instance))) {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }

    if ((os_err = AudioUnitInitialize(isca->instance))) {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }

    UInt32 enable_io = 1;
    if ((os_err = AudioUnitSetProperty(isca->instance, kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Input, INPUT_ELEMENT, &enable_io, sizeof(UInt32))))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }

    enable_io = 0;
    if ((os_err = AudioUnitSetProperty(isca->instance, kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Output, OUTPUT_ELEMENT, &enable_io, sizeof(UInt32))))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }

    if ((os_err = AudioUnitSetProperty(isca->instance, kAudioOutputUnitProperty_CurrentDevice,
        kAudioUnitScope_Output, INPUT_ELEMENT, &dca->device_id, sizeof(AudioDeviceID))))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }


    AudioStreamBasicDescription format = {0};
    format.mSampleRate = instream->sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mBytesPerPacket = instream->bytes_per_frame;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = instream->bytes_per_frame;
    format.mChannelsPerFrame = instream->layout.channel_count;

    int err;
    if ((err = set_ca_desc(instream->format, &format))) {
        instream_destroy_ca(si, is);
        return err;
    }

    if ((os_err = AudioUnitSetProperty(isca->instance, kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Output, INPUT_ELEMENT, &format, sizeof(AudioStreamBasicDescription))))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }

    AURenderCallbackStruct input_callback = {read_callback_ca, is};
    if ((os_err = AudioUnitSetProperty(isca->instance, kAudioOutputUnitProperty_SetInputCallback,
        kAudioUnitScope_Output, INPUT_ELEMENT, &input_callback, sizeof(AURenderCallbackStruct))))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }


    prop_address.mSelector = kAudioDevicePropertyBufferFrameSize;
    prop_address.mScope = kAudioObjectPropertyScopeOutput;
    prop_address.mElement = INPUT_ELEMENT;
    UInt32 buffer_frame_size = instream->software_latency * instream->sample_rate;
    if ((os_err = AudioObjectSetPropertyData(dca->device_id, &prop_address,
        0, NULL, sizeof(UInt32), &buffer_frame_size)))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }

    prop_address.mSelector = kAudioDeviceProcessorOverload;
    prop_address.mScope = kAudioObjectPropertyScopeGlobal;
    if ((os_err = AudioObjectAddPropertyListener(dca->device_id, &prop_address,
        on_instream_device_overload, is)))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }

    isca->hardware_latency = dca->latency_frames / (double)instream->sample_rate;

    return 0;
}

static int instream_pause_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is, bool pause) {
    struct SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;
    OSStatus os_err;
    if (pause) {
        if ((os_err = AudioOutputUnitStop(isca->instance))) {
            return SoundIoErrorStreaming;
        }
    } else {
        if ((os_err = AudioOutputUnitStart(isca->instance))) {
            return SoundIoErrorStreaming;
        }
    }

    return 0;
}

static int instream_start_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    return instream_pause_ca(si, is, false);
}

static int instream_begin_read_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is,
        struct SoundIoChannelArea **out_areas, int *frame_count)
{
    struct SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;

    if (*frame_count != isca->frames_left)
        return SoundIoErrorInvalid;

    *out_areas = isca->areas;

    return 0;
}

static int instream_end_read_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    struct SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;
    isca->frames_left = 0;
    return 0;
}

static int instream_get_latency_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is,
        double *out_latency)
{
    struct SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;
    *out_latency = isca->hardware_latency;
    return 0;
}


int soundio_coreaudio_init(struct SoundIoPrivate *si) {
    struct SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    int err;

    SOUNDIO_ATOMIC_STORE(sica->have_devices_flag, false);
    SOUNDIO_ATOMIC_STORE(sica->device_scan_queued, true);
    SOUNDIO_ATOMIC_STORE(sica->service_restarted, false);
    SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(sica->abort_flag);

    sica->mutex = soundio_os_mutex_create();
    if (!sica->mutex) {
        destroy_ca(si);
        return SoundIoErrorNoMem;
    }

    sica->cond = soundio_os_cond_create();
    if (!sica->cond) {
        destroy_ca(si);
        return SoundIoErrorNoMem;
    }

    sica->have_devices_cond = soundio_os_cond_create();
    if (!sica->have_devices_cond) {
        destroy_ca(si);
        return SoundIoErrorNoMem;
    }

    sica->scan_devices_cond = soundio_os_cond_create();
    if (!sica->scan_devices_cond) {
        destroy_ca(si);
        return SoundIoErrorNoMem;
    }

    AudioObjectPropertyAddress prop_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    if ((err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &prop_address,
        on_devices_changed, si)))
    {
        destroy_ca(si);
        return SoundIoErrorSystemResources;
    }

    prop_address.mSelector = kAudioHardwarePropertyServiceRestarted;
    if ((err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &prop_address,
        on_service_restarted, si)))
    {
        destroy_ca(si);
        return SoundIoErrorSystemResources;
    }

    if ((err = soundio_os_thread_create(device_thread_run, si, NULL, &sica->thread))) {
        destroy_ca(si);
        return err;
    }

    si->destroy = destroy_ca;
    si->flush_events = flush_events_ca;
    si->wait_events = wait_events_ca;
    si->wakeup = wakeup_ca;
    si->force_device_scan = force_device_scan_ca;

    si->outstream_open = outstream_open_ca;
    si->outstream_destroy = outstream_destroy_ca;
    si->outstream_start = outstream_start_ca;
    si->outstream_begin_write = outstream_begin_write_ca;
    si->outstream_end_write = outstream_end_write_ca;
    si->outstream_clear_buffer = outstream_clear_buffer_ca;
    si->outstream_pause = outstream_pause_ca;
    si->outstream_get_latency = outstream_get_latency_ca;
    si->outstream_set_volume = outstream_set_volume_ca;

    si->instream_open = instream_open_ca;
    si->instream_destroy = instream_destroy_ca;
    si->instream_start = instream_start_ca;
    si->instream_begin_read = instream_begin_read_ca;
    si->instream_end_read = instream_end_read_ca;
    si->instream_pause = instream_pause_ca;
    si->instream_get_latency = instream_get_latency_ca;

    return 0;
}
