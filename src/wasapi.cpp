#include "wasapi.hpp"
#include "soundio.hpp"

#include <stdio.h>

#define INITGUID
#define CINTERFACE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmreg.h>
#include <audioclient.h>

// Attempting to use the Windows-supplied versions of these constants resulted
// in `undefined reference` linker errors.
const static GUID SOUNDIO_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {
    0x00000003,0x0000,0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

const static GUID SOUNDIO_KSDATAFORMAT_SUBTYPE_PCM = {
    0x00000001,0x0000,0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

// converts a windows wide string to a UTF-8 encoded char *
// Possible errors:
//  * SoundIoErrorNoMem
//  * SoundIoErrorEncodingString
static int from_lpwstr(LPWSTR lpwstr, char **out_str, int *out_str_len) {
    DWORD flags = 0;
    int buf_size = WideCharToMultiByte(CP_UTF8, flags, lpwstr, -1, nullptr, 0, nullptr, nullptr);

    if (buf_size == 0)
        return SoundIoErrorEncodingString;

    char *buf = allocate<char>(buf_size + 1);
    if (!buf)
        return SoundIoErrorNoMem;

    if (WideCharToMultiByte(CP_UTF8, flags, lpwstr, -1, buf, buf_size, nullptr, nullptr) != buf_size) {
        free(buf);
        return SoundIoErrorEncodingString;
    }

    *out_str = buf;
    *out_str_len = buf_size;

    return 0;
}

static void from_wave_format_layout(WAVEFORMATEXTENSIBLE *wave_format, SoundIoChannelLayout *layout) {
    assert(wave_format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE);
    layout->channel_count = 0;
    if (wave_format->dwChannelMask & SPEAKER_FRONT_LEFT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdFrontLeft;
    if (wave_format->dwChannelMask & SPEAKER_FRONT_RIGHT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdFrontRight;
    if (wave_format->dwChannelMask & SPEAKER_FRONT_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdFrontCenter;
    if (wave_format->dwChannelMask & SPEAKER_LOW_FREQUENCY)
        layout->channels[layout->channel_count++] = SoundIoChannelIdLfe;
    if (wave_format->dwChannelMask & SPEAKER_BACK_LEFT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdBackLeft;
    if (wave_format->dwChannelMask & SPEAKER_BACK_RIGHT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdBackRight;
    if (wave_format->dwChannelMask & SPEAKER_FRONT_LEFT_OF_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdFrontLeftCenter;
    if (wave_format->dwChannelMask & SPEAKER_FRONT_RIGHT_OF_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdFrontRightCenter;
    if (wave_format->dwChannelMask & SPEAKER_BACK_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdBackCenter;
    if (wave_format->dwChannelMask & SPEAKER_SIDE_LEFT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdSideLeft;
    if (wave_format->dwChannelMask & SPEAKER_SIDE_RIGHT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdSideRight;
    if (wave_format->dwChannelMask & SPEAKER_TOP_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopCenter;
    if (wave_format->dwChannelMask & SPEAKER_TOP_FRONT_LEFT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopFrontLeft;
    if (wave_format->dwChannelMask & SPEAKER_TOP_FRONT_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopFrontCenter;
    if (wave_format->dwChannelMask & SPEAKER_TOP_FRONT_RIGHT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopFrontRight;
    if (wave_format->dwChannelMask & SPEAKER_TOP_BACK_LEFT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopBackLeft;
    if (wave_format->dwChannelMask & SPEAKER_TOP_BACK_CENTER)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopBackCenter;
    if (wave_format->dwChannelMask & SPEAKER_TOP_BACK_RIGHT)
        layout->channels[layout->channel_count++] = SoundIoChannelIdTopBackRight;

    soundio_channel_layout_detect_builtin(layout);
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

static SoundIoDeviceAim data_flow_to_aim(EDataFlow data_flow) {
    return (data_flow == eRender) ? SoundIoDeviceAimOutput : SoundIoDeviceAimInput;
}


static double from_reference_time(REFERENCE_TIME rt) {
    return ((double)rt) / 10000000.0;
}

struct RefreshDevices {
    IMMDeviceCollection *collection;
    IMMDevice *mm_device;
    IMMDevice *default_render_device;
    IMMDevice *default_capture_device;
    IMMEndpoint *endpoint;
    IPropertyStore *prop_store;
    LPWSTR lpwstr;
    PROPVARIANT prop_variant_value;
    WAVEFORMATEXTENSIBLE *wave_format;
    bool prop_variant_value_inited;
    SoundIoDevicesInfo *devices_info;
    SoundIoDevice *device;
    char *default_render_id;
    int default_render_id_len;
    char *default_capture_id;
    int default_capture_id_len;
};

static void deinit_refresh_devices(RefreshDevices *rd) {
    soundio_destroy_devices_info(rd->devices_info);
    soundio_device_unref(rd->device);
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
}

static void destruct_device(SoundIoDevicePrivate *dev) {
    SoundIoDeviceWasapi *dw = &dev->backend_data.wasapi;
    if (dw->audio_client)
        IUnknown_Release(dw->audio_client);
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
    if (rd.lpwstr)
        CoTaskMemFree(rd.lpwstr);
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
    if (rd.lpwstr)
        CoTaskMemFree(rd.lpwstr);
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
        if (rd.mm_device)
            IMMDevice_Release(rd.mm_device);
        if (FAILED(hr = IMMDeviceCollection_Item(rd.collection, device_i, &rd.mm_device))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        if (rd.lpwstr)
            CoTaskMemFree(rd.lpwstr);
        if (FAILED(hr = IMMDevice_GetId(rd.mm_device, &rd.lpwstr))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }



        SoundIoDevicePrivate *dev = allocate<SoundIoDevicePrivate>(1);
        if (!dev) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorNoMem;
        }
        SoundIoDeviceWasapi *dw = &dev->backend_data.wasapi;
        dev->destruct = destruct_device;
        assert(!rd.device);
        rd.device = &dev->pub;
        rd.device->ref_count = 1;
        rd.device->soundio = soundio;
        rd.device->is_raw = false; // TODO

        int device_id_len;
        if ((err = from_lpwstr(rd.lpwstr, &rd.device->id, &device_id_len))) {
            deinit_refresh_devices(&rd);
            return err;
        }

        if (FAILED(hr = IMMDevice_Activate(rd.mm_device, IID_IAudioClient,
                        CLSCTX_ALL, nullptr, (void**)&dw->audio_client)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        REFERENCE_TIME default_device_period;
        REFERENCE_TIME min_device_period;
        if (FAILED(hr = IAudioClient_GetDevicePeriod(dw->audio_client,
                        &default_device_period, &min_device_period)))
        {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        rd.device->period_duration_current = from_reference_time(default_device_period);
        rd.device->period_duration_min = from_reference_time(min_device_period);


        if (rd.endpoint)
            IMMEndpoint_Release(rd.endpoint);
        if (FAILED(hr = IMMDevice_QueryInterface(rd.mm_device, IID_IMMEndpoint, (void**)&rd.endpoint))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        EDataFlow data_flow;
        if (FAILED(hr = IMMEndpoint_GetDataFlow(rd.endpoint, &data_flow))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        rd.device->aim = data_flow_to_aim(data_flow);

        if (rd.prop_store)
            IPropertyStore_Release(rd.prop_store);
        if (FAILED(hr = IMMDevice_OpenPropertyStore(rd.mm_device, STGM_READ, &rd.prop_store))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        if (rd.prop_variant_value_inited)
            PropVariantClear(&rd.prop_variant_value);
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
        if ((err = from_lpwstr(rd.prop_variant_value.pwszVal, &rd.device->name, &device_name_len))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }

        if (rd.wave_format)
            CoTaskMemFree(rd.wave_format);
        if (FAILED(hr = IAudioClient_GetMixFormat(dw->audio_client, (WAVEFORMATEX**)&rd.wave_format))) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        if (rd.wave_format->Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorOpeningDevice;
        }
        rd.device->sample_rate_current = rd.wave_format->Format.nSamplesPerSec;
        // WASAPI performs resampling in shared mode, so any value is valid.
        // Let's pick some reasonable min and max values.
        rd.device->sample_rate_count = 1;
        rd.device->sample_rates = &dev->prealloc_sample_rate_range;
        rd.device->sample_rates[0].min = min(SOUNDIO_MIN_SAMPLE_RATE, rd.device->sample_rate_current);
        rd.device->sample_rates[0].max = max(SOUNDIO_MAX_SAMPLE_RATE, rd.device->sample_rate_current);

        rd.device->current_format = from_wave_format_format(rd.wave_format);
        rd.device->format_count = 1;
        rd.device->formats = &dev->prealloc_format;
        rd.device->formats[0] = rd.device->current_format;

        from_wave_format_layout(rd.wave_format, &rd.device->current_layout);
        rd.device->layout_count = 1;
        rd.device->layouts = allocate<SoundIoChannelLayout>(1);

        if (!rd.device->layouts) {
            deinit_refresh_devices(&rd);
            return SoundIoErrorNoMem;
        }

        rd.device->layouts[0] = rd.device->current_layout;



        SoundIoList<SoundIoDevice *> *device_list;
        if (rd.device->aim == SoundIoDeviceAimOutput) {
            device_list = &rd.devices_info->output_devices;
            if (soundio_streql(rd.device->id, device_id_len, rd.default_render_id, rd.default_render_id_len))
                rd.devices_info->default_output_index = device_list->length;
        } else {
            assert(rd.device->aim == SoundIoDeviceAimInput);
            device_list = &rd.devices_info->input_devices;
            if (soundio_streql(rd.device->id, device_id_len, rd.default_capture_id, rd.default_capture_id_len))
                rd.devices_info->default_input_index = device_list->length;
        }

        if ((err = device_list->append(rd.device))) {
            deinit_refresh_devices(&rd);
            return err;
        }
        rd.device = nullptr;
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


    for (;;) {
        if (!siw->abort_flag.test_and_set())
            break;
        if (siw->device_scan_queued.exchange(false)) {
            err = refresh_devices(si);
            if (err)
                shutdown_backend(si, err);
            if (!siw->have_devices_flag.exchange(true)) {
                // TODO separate cond for signaling devices like coreaudio
                soundio_os_cond_signal(siw->cond, nullptr);
                soundio->on_events_signal(soundio);
            }
            if (err)
                break;
            soundio_os_cond_signal(siw->cond, nullptr);
        }
        soundio_os_cond_wait(siw->cond, nullptr);
    }

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
    soundio_panic("TODO");
}

static int outstream_open_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static int outstream_pause_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os, bool pause) {
    soundio_panic("TODO");
}

static int outstream_start_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static int outstream_begin_write_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    soundio_panic("TODO");
}

static int outstream_end_write_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static int outstream_clear_buffer_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
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
        soundio_os_cond_signal(siw->cond, nullptr);
        soundio_os_thread_destroy(siw->thread);
    }

    if (siw->cond)
        soundio_os_cond_destroy(siw->cond);

    if (siw->mutex)
        soundio_os_mutex_destroy(siw->mutex);

    soundio_destroy_devices_info(siw->ready_devices_info);
}

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
