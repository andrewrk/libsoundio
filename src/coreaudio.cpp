#include "coreaudio.hpp"
#include "soundio.hpp"

#include <assert.h>

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
    soundio_os_cond_signal(sica->cond, nullptr);

    return noErr;
}

static OSStatus on_service_restarted(AudioObjectID in_object_id, UInt32 in_number_addresses,
    const AudioObjectPropertyAddress in_addresses[], void *in_client_data)
{
    SoundIoPrivate *si = (SoundIoPrivate*)in_client_data;
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    sica->service_restarted.store(true);
    soundio_os_cond_signal(sica->cond, nullptr);

    return noErr;
}

static void destroy_ca(struct SoundIoPrivate *si) {
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    int err;
    AudioObjectPropertyAddress prop_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    err = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &prop_address,
        on_devices_changed, si);
    assert(!err);

    prop_address.mSelector = kAudioHardwarePropertyServiceRestarted;
    err = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &prop_address,
        on_service_restarted, si);
    assert(!err);

    if (sica->thread) {
        sica->abort_flag.clear();
        soundio_os_cond_signal(sica->cond, nullptr);
        soundio_os_thread_destroy(sica->thread);
    }

    if (sica->cond)
        soundio_os_cond_destroy(sica->cond);

    if (sica->have_devices_cond)
        soundio_os_cond_destroy(sica->have_devices_cond);

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
        deallocate(buf, max_size);
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
// TODO
/*
    @constant       kAudioHardwarePropertyDefaultInputDevice
                        The AudioObjectID of the default input AudioDevice.
    @constant       kAudioHardwarePropertyDefaultOutputDevice
                        The AudioObjectID of the default output AudioDevice.
                        */
/*
 *
    @constant       kAudioDevicePropertyDeviceHasChanged
                        The type of this property is a UInt32, but its value has no meaning. This
                        property exists so that clients can listen to it and be told when the
                        configuration of the AudioDevice has changed in ways that cannot otherwise
                        be conveyed through other notifications. In response to this notification,
                        clients should re-evaluate everything they need to know about the device,
                        particularly the layout and values of the controls.
                        */
/*
    @constant       kAudioDevicePropertyBufferFrameSize
                        A UInt32 whose value indicates the number of frames in the IO buffers.
    @constant       kAudioDevicePropertyBufferFrameSizeRange
                        An AudioValueRange indicating the minimum and maximum values, inclusive, for
                        kAudioDevicePropertyBufferFrameSize.
                        */
/*
    @constant       kAudioDevicePropertyStreamConfiguration
                        This property returns the stream configuration of the device in an
                        AudioBufferList (with the buffer pointers set to NULL) which describes the
                        list of streams and the number of channels in each stream. This corresponds
                        to what will be passed into the IOProc.
                        */
    

// TODO go through here and make sure all allocations are freed
static int refresh_devices(struct SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoCoreAudio *sica = &si->backend_data.coreaudio;

    SoundIoDevicesInfo *devices_info = create<SoundIoDevicesInfo>();
    if (!devices_info) {
        soundio_panic("out of mem");
    }

    AudioObjectPropertyAddress prop_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    UInt32 the_data_size = 0;
    OSStatus err;
    if ((err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
        &prop_address, 0, nullptr, &the_data_size)))
    {
        soundio_panic("get prop data size");
    }

    int devices_available = the_data_size / (UInt32)sizeof(AudioObjectID);

    if (devices_available < 1) {
        soundio_panic("no devices");
    }

    AudioObjectID *devices = (AudioObjectID *)allocate<char>(the_data_size);
    if (!devices) {
        soundio_panic("out of mem");
    }

    if ((err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop_address, 0, nullptr,
        &the_data_size, devices)))
    {
        soundio_panic("get property data");
    }

    fprintf(stderr, "device count: %d\n", devices_available);

    for (int i = 0; i < devices_available; i += 1) {
        AudioObjectID deviceID = devices[i];

        prop_address.mSelector = kAudioObjectPropertyName;
        CFStringRef temp_string_ref = nullptr;
        UInt32 io_size = sizeof(CFStringRef);
        if ((err = AudioObjectGetPropertyData(deviceID, &prop_address,
            0, nullptr, &io_size, &temp_string_ref)))
        {
            soundio_panic("error getting name");
        }

        char *device_name;
        int device_name_len;
        if ((err = from_cf_string(temp_string_ref, &device_name, &device_name_len))) {
            soundio_panic("error decoding");
        }
        fprintf(stderr, "device name: %s\n", device_name);

        CFRelease(temp_string_ref);
        temp_string_ref = nullptr;


        for (int aim_i = 0; aim_i < array_length(aims); aim_i += 1) {
            SoundIoDeviceAim aim = aims[aim_i];

            io_size = 0;
            prop_address.mSelector = kAudioDevicePropertyStreamConfiguration;
            prop_address.mScope = aim_to_scope(aim);
            prop_address.mElement = 0;
            if ((err = AudioObjectGetPropertyDataSize(deviceID, &prop_address, 0, nullptr, &io_size))) {
                soundio_panic("input channel get prop data size");
            }

            AudioBufferList *buffer_list = (AudioBufferList*)allocate<char>(io_size);
            if (!buffer_list) {
                soundio_panic("out of mem");
            }

            if ((err = AudioObjectGetPropertyData(deviceID, &prop_address, 0, nullptr,
                &io_size, buffer_list)))
            {
                soundio_panic("get input channel data");
            }

            int channel_count = 0;
            for (int i = 0; i < buffer_list->mNumberBuffers; i += 1) {
                channel_count += buffer_list->mBuffers[i].mNumberChannels;
            }
            deallocate((char*)buffer_list, io_size);

            fprintf(stderr, "%d channel count: %d\n", aim_i, channel_count);
        }

    }


    soundio_os_mutex_lock(sica->mutex);
    soundio_destroy_devices_info(sica->ready_devices_info);
    sica->ready_devices_info = devices_info;
    soundio->on_events_signal(soundio);
    soundio_os_mutex_unlock(sica->mutex);
    if (!sica->have_devices_flag.exchange(true))
        soundio_os_cond_signal(sica->have_devices_cond, nullptr);


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
            if ((err = refresh_devices(si))) {
                shutdown_backend(si, err);
                return;
            }
        }
        soundio_os_cond_wait(sica->cond, nullptr);
    }
}

static int outstream_open_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static void outstream_destroy_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static int outstream_start_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static int outstream_begin_write_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    soundio_panic("TODO");
}

static int outstream_end_write_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os,
    int frame_count)
{
    soundio_panic("TODO");
}

static int outstream_clear_buffer_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static int outstream_pause_ca(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os, bool pause) {
    soundio_panic("TODO");
}



static int instream_open_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static void instream_destroy_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int instream_start_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int instream_begin_read_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    soundio_panic("TODO");
}

static int instream_end_read_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int instream_pause_ca(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is, bool pause) {
    soundio_panic("TODO");
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

    AudioObjectPropertyAddress prop_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    if ((err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &prop_address,
        on_devices_changed, si)))
    {
        soundio_panic("add prop listener");
    }

    prop_address.mSelector = kAudioHardwarePropertyServiceRestarted;
    if ((err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &prop_address,
        on_service_restarted, si)))
    {
        soundio_panic("add prop listener 2");
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
