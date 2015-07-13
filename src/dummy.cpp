/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "dummy.hpp"
#include "soundio.hpp"
#include "os.hpp"
#include "atomics.hpp"
#include "ring_buffer.hpp"

#include <stdio.h>
#include <string.h>

struct SoundIoOutStreamDummy {
    struct SoundIoOsThread *thread;
    struct SoundIoOsCond *cond;
    atomic_flag abort_flag;
    int buffer_size;
    double period;
    struct SoundIoRingBuffer ring_buffer;
};

struct SoundIoInStreamDummy {
    // TODO
};

struct SoundIoDummy {
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;
    bool devices_emitted;
};

static void playback_thread_run(void *arg) {
    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate *)arg;
    SoundIoOutStream *outstream = &os->pub;
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)os->backend_data;

    double start_time = soundio_os_get_time();
    long frames_consumed = 0;

    double time_per_frame = 1.0 / (double)outstream->sample_rate;
    while (osd->abort_flag.test_and_set()) {
        soundio_os_cond_timed_wait(osd->cond, nullptr, osd->period);

        double now = soundio_os_get_time();
        double total_time = now - start_time;
        long total_frames = total_time / time_per_frame;
        int frames_to_kill = total_frames - frames_consumed;
        int fill_count = soundio_ring_buffer_fill_count(&osd->ring_buffer);
        int frames_in_buffer = fill_count / outstream->bytes_per_frame;
        int read_count = min(frames_to_kill, frames_in_buffer);
        int frames_left = frames_to_kill - read_count;
        int byte_count = read_count * outstream->bytes_per_frame;
        soundio_ring_buffer_advance_read_ptr(&osd->ring_buffer, byte_count);
        frames_consumed += read_count;

        if (frames_left > 0) {
            outstream->underrun_callback(outstream);
        } else if (read_count > 0) {
            outstream->write_callback(outstream, read_count);
        }
    }
}

/*
static void recording_thread_run(void *arg) {
    SoundIoInStream *instream = (SoundIoInStream *)arg;
    SoundIoDevice *device = instream->device;
    SoundIo *soundio = device->soundio;
    // TODO
}
*/

static void destroy_dummy(SoundIoPrivate *si) {
    SoundIoDummy *sid = (SoundIoDummy *)si->backend_data;
    if (!sid)
        return;

    if (sid->cond)
        soundio_os_cond_destroy(sid->cond);

    if (sid->mutex)
        soundio_os_mutex_destroy(sid->mutex);

    destroy(sid);
    si->backend_data = nullptr;
}

static void flush_events(SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoDummy *sid = (SoundIoDummy *)si->backend_data;
    if (sid->devices_emitted)
        return;
    sid->devices_emitted = true;
    soundio->on_devices_change(soundio);
}

static void wait_events(SoundIoPrivate *si) {
    SoundIoDummy *sid = (SoundIoDummy *)si->backend_data;
    flush_events(si);
    soundio_os_cond_wait(sid->cond, nullptr);
}

static void wakeup(SoundIoPrivate *si) {
    SoundIoDummy *sid = (SoundIoDummy *)si->backend_data;
    soundio_os_cond_signal(sid->cond, nullptr);
}

static void outstream_destroy_dummy(SoundIoPrivate *si, SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)os->backend_data;
    if (!osd)
        return;

    if (osd->thread) {
        if (osd->thread) {
            osd->abort_flag.clear();
            soundio_os_cond_signal(osd->cond, nullptr);
            soundio_os_thread_destroy(osd->thread);
            osd->thread = nullptr;
        }
    }
    soundio_os_cond_destroy(osd->cond);
    osd->cond = nullptr;

    soundio_ring_buffer_deinit(&osd->ring_buffer);

    destroy(osd);
    os->backend_data = nullptr;
}

static int outstream_init_dummy(SoundIoPrivate *si, SoundIoOutStreamPrivate *os) {
    SoundIoOutStream *outstream = &os->pub;
    SoundIoOutStreamDummy *osd = create<SoundIoOutStreamDummy>();
    if (!osd) {
        outstream_destroy_dummy(si, os);
        return SoundIoErrorNoMem;
    }
    os->backend_data = osd;

    int buffer_frame_count = outstream->latency * outstream->sample_rate;
    osd->buffer_size = outstream->bytes_per_frame * buffer_frame_count;
    osd->period = outstream->latency * 0.5;

    soundio_ring_buffer_init(&osd->ring_buffer, osd->buffer_size);

    osd->cond = soundio_os_cond_create();
    if (!osd->cond) {
        outstream_destroy_dummy(si, os);
        return SoundIoErrorNoMem;
    }

    return 0;
}

static int outstream_start_dummy(SoundIoPrivate *soundio, SoundIoOutStreamPrivate *os) {
    SoundIoOutStream *outstream = &os->pub;
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)os->backend_data;

    soundio_outstream_fill_with_silence(outstream);
    assert(soundio_ring_buffer_fill_count(&osd->ring_buffer) == osd->buffer_size);

    osd->abort_flag.test_and_set();
    int err;
    if ((err = soundio_os_thread_create(playback_thread_run, os, true, &osd->thread))) {
        return err;
    }

    return 0;
}

static int outstream_free_count_dummy(SoundIoPrivate *soundio, SoundIoOutStreamPrivate *os) {
    SoundIoOutStream *outstream = &os->pub;
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)os->backend_data;
    int fill_count = soundio_ring_buffer_fill_count(&osd->ring_buffer);
    int bytes_free_count = osd->buffer_size - fill_count;
    return bytes_free_count / outstream->bytes_per_frame;
}

static void outstream_begin_write_dummy(SoundIoPrivate *si,
        SoundIoOutStreamPrivate *os, char **data, int *frame_count)
{
    SoundIoOutStream *outstream = &os->pub;
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)os->backend_data;

    int byte_count = *frame_count * outstream->bytes_per_frame;
    assert(byte_count <= osd->buffer_size);
    *data = osd->ring_buffer.address;
}

static void outstream_write_dummy(SoundIoPrivate *si,
        SoundIoOutStreamPrivate *os, char *data, int frame_count)
{
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)os->backend_data;
    SoundIoOutStream *outstream = &os->pub;
    assert(data == osd->ring_buffer.address);
    int byte_count = frame_count * outstream->bytes_per_frame;
    soundio_ring_buffer_advance_write_ptr(&osd->ring_buffer, byte_count);
}

static void outstream_clear_buffer_dummy(SoundIoPrivate *si, SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)os->backend_data;
    soundio_ring_buffer_clear(&osd->ring_buffer);
}

static int instream_init_dummy(SoundIoPrivate *si, SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static void instream_destroy_dummy(SoundIoPrivate *si, SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int instream_start_dummy(SoundIoPrivate *si, SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static void instream_peek_dummy(SoundIoPrivate *si,
        SoundIoInStreamPrivate *is, const char **data, int *frame_count)
{
    soundio_panic("TODO");
}

static void instream_drop_dummy(SoundIoPrivate *si, SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static void instream_clear_buffer_dummy(SoundIoPrivate *si, SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int set_all_device_formats(SoundIoDevice *device) {
    device->format_count = 18;
    device->formats = allocate<SoundIoFormat>(device->format_count);
    if (!device->formats)
        return SoundIoErrorNoMem;

    device->formats[0] = SoundIoFormatS8;
    device->formats[1] = SoundIoFormatU8;
    device->formats[2] = SoundIoFormatS16LE;
    device->formats[3] = SoundIoFormatS16BE;
    device->formats[4] = SoundIoFormatU16LE;
    device->formats[5] = SoundIoFormatU16BE;
    device->formats[6] = SoundIoFormatS24LE;
    device->formats[7] = SoundIoFormatS24BE;
    device->formats[8] = SoundIoFormatU24LE;
    device->formats[9] = SoundIoFormatU24BE;
    device->formats[10] = SoundIoFormatS32LE;
    device->formats[11] = SoundIoFormatS32BE;
    device->formats[12] = SoundIoFormatU32LE;
    device->formats[13] = SoundIoFormatU32BE;
    device->formats[14] = SoundIoFormatFloat32LE;
    device->formats[15] = SoundIoFormatFloat32BE;
    device->formats[16] = SoundIoFormatFloat64LE;
    device->formats[17] = SoundIoFormatFloat64BE;

    return 0;
}

int soundio_dummy_init(SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    assert(!si->backend_data);
    SoundIoDummy *sid = create<SoundIoDummy>();
    if (!sid) {
        destroy_dummy(si);
        return SoundIoErrorNoMem;
    }
    si->backend_data = sid;

    sid->mutex = soundio_os_mutex_create();
    if (!sid->mutex) {
        destroy_dummy(si);
        return SoundIoErrorNoMem;
    }

    sid->cond = soundio_os_cond_create();
    if (!sid->cond) {
        destroy_dummy(si);
        return SoundIoErrorNoMem;
    }

    assert(!si->safe_devices_info);
    si->safe_devices_info = create<SoundIoDevicesInfo>();
    if (!si->safe_devices_info) {
        destroy_dummy(si);
        return SoundIoErrorNoMem;
    }

    si->safe_devices_info->default_input_index = 0;
    si->safe_devices_info->default_output_index = 0;

    // create output device
    {
        SoundIoDevice *device = create<SoundIoDevice>();
        if (!device) {
            destroy_dummy(si);
            return SoundIoErrorNoMem;
        }

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup("dummy-out");
        device->description = strdup("Dummy Output Device");
        if (!device->name || !device->description) {
            soundio_device_unref(device);
            destroy_dummy(si);
            return SoundIoErrorNoMem;
        }
        device->layout_count = 1;
        device->layouts = allocate<SoundIoChannelLayout>(1);
        if (!device->layouts) {
            soundio_device_unref(device);
            destroy_dummy(si);
            return SoundIoErrorNoMem;
        }
        device->layouts[0] = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);

        int err;
        if ((err = set_all_device_formats(device))) {
            soundio_device_unref(device);
            destroy_dummy(si);
            return err;
        }

        device->default_latency = 0.01;
        device->sample_rate_min = 2;
        device->sample_rate_max = 5644800;
        device->sample_rate_current = 48000;
        device->purpose = SoundIoDevicePurposeOutput;

        if (si->safe_devices_info->output_devices.append(device)) {
            soundio_device_unref(device);
            destroy_dummy(si);
            return SoundIoErrorNoMem;
        }
    }

    // create input device
    {
        SoundIoDevice *device = create<SoundIoDevice>();
        if (!device) {
            destroy_dummy(si);
            return SoundIoErrorNoMem;
        }

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup("dummy-in");
        device->description = strdup("Dummy input device");
        if (!device->name || !device->description) {
            soundio_device_unref(device);
            destroy_dummy(si);
            return SoundIoErrorNoMem;
        }

        device->layout_count = 1;
        device->layouts = allocate<SoundIoChannelLayout>(1);
        if (!device->layouts) {
            soundio_device_unref(device);
            destroy_dummy(si);
            return SoundIoErrorNoMem;
        }
        device->layouts[0] = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);

        int err;
        if ((err = set_all_device_formats(device))) {
            soundio_device_unref(device);
            destroy_dummy(si);
            return err;
        }
        device->default_latency = 0.01;
        device->sample_rate_min = 2;
        device->sample_rate_max = 5644800;
        device->sample_rate_current = 48000;
        device->purpose = SoundIoDevicePurposeInput;

        if (si->safe_devices_info->input_devices.append(device)) {
            soundio_device_unref(device);
            destroy_dummy(si);
            return SoundIoErrorNoMem;
        }
    }


    si->destroy = destroy_dummy;
    si->flush_events = flush_events;
    si->wait_events = wait_events;
    si->wakeup = wakeup;

    si->outstream_init = outstream_init_dummy;
    si->outstream_destroy = outstream_destroy_dummy;
    si->outstream_start = outstream_start_dummy;
    si->outstream_free_count = outstream_free_count_dummy;
    si->outstream_begin_write = outstream_begin_write_dummy;
    si->outstream_write = outstream_write_dummy;
    si->outstream_clear_buffer = outstream_clear_buffer_dummy;

    si->instream_init = instream_init_dummy;
    si->instream_destroy = instream_destroy_dummy;
    si->instream_start = instream_start_dummy;
    si->instream_peek = instream_peek_dummy;
    si->instream_drop = instream_drop_dummy;
    si->instream_clear_buffer = instream_clear_buffer_dummy;

    return 0;
}
