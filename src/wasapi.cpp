/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "wasapi.hpp"
#include "soundio.hpp"

#include <stdio.h>

// Attempting to use the Windows-supplied versions of these constants resulted
// in `undefined reference` linker errors.
const static GUID SOUNDIO_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {
    0x00000003,0x0000,0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

const static GUID SOUNDIO_KSDATAFORMAT_SUBTYPE_PCM = {
    0x00000001,0x0000,0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

const static DWORD SOUNDIO_AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM = 0x80000000;

// Adding more common sample rates helps the heuristics; feel free to do that.
static int test_sample_rates[] = {
    8000,
    11025,
    16000,
    22050,
    32000,
    37800,
    44056,
    44100,
    47250,
    48000,
    50000,
    50400,
    88200,
    96000,
    176400,
    192000,
    352800,
    2822400,
    5644800,
};

// If you modify this list, also modify `to_wave_format_format` appropriately.
static SoundIoFormat test_formats[] = {
    SoundIoFormatU8,
    SoundIoFormatS16LE,
    SoundIoFormatS24LE,
    SoundIoFormatS32LE,
    SoundIoFormatFloat32LE,
    SoundIoFormatFloat64LE,
};

// If you modify this list, also modify `to_wave_format_layout` appropriately.
static SoundIoChannelLayoutId test_layouts[] = {
    SoundIoChannelLayoutIdMono,
    SoundIoChannelLayoutIdStereo,
    SoundIoChannelLayoutIdQuad,
    SoundIoChannelLayoutId4Point0,
    SoundIoChannelLayoutId5Point1,
    SoundIoChannelLayoutId7Point1,
    SoundIoChannelLayoutId5Point1Back,
};

// converts a windows wide string to a UTF-8 encoded char *
// Possible errors:
//  * SoundIoErrorNoMem
//  * SoundIoErrorEncodingString
static int from_lpwstr(LPWSTR lpwstr, char **out_str, int *out_str_len) {
    DWORD flags = 0;
    int buf_size = WideCharToMultiByte(CP_UTF8, flags, lpwstr, -1, nullptr, 0, nullptr, nullptr);

    if (buf_size == 0)
        return SoundIoErrorEncodingString;

    char *buf = allocate<char>(buf_size);
    if (!buf)
        return SoundIoErrorNoMem;

    if (WideCharToMultiByte(CP_UTF8, flags, lpwstr, -1, buf, buf_size, nullptr, nullptr) != buf_size) {
        free(buf);
        return SoundIoErrorEncodingString;
    }

    *out_str = buf;
    *out_str_len = buf_size - 1;

    return 0;
}

static void from_channel_mask_layout(UINT channel_mask, SoundIoChannelLayout *layout) {
    layout->channel_count = 0;
    if (channel_mask & SPEAKER_FRONT_LEFT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdFrontLeft;
    if (channel_mask & SPEAKER_FRONT_RIGHT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdFrontRight;
    if (channel_mask & SPEAKER_FRONT_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdFrontCenter;
    if (channel_mask & SPEAKER_LOW_FREQUENCY)
        layout->channels[layout->channel_count++] = SoundIoChannelIdLfe;
    if (channel_mask & SPEAKER_BACK_LEFT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdBackLeft;
    if (channel_mask & SPEAKER_BACK_RIGHT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdBackRight;
    if (channel_mask & SPEAKER_FRONT_LEFT_OF_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdFrontLeftCenter;
    if (channel_mask & SPEAKER_FRONT_RIGHT_OF_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdFrontRightCenter;
    if (channel_mask & SPEAKER_BACK_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdBackCenter;
    if (channel_mask & SPEAKER_SIDE_LEFT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdSideLeft;
    if (channel_mask & SPEAKER_SIDE_RIGHT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdSideRight;
    if (channel_mask & SPEAKER_TOP_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopCenter;
    if (channel_mask & SPEAKER_TOP_FRONT_LEFT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopFrontLeft;
    if (channel_mask & SPEAKER_TOP_FRONT_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopFrontCenter;
    if (channel_mask & SPEAKER_TOP_FRONT_RIGHT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopFrontRight;
    if (channel_mask & SPEAKER_TOP_BACK_LEFT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopBackLeft;
    if (channel_mask & SPEAKER_TOP_BACK_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopBackCenter;
    if (channel_mask & SPEAKER_TOP_BACK_RIGHT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopBackRight;

    soundio_channel_layout_detect_builtin(layout);
}

static void from_wave_format_layout(WAVEFORMATEXTENSIBLE *wave_format, SoundIoChannelLayout *layout) {
    assert(wave_format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE);
    layout->channel_count = 0;
    from_channel_mask_layout(wave_format->dwChannelMask, layout);
}

static SoundIoFormat from_wave_format_format(WAVEFORMATEXTENSIBLE *wave_format) {
    assert(wave_format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE);
    bool is_pcm = IsEqualGUID(wave_format->SubFormat, SOUNDIO_KSDATAFORMAT_SUBTYPE_PCM);
    bool is_float = IsEqualGUID(wave_format->SubFormat, SOUNDIO_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

    if (wave_format->Samples.wValidBitsPerSample == wave_format->Format.wBitsPerSample) {
        if (wave_format->Format.wBitsPerSample == 8) {
            if (is_pcm)
                return SoundIoFormatU8;
        } else if (wave_format->Format.wBitsPerSample == 16) {
            if (is_pcm)
                return SoundIoFormatS16LE;
        } else if (wave_format->Format.wBitsPerSample == 32) {
            if (is_pcm)
                return SoundIoFormatS32LE;
            else if (is_float)
                return SoundIoFormatFloat32LE;
        } else if (wave_format->Format.wBitsPerSample == 64) {
            if (is_float)
                return SoundIoFormatFloat64LE;
        }
    } else if (wave_format->Format.wBitsPerSample == 32 &&
            wave_format->Samples.wValidBitsPerSample == 24)
    {
        return SoundIoFormatS24LE;
    }

    return SoundIoFormatInvalid;
}

// only needs to support the layouts in test_layouts
static void to_wave_format_layout(const SoundIoChannelLayout *layout, WAVEFORMATEXTENSIBLE *wave_format) {
    wave_format->dwChannelMask = 0;
    wave_format->Format.nChannels = layout->channel_count;
    for (int i = 0; i < layout->channel_count; i += 1) {
        SoundIoChannelId channel_id = layout->channels[i];
        switch (channel_id) {
            case SoundIoChannelIdFrontLeft:
                wave_format->dwChannelMask |= SPEAKER_FRONT_LEFT;
                break;
            case SoundIoChannelIdFrontRight:
                wave_format->dwChannelMask |= SPEAKER_FRONT_RIGHT;
                break;
            case SoundIoChannelIdFrontCenter:
                wave_format->dwChannelMask |= SPEAKER_FRONT_CENTER;
                break;
            case SoundIoChannelIdLfe:
                wave_format->dwChannelMask |= SPEAKER_LOW_FREQUENCY;
                break;
            case SoundIoChannelIdBackLeft:
                wave_format->dwChannelMask |= SPEAKER_BACK_LEFT;
                break;
            case SoundIoChannelIdBackRight:
                wave_format->dwChannelMask |= SPEAKER_BACK_RIGHT;
                break;
            case SoundIoChannelIdFrontLeftCenter:
                wave_format->dwChannelMask |= SPEAKER_FRONT_LEFT_OF_CENTER;
                break;
            case SoundIoChannelIdFrontRightCenter:
                wave_format->dwChannelMask |= SPEAKER_FRONT_RIGHT_OF_CENTER;
                break;
            case SoundIoChannelIdBackCenter:
                wave_format->dwChannelMask |= SPEAKER_BACK_CENTER;
                break;
            case SoundIoChannelIdSideLeft:
                wave_format->dwChannelMask |= SPEAKER_SIDE_LEFT;
                break;
            case SoundIoChannelIdSideRight:
                wave_format->dwChannelMask |= SPEAKER_SIDE_RIGHT;
                break;
            case SoundIoChannelIdTopCenter:
                wave_format->dwChannelMask |= SPEAKER_TOP_CENTER;
                break;
            case SoundIoChannelIdTopFrontLeft:
                wave_format->dwChannelMask |= SPEAKER_TOP_FRONT_LEFT;
                break;
            case SoundIoChannelIdTopFrontCenter:
                wave_format->dwChannelMask |= SPEAKER_TOP_FRONT_CENTER;
                break;
            case SoundIoChannelIdTopFrontRight:
                wave_format->dwChannelMask |= SPEAKER_TOP_FRONT_RIGHT;
                break;
            case SoundIoChannelIdTopBackLeft:
                wave_format->dwChannelMask |= SPEAKER_TOP_BACK_LEFT;
                break;
            case SoundIoChannelIdTopBackCenter:
                wave_format->dwChannelMask |= SPEAKER_TOP_BACK_CENTER;
                break;
            case SoundIoChannelIdTopBackRight:
                wave_format->dwChannelMask |= SPEAKER_TOP_BACK_RIGHT;
                break;
            default:
                soundio_panic("to_wave_format_layout: unsupported channel id");
        }
    }
}

// only needs to support the formats in test_formats
static void to_wave_format_format(SoundIoFormat format, WAVEFORMATEXTENSIBLE *wave_format) {
    switch (format) {
    case SoundIoFormatU8:
        wave_format->SubFormat = SOUNDIO_KSDATAFORMAT_SUBTYPE_PCM;
        wave_format->Format.wBitsPerSample = 8;
        wave_format->Samples.wValidBitsPerSample = 8;
        break;
    case SoundIoFormatS16LE:
        wave_format->SubFormat = SOUNDIO_KSDATAFORMAT_SUBTYPE_PCM;
        wave_format->Format.wBitsPerSample = 16;
        wave_format->Samples.wValidBitsPerSample = 16;
        break;
    case SoundIoFormatS24LE:
        wave_format->SubFormat = SOUNDIO_KSDATAFORMAT_SUBTYPE_PCM;
        wave_format->Format.wBitsPerSample = 32;
        wave_format->Samples.wValidBitsPerSample = 24;
        break;
    case SoundIoFormatS32LE:
        wave_format->SubFormat = SOUNDIO_KSDATAFORMAT_SUBTYPE_PCM;
        wave_format->Format.wBitsPerSample = 32;
        wave_format->Samples.wValidBitsPerSample = 32;
        break;
    case SoundIoFormatFloat32LE:
        wave_format->SubFormat = SOUNDIO_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        wave_format->Format.wBitsPerSample = 32;
        wave_format->Samples.wValidBitsPerSample = 32;
        break;
    case SoundIoFormatFloat64LE:
        wave_format->SubFormat = SOUNDIO_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        wave_format->Format.wBitsPerSample = 64;
        wave_format->Samples.wValidBitsPerSample = 64;
        break;
    default:
        soundio_panic("to_wave_format_format: unsupported format");
    }
}

static void complete_wave_format_data(WAVEFORMATEXTENSIBLE *wave_format) {
    wave_format->Format.nBlockAlign = (wave_format->Format.wBitsPerSample * wave_format->Format.nChannels) / 8;
    wave_format->Format.nAvgBytesPerSec = wave_format->Format.nSamplesPerSec * wave_format->Format.nBlockAlign;
}

static SoundIoDeviceAim data_flow_to_aim(EDataFlow data_flow) {
    return (data_flow == eRender) ? SoundIoDeviceAimOutput : SoundIoDeviceAimInput;
}


static double from_reference_time(REFERENCE_TIME rt) {
    return ((double)rt) / 10000000.0;
}

static REFERENCE_TIME to_reference_time(double seconds) {
    return seconds * 10000000.0 + 0.5;
}

static void destruct_device(SoundIoDevicePrivate *dev) {
    SoundIoDeviceWasapi *dw = &dev->backend_data.wasapi;
    if (dw->mm_device)
        IMMDevice_Release(dw->mm_device);
    dw->sample_rates.deinit();
}

struct RefreshDevices {
    IMMDeviceCollection *collection;
    IMMDevice *mm_device;
    IMMDevice *default_render_device;
    IMMDevice *default_capture_device;
    IMMEndpoint *endpoint;
    IPropertyStore *prop_store;
    IAudioClient *audio_client;
    LPWSTR lpwstr;
    PROPVARIANT prop_variant_value;
    WAVEFORMATEXTENSIBLE *wave_format;
    bool prop_variant_value_inited;
    SoundIoDevicesInfo *devices_info;
    SoundIoDevice *device_shared;
    SoundIoDevice *device_raw;
    char *default_render_id;
    int default_render_id_len;
    char *default_capture_id;
    int default_capture_id_len;
};

static void deinit_refresh_devices(RefreshDevices *rd) {
    soundio_destroy_devices_info(rd->devices_info);
    soundio_device_unref(rd->device_shared);
    soundio_device_unref(rd->device_raw);
    if (rd->mm_device)
        IMMDevice_Release(rd->mm_device);
    if (rd->default_render_device)
        IMMDevice_Release(rd->default_render_device);
    if (rd->default_capture_device)
        IMMDevice_Release(rd->default_capture_device);
    if (rd->collection)
        IMMDeviceCollection_Release(rd->collection);
    if (rd->lpwstr)
        CoTaskMemFree(rd->lpwstr);
    if (rd->endpoint)
        IMMEndpoint_Release(rd->endpoint);
    if (rd->prop_store)
        IPropertyStore_Release(rd->prop_store);
    if (rd->prop_variant_value_inited)
        PropVariantClear(&rd->prop_variant_value);
    if (rd->wave_format)
        CoTaskMemFree(rd->wave_format);
    if (rd->audio_client)
        IUnknown_Release(rd->audio_client);
}

static int detect_valid_layouts(RefreshDevices *rd, WAVEFORMATEXTENSIBLE *wave_format,
        SoundIoDevicePrivate *dev, AUDCLNT_SHAREMODE share_mode)
{
    SoundIoDevice *device = &dev->pub;
    HRESULT hr;

    device->layout_count = 0;
    device->layouts = allocate<SoundIoChannelLayout>(array_length(test_layouts));
    if (!device->layouts)
        return SoundIoErrorNoMem;

    WAVEFORMATEX *closest_match = nullptr;
    WAVEFORMATEXTENSIBLE orig_wave_format = *wave_format;

    for (int i = 0; i < array_length(test_formats); i += 1) {
        SoundIoChannelLayoutId test_layout_id = test_layouts[i];
        const SoundIoChannelLayout *test_layout = soundio_channel_layout_get_builtin(test_layout_id);
        to_wave_format_layout(test_layout, wave_format);
        complete_wave_format_data(wave_format);

        hr = IAudioClient_IsFormatSupported(rd->audio_client, share_mode,
                (WAVEFORMATEX*)wave_format, &closest_match);
        if (closest_match) {
            CoTaskMemFree(closest_match);
            closest_match = nullptr;
        }
        if (hr == S_OK) {
            device->layouts[device->layout_count++] = *test_layout;
        } else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT || hr == S_FALSE || hr == E_INVALIDARG) {
            continue;
        } else {
            *wave_format = orig_wave_format;
            return SoundIoErrorOpeningDevice;
        }
    }

    *wave_format = orig_wave_format;
    return 0;
}

static int detect_valid_formats(RefreshDevices *rd, WAVEFORMATEXTENSIBLE *wave_format,
        SoundIoDevicePrivate *dev, AUDCLNT_SHAREMODE share_mode)
{
    SoundIoDevice *device = &dev->pub;
    HRESULT hr;

    device->format_count = 0;
    device->formats = allocate<SoundIoFormat>(array_length(test_formats));
    if (!device->formats)
        return SoundIoErrorNoMem;

    WAVEFORMATEX *closest_match = nullptr;
    WAVEFORMATEXTENSIBLE orig_wave_format = *wave_format;

    for (int i = 0; i < array_length(test_formats); i += 1) {
        SoundIoFormat test_format = test_formats[i];
        to_wave_format_format(test_format, wave_format);
        complete_wave_format_data(wave_format);

        hr = IAudioClient_IsFormatSupported(rd->audio_client, share_mode,
                (WAVEFORMATEX*)wave_format, &closest_match);
        if (closest_match) {
            CoTaskMemFree(closest_match);
            closest_match = nullptr;
        }
        if (hr == S_OK) {
            device->formats[device->format_count++] = test_format;
        } else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT || hr == S_FALSE || hr == E_INVALIDARG) {
            continue;
        } else {
            *wave_format = orig_wave_format;
            return SoundIoErrorOpeningDevice;
        }
    }

    *wave_format = orig_wave_format;
    return 0;
}

static int add_sample_rate(SoundIoList<SoundIoSampleRateRange> *sample_rates, int *current_min, int the_max) {
    int err;
    if ((err = sample_rates->add_one()))
        return err;

    SoundIoSampleRateRange *last_range = &sample_rates->last();
    last_range->min = *current_min;
    last_range->max = the_max;
    return 0;
}

static int do_sample_rate_test(RefreshDevices *rd, SoundIoDevicePrivate *dev, WAVEFORMATEXTENSIBLE *wave_format,
        int test_sample_rate, AUDCLNT_SHAREMODE share_mode, int *current_min, int *last_success_rate)
{
    SoundIoDeviceWasapi *dw = &dev->backend_data.wasapi;
    WAVEFORMATEX *closest_match = nullptr;
    int err;

    wave_format->Format.nSamplesPerSec = test_sample_rate;
    HRESULT hr = IAudioClient_IsFormatSupported(rd->audio_client, share_mode,
            (WAVEFORMATEX*)wave_format, &closest_match);
    if (closest_match) {
        CoTaskMemFree(closest_match);
        closest_match = nullptr;
    }
    if (hr == S_OK) {
        if (*current_min == -1) {
            *current_min = test_sample_rate;
        }
        *last_success_rate = test_sample_rate;
    } else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT || hr == S_FALSE || hr == E_INVALIDARG) {
        if (*current_min != -1) {
            if ((err = add_sample_rate(&dw->sample_rates, current_min, *last_success_rate)))
                return err;
            *current_min = -1;
        }
    } else {
        return SoundIoErrorOpeningDevice;
    }

    return 0;
}

static int detect_valid_sample_rates(RefreshDevices *rd, WAVEFORMATEXTENSIBLE *wave_format,
        SoundIoDevicePrivate *dev, AUDCLNT_SHAREMODE share_mode)
{
    SoundIoDeviceWasapi *dw = &dev->backend_data.wasapi;
    int err;

    DWORD orig_sample_rate = wave_format->Format.nSamplesPerSec;

    assert(dw->sample_rates.length == 0);

    int current_min = -1;
    int last_success_rate = -1;
    for (int i = 0; i < array_length(test_sample_rates); i += 1) {
        for (int offset = -1; offset <= 1; offset += 1) {
            int test_sample_rate = test_sample_rates[i] + offset;
            if ((err = do_sample_rate_test(rd, dev, wave_format, test_sample_rate, share_mode,
                            &current_min, &last_success_rate)))
            {
                wave_format->Format.nSamplesPerSec = orig_sample_rate;
                return err;
            }
        }
    }

    if (current_min != -1) {
        if ((err = add_sample_rate(&dw->sample_rates, &current_min, last_success_rate))) {
            wave_format->Format.nSamplesPerSec = orig_sample_rate;
            return err;
        }
    }

    SoundIoDevice *device = &dev->pub;

    device->sample_rate_count = dw->sample_rates.length;
    device->sample_rates = dw->sample_rates.items;

    wave_format->Format.nSamplesPerSec = orig_sample_rate;
    return 0;
}


static int refresh_devices(SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoWasapi *siw = &si->backend_data.wasapi;
    RefreshDevices rd = {0};
    int err;
    HRESULT hr;

    if (FAILED(hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(siw->device_enumerator, eRender,
                    eMultimedia, &rd.default_render_device)))
    {
        deinit_refresh_devices(&rd);
        return SoundIoErrorOpeningDevice;
    }
    if (rd.lpwstr) {
        CoTaskMemFree(rd.lpwstr);
        rd.lpwstr = nullptr;
    }
    if (FAILED(hr = IMMDevice_GetId(rd.default_render_device, &rd.lpwstr))) {
        deinit_refresh_devices(&rd);
        return SoundIoErrorOpeningDevice;
    }
    if ((err = from_lpwstr(rd.lpwstr, &rd.default_render_id, &rd.default_render_id_len))) {
        deinit_refresh_devices(&rd);
        return err;
    }


    if (FAILED(hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(siw->device_enumerator, eCapture,
                    eMultimedia, &rd.default_capture_device)))
    {
        deinit_refresh_devices(&rd);
        return SoundIoErrorOpeningDevice;
    }
    if (rd.lpwstr) {
        CoTaskMemFree(rd.lpwstr);
        rd.lpwstr = nullptr;
    }
    if (FAILED(hr = IMMDevice_GetId(rd.default_capture_device, &rd.lpwstr))) {
        deinit_refresh_devices(&rd);
        return SoundIoErrorOpeningDevice;
    }
    if ((err = from_lpwstr(rd.lpwstr, &rd.default_capture_id, &rd.default_capture_id_len))) {
        deinit_refresh_devices(&rd);
        return err;
    }


    if (FAILED(hr = IMMDeviceEnumerator_EnumAudioEndpoints(siw->device_enumerator,
                    eAll, DEVICE_STATE_ACTIVE, &rd.collection)))
    {
        deinit_refresh_devices(&rd);
        return SoundIoErrorOpeningDevice;
    }

    UINT unsigned_count;
    if (FAILED(hr = IMMDeviceCollection_GetCount(rd.collection, &unsigned_count))) {
        deinit_refresh_devices(&rd);
        return SoundIoErrorOpeningDevice;
    }

    if (unsigned_count > (UINT)INT_MAX) {
        deinit_refresh_devices(&rd);
        return SoundIoErrorIncompatibleDevice;
    }

    int device_count = unsigned_count;

    if (!(rd.devices_info = allocate<SoundIoDevicesInfo>(1))) {
        deinit_refresh_devices(&rd);
        return SoundIoErrorNoMem;
    }
    rd.devices_info->default_input_index = -1;
    rd.devices_info->default_output_index = -1;

    for (int device_i = 0; device_i < device_count; device_i += 1) {
        if (rd.mm_device) {
            IMMDevice_Release(rd.mm_device);
            rd.mm_device = nullptr;
        }
        if (FAILED(hr = IMMDeviceCollection_Item(rd.collection, device_i, &rd.mm_device))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        if (rd.lpwstr) {
            CoTaskMemFree(rd.lpwstr);
            rd.lpwstr = nullptr;
        }
        if (FAILED(hr = IMMDevice_GetId(rd.mm_device, &rd.lpwstr))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }



        SoundIoDevicePrivate *dev_shared = allocate<SoundIoDevicePrivate>(1);
        if (!dev_shared) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorNoMem;
        }
        SoundIoDeviceWasapi *dev_w_shared = &dev_shared->backend_data.wasapi;
        dev_shared->destruct = destruct_device;
        assert(!rd.device_shared);
        rd.device_shared = &dev_shared->pub;
        rd.device_shared->ref_count = 1;
        rd.device_shared->soundio = soundio;
        rd.device_shared->is_raw = false;

        SoundIoDevicePrivate *dev_raw = allocate<SoundIoDevicePrivate>(1);
        if (!dev_raw) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorNoMem;
        }
        SoundIoDeviceWasapi *dev_w_raw = &dev_raw->backend_data.wasapi;
        dev_raw->destruct = destruct_device;
        assert(!rd.device_raw);
        rd.device_raw = &dev_raw->pub;
        rd.device_raw->ref_count = 1;
        rd.device_raw->soundio = soundio;
        rd.device_raw->is_raw = true;

        int device_id_len;
        if ((err = from_lpwstr(rd.lpwstr, &rd.device_shared->id, &device_id_len))) {
            deinit_refresh_devices(&rd);
            return err;
        }

        rd.device_raw->id = soundio_str_dupe(rd.device_shared->id, device_id_len);
        if (!rd.device_raw->id) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorNoMem;
        }

        if (rd.audio_client) {
            IUnknown_Release(rd.audio_client);
            rd.audio_client = nullptr;
        }
        if (FAILED(hr = IMMDevice_Activate(rd.mm_device, IID_IAudioClient,
                        CLSCTX_ALL, nullptr, (void**)&rd.audio_client)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        REFERENCE_TIME default_device_period;
        REFERENCE_TIME min_device_period;
        if (FAILED(hr = IAudioClient_GetDevicePeriod(rd.audio_client,
                        &default_device_period, &min_device_period)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        dev_w_shared->period_duration = from_reference_time(default_device_period);
        dev_w_raw->period_duration = from_reference_time(min_device_period);


        if (rd.endpoint) {
            IMMEndpoint_Release(rd.endpoint);
            rd.endpoint = nullptr;
        }
        if (FAILED(hr = IMMDevice_QueryInterface(rd.mm_device, IID_IMMEndpoint, (void**)&rd.endpoint))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        EDataFlow data_flow;
        if (FAILED(hr = IMMEndpoint_GetDataFlow(rd.endpoint, &data_flow))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        rd.device_shared->aim = data_flow_to_aim(data_flow);
        rd.device_raw->aim = rd.device_shared->aim;

        if (rd.prop_store) {
            IPropertyStore_Release(rd.prop_store);
            rd.prop_store = nullptr;
        }
        if (FAILED(hr = IMMDevice_OpenPropertyStore(rd.mm_device, STGM_READ, &rd.prop_store))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        if (rd.prop_variant_value_inited) {
            PropVariantClear(&rd.prop_variant_value);
            rd.prop_variant_value_inited = false;
        }
        PropVariantInit(&rd.prop_variant_value);
        rd.prop_variant_value_inited = true;
        if (FAILED(hr = IPropertyStore_GetValue(rd.prop_store,
                        PKEY_Device_FriendlyName, &rd.prop_variant_value)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        if (!rd.prop_variant_value.pwszVal) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        int device_name_len;
        if ((err = from_lpwstr(rd.prop_variant_value.pwszVal, &rd.device_shared->name, &device_name_len))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        rd.device_raw->name = soundio_str_dupe(rd.device_shared->name, device_name_len);
        if (!rd.device_raw->name) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorNoMem;
        }

        // Get the format that WASAPI opens the device with for shared streams.
        // This is guaranteed to work, so we use this to modulate the sample
        // rate while holding the format constant and vice versa.
        if (rd.prop_variant_value_inited) {
            PropVariantClear(&rd.prop_variant_value);
            rd.prop_variant_value_inited = false;
        }
        PropVariantInit(&rd.prop_variant_value);
        rd.prop_variant_value_inited = true;
        if (FAILED(hr = IPropertyStore_GetValue(rd.prop_store, PKEY_AudioEngine_DeviceFormat,
                        &rd.prop_variant_value)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        WAVEFORMATEXTENSIBLE *valid_wave_format = (WAVEFORMATEXTENSIBLE *)rd.prop_variant_value.blob.pBlobData;
        if (valid_wave_format->Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        if ((err = detect_valid_sample_rates(&rd, valid_wave_format, dev_raw,
                        AUDCLNT_SHAREMODE_EXCLUSIVE)))
        {
            deinit_refresh_devices(&rd);
            return err;
        }
        if ((err = detect_valid_formats(&rd, valid_wave_format, dev_raw,
                        AUDCLNT_SHAREMODE_EXCLUSIVE)))
        {
            deinit_refresh_devices(&rd);
            return err;
        }

        if (rd.wave_format) {
            CoTaskMemFree(rd.wave_format);
            rd.wave_format = nullptr;
        }
        if (FAILED(hr = IAudioClient_GetMixFormat(rd.audio_client, (WAVEFORMATEX**)&rd.wave_format))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        if (rd.wave_format->Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        rd.device_shared->sample_rate_current = rd.wave_format->Format.nSamplesPerSec;
        rd.device_shared->current_format = from_wave_format_format(rd.wave_format);


        // WASAPI performs resampling in shared mode, so any value is valid.
        // Let's pick some reasonable min and max values.
        rd.device_shared->sample_rate_count = 1;
        rd.device_shared->sample_rates = &dev_shared->prealloc_sample_rate_range;
        rd.device_shared->sample_rates[0].min = min(SOUNDIO_MIN_SAMPLE_RATE,
                rd.device_shared->sample_rate_current);
        rd.device_shared->sample_rates[0].max = max(SOUNDIO_MAX_SAMPLE_RATE,
                rd.device_shared->sample_rate_current);

        if ((err = detect_valid_formats(&rd, rd.wave_format, dev_shared,
                        AUDCLNT_SHAREMODE_SHARED)))
        {
            deinit_refresh_devices(&rd);
            return err;
        }

        from_wave_format_layout(rd.wave_format, &rd.device_shared->current_layout);
        rd.device_shared->layout_count = 1;
        rd.device_shared->layouts = &rd.device_shared->current_layout;

        if ((err = detect_valid_layouts(&rd, valid_wave_format, dev_raw,
                        AUDCLNT_SHAREMODE_EXCLUSIVE)))
        {
            deinit_refresh_devices(&rd);
            return err;
        }

        IMMDevice_AddRef(rd.mm_device);
        dev_w_shared->mm_device = rd.mm_device;
        dev_w_raw->mm_device = rd.mm_device;
        rd.mm_device = nullptr;

        SoundIoList<SoundIoDevice *> *device_list;
        if (rd.device_shared->aim == SoundIoDeviceAimOutput) {
            device_list = &rd.devices_info->output_devices;
            if (soundio_streql(rd.device_shared->id, device_id_len,
                        rd.default_render_id, rd.default_render_id_len))
            {
                rd.devices_info->default_output_index = device_list->length;
            }
        } else {
            assert(rd.device_shared->aim == SoundIoDeviceAimInput);
            device_list = &rd.devices_info->input_devices;
            if (soundio_streql(rd.device_shared->id, device_id_len,
                        rd.default_capture_id, rd.default_capture_id_len))
            {
                rd.devices_info->default_input_index = device_list->length;
            }
        }

        if ((err = device_list->append(rd.device_shared))) {
            deinit_refresh_devices(&rd);
            return err;
        }
        rd.device_shared = nullptr;

        if ((err = device_list->append(rd.device_raw))) {
            deinit_refresh_devices(&rd);
            return err;
        }
        rd.device_raw = nullptr;
    }

    soundio_os_mutex_lock(siw->mutex);
    soundio_destroy_devices_info(siw->ready_devices_info);
    siw->ready_devices_info = rd.devices_info;
    soundio->on_events_signal(soundio);
    soundio_os_mutex_unlock(siw->mutex);

    rd.devices_info = nullptr;
    deinit_refresh_devices(&rd);

    return 0;
}


static void shutdown_backend(SoundIoPrivate *si, int err) {
    SoundIo *soundio = &si->pub;
    SoundIoWasapi *siw = &si->backend_data.wasapi;
    soundio_os_mutex_lock(siw->mutex);
    siw->shutdown_err = err;
    soundio->on_events_signal(soundio);
    soundio_os_mutex_unlock(siw->mutex);
}

static void device_thread_run(void *arg) {
    SoundIoPrivate *si = (SoundIoPrivate *)arg;
    SoundIo *soundio = &si->pub;
    SoundIoWasapi *siw = &si->backend_data.wasapi;
    int err;

    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr,
            CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&siw->device_enumerator);
    if (FAILED(hr)) {
        shutdown_backend(si, SoundIoErrorSystemResources);
        if (!siw->have_devices_flag.exchange(true)) {
            soundio_os_cond_signal(siw->cond, nullptr);
            soundio->on_events_signal(soundio);
        }
        return;
    }

    if (FAILED(hr = IMMDeviceEnumerator_RegisterEndpointNotificationCallback(
                    siw->device_enumerator, &siw->device_events)))
    {
        shutdown_backend(si, SoundIoErrorSystemResources);
        if (!siw->have_devices_flag.exchange(true)) {
            soundio_os_cond_signal(siw->cond, nullptr);
            soundio->on_events_signal(soundio);
        }
        return;
    }

    for (;;) {
        if (!siw->abort_flag.test_and_set())
            break;
        if (siw->device_scan_queued.exchange(false)) {
            err = refresh_devices(si);
            if (err)
                shutdown_backend(si, err);
            if (!siw->have_devices_flag.exchange(true)) {
                soundio_os_cond_signal(siw->cond, nullptr);
                soundio->on_events_signal(soundio);
            }
            if (err)
                break;
            soundio_os_cond_signal(siw->cond, nullptr);
        }
        soundio_os_cond_wait(siw->scan_devices_cond, nullptr);
    }

    IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(siw->device_enumerator, &siw->device_events);
    IMMDeviceEnumerator_Release(siw->device_enumerator);
    siw->device_enumerator = nullptr;
}

static void block_until_have_devices(SoundIoWasapi *siw) {
    if (siw->have_devices_flag.load())
        return;
    while (!siw->have_devices_flag.load())
        soundio_os_cond_wait(siw->cond, nullptr);
}

static void flush_events_wasapi(struct SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoWasapi *siw = &si->backend_data.wasapi;
    block_until_have_devices(siw);

    bool change = false;
    bool cb_shutdown = false;
    SoundIoDevicesInfo *old_devices_info = nullptr;

    soundio_os_mutex_lock(siw->mutex);

    if (siw->shutdown_err && !siw->emitted_shutdown_cb) {
        siw->emitted_shutdown_cb = true;
        cb_shutdown = true;
    } else if (siw->ready_devices_info) {
        old_devices_info = si->safe_devices_info;
        si->safe_devices_info = siw->ready_devices_info;
        siw->ready_devices_info = nullptr;
        change = true;
    }

    soundio_os_mutex_unlock(siw->mutex);

    if (cb_shutdown)
        soundio->on_backend_disconnect(soundio, siw->shutdown_err);
    else if (change)
        soundio->on_devices_change(soundio);

    soundio_destroy_devices_info(old_devices_info);
}

static void wait_events_wasapi(struct SoundIoPrivate *si) {
    SoundIoWasapi *siw = &si->backend_data.wasapi;
    flush_events_wasapi(si);
    soundio_os_cond_wait(siw->cond, nullptr);
}

static void wakeup_wasapi(struct SoundIoPrivate *si) {
    SoundIoWasapi *siw = &si->backend_data.wasapi;
    soundio_os_cond_signal(siw->cond, nullptr);
}

static void outstream_destroy_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamWasapi *osw = &os->backend_data.wasapi;

    if (osw->thread) {
        osw->thread_exit_flag.clear();
        soundio_os_mutex_lock(osw->mutex);
        soundio_os_cond_signal(osw->cond, osw->mutex);
        soundio_os_mutex_unlock(osw->mutex);
        soundio_os_thread_destroy(osw->thread);
    }

    if (osw->audio_render_client)
        IUnknown_Release(osw->audio_render_client);
    if (osw->audio_clock_adjustment)
        IUnknown_Release(osw->audio_clock_adjustment);
    if (osw->audio_client)
        IUnknown_Release(osw->audio_client);

    soundio_os_cond_destroy(osw->cond);
    soundio_os_mutex_destroy(osw->mutex);

    CoUninitialize();
}

static int outstream_open_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamWasapi *osw = &os->backend_data.wasapi;
    SoundIoOutStream *outstream = &os->pub;
    SoundIoDevice *device = outstream->device;
    SoundIoDevicePrivate *dev = (SoundIoDevicePrivate *)device;
    SoundIoDeviceWasapi *dw = &dev->backend_data.wasapi;
    HRESULT hr;

    if (FAILED(hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        outstream_destroy_wasapi(si, os);
        return SoundIoErrorNoMem;
    }

    osw->is_raw = device->is_raw;

    if (!(osw->cond = soundio_os_cond_create())) {
        outstream_destroy_wasapi(si, os);
        return SoundIoErrorNoMem;
    }

    if (!(osw->mutex = soundio_os_mutex_create())) {
        outstream_destroy_wasapi(si, os);
        return SoundIoErrorNoMem;
    }

    if (FAILED(hr = IMMDevice_Activate(dw->mm_device, IID_IAudioClient,
                    CLSCTX_ALL, nullptr, (void**)&osw->audio_client)))
    {
        outstream_destroy_wasapi(si, os);
        return SoundIoErrorOpeningDevice;
    }


    AUDCLNT_SHAREMODE share_mode;
    DWORD flags;
    REFERENCE_TIME buffer_duration;
    REFERENCE_TIME periodicity;
    WAVEFORMATEXTENSIBLE wave_format = {0};
    wave_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wave_format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    if (osw->is_raw) {
        wave_format.Format.nSamplesPerSec = outstream->sample_rate;
        flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        share_mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
        periodicity = to_reference_time(dw->period_duration);
        buffer_duration = periodicity;
    } else {
        WAVEFORMATEXTENSIBLE *mix_format;
        if (FAILED(hr = IAudioClient_GetMixFormat(osw->audio_client, (WAVEFORMATEX **)&mix_format))) {
            outstream_destroy_wasapi(si, os);
            return SoundIoErrorOpeningDevice;
        }
        wave_format.Format.nSamplesPerSec = mix_format->Format.nSamplesPerSec;
        CoTaskMemFree(mix_format);
        mix_format = nullptr;
        osw->need_resample = (wave_format.Format.nSamplesPerSec != (DWORD)outstream->sample_rate);
        // TODO do we need SOUNDIO_AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM ?
        flags = osw->need_resample ? AUDCLNT_STREAMFLAGS_RATEADJUST : 0;
        share_mode = AUDCLNT_SHAREMODE_SHARED;
        periodicity = 0;
        buffer_duration = to_reference_time(4.0);
    }
    to_wave_format_layout(&outstream->layout, &wave_format);
    to_wave_format_format(outstream->format, &wave_format);
    complete_wave_format_data(&wave_format);

    if (FAILED(hr = IAudioClient_Initialize(osw->audio_client, share_mode, flags,
            buffer_duration, periodicity, (WAVEFORMATEX*)&wave_format, nullptr)))
    {
        if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
            if (FAILED(hr = IAudioClient_GetBufferSize(osw->audio_client, &osw->buffer_frame_count))) {
                outstream_destroy_wasapi(si, os);
                return SoundIoErrorOpeningDevice;
            }
            IUnknown_Release(osw->audio_client);
            osw->audio_client = nullptr;
            if (FAILED(hr = IMMDevice_Activate(dw->mm_device, IID_IAudioClient,
                            CLSCTX_ALL, nullptr, (void**)&osw->audio_client)))
            {
                outstream_destroy_wasapi(si, os);
                return SoundIoErrorOpeningDevice;
            }
            if (!osw->is_raw) {
                WAVEFORMATEXTENSIBLE *mix_format;
                if (FAILED(hr = IAudioClient_GetMixFormat(osw->audio_client, (WAVEFORMATEX **)&mix_format))) {
                    outstream_destroy_wasapi(si, os);
                    return SoundIoErrorOpeningDevice;
                }
                wave_format.Format.nSamplesPerSec = mix_format->Format.nSamplesPerSec;
                CoTaskMemFree(mix_format);
                mix_format = nullptr;
                osw->need_resample = (wave_format.Format.nSamplesPerSec != (DWORD)outstream->sample_rate);
                // TODO do we need SOUNDIO_AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM ?
                flags = osw->need_resample ? AUDCLNT_STREAMFLAGS_RATEADJUST : 0;
                to_wave_format_layout(&outstream->layout, &wave_format);
                to_wave_format_format(outstream->format, &wave_format);
                complete_wave_format_data(&wave_format);
            }

            buffer_duration = to_reference_time(osw->buffer_frame_count / outstream->sample_rate);
            if (osw->is_raw)
                periodicity = buffer_duration;
            if (FAILED(hr = IAudioClient_Initialize(osw->audio_client, share_mode, flags,
                    buffer_duration, periodicity, (WAVEFORMATEX*)&wave_format, nullptr)))
            {
                outstream_destroy_wasapi(si, os);
                return SoundIoErrorOpeningDevice;
            }
        } else {
            outstream_destroy_wasapi(si, os);
            return SoundIoErrorOpeningDevice;
        }
    }
    if (FAILED(hr = IAudioClient_GetBufferSize(osw->audio_client, &osw->buffer_frame_count))) {
        outstream_destroy_wasapi(si, os);
        return SoundIoErrorOpeningDevice;
    }
    outstream->software_latency = osw->buffer_frame_count / (double)outstream->sample_rate;

    if (osw->is_raw) {
        soundio_panic("TODO event callback");
    } else if (osw->need_resample) {
        if (FAILED(hr = IAudioClient_GetService(osw->audio_client, IID_IAudioClockAdjustment,
                        (void**)&osw->audio_clock_adjustment)))
        {
            outstream_destroy_wasapi(si, os);
            return SoundIoErrorOpeningDevice;
        }
        if (FAILED(hr = IAudioClockAdjustment_SetSampleRate(osw->audio_clock_adjustment,
                        outstream->sample_rate)))
        {
            outstream_destroy_wasapi(si, os);
            return SoundIoErrorOpeningDevice;
        }
    }

    if (FAILED(hr = IAudioClient_GetService(osw->audio_client, IID_IAudioRenderClient,
                    (void **)&osw->audio_render_client)))
    {
        outstream_destroy_wasapi(si, os);
        return SoundIoErrorOpeningDevice;
    }

    return 0;
}


static int outstream_pause_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os, bool pause) {
    SoundIoOutStreamWasapi *osw = &os->backend_data.wasapi;
    HRESULT hr;
    if (FAILED(hr = IAudioClient_Stop(osw->audio_client)))
        return SoundIoErrorStreaming;
    return 0;
}

void outstream_shared_run(void *arg) {
    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate *) arg;
    SoundIoOutStreamWasapi *osw = &os->backend_data.wasapi;
    SoundIoOutStream *outstream = &os->pub;

    HRESULT hr;

    UINT32 frames_used;
    if (FAILED(hr = IAudioClient_GetCurrentPadding(osw->audio_client, &frames_used))) {
        outstream->error_callback(outstream, SoundIoErrorStreaming);
        return;
    }
    osw->writable_frame_count = osw->buffer_frame_count - frames_used;
    if (osw->writable_frame_count <= 0) {
        outstream->error_callback(outstream, SoundIoErrorStreaming);
        return;
    }
    outstream->write_callback(outstream, 0, osw->writable_frame_count);

    if (FAILED(hr = IAudioClient_Start(osw->audio_client))) {
        outstream->error_callback(outstream, SoundIoErrorStreaming);
        return;
    }

    for (;;) {
        if (FAILED(hr = IAudioClient_GetCurrentPadding(osw->audio_client, &frames_used))) {
            outstream->error_callback(outstream, SoundIoErrorStreaming);
            return;
        }
        osw->writable_frame_count = osw->buffer_frame_count - frames_used;
        double time_until_underrun = frames_used / (double)outstream->sample_rate;
        double wait_time = time_until_underrun / 2.0;
        soundio_os_mutex_lock(osw->mutex);
        soundio_os_cond_timed_wait(osw->cond, osw->mutex, wait_time);
        if (!osw->thread_exit_flag.test_and_set()) {
            soundio_os_mutex_unlock(osw->mutex);
            return;
        }
        soundio_os_mutex_unlock(osw->mutex);

        if (FAILED(hr = IAudioClient_GetCurrentPadding(osw->audio_client, &frames_used))) {
            outstream->error_callback(outstream, SoundIoErrorStreaming);
            return;
        }
        osw->writable_frame_count = osw->buffer_frame_count - frames_used;
        if (osw->writable_frame_count > 0)
            outstream->write_callback(outstream, 0, osw->writable_frame_count);
    }
}

static int outstream_start_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamWasapi *osw = &os->backend_data.wasapi;
    int err;

    if (osw->is_raw) {
        soundio_panic("TODO start raw");
    } else {
        assert(!osw->thread);

        osw->thread_exit_flag.test_and_set();
        if ((err = soundio_os_thread_create(outstream_shared_run, os, true, &osw->thread)))
            return err;
    }

    return 0;
}

static int outstream_begin_write_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    SoundIoOutStreamWasapi *osw = &os->backend_data.wasapi;
    SoundIoOutStream *outstream = &os->pub;
    HRESULT hr;

    osw->write_frame_count = *frame_count;

    char *data;
    if (FAILED(hr = IAudioRenderClient_GetBuffer(osw->audio_render_client,
                    osw->write_frame_count, (BYTE**)&data)))
    {
        return SoundIoErrorStreaming;
    }

    for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
        osw->areas[ch].ptr = data + ch * outstream->bytes_per_sample;
        osw->areas[ch].step = outstream->bytes_per_frame;
    }

    *out_areas = osw->areas;

    return 0;
}

static int outstream_end_write_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamWasapi *osw = &os->backend_data.wasapi;
    HRESULT hr;
    if (FAILED(hr = IAudioRenderClient_ReleaseBuffer(osw->audio_render_client, osw->write_frame_count, 0))) {
        return SoundIoErrorStreaming;
    }
    return 0;
}

static int outstream_clear_buffer_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamWasapi *osw = &os->backend_data.wasapi;
    HRESULT hr;
    if (FAILED(hr = IAudioClient_Reset(osw->audio_client)))
        return SoundIoErrorStreaming;

    return 0;
}



static void instream_destroy_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int instream_open_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int instream_pause_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is, bool pause) {
    soundio_panic("TODO");
}

static int instream_start_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int instream_begin_read_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    soundio_panic("TODO");
}

static int instream_end_read_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}


static void destroy_wasapi(struct SoundIoPrivate *si) {
    SoundIoWasapi *siw = &si->backend_data.wasapi;

    if (siw->thread) {
        siw->abort_flag.clear();
        soundio_os_cond_signal(siw->scan_devices_cond, nullptr);
        soundio_os_thread_destroy(siw->thread);
    }

    if (siw->cond)
        soundio_os_cond_destroy(siw->cond);

    if (siw->scan_devices_cond)
        soundio_os_cond_destroy(siw->scan_devices_cond);

    if (siw->mutex)
        soundio_os_mutex_destroy(siw->mutex);

    soundio_destroy_devices_info(siw->ready_devices_info);
}

static inline SoundIoPrivate *soundio_MMNotificationClient_si(IMMNotificationClient *client) {
    SoundIoWasapi *siw = (SoundIoWasapi *)(((char *)client) - offsetof(SoundIoWasapi, device_events));
    SoundIoPrivate *si = (SoundIoPrivate *)(((char *)siw) - offsetof(SoundIoPrivate, backend_data));
    return si;
}

static STDMETHODIMP soundio_MMNotificationClient_QueryInterface(IMMNotificationClient *client,
        REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IMMNotificationClient)) {
        *ppv = client;
        IUnknown_AddRef(client);
        return S_OK;
    } else {
       *ppv = nullptr;
        return E_NOINTERFACE;
    }
}

static STDMETHODIMP_(ULONG) soundio_MMNotificationClient_AddRef(IMMNotificationClient *client) {
    SoundIoPrivate *si = soundio_MMNotificationClient_si(client);
    SoundIoWasapi *siw = &si->backend_data.wasapi;
    return InterlockedIncrement(&siw->device_events_refs);
}

static STDMETHODIMP_(ULONG) soundio_MMNotificationClient_Release(IMMNotificationClient *client) {
    SoundIoPrivate *si = soundio_MMNotificationClient_si(client);
    SoundIoWasapi *siw = &si->backend_data.wasapi;
    return InterlockedDecrement(&siw->device_events_refs);
}

static HRESULT queue_device_scan(IMMNotificationClient *client) {
    SoundIoPrivate *si = soundio_MMNotificationClient_si(client);
    SoundIoWasapi *siw = &si->backend_data.wasapi;

    siw->device_scan_queued.store(true);
    soundio_os_cond_signal(siw->scan_devices_cond, nullptr);

    return S_OK;
}

static STDMETHODIMP soundio_MMNotificationClient_OnDeviceStateChanged(IMMNotificationClient *client,
        LPCWSTR wid, DWORD state)
{
    return queue_device_scan(client);
}

static STDMETHODIMP soundio_MMNotificationClient_OnDeviceAdded(IMMNotificationClient *client, LPCWSTR wid) {
    return queue_device_scan(client);
}

static STDMETHODIMP soundio_MMNotificationClient_OnDeviceRemoved(IMMNotificationClient *client, LPCWSTR wid) {
    return queue_device_scan(client);
}

static STDMETHODIMP soundio_MMNotificationClient_OnDefaultDeviceChange(IMMNotificationClient *client,
        EDataFlow flow, ERole role, LPCWSTR wid)
{
    return queue_device_scan(client);
}

static STDMETHODIMP soundio_MMNotificationClient_OnPropertyValueChanged(IMMNotificationClient *client,
        LPCWSTR wid, const PROPERTYKEY key)
{
    return queue_device_scan(client);
}


static struct IMMNotificationClientVtbl soundio_MMNotificationClient = {
    soundio_MMNotificationClient_QueryInterface,
    soundio_MMNotificationClient_AddRef,
    soundio_MMNotificationClient_Release,
    soundio_MMNotificationClient_OnDeviceStateChanged,
    soundio_MMNotificationClient_OnDeviceAdded,
    soundio_MMNotificationClient_OnDeviceRemoved,
    soundio_MMNotificationClient_OnDefaultDeviceChange,
    soundio_MMNotificationClient_OnPropertyValueChanged,
};

int soundio_wasapi_init(SoundIoPrivate *si) {
    SoundIoWasapi *siw = &si->backend_data.wasapi;
    int err;

    siw->have_devices_flag.store(false);
    siw->device_scan_queued.store(true);
    siw->abort_flag.test_and_set();

    siw->mutex = soundio_os_mutex_create();
    if (!siw->mutex) {
        destroy_wasapi(si);
        return SoundIoErrorNoMem;
    }

    siw->cond = soundio_os_cond_create();
    if (!siw->cond) {
        destroy_wasapi(si);
        return SoundIoErrorNoMem;
    }

    siw->scan_devices_cond = soundio_os_cond_create();
    if (!siw->scan_devices_cond) {
        destroy_wasapi(si);
        return SoundIoErrorNoMem;
    }

    siw->device_events.lpVtbl = &soundio_MMNotificationClient;
    siw->device_events_refs = 1;

    if ((err = soundio_os_thread_create(device_thread_run, si, false, &siw->thread))) {
        destroy_wasapi(si);
        return err;
    }

    si->destroy = destroy_wasapi;
    si->flush_events = flush_events_wasapi;
    si->wait_events = wait_events_wasapi;
    si->wakeup = wakeup_wasapi;

    si->outstream_open = outstream_open_wasapi;
    si->outstream_destroy = outstream_destroy_wasapi;
    si->outstream_start = outstream_start_wasapi;
    si->outstream_begin_write = outstream_begin_write_wasapi;
    si->outstream_end_write = outstream_end_write_wasapi;
    si->outstream_clear_buffer = outstream_clear_buffer_wasapi;
    si->outstream_pause = outstream_pause_wasapi;

    si->instream_open = instream_open_wasapi;
    si->instream_destroy = instream_destroy_wasapi;
    si->instream_start = instream_start_wasapi;
    si->instream_begin_read = instream_begin_read_wasapi;
    si->instream_end_read = instream_end_read_wasapi;
    si->instream_pause = instream_pause_wasapi;

    return 0;
}
