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
    SoundIoOutStream *out_stream = (SoundIoOutStream *)arg;
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)out_stream->backend_data;

    double start_time = soundio_os_get_time();
    long frames_consumed = 0;

    double time_per_frame = 1.0 / (double)out_stream->sample_rate;
    while (osd->abort_flag.test_and_set()) {
        soundio_os_cond_timed_wait(osd->cond, nullptr, osd->period);

        double now = soundio_os_get_time();
        double total_time = now - start_time;
        long total_frames = total_time / time_per_frame;
        int frames_to_kill = total_frames - frames_consumed;
        int fill_count = soundio_ring_buffer_fill_count(&osd->ring_buffer);
        int frames_in_buffer = fill_count / out_stream->bytes_per_frame;
        int read_count = min(frames_to_kill, frames_in_buffer);
        int frames_left = frames_to_kill - read_count;
        int byte_count = read_count * out_stream->bytes_per_frame;
        soundio_ring_buffer_advance_read_ptr(&osd->ring_buffer, byte_count);
        frames_consumed += read_count;

        if (frames_left > 0) {
            out_stream->underrun_callback(out_stream);
        } else if (read_count > 0) {
            out_stream->write_callback(out_stream, read_count);
        }
    }
}

/*
static void recording_thread_run(void *arg) {
    SoundIoInStream *in_stream = (SoundIoInStream *)arg;
    SoundIoDevice *device = in_stream->device;
    SoundIo *soundio = device->soundio;
    // TODO
}
*/

static void destroy_dummy(SoundIo *soundio) {
    SoundIoDummy *sid = (SoundIoDummy *)soundio->backend_data;
    if (!sid)
        return;

    if (sid->cond)
        soundio_os_cond_destroy(sid->cond);

    if (sid->mutex)
        soundio_os_mutex_destroy(sid->mutex);

    destroy(sid);
    soundio->backend_data = nullptr;
}

static void flush_events(SoundIo *soundio) {
    SoundIoDummy *sid = (SoundIoDummy *)soundio->backend_data;
    if (sid->devices_emitted)
        return;
    sid->devices_emitted = true;
    soundio->on_devices_change(soundio);
}

static void wait_events(SoundIo *soundio) {
    SoundIoDummy *sid = (SoundIoDummy *)soundio->backend_data;
    flush_events(soundio);
    soundio_os_cond_wait(sid->cond, nullptr);
}

static void wakeup(SoundIo *soundio) {
    SoundIoDummy *sid = (SoundIoDummy *)soundio->backend_data;
    soundio_os_cond_signal(sid->cond, nullptr);
}

static void out_stream_destroy_dummy(SoundIo *soundio,
        SoundIoOutStream *out_stream)
{
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)out_stream->backend_data;
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
    out_stream->backend_data = nullptr;
}

static int out_stream_init_dummy(SoundIo *soundio,
        SoundIoOutStream *out_stream)
{

    SoundIoOutStreamDummy *osd = create<SoundIoOutStreamDummy>();
    if (!osd) {
        out_stream_destroy_dummy(soundio, out_stream);
        return SoundIoErrorNoMem;
    }
    out_stream->backend_data = osd;

    int buffer_frame_count = out_stream->latency * out_stream->sample_rate;
    osd->buffer_size = out_stream->bytes_per_frame * buffer_frame_count;
    osd->period = out_stream->latency * 0.5;

    soundio_ring_buffer_init(&osd->ring_buffer, osd->buffer_size);

    osd->cond = soundio_os_cond_create();
    if (!osd->cond) {
        out_stream_destroy_dummy(soundio, out_stream);
        return SoundIoErrorNoMem;
    }

    return 0;
}

static int out_stream_start_dummy(SoundIo *soundio,
        SoundIoOutStream *out_stream)
{
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)out_stream->backend_data;

    soundio_out_stream_fill_with_silence(out_stream);
    assert(soundio_ring_buffer_fill_count(&osd->ring_buffer) == osd->buffer_size);

    osd->abort_flag.test_and_set();
    int err;
    if ((err = soundio_os_thread_create(playback_thread_run, out_stream, true, &osd->thread))) {
        return err;
    }

    return 0;
}

static int out_stream_free_count_dummy(SoundIo *soundio,
        SoundIoOutStream *out_stream)
{
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)out_stream->backend_data;
    int fill_count = soundio_ring_buffer_fill_count(&osd->ring_buffer);
    int bytes_free_count = osd->buffer_size - fill_count;
    return bytes_free_count / out_stream->bytes_per_frame;
}

static void out_stream_begin_write_dummy(SoundIo *soundio,
        SoundIoOutStream *out_stream, char **data, int *frame_count)
{
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)out_stream->backend_data;

    int byte_count = *frame_count * out_stream->bytes_per_frame;
    assert(byte_count <= osd->buffer_size);
    *data = osd->ring_buffer.address;
}

static void out_stream_write_dummy(SoundIo *soundio,
        SoundIoOutStream *out_stream, char *data, int frame_count)
{
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)out_stream->backend_data;
    assert(data == osd->ring_buffer.address);
    int byte_count = frame_count * out_stream->bytes_per_frame;
    soundio_ring_buffer_advance_write_ptr(&osd->ring_buffer, byte_count);
}

static void out_stream_clear_buffer_dummy(SoundIo *soundio,
        SoundIoOutStream *out_stream)
{
    SoundIoOutStreamDummy *osd = (SoundIoOutStreamDummy *)out_stream->backend_data;
    soundio_ring_buffer_clear(&osd->ring_buffer);
}

static int in_stream_init_dummy(SoundIo *soundio,
        SoundIoInStream *in_stream)
{
    soundio_panic("TODO");
}

static void in_stream_destroy_dummy(SoundIo *soundio,
        SoundIoInStream *in_stream)
{
    soundio_panic("TODO");
}

static int in_stream_start_dummy(SoundIo *soundio,
        SoundIoInStream *in_stream)
{
    soundio_panic("TODO");
}

static void in_stream_peek_dummy(SoundIo *soundio,
        SoundIoInStream *in_stream, const char **data, int *frame_count)
{
    soundio_panic("TODO");
}

static void in_stream_drop_dummy(SoundIo *soundio,
        SoundIoInStream *in_stream)
{
    soundio_panic("TODO");
}

static void in_stream_clear_buffer_dummy(SoundIo *soundio,
        SoundIoInStream *in_stream)
{
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

int soundio_dummy_init(SoundIo *soundio) {
    assert(!soundio->backend_data);
    SoundIoDummy *sid = create<SoundIoDummy>();
    if (!sid) {
        destroy_dummy(soundio);
        return SoundIoErrorNoMem;
    }
    soundio->backend_data = sid;

    sid->mutex = soundio_os_mutex_create();
    if (!sid->mutex) {
        destroy_dummy(soundio);
        return SoundIoErrorNoMem;
    }

    sid->cond = soundio_os_cond_create();
    if (!sid->cond) {
        destroy_dummy(soundio);
        return SoundIoErrorNoMem;
    }

    assert(!soundio->safe_devices_info);
    soundio->safe_devices_info = create<SoundIoDevicesInfo>();
    if (!soundio->safe_devices_info) {
        destroy_dummy(soundio);
        return SoundIoErrorNoMem;
    }

    soundio->safe_devices_info->default_input_index = 0;
    soundio->safe_devices_info->default_output_index = 0;

    // create output device
    {
        SoundIoDevice *device = create<SoundIoDevice>();
        if (!device) {
            destroy_dummy(soundio);
            return SoundIoErrorNoMem;
        }

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup("dummy-out");
        device->description = strdup("Dummy Output Device");
        if (!device->name || !device->description) {
            soundio_device_unref(device);
            destroy_dummy(soundio);
            return SoundIoErrorNoMem;
        }
        device->channel_layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);
        int err;
        if ((err = set_all_device_formats(device))) {
            soundio_device_unref(device);
            destroy_dummy(soundio);
            return err;
        }

        device->default_latency = 0.01;
        device->sample_rate_min = 2;
        device->sample_rate_max = 5644800;
        device->sample_rate_current = 48000;
        device->purpose = SoundIoDevicePurposeOutput;

        if (soundio->safe_devices_info->output_devices.append(device)) {
            soundio_device_unref(device);
            destroy_dummy(soundio);
            return SoundIoErrorNoMem;
        }
    }

    // create input device
    {
        SoundIoDevice *device = create<SoundIoDevice>();
        if (!device) {
            destroy_dummy(soundio);
            return SoundIoErrorNoMem;
        }

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup("dummy-in");
        device->description = strdup("Dummy input device");
        if (!device->name || !device->description) {
            soundio_device_unref(device);
            destroy_dummy(soundio);
            return SoundIoErrorNoMem;
        }
        device->channel_layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);
        int err;
        if ((err = set_all_device_formats(device))) {
            soundio_device_unref(device);
            destroy_dummy(soundio);
            return err;
        }
        device->default_latency = 0.01;
        device->sample_rate_min = 2;
        device->sample_rate_max = 5644800;
        device->sample_rate_current = 48000;
        device->purpose = SoundIoDevicePurposeInput;

        if (soundio->safe_devices_info->input_devices.append(device)) {
            soundio_device_unref(device);
            destroy_dummy(soundio);
            return SoundIoErrorNoMem;
        }
    }


    soundio->destroy = destroy_dummy;
    soundio->flush_events = flush_events;
    soundio->wait_events = wait_events;
    soundio->wakeup = wakeup;

    soundio->out_stream_init = out_stream_init_dummy;
    soundio->out_stream_destroy = out_stream_destroy_dummy;
    soundio->out_stream_start = out_stream_start_dummy;
    soundio->out_stream_free_count = out_stream_free_count_dummy;
    soundio->out_stream_begin_write = out_stream_begin_write_dummy;
    soundio->out_stream_write = out_stream_write_dummy;
    soundio->out_stream_clear_buffer = out_stream_clear_buffer_dummy;

    soundio->in_stream_init = in_stream_init_dummy;
    soundio->in_stream_destroy = in_stream_destroy_dummy;
    soundio->in_stream_start = in_stream_start_dummy;
    soundio->in_stream_peek = in_stream_peek_dummy;
    soundio->in_stream_drop = in_stream_drop_dummy;
    soundio->in_stream_clear_buffer = in_stream_clear_buffer_dummy;

    return 0;
}
