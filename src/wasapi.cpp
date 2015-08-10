#include "wasapi.hpp"
#include "soundio.hpp"

#include <stdio.h>

#define INITGUID
#define CINTERFACE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <mmdeviceapi.h>

struct RefreshDevices {
    IMMDeviceCollection *collection;
};

static void deinit_refresh_devices(RefreshDevices *rd) {
    if (rd->collection)
        IMMDeviceCollection_Release(rd->collection);
}

static int refresh_devices(SoundIoPrivate *si) {
    SoundIoWasapi *siw = &si->backend_data.wasapi;
    RefreshDevices rd = {0};
    HRESULT hr;
    if (FAILED(hr = IMMDeviceEnumerator_EnumAudioEndpoints(siw->device_enumerator,
                    eAll, DEVICE_STATE_ACTIVE, &rd.collection)))
    {
        deinit_refresh_devices(&rd);
        return SoundIoErrorOpeningDevice;
    }

    UINT device_count;
    if (FAILED(hr = IMMDeviceCollection_GetCount(rd.collection, &device_count))) {
        deinit_refresh_devices(&rd);
        return SoundIoErrorOpeningDevice;
    }

    // TODO
    fprintf(stderr, "device count: %d\n", (int) device_count);
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
