/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "dummy.hpp"
#include "soundio.hpp"

#include <stdio.h>
#include <string.h>

static void playback_thread_run(void *arg) {
    SoundIoOutputDevice *output_device = (SoundIoOutputDevice *)arg;
    SoundIoDevice *device = output_device->device;
    SoundIoOutputDeviceDummy *opd = (SoundIoOutputDeviceDummy *)output_device->backend_data;

    double start_time = soundio_os_get_time();
    long frames_consumed = 0;

    double time_per_frame = 1.0 / (double)device->default_sample_rate;
    while (opd->abort_flag.test_and_set()) {
        soundio_os_mutex_lock(opd->mutex);
        soundio_os_cond_timed_wait(opd->cond, opd->mutex, opd->period);
        soundio_os_mutex_unlock(opd->mutex);

        double now = soundio_os_get_time();
        double total_time = now - start_time;
        long total_frames = total_time / time_per_frame;
        int frames_to_kill = total_frames - frames_consumed;
        int fill_count = soundio_dummy_ring_buffer_fill_count(&opd->ring_buffer);
        int frames_in_buffer = fill_count / output_device->bytes_per_frame;
        int read_count = min(frames_to_kill, frames_in_buffer);
        int frames_left = frames_to_kill - read_count;
        int byte_count = read_count * output_device->bytes_per_frame;
        soundio_dummy_ring_buffer_advance_read_ptr(&opd->ring_buffer, byte_count);
        frames_consumed += read_count;

        if (frames_left > 0) {
            output_device->underrun_callback(output_device);
        } else if (read_count > 0) {
            output_device->write_callback(output_device, read_count);
        }
    }
}

/*
static void recording_thread_run(void *arg) {
    SoundIoInputDevice *input_device = (SoundIoInputDevice *)arg;
    SoundIoDevice *device = input_device->device;
    SoundIo *soundio = device->soundio;
    // TODO
}
*/

static void destroy_audio_hardware_dummy(SoundIo *soundio) { }

static void flush_events(SoundIo *soundio) { }

static void refresh_audio_devices(SoundIo *soundio) { }

static void output_device_destroy_dummy(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    SoundIoOutputDeviceDummy *opd = (SoundIoOutputDeviceDummy *)output_device->backend_data;
    if (opd->thread) {
        if (opd->thread) {
            opd->abort_flag.clear();
            soundio_os_mutex_lock(opd->mutex);
            soundio_os_cond_signal(opd->cond);
            soundio_os_mutex_unlock(opd->mutex);
            soundio_os_thread_destroy(opd->thread);
            opd->thread = nullptr;
        }
    }
    soundio_os_mutex_destroy(opd->mutex);
    opd->mutex = nullptr;

    soundio_os_cond_destroy(opd->cond);
    opd->cond = nullptr;
}

static int output_device_init_dummy(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    SoundIoOutputDeviceDummy *opd = (SoundIoOutputDeviceDummy *)output_device->backend_data;
    SoundIoDevice *device = output_device->device;
    int buffer_frame_count = output_device->latency * device->default_sample_rate;
    opd->buffer_size = output_device->bytes_per_frame * buffer_frame_count;
    opd->period = output_device->latency * 0.5;

    soundio_dummy_ring_buffer_init(&opd->ring_buffer, opd->buffer_size);

    opd->mutex = soundio_os_mutex_create();
    if (!opd->mutex) {
        output_device_destroy_dummy(soundio, output_device);
        return SoundIoErrorNoMem;
    }

    opd->cond = soundio_os_cond_create();
    if (!opd->cond) {
        output_device_destroy_dummy(soundio, output_device);
        return SoundIoErrorNoMem;
    }

    return 0;
}

static int output_device_start_dummy(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    SoundIoOutputDeviceDummy *opd = (SoundIoOutputDeviceDummy *)output_device->backend_data;

    soundio_output_device_fill_with_silence(output_device);
    assert(soundio_dummy_ring_buffer_fill_count(&opd->ring_buffer) == opd->buffer_size);

    opd->abort_flag.test_and_set();
    int err;
    if ((err = soundio_os_thread_create(playback_thread_run, output_device, true, &opd->thread))) {
        return err;
    }

    return 0;
}

static int output_device_free_count_dummy(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    SoundIoOutputDeviceDummy *opd = (SoundIoOutputDeviceDummy *)output_device->backend_data;
    int fill_count = soundio_dummy_ring_buffer_fill_count(&opd->ring_buffer);
    int bytes_free_count = opd->buffer_size - fill_count;
    return bytes_free_count / output_device->bytes_per_frame;
}

static void output_device_begin_write_dummy(SoundIo *soundio,
        SoundIoOutputDevice *output_device, char **data, int *frame_count)
{
    SoundIoOutputDeviceDummy *opd = (SoundIoOutputDeviceDummy *)output_device->backend_data;

    int byte_count = *frame_count * output_device->bytes_per_frame;
    assert(byte_count <= opd->buffer_size);
    *data = opd->ring_buffer.address;
}

static void output_device_write_dummy(SoundIo *soundio,
        SoundIoOutputDevice *output_device, char *data, int frame_count)
{
    SoundIoOutputDeviceDummy *opd = (SoundIoOutputDeviceDummy *)output_device->backend_data;
    assert(data == opd->ring_buffer.address);
    int byte_count = frame_count * output_device->bytes_per_frame;
    soundio_dummy_ring_buffer_advance_write_ptr(&opd->ring_buffer, byte_count);
}

static void output_device_clear_buffer_dummy(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    SoundIoOutputDeviceDummy *opd = (SoundIoOutputDeviceDummy *)output_device->backend_data;
    soundio_dummy_ring_buffer_clear(&opd->ring_buffer);
}

static int input_device_init_dummy(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    // TODO
    return 0;
}

static void input_device_destroy_dummy(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    // TODO
}

static int input_device_start_dummy(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    // TODO
    return 0;
}

static void input_device_peek_dummy(SoundIo *soundio,
        SoundIoInputDevice *input_device, const char **data, int *frame_count)
{
    // TODO
}

static void input_device_drop_dummy(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    // TODO
}

static void input_device_clear_buffer_dummy(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    // TODO
}

int audio_hardware_init_dummy(SoundIo *soundio) {
    soundio->safe_devices_info = create<SoundIoDevicesInfo>();
    soundio->safe_devices_info->default_input_index = 0;
    soundio->safe_devices_info->default_output_index = 0;

    // create output device
    {
        SoundIoDevice *device = create<SoundIoDevice>();
        if (!device)
            return SoundIoErrorNoMem;

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup("dummy-out");
        device->description = strdup("Dummy output device");
        if (!device->name || !device->description) {
            free(device->name);
            free(device->description);
            return SoundIoErrorNoMem;
        }
        device->channel_layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);
        device->default_sample_format = SoundIoSampleFormatFloat;
        device->default_latency = 0.01;
        device->default_sample_rate = 48000;
        device->purpose = SoundIoDevicePurposeOutput;

        if (soundio->safe_devices_info->devices.append(device)) {
            soundio_audio_device_unref(device);
            return SoundIoErrorNoMem;
        }
    }

    // create input device
    {
        SoundIoDevice *device = create<SoundIoDevice>();
        if (!device)
            return SoundIoErrorNoMem;

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup("dummy-in");
        device->description = strdup("Dummy input device");
        if (!device->name || !device->description) {
            free(device->name);
            free(device->description);
            return SoundIoErrorNoMem;
        }
        device->channel_layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);
        device->default_sample_format = SoundIoSampleFormatFloat;
        device->default_latency = 0.01;
        device->default_sample_rate = 48000;
        device->purpose = SoundIoDevicePurposeInput;

        if (soundio->safe_devices_info->devices.append(device)) {
            soundio_audio_device_unref(device);
            return SoundIoErrorNoMem;
        }
    }


    soundio->destroy = destroy_audio_hardware_dummy;
    soundio->flush_events = flush_events;
    soundio->refresh_audio_devices = refresh_audio_devices;

    soundio->output_device_init = output_device_init_dummy;
    soundio->output_device_destroy = output_device_destroy_dummy;
    soundio->output_device_start = output_device_start_dummy;
    soundio->output_device_free_count = output_device_free_count_dummy;
    soundio->output_device_begin_write = output_device_begin_write_dummy;
    soundio->output_device_write = output_device_write_dummy;
    soundio->output_device_clear_buffer = output_device_clear_buffer_dummy;

    soundio->input_device_init = input_device_init_dummy;
    soundio->input_device_destroy = input_device_destroy_dummy;
    soundio->input_device_start = input_device_start_dummy;
    soundio->input_device_peek = input_device_peek_dummy;
    soundio->input_device_drop = input_device_drop_dummy;
    soundio->input_device_clear_buffer = input_device_clear_buffer_dummy;

    return 0;
}

