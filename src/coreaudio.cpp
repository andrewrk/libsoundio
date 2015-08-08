#include "coreaudio.hpp"
#include "soundio.hpp"

#include <assert.h>

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
    },
};

static SoundIoDeviceAim aims[] = {
    SoundIoDeviceAimInput,
    SoundIoDeviceAimOutput,
};

static OSStatus on_devices_changed(AudioObjectID in_object_id, UInt32 in_number_addresses,
    const AudioObjectPropertyAddress in_addresses[], void *in_client_data)
{
    SoundIoPrivate *si = (SoundIoPrivate*)in_client_data;
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    sica->device_scan_queued.store(true);
    soundio_os_cond_signal(sica->scan_devices_cond, nullptr);

    return noErr;
}

static OSStatus on_service_restarted(AudioObjectID in_object_id, UInt32 in_number_addresses,
    const AudioObjectPropertyAddress in_addresses[], void *in_client_data)
{
    SoundIoPrivate *si = (SoundIoPrivate*)in_client_data;
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    sica->service_restarted.store(true);
    soundio_os_cond_signal(sica->scan_devices_cond, nullptr);

    return noErr;
}

static void unsubscribe_device_listeners(SoundIoPrivate *si) {
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    for (int device_index = 0; device_index < sica->registered_listeners.length; device_index += 1) {
        AudioDeviceID device_id = sica->registered_listeners.at(device_index);
        for (int i = 0; i < array_length(device_listen_props); i += 1) {
            AudioObjectRemovePropertyListener(device_id, &device_listen_props[i],
                on_devices_changed, si);
        }
    }
    sica->registered_listeners.clear();
}

static void destroy_ca(struct SoundIoPrivate *si) {
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    AudioObjectPropertyAddress prop_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &prop_address, on_devices_changed, si);

    prop_address.mSelector = kAudioHardwarePropertyServiceRestarted;
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &prop_address, on_service_restarted, si);

    unsubscribe_device_listeners(si);
    sica->registered_listeners.deinit();

    if (sica->thread) {
        sica->abort_flag.clear();
        soundio_os_cond_signal(sica->scan_devices_cond, nullptr);
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
    CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    char *buf = allocate_nonzero<char>(max_size);
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

static int aim_to_scope(SoundIoDeviceAim aim) {
    return (aim == SoundIoDeviceAimInput) ?
        kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput;
}

static SoundIoChannelId from_channel_descr(const AudioChannelDescription *descr) {
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
static int from_coreaudio_layout(const AudioChannelLayout *ca_layout, SoundIoChannelLayout *layout) {
    switch (ca_layout->mChannelLayoutTag) {
    case kAudioChannelLayoutTag_UseChannelDescriptions:
    {
        layout->channel_count = ca_layout->mNumberChannelDescriptions;
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

struct RefreshDevices {
    SoundIoPrivate *si;
    SoundIoDevicesInfo *devices_info;
    int devices_size;
    AudioObjectID *devices;
    CFStringRef string_ref;
    char *device_name;
    int device_name_len;
    AudioBufferList *buffer_list;
    SoundIoDevice *device;
    AudioChannelLayout *audio_channel_layout;
    char *device_uid;
    int device_uid_len;
    AudioValueRange *avr_array;
    bool ok;
};

static void deinit_refresh_devices(RefreshDevices *rd) {
    if (!rd->ok)
        unsubscribe_device_listeners(rd->si);
    soundio_destroy_devices_info(rd->devices_info);
    free(rd->devices);
    if (rd->string_ref)
        CFRelease(rd->string_ref);
    free(rd->device_name);
    free(rd->buffer_list);
    soundio_device_unref(rd->device);
    free(rd->audio_channel_layout);
    free(rd->device_uid);
    free(rd->avr_array);
}

static int refresh_devices(struct SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    UInt32 io_size;
    OSStatus os_err;
    int err;

    unsubscribe_device_listeners(si);

    RefreshDevices rd = {0};
    rd.si = si;

    if (!(rd.devices_info = allocate<SoundIoDevicesInfo>(1))) {
        deinit_refresh_devices(&rd);
        return SoundIoErrorNoMem;
    }

    AudioObjectPropertyAddress prop_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    if ((os_err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
        &prop_address, 0, nullptr, &io_size)))
    {
        deinit_refresh_devices(&rd);
        return SoundIoErrorOpeningDevice;
    }

    AudioObjectID default_input_id;
    AudioObjectID default_output_id;

    int device_count = io_size / (UInt32)sizeof(AudioObjectID);
    if (device_count >= 1) {
        rd.devices_size = io_size;
        rd.devices = (AudioObjectID *)allocate<char>(rd.devices_size);
        if (!rd.devices) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorNoMem;
        }

        if ((os_err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_address, 0, nullptr,
            &io_size, rd.devices)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }


        io_size = sizeof(AudioObjectID);
        prop_address.mSelector = kAudioHardwarePropertyDefaultInputDevice;
        if ((os_err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_address,
            0, nullptr, &io_size, &default_input_id)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        io_size = sizeof(AudioObjectID);
        prop_address.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        if ((os_err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_address,
            0, nullptr, &io_size, &default_output_id)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
    }

    for (int device_i = 0; device_i < device_count; device_i += 1) {
        AudioObjectID device_id = rd.devices[device_i];

        for (int i = 0; i < array_length(device_listen_props); i += 1) {
            if ((err = sica->registered_listeners.add_one())) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }
            if ((os_err = AudioObjectAddPropertyListener(device_id, &device_listen_props[i],
                on_devices_changed, si)))
            {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }
            sica->registered_listeners.last() = device_id;
        }

        prop_address.mSelector = kAudioObjectPropertyName;
        prop_address.mScope = kAudioObjectPropertyScopeGlobal;
        prop_address.mElement = kAudioObjectPropertyElementMaster;
        io_size = sizeof(CFStringRef);
        if (rd.string_ref) {
            CFRelease(rd.string_ref);
            rd.string_ref = nullptr;
        }
        if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address,
            0, nullptr, &io_size, &rd.string_ref)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        free(rd.device_name);
        rd.device_name = nullptr;
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
            rd.string_ref = nullptr;
        }
        if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address,
            0, nullptr, &io_size, &rd.string_ref)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        free(rd.device_uid);
        rd.device_uid = nullptr;
        if ((err = from_cf_string(rd.string_ref, &rd.device_uid, &rd.device_uid_len))) {
            deinit_refresh_devices(&rd);
            return err;
        }


        for (int aim_i = 0; aim_i < array_length(aims); aim_i += 1) {
            SoundIoDeviceAim aim = aims[aim_i];

            io_size = 0;
            prop_address.mSelector = kAudioDevicePropertyStreamConfiguration;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            if ((os_err = AudioObjectGetPropertyDataSize(device_id, &prop_address, 0, nullptr, &io_size))) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }

            free(rd.buffer_list);
            rd.buffer_list = (AudioBufferList*)allocate_nonzero<char>(io_size);
            if (!rd.buffer_list) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorNoMem;
            }

            if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, nullptr,
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

            SoundIoDevicePrivate *dev = allocate<SoundIoDevicePrivate>(1);
            if (!dev) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorNoMem;
            }
            SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;
            dca->device_id = device_id;
            assert(!rd.device);
            rd.device = &dev->pub;
            rd.device->ref_count = 1;
            rd.device->soundio = soundio;
            rd.device->is_raw = false;
            rd.device->aim = aim;
            rd.device->id = soundio_str_dupe(rd.device_uid, rd.device_uid_len);
            rd.device->name = soundio_str_dupe(rd.device_name, rd.device_name_len);
            rd.device->layout_count = 1;
            rd.device->layouts = allocate<SoundIoChannelLayout>(1);
            rd.device->format_count = 1;
            rd.device->formats = allocate<SoundIoFormat>(1);

            if (!rd.device->id || !rd.device->name || !rd.device->layouts || !rd.device->formats) {
                deinit_refresh_devices(&rd);
                return SoundIoErrorNoMem;
            }

            prop_address.mSelector = kAudioDevicePropertyPreferredChannelLayout;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            if (!(os_err = AudioObjectGetPropertyDataSize(device_id, &prop_address,
                0, nullptr, &io_size)))
            {
                rd.audio_channel_layout = (AudioChannelLayout *)allocate<char>(io_size);
                if (!rd.audio_channel_layout) {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorNoMem;
                }
                if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, nullptr,
                    &io_size, rd.audio_channel_layout)))
                {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorOpeningDevice;
                }
                if ((err = from_coreaudio_layout(rd.audio_channel_layout, &rd.device->current_layout))) {
                    rd.device->current_layout.channel_count = channel_count;
                }
            }
            if (all_channels_invalid(&rd.device->current_layout)) {
                const struct SoundIoChannelLayout *guessed_layout =
                    soundio_channel_layout_get_default(channel_count);
                if (guessed_layout)
                    rd.device->current_layout = *guessed_layout;
            }

            rd.device->layouts[0] = rd.device->current_layout;
            // in CoreAudio, format is always 32-bit native endian float
            rd.device->formats[0] = SoundIoFormatFloat32NE;

            prop_address.mSelector = kAudioDevicePropertyNominalSampleRate;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            io_size = sizeof(double);
            double value;
            if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, nullptr,
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
            rd.device->sample_rate_current = (int)floored_value;

            // If you try to open an input stream with anything but the current
            // nominal sample rate, AudioUnitRender returns an error.
            if (aim == SoundIoDeviceAimInput) {
                rd.device->sample_rate_count = 1;
                rd.device->sample_rates = &dev->prealloc_sample_rate_range;
                rd.device->sample_rates[0].min = rd.device->sample_rate_current;
                rd.device->sample_rates[0].max = rd.device->sample_rate_current;
            } else {
                prop_address.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
                prop_address.mScope = aim_to_scope(aim);
                prop_address.mElement = kAudioObjectPropertyElementMaster;
                if ((os_err = AudioObjectGetPropertyDataSize(device_id, &prop_address, 0, nullptr,
                    &io_size)))
                {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorOpeningDevice;
                }
                int avr_array_len = io_size / sizeof(AudioValueRange);
                rd.avr_array = (AudioValueRange*)allocate<char>(io_size);

                if (!rd.avr_array) {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorNoMem;
                }

                if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, nullptr,
                    &io_size, rd.avr_array)))
                {
                    deinit_refresh_devices(&rd);
                    return SoundIoErrorOpeningDevice;
                }

                if (avr_array_len == 1) {
                    rd.device->sample_rate_count = 1;
                    rd.device->sample_rates = &dev->prealloc_sample_rate_range;
                    rd.device->sample_rates[0].min = ceil(rd.avr_array[0].mMinimum);
                    rd.device->sample_rates[0].max = floor(rd.avr_array[0].mMaximum);
                } else {
                    rd.device->sample_rate_count = avr_array_len;
                    rd.device->sample_rates = allocate<SoundIoSampleRateRange>(avr_array_len);
                    if (!rd.device->sample_rates) {
                        deinit_refresh_devices(&rd);
                        return SoundIoErrorNoMem;
                    }
                    for (int i = 0; i < avr_array_len; i += 1) {
                        AudioValueRange *avr = &rd.avr_array[i];
                        int min_val = ceil(avr->mMinimum);
                        int max_val = floor(avr->mMaximum);
                        rd.device->sample_rates[i].min = min_val;
                        rd.device->sample_rates[i].max = max_val;
                    }
                }
            }

            prop_address.mSelector = kAudioDevicePropertyBufferFrameSize;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            io_size = sizeof(UInt32);
            UInt32 buffer_frame_size;
            if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, nullptr,
                &io_size, &buffer_frame_size)))
            {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }
            double use_sample_rate = rd.device->sample_rate_current;
            rd.device->buffer_duration_current = buffer_frame_size / use_sample_rate;

            prop_address.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = kAudioObjectPropertyElementMaster;
            io_size = sizeof(AudioValueRange);
            AudioValueRange avr;
            if ((os_err = AudioObjectGetPropertyData(device_id, &prop_address, 0, nullptr,
                &io_size, &avr)))
            {
                deinit_refresh_devices(&rd);
                return SoundIoErrorOpeningDevice;
            }
            rd.device->buffer_duration_min = avr.mMinimum / use_sample_rate;
            rd.device->buffer_duration_max = avr.mMaximum / use_sample_rate;



            SoundIoList<SoundIoDevice *> *device_list;
            if (rd.device->aim == SoundIoDeviceAimOutput) {
                device_list = &rd.devices_info->output_devices;
                if (device_id == default_output_id)
                    rd.devices_info->default_output_index = device_list->length;
            } else {
                assert(rd.device->aim == SoundIoDeviceAimInput);
                device_list = &rd.devices_info->input_devices;
                if (device_id == default_input_id)
                    rd.devices_info->default_input_index = device_list->length;
            }

            if ((err = device_list->append(rd.device))) {
                deinit_refresh_devices(&rd);
                return err;
            }
            rd.device = nullptr;
        }
    }

    soundio_os_mutex_lock(sica->mutex);
    soundio_destroy_devices_info(sica->ready_devices_info);
    sica->ready_devices_info = rd.devices_info;
    soundio->on_events_signal(soundio);
    soundio_os_mutex_unlock(sica->mutex);

    rd.devices_info = nullptr;
    rd.ok = true;
    deinit_refresh_devices(&rd);

    return 0;
}

static void shutdown_backend(SoundIoPrivate *si, int err) {
    SoundIo *soundio = &si->pub;
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    soundio_os_mutex_lock(sica->mutex);
    sica->shutdown_err = err;
    soundio->on_events_signal(soundio);
    soundio_os_mutex_unlock(sica->mutex);
}

static void block_until_have_devices(SoundIoCoreAudio *sica) {
    if (sica->have_devices_flag.load())
        return;
    while (!sica->have_devices_flag.load())
        soundio_os_cond_wait(sica->have_devices_cond, nullptr);
}

static void flush_events_ca(struct SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    block_until_have_devices(sica);

    bool change = false;
    bool cb_shutdown = false;
    SoundIoDevicesInfo *old_devices_info = nullptr;

    soundio_os_mutex_lock(sica->mutex);

    if (sica->shutdown_err && !sica->emitted_shutdown_cb) {
        sica->emitted_shutdown_cb = true;
        cb_shutdown = true;
    } else if (sica->ready_devices_info) {
        old_devices_info = si->safe_devices_info;
        si->safe_devices_info = sica->ready_devices_info;
        sica->ready_devices_info = nullptr;
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
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    flush_events_ca(si);
    soundio_os_cond_wait(sica->cond, nullptr);
}

static void wakeup_ca(struct SoundIoPrivate *si) {
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    soundio_os_cond_signal(sica->cond, nullptr);
}

static void device_thread_run(void *arg) {
    SoundIoPrivate *si = (SoundIoPrivate *)arg;
    SoundIo *soundio = &si->pub;
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    int err;

    for (;;) {
        if (!sica->abort_flag.test_and_set())
            break;
        if (sica->service_restarted.load()) {
            shutdown_backend(si, SoundIoErrorBackendDisconnected);
            return;
        }
        if (sica->device_scan_queued.exchange(false)) {
            err = refresh_devices(si);
            if (err)
                shutdown_backend(si, err);
            if (!sica->have_devices_flag.exchange(true)) {
                soundio_os_cond_signal(sica->have_devices_cond, nullptr);
                soundio->on_events_signal(soundio);
            }
            if (err)
                return;
            soundio_os_cond_signal(sica->cond, nullptr);
        }
        soundio_os_cond_wait(sica->scan_devices_cond, nullptr);
    }
}

static OSStatus on_outstream_device_overload(AudioObjectID in_object_id, UInt32 in_number_addresses,
    const AudioObjectPropertyAddress in_addresses[], void *in_client_data)
{
    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate *)in_client_data;
    SoundIoOutStream *outstream = &os->pub;
    outstream->underflow_callback(outstream);
    return noErr;
}

static void outstream_destroy_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    SoundIoOutStream *outstream = &os->pub;
    SoundIoDevice *device = outstream->device;
    SoundIoDevicePrivate *dev = (SoundIoDevicePrivate *)device;
    SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;

    AudioObjectPropertyAddress prop_address = {
        kAudioDeviceProcessorOverload,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    AudioObjectRemovePropertyListener(dca->device_id, &prop_address, on_outstream_device_overload, os);

    if (osca->instance) {
        AudioOutputUnitStop(osca->instance);
        AudioComponentInstanceDispose(osca->instance);
    }
}

static OSStatus write_callback_ca(void *userdata, AudioUnitRenderActionFlags *io_action_flags,
    const AudioTimeStamp *in_time_stamp, UInt32 in_bus_number, UInt32 in_number_frames,
    AudioBufferList *io_data)
{
    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate *) userdata;
    SoundIoOutStream *outstream = &os->pub;
    SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;

    osca->io_data = io_data;
    osca->buffer_index = 0;
    osca->frames_left = in_number_frames;
    outstream->write_callback(outstream, osca->frames_left, osca->frames_left);
    osca->io_data = nullptr;

    return noErr;
}

static int outstream_open_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    SoundIoOutStream *outstream = &os->pub;
    SoundIoDevice *device = outstream->device;
    SoundIoDevicePrivate *dev = (SoundIoDevicePrivate *)device;
    SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;

    if (outstream->buffer_duration == 0.0)
        outstream->buffer_duration = device->buffer_duration_current;

    outstream->buffer_duration = clamp(
            device->buffer_duration_min,
            outstream->buffer_duration,
            device->buffer_duration_max);

    AudioComponentDescription desc = {0};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
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
    format.mFormatFlags = kAudioFormatFlagIsFloat;
    format.mBytesPerPacket = outstream->bytes_per_frame;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = outstream->bytes_per_frame;
    format.mChannelsPerFrame = outstream->layout.channel_count;
    format.mBitsPerChannel = 32;

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
        return SoundIoErrorOpeningDevice;
    }

    AURenderCallbackStruct render_callback = {write_callback_ca, os};
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
    UInt32 buffer_frame_size = outstream->buffer_duration * outstream->sample_rate;
    if ((os_err = AudioObjectSetPropertyData(dca->device_id, &prop_address,
        0, nullptr, sizeof(UInt32), &buffer_frame_size)))
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

    return 0;
}

static int outstream_pause_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os, bool pause) {
    SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    OSStatus os_err;
    if (pause) {
        if ((os_err = AudioOutputUnitStop(osca->instance))) {
            return SoundIoErrorStreaming;
        }
    } else {
        if ((os_err = AudioOutputUnitStart(osca->instance))) {
            return SoundIoErrorStreaming;
        }
    }

    return 0;
}

static int outstream_start_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    return outstream_pause_ca(si, os, false);
}

static int outstream_begin_write_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    SoundIoOutStream *outstream = &os->pub;
    SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;

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
    SoundIoOutStreamCoreAudio *osca = &os->backend_data.coreaudio;
    osca->buffer_index += 1;
    osca->frames_left -= osca->write_frame_count;
    assert(osca->frames_left >= 0);
    return 0;
}

static int outstream_clear_buffer_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    return 0;
}


static OSStatus on_instream_device_overload(AudioObjectID in_object_id, UInt32 in_number_addresses,
    const AudioObjectPropertyAddress in_addresses[], void *in_client_data)
{
    //SoundIoInStreamPrivate *os = (SoundIoInStreamPrivate *)in_client_data;
    //SoundIoInStream *instream = &os->pub;
    fprintf(stderr, "TODO overflow\n");
    //instream->underflow_callback(instream);
    return noErr;
}

static void instream_destroy_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;
    SoundIoInStream *instream = &is->pub;
    SoundIoDevice *device = instream->device;
    SoundIoDevicePrivate *dev = (SoundIoDevicePrivate *)device;
    SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;

    AudioObjectPropertyAddress prop_address = {
        kAudioDeviceProcessorOverload,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    AudioObjectRemovePropertyListener(dca->device_id, &prop_address, on_instream_device_overload, is);

    if (isca->instance) {
        AudioOutputUnitStop(isca->instance);
        AudioComponentInstanceDispose(isca->instance);
    }

    free(isca->buffer_list);
}

static OSStatus read_callback_ca(void *userdata, AudioUnitRenderActionFlags *io_action_flags,
    const AudioTimeStamp *in_time_stamp, UInt32 in_bus_number, UInt32 in_number_frames,
    AudioBufferList *io_data)
{
    SoundIoInStreamPrivate *is = (SoundIoInStreamPrivate *) userdata;
    SoundIoInStream *instream = &is->pub;
    SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;

    for (int i = 0; i < isca->buffer_list->mNumberBuffers; i += 1) {
        isca->buffer_list->mBuffers[i].mData = nullptr;
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
            isca->areas[ch].ptr = ((char*)audio_buffer->mData) + instream->bytes_per_sample;
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
    SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;
    SoundIoInStream *instream = &is->pub;
    SoundIoDevice *device = instream->device;
    SoundIoDevicePrivate *dev = (SoundIoDevicePrivate *)device;
    SoundIoDeviceCoreAudio *dca = &dev->backend_data.coreaudio;
    UInt32 io_size;
    OSStatus os_err;

    if (instream->buffer_duration == 0.0)
        instream->buffer_duration = device->buffer_duration_current;

    instream->buffer_duration = clamp(
            device->buffer_duration_min,
            instream->buffer_duration,
            device->buffer_duration_max);


    AudioObjectPropertyAddress prop_address;
    prop_address.mSelector = kAudioDevicePropertyStreamConfiguration;
    prop_address.mScope = kAudioObjectPropertyScopeInput;
    prop_address.mElement = kAudioObjectPropertyElementMaster;
    io_size = 0;
    if ((os_err = AudioObjectGetPropertyDataSize(dca->device_id, &prop_address,
        0, nullptr, &io_size)))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }

    isca->buffer_list = (AudioBufferList*)allocate_nonzero<char>(io_size);
    if (!isca->buffer_list) {
        instream_destroy_ca(si, is);
        return SoundIoErrorNoMem;
    }

    if ((os_err = AudioObjectGetPropertyData(dca->device_id, &prop_address,
        0, nullptr, &io_size, isca->buffer_list)))
    {
        instream_destroy_ca(si, is);
        return SoundIoErrorOpeningDevice;
    }


    AudioComponentDescription desc = {0};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
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
    format.mFormatFlags = kAudioFormatFlagIsFloat;
    format.mBytesPerPacket = instream->bytes_per_frame;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = instream->bytes_per_frame;
    format.mChannelsPerFrame = instream->layout.channel_count;
    format.mBitsPerChannel = 32;

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
    UInt32 buffer_frame_size = instream->buffer_duration * instream->sample_rate;
    if ((os_err = AudioObjectSetPropertyData(dca->device_id, &prop_address,
        0, nullptr, sizeof(UInt32), &buffer_frame_size)))
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

    return 0;
}

static int instream_pause_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is, bool pause) {
    SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;
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
        SoundIoChannelArea **out_areas, int *frame_count)
{
    SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;

    if (*frame_count != isca->frames_left)
        return SoundIoErrorInvalid;

    *out_areas = isca->areas;

    return 0;
}

static int instream_end_read_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    SoundIoInStreamCoreAudio *isca = &is->backend_data.coreaudio;
    isca->frames_left = 0;
    return 0;
}



// Possible errors:
// * SoundIoErrorNoMem
int soundio_coreaudio_init(SoundIoPrivate *si) {
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;
    int err;

    sica->have_devices_flag.store(false);
    sica->device_scan_queued.store(true);
    sica->service_restarted.store(false);
    sica->abort_flag.test_and_set();

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

    if ((err = soundio_os_thread_create(device_thread_run, si, false, &sica->thread))) {
        destroy_ca(si);
        return err;
    }

    si->destroy = destroy_ca;
    si->flush_events = flush_events_ca;
    si->wait_events = wait_events_ca;
    si->wakeup = wakeup_ca;

    si->outstream_open = outstream_open_ca;
    si->outstream_destroy = outstream_destroy_ca;
    si->outstream_start = outstream_start_ca;
    si->outstream_begin_write = outstream_begin_write_ca;
    si->outstream_end_write = outstream_end_write_ca;
    si->outstream_clear_buffer = outstream_clear_buffer_ca;
    si->outstream_pause = outstream_pause_ca;

    si->instream_open = instream_open_ca;
    si->instream_destroy = instream_destroy_ca;
    si->instream_start = instream_start_ca;
    si->instream_begin_read = instream_begin_read_ca;
    si->instream_end_read = instream_end_read_ca;
    si->instream_pause = instream_pause_ca;

    return 0;
}
