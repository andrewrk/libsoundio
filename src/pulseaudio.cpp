/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "pulseaudio.hpp"
#include "soundio.hpp"

#include <string.h>
#include <math.h>
#include <stdio.h>


static void subscribe_callback(pa_context *context,
        pa_subscription_event_type_t event_bits, uint32_t index, void *userdata)
{
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    sipa->device_scan_queued = true;
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

static void subscribe_to_events(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    pa_subscription_mask_t events = (pa_subscription_mask_t)(
            PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE|PA_SUBSCRIPTION_MASK_SERVER);
    pa_operation *subscribe_op = pa_context_subscribe(sipa->pulse_context,
            events, nullptr, si);
    if (!subscribe_op)
        soundio_panic("pa_context_subscribe failed: %s", pa_strerror(pa_context_errno(sipa->pulse_context)));
    pa_operation_unref(subscribe_op);
}

static void context_state_callback(pa_context *context, void *userdata) {
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    switch (pa_context_get_state(context)) {
    case PA_CONTEXT_UNCONNECTED: // The context hasn't been connected yet.
        return;
    case PA_CONTEXT_CONNECTING: // A connection is being established.
        return;
    case PA_CONTEXT_AUTHORIZING: // The client is authorizing itself to the daemon.
        return;
    case PA_CONTEXT_SETTING_NAME: // The client is passing its application name to the daemon.
        return;
    case PA_CONTEXT_READY: // The connection is established, the context is ready to execute operations.
        sipa->device_scan_queued = true;
        subscribe_to_events(si);
        sipa->ready_flag = true;
        pa_threaded_mainloop_signal(sipa->main_loop, 0);
        return;
    case PA_CONTEXT_TERMINATED: // The connection was terminated cleanly.
        pa_threaded_mainloop_signal(sipa->main_loop, 0);
        return;
    case PA_CONTEXT_FAILED: // The connection failed or was disconnected.
        {
            int err_number = pa_context_errno(context);
            if (err_number == PA_ERR_CONNECTIONREFUSED) {
                sipa->connection_refused = true;
            } else {
                soundio_panic("pulseaudio connect failure: %s", pa_strerror(pa_context_errno(context)));
            }
            return;
        }
    }
}

static void destroy_pa(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    if (sipa->main_loop)
        pa_threaded_mainloop_stop(sipa->main_loop);

    soundio_destroy_devices_info(sipa->current_devices_info);
    soundio_destroy_devices_info(sipa->ready_devices_info);

    pa_context_disconnect(sipa->pulse_context);
    pa_context_unref(sipa->pulse_context);

    if (sipa->main_loop)
        pa_threaded_mainloop_free(sipa->main_loop);

    if (sipa->props)
        pa_proplist_free(sipa->props);

    free(sipa->default_sink_name);
    free(sipa->default_source_name);
}

static SoundIoFormat from_pulseaudio_format(pa_sample_spec sample_spec) {
    switch (sample_spec.format) {
    case PA_SAMPLE_U8:          return SoundIoFormatU8;
    case PA_SAMPLE_S16LE:       return SoundIoFormatS16LE;
    case PA_SAMPLE_S16BE:       return SoundIoFormatS16BE;
    case PA_SAMPLE_FLOAT32LE:   return SoundIoFormatFloat32LE;
    case PA_SAMPLE_FLOAT32BE:   return SoundIoFormatFloat32BE;
    case PA_SAMPLE_S32LE:       return SoundIoFormatS32LE;
    case PA_SAMPLE_S32BE:       return SoundIoFormatS32BE;
    case PA_SAMPLE_S24_32LE:    return SoundIoFormatS24LE;
    case PA_SAMPLE_S24_32BE:    return SoundIoFormatS24BE;

    case PA_SAMPLE_MAX:
    case PA_SAMPLE_INVALID:
    case PA_SAMPLE_ALAW:
    case PA_SAMPLE_ULAW:
    case PA_SAMPLE_S24LE:
    case PA_SAMPLE_S24BE:
        return SoundIoFormatInvalid;
    }
    return SoundIoFormatInvalid;
}

static SoundIoChannelId from_pulseaudio_channel_pos(pa_channel_position_t pos) {
    switch (pos) {
    case PA_CHANNEL_POSITION_MONO: return SoundIoChannelIdFrontCenter;
    case PA_CHANNEL_POSITION_FRONT_LEFT: return SoundIoChannelIdFrontLeft;
    case PA_CHANNEL_POSITION_FRONT_RIGHT: return SoundIoChannelIdFrontRight;
    case PA_CHANNEL_POSITION_FRONT_CENTER: return SoundIoChannelIdFrontCenter;
    case PA_CHANNEL_POSITION_REAR_CENTER: return SoundIoChannelIdBackCenter;
    case PA_CHANNEL_POSITION_REAR_LEFT: return SoundIoChannelIdBackLeft;
    case PA_CHANNEL_POSITION_REAR_RIGHT: return SoundIoChannelIdBackRight;
    case PA_CHANNEL_POSITION_LFE: return SoundIoChannelIdLfe;
    case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER: return SoundIoChannelIdFrontLeftCenter;
    case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER: return SoundIoChannelIdFrontRightCenter;
    case PA_CHANNEL_POSITION_SIDE_LEFT: return SoundIoChannelIdSideLeft;
    case PA_CHANNEL_POSITION_SIDE_RIGHT: return SoundIoChannelIdSideRight;
    case PA_CHANNEL_POSITION_TOP_CENTER: return SoundIoChannelIdTopCenter;
    case PA_CHANNEL_POSITION_TOP_FRONT_LEFT: return SoundIoChannelIdTopFrontLeft;
    case PA_CHANNEL_POSITION_TOP_FRONT_RIGHT: return SoundIoChannelIdTopFrontRight;
    case PA_CHANNEL_POSITION_TOP_FRONT_CENTER: return SoundIoChannelIdTopFrontCenter;
    case PA_CHANNEL_POSITION_TOP_REAR_LEFT: return SoundIoChannelIdTopBackLeft;
    case PA_CHANNEL_POSITION_TOP_REAR_RIGHT: return SoundIoChannelIdTopBackRight;
    case PA_CHANNEL_POSITION_TOP_REAR_CENTER: return SoundIoChannelIdTopBackCenter;

    default:
        soundio_panic("cannot map pulseaudio channel to libsoundio");
    }
}

static void set_from_pulseaudio_channel_map(pa_channel_map channel_map, SoundIoChannelLayout *channel_layout) {
    channel_layout->channel_count = channel_map.channels;
    for (int i = 0; i < channel_map.channels; i += 1) {
        channel_layout->channels[i] = from_pulseaudio_channel_pos(channel_map.map[i]);
    }
    channel_layout->name = nullptr;
    int builtin_layout_count = soundio_channel_layout_builtin_count();
    for (int i = 0; i < builtin_layout_count; i += 1) {
        const SoundIoChannelLayout *builtin_layout = soundio_channel_layout_get_builtin(i);
        if (soundio_channel_layout_equal(builtin_layout, channel_layout)) {
            channel_layout->name = builtin_layout->name;
            break;
        }
    }
}

static int set_all_device_channel_layouts(SoundIoDevice *device) {
    device->layout_count = soundio_channel_layout_builtin_count();
    device->layouts = allocate<SoundIoChannelLayout>(device->layout_count);
    if (!device->layouts)
        return SoundIoErrorNoMem;
    for (int i = 0; i < device->layout_count; i += 1)
        device->layouts[i] = *soundio_channel_layout_get_builtin(i);
    return 0;
}

static int set_all_device_formats(SoundIoDevice *device) {
    device->format_count = 9;
    device->formats = allocate<SoundIoFormat>(device->format_count);
    if (!device->formats)
        return SoundIoErrorNoMem;
    device->formats[0] = SoundIoFormatU8;
    device->formats[1] = SoundIoFormatS16LE;
    device->formats[2] = SoundIoFormatS16BE;
    device->formats[3] = SoundIoFormatFloat32LE;
    device->formats[4] = SoundIoFormatFloat32BE;
    device->formats[5] = SoundIoFormatS32LE;
    device->formats[6] = SoundIoFormatS32BE;
    device->formats[7] = SoundIoFormatS24LE;
    device->formats[8] = SoundIoFormatS24BE;
    return 0;
}

static int perform_operation(SoundIoPrivate *si, pa_operation *op) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    for (;;) {
        switch (pa_operation_get_state(op)) {
        case PA_OPERATION_RUNNING:
            pa_threaded_mainloop_wait(sipa->main_loop);
            continue;
        case PA_OPERATION_DONE:
            pa_operation_unref(op);
            return 0;
        case PA_OPERATION_CANCELLED:
            pa_operation_unref(op);
            return -1;
        }
    }
}

static void finish_device_query(SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    if (!sipa->have_sink_list ||
        !sipa->have_source_list ||
        !sipa->have_default_sink)
    {
        return;
    }

    // based on the default sink name, figure out the default output index
    // if the name doesn't match just pick the first one. if there are no
    // devices then we need to set it to -1.
    sipa->current_devices_info->default_output_index = -1;
    sipa->current_devices_info->default_input_index = -1;

    if (sipa->current_devices_info->input_devices.length > 0) {
        sipa->current_devices_info->default_input_index = 0;
        for (int i = 0; i < sipa->current_devices_info->input_devices.length; i += 1) {
            SoundIoDevice *device = sipa->current_devices_info->input_devices.at(i);
            assert(device->purpose == SoundIoDevicePurposeInput);
            if (strcmp(device->name, sipa->default_source_name) == 0) {
                sipa->current_devices_info->default_input_index = i;
            }
        }
    }

    if (sipa->current_devices_info->output_devices.length > 0) {
        sipa->current_devices_info->default_output_index = 0;
        for (int i = 0; i < sipa->current_devices_info->output_devices.length; i += 1) {
            SoundIoDevice *device = sipa->current_devices_info->output_devices.at(i);
            assert(device->purpose == SoundIoDevicePurposeOutput);
            if (strcmp(device->name, sipa->default_sink_name) == 0) {
                sipa->current_devices_info->default_output_index = i;
            }
        }
    }

    soundio_destroy_devices_info(sipa->ready_devices_info);
    sipa->ready_devices_info = sipa->current_devices_info;
    sipa->current_devices_info = NULL;
    sipa->have_devices_flag = true;
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
    soundio->on_events_signal(soundio);
}

static void sink_info_callback(pa_context *pulse_context, const pa_sink_info *info, int eol, void *userdata) {
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    int err;
    if (eol) {
        sipa->have_sink_list = true;
        finish_device_query(si);
    } else {
        SoundIoDevicePrivate *dev = create<SoundIoDevicePrivate>();
        if (!dev)
            soundio_panic("out of memory");
        SoundIoDevice *device = &dev->pub;

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup(info->name);
        device->description = strdup(info->description);
        if (!device->name || !device->description)
            soundio_panic("out of memory");

        device->sample_rate_current = info->sample_spec.rate;
        // PulseAudio performs resampling, so any value is valid. Let's pick
        // some reasonable min and max values.
        device->sample_rate_min = min(8000, device->sample_rate_current);
        device->sample_rate_max = max(5644800, device->sample_rate_current);

        device->current_format = from_pulseaudio_format(info->sample_spec);
        // PulseAudio performs sample format conversion, so any PulseAudio
        // value is valid.
        if ((err = set_all_device_formats(device)))
            soundio_panic("out of memory");

        set_from_pulseaudio_channel_map(info->channel_map, &device->current_layout);
        // PulseAudio does channel layout remapping, so any channel layout is valid.
        if ((err = set_all_device_channel_layouts(device)))
            soundio_panic("out of memory");

        device->buffer_duration_min = 0.10;
        device->buffer_duration_max = 4.0;

        // "period" is not a recognized concept in PulseAudio.

        device->purpose = SoundIoDevicePurposeOutput;

        if (sipa->current_devices_info->output_devices.append(device))
            soundio_panic("out of memory");
    }
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

static void source_info_callback(pa_context *pulse_context, const pa_source_info *info, int eol, void *userdata) {
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    int err;
    if (eol) {
        sipa->have_source_list = true;
        finish_device_query(si);
    } else {
        SoundIoDevicePrivate *dev = create<SoundIoDevicePrivate>();
        if (!dev)
            soundio_panic("out of memory");
        SoundIoDevice *device = &dev->pub;

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup(info->name);
        device->description = strdup(info->description);
        if (!device->name || !device->description)
            soundio_panic("out of memory");

        device->sample_rate_current = info->sample_spec.rate;
        // PulseAudio performs resampling, so any value is valid. Let's pick
        // some reasonable min and max values.
        device->sample_rate_min = min(8000, device->sample_rate_current);
        device->sample_rate_max = max(5644800, device->sample_rate_current);

        device->current_format = from_pulseaudio_format(info->sample_spec);
        // PulseAudio performs sample format conversion, so any PulseAudio
        // value is valid.
        if ((err = set_all_device_formats(device)))
            soundio_panic("out of memory");

        set_from_pulseaudio_channel_map(info->channel_map, &device->current_layout);
        // PulseAudio does channel layout remapping, so any channel layout is valid.
        if ((err = set_all_device_channel_layouts(device)))
            soundio_panic("out of memory");

        device->buffer_duration_min = 0.10;
        device->buffer_duration_max = 4.0;
        device->buffer_duration_current = -1.0;

        // "period" is not a recognized concept in PulseAudio.

        device->purpose = SoundIoDevicePurposeInput;

        if (sipa->current_devices_info->input_devices.append(device))
            soundio_panic("out of memory");
    }
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

static void server_info_callback(pa_context *pulse_context, const pa_server_info *info, void *userdata) {
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    assert(si);
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    free(sipa->default_sink_name);
    free(sipa->default_source_name);

    sipa->default_sink_name = strdup(info->default_sink_name);
    sipa->default_source_name = strdup(info->default_source_name);

    if (!sipa->default_sink_name || !sipa->default_source_name)
        soundio_panic("out of memory");

    sipa->have_default_sink = true;
    finish_device_query(si);
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

static void scan_devices(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    sipa->have_sink_list = false;
    sipa->have_default_sink = false;
    sipa->have_source_list = false;

    soundio_destroy_devices_info(sipa->current_devices_info);
    sipa->current_devices_info = create<SoundIoDevicesInfo>();
    if (!sipa->current_devices_info)
        soundio_panic("out of memory");

    pa_threaded_mainloop_lock(sipa->main_loop);

    pa_operation *list_sink_op = pa_context_get_sink_info_list(sipa->pulse_context,
            sink_info_callback, si);
    pa_operation *list_source_op = pa_context_get_source_info_list(sipa->pulse_context,
            source_info_callback, si);
    pa_operation *server_info_op = pa_context_get_server_info(sipa->pulse_context,
            server_info_callback, si);

    if (perform_operation(si, list_sink_op))
        soundio_panic("list sinks failed");
    if (perform_operation(si, list_source_op))
        soundio_panic("list sources failed");
    if (perform_operation(si, server_info_op))
        soundio_panic("get server info failed");

    pa_threaded_mainloop_signal(sipa->main_loop, 0);

    pa_threaded_mainloop_unlock(sipa->main_loop);
}

static void block_until_have_devices(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    if (sipa->have_devices_flag)
        return;
    pa_threaded_mainloop_lock(sipa->main_loop);
    while (!sipa->have_devices_flag) {
        pa_threaded_mainloop_wait(sipa->main_loop);
    }
    pa_threaded_mainloop_unlock(sipa->main_loop);
}

static void block_until_ready(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    if (sipa->ready_flag)
        return;
    pa_threaded_mainloop_lock(sipa->main_loop);
    while (!sipa->ready_flag) {
        pa_threaded_mainloop_wait(sipa->main_loop);
    }
    pa_threaded_mainloop_unlock(sipa->main_loop);
}

static void flush_events(SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    block_until_ready(si);

    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    if (sipa->device_scan_queued) {
        sipa->device_scan_queued = false;
        scan_devices(si);
    }

    SoundIoDevicesInfo *old_devices_info = nullptr;
    bool change = false;

    pa_threaded_mainloop_lock(sipa->main_loop);

    if (sipa->ready_devices_info) {
        old_devices_info = si->safe_devices_info;
        si->safe_devices_info = sipa->ready_devices_info;
        sipa->ready_devices_info = nullptr;
        change = true;
    }

    pa_threaded_mainloop_unlock(sipa->main_loop);

    if (change)
        soundio->on_devices_change(soundio);

    soundio_destroy_devices_info(old_devices_info);

    block_until_have_devices(si);
}

static void wait_events(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    flush_events(si);
    pa_threaded_mainloop_lock(sipa->main_loop);
    pa_threaded_mainloop_wait(sipa->main_loop);
    pa_threaded_mainloop_unlock(sipa->main_loop);
}

static void wakeup(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

static pa_sample_format_t to_pulseaudio_format(SoundIoFormat format) {
    switch (format) {
    case SoundIoFormatU8:         return PA_SAMPLE_U8;
    case SoundIoFormatS16LE:      return PA_SAMPLE_S16LE;
    case SoundIoFormatS16BE:      return PA_SAMPLE_S16BE;
    case SoundIoFormatS24LE:      return PA_SAMPLE_S24_32LE;
    case SoundIoFormatS24BE:      return PA_SAMPLE_S24_32BE;
    case SoundIoFormatS32LE:      return PA_SAMPLE_S32LE;
    case SoundIoFormatS32BE:      return PA_SAMPLE_S32BE;
    case SoundIoFormatFloat32LE:  return PA_SAMPLE_FLOAT32LE;
    case SoundIoFormatFloat32BE:  return PA_SAMPLE_FLOAT32BE;

    case SoundIoFormatInvalid:
    case SoundIoFormatS8:
    case SoundIoFormatU16LE:
    case SoundIoFormatU16BE:
    case SoundIoFormatU24LE:
    case SoundIoFormatU24BE:
    case SoundIoFormatU32LE:
    case SoundIoFormatU32BE:
    case SoundIoFormatFloat64LE:
    case SoundIoFormatFloat64BE:
        return PA_SAMPLE_INVALID;
    }
    return PA_SAMPLE_INVALID;
}

static pa_channel_position_t to_pulseaudio_channel_pos(SoundIoChannelId channel_id) {
    switch (channel_id) {
    case SoundIoChannelIdFrontLeft: return PA_CHANNEL_POSITION_FRONT_LEFT;
    case SoundIoChannelIdFrontRight: return PA_CHANNEL_POSITION_FRONT_RIGHT;
    case SoundIoChannelIdFrontCenter: return PA_CHANNEL_POSITION_FRONT_CENTER;
    case SoundIoChannelIdLfe: return PA_CHANNEL_POSITION_LFE;
    case SoundIoChannelIdBackLeft: return PA_CHANNEL_POSITION_REAR_LEFT;
    case SoundIoChannelIdBackRight: return PA_CHANNEL_POSITION_REAR_RIGHT;
    case SoundIoChannelIdFrontLeftCenter: return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
    case SoundIoChannelIdFrontRightCenter: return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
    case SoundIoChannelIdBackCenter: return PA_CHANNEL_POSITION_REAR_CENTER;
    case SoundIoChannelIdSideLeft: return PA_CHANNEL_POSITION_SIDE_LEFT;
    case SoundIoChannelIdSideRight: return PA_CHANNEL_POSITION_SIDE_RIGHT;
    case SoundIoChannelIdTopCenter: return PA_CHANNEL_POSITION_TOP_CENTER;
    case SoundIoChannelIdTopFrontLeft: return PA_CHANNEL_POSITION_TOP_FRONT_LEFT;
    case SoundIoChannelIdTopFrontCenter: return PA_CHANNEL_POSITION_TOP_FRONT_CENTER;
    case SoundIoChannelIdTopFrontRight: return PA_CHANNEL_POSITION_TOP_FRONT_RIGHT;
    case SoundIoChannelIdTopBackLeft: return PA_CHANNEL_POSITION_TOP_REAR_LEFT;
    case SoundIoChannelIdTopBackCenter: return PA_CHANNEL_POSITION_TOP_REAR_CENTER;
    case SoundIoChannelIdTopBackRight: return PA_CHANNEL_POSITION_TOP_REAR_RIGHT;

    case SoundIoChannelIdInvalid:
    case SoundIoChannelIdBackLeftCenter:
    case SoundIoChannelIdBackRightCenter:
    case SoundIoChannelIdFrontLeftWide:
    case SoundIoChannelIdFrontRightWide:
    case SoundIoChannelIdFrontLeftHigh:
    case SoundIoChannelIdFrontCenterHigh:
    case SoundIoChannelIdFrontRightHigh:
    case SoundIoChannelIdTopFrontLeftCenter:
    case SoundIoChannelIdTopFrontRightCenter:
    case SoundIoChannelIdTopSideLeft:
    case SoundIoChannelIdTopSideRight:
    case SoundIoChannelIdLeftLfe:
    case SoundIoChannelIdRightLfe:
    case SoundIoChannelIdBottomCenter:
    case SoundIoChannelIdBottomLeftCenter:
    case SoundIoChannelIdBottomRightCenter:
        return PA_CHANNEL_POSITION_INVALID;
    }
    return PA_CHANNEL_POSITION_INVALID;
}

static pa_channel_map to_pulseaudio_channel_map(const SoundIoChannelLayout *channel_layout) {
    pa_channel_map channel_map;
    channel_map.channels = channel_layout->channel_count;

    if ((unsigned)channel_layout->channel_count > PA_CHANNELS_MAX)
        soundio_panic("channel layout greater than pulseaudio max channels");

    for (int i = 0; i < channel_layout->channel_count; i += 1)
        channel_map.map[i] = to_pulseaudio_channel_pos(channel_layout->channels[i]);

    return channel_map;
}

static void playback_stream_state_callback(pa_stream *stream, void *userdata) {
    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate*) userdata;
    SoundIoOutStream *outstream = &os->pub;
    SoundIo *soundio = outstream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_READY:
            ospa->stream_ready = true;
            pa_threaded_mainloop_signal(sipa->main_loop, 0);
            break;
        case PA_STREAM_FAILED:
            soundio_panic("pulseaudio stream error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
            break;
    }
}

static void playback_stream_underflow_callback(pa_stream *stream, void *userdata) {
    SoundIoOutStream *outstream = (SoundIoOutStream*)userdata;
    outstream->underflow_callback(outstream);
}

static void playback_stream_write_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    SoundIoOutStream *outstream = (SoundIoOutStream*)(userdata);
    int frame_count = ((int)nbytes) / outstream->bytes_per_frame;
    outstream->write_callback(outstream, frame_count);
}

static void outstream_destroy_pa(SoundIoPrivate *si, SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;

    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    pa_stream *stream = ospa->stream;
    if (stream) {
        pa_threaded_mainloop_lock(sipa->main_loop);

        pa_stream_set_write_callback(stream, nullptr, nullptr);
        pa_stream_set_state_callback(stream, nullptr, nullptr);
        pa_stream_set_underflow_callback(stream, nullptr, nullptr);
        pa_stream_disconnect(stream);

        pa_stream_unref(stream);

        pa_threaded_mainloop_unlock(sipa->main_loop);

        ospa->stream = nullptr;
    }
}

static int outstream_open_pa(SoundIoPrivate *si, SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    SoundIoOutStream *outstream = &os->pub;

    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    ospa->stream_ready = false;

    assert(sipa->pulse_context);

    pa_threaded_mainloop_lock(sipa->main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_format(outstream->format);
    sample_spec.rate = outstream->sample_rate;

    sample_spec.channels = outstream->layout.channel_count;
    pa_channel_map channel_map = to_pulseaudio_channel_map(&outstream->layout);

    ospa->stream = pa_stream_new(sipa->pulse_context, outstream->name, &sample_spec, &channel_map);
    if (!ospa->stream) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        outstream_destroy_pa(si, os);
        return SoundIoErrorNoMem;
    }
    pa_stream_set_state_callback(ospa->stream, playback_stream_state_callback, os);
    pa_stream_set_write_callback(ospa->stream, playback_stream_write_callback, os);
    pa_stream_set_underflow_callback(ospa->stream, playback_stream_underflow_callback, outstream);

    ospa->buffer_attr.maxlength = UINT32_MAX;
    ospa->buffer_attr.tlength = UINT32_MAX;
    ospa->buffer_attr.prebuf = 0;
    ospa->buffer_attr.minreq = UINT32_MAX;
    ospa->buffer_attr.fragsize = UINT32_MAX;

    int bytes_per_second = outstream->bytes_per_frame * outstream->sample_rate;
    if (outstream->buffer_duration > 0.0) {
        int buffer_length = outstream->bytes_per_frame *
            ceil(outstream->buffer_duration * bytes_per_second / (double)outstream->bytes_per_frame);

        ospa->buffer_attr.maxlength = buffer_length;
        ospa->buffer_attr.tlength = buffer_length;
    }

    pa_threaded_mainloop_unlock(sipa->main_loop);

    return 0;
}

static int outstream_start_pa(SoundIoPrivate *si, SoundIoOutStreamPrivate *os) {
    SoundIoOutStream *outstream = &os->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;

    pa_threaded_mainloop_lock(sipa->main_loop);

    pa_stream_flags_t flags = (outstream->buffer_duration > 0.0) ? PA_STREAM_ADJUST_LATENCY : PA_STREAM_NOFLAGS;

    int err = pa_stream_connect_playback(ospa->stream,
            outstream->device->name, &ospa->buffer_attr,
            flags, nullptr, nullptr);
    if (err) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        return SoundIoErrorOpeningDevice;
    }

    while (!ospa->stream_ready)
        pa_threaded_mainloop_wait(sipa->main_loop);

    const pa_buffer_attr *attr = pa_stream_get_buffer_attr(ospa->stream);
    outstream->buffer_duration = (attr->maxlength /
        (double)outstream->bytes_per_frame) / (double)outstream->sample_rate;

    pa_threaded_mainloop_unlock(sipa->main_loop);

    return 0;
}

static int outstream_begin_write_pa(SoundIoPrivate *si,
        SoundIoOutStreamPrivate *os, SoundIoChannelArea **out_areas, int *frame_count)
{
    *out_areas = nullptr;

    SoundIoOutStream *outstream = &os->pub;
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    pa_stream *stream = ospa->stream;

    size_t byte_count = *frame_count * outstream->bytes_per_frame;

    if (pa_stream_begin_write(stream, (void**)&ospa->write_ptr, &byte_count))
        return SoundIoErrorStreaming;

    for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
        ospa->areas[ch].ptr = ospa->write_ptr + outstream->bytes_per_sample * ch;
        ospa->areas[ch].step = outstream->bytes_per_frame;
    }

    *frame_count = byte_count / outstream->bytes_per_frame;
    *out_areas = ospa->areas;

    return 0;
}

static int outstream_end_write_pa(SoundIoPrivate *si, SoundIoOutStreamPrivate *os, int frame_count) {
    SoundIoOutStream *outstream = &os->pub;
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    pa_stream *stream = ospa->stream;
    assert(frame_count >= 0);
    size_t byte_count = frame_count * outstream->bytes_per_frame;
    assert(byte_count < 1 * 1024 * 1024 * 1024); // attempting to write > 1GB is certainly a mistake
    if (pa_stream_write(stream, ospa->write_ptr, byte_count, NULL, 0, PA_SEEK_RELATIVE))
        return SoundIoErrorStreaming;
    return 0;
}

static int outstream_clear_buffer_pa(SoundIoPrivate *si,
        SoundIoOutStreamPrivate *os)
{
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    pa_stream *stream = ospa->stream;
    pa_threaded_mainloop_lock(sipa->main_loop);
    pa_operation *op = pa_stream_flush(stream, NULL, NULL);
    if (!op)
        return SoundIoErrorStreaming;
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(sipa->main_loop);
    return 0;
}

static int outstream_pause_pa(SoundIoPrivate *si, SoundIoOutStreamPrivate *os, bool pause) {
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    pa_threaded_mainloop_lock(sipa->main_loop);

    if (pause != pa_stream_is_corked(ospa->stream)) {
        pa_operation *op = pa_stream_cork(ospa->stream, pause, NULL, NULL);
        if (!op)
            return SoundIoErrorStreaming;
        pa_operation_unref(op);
    }

    pa_threaded_mainloop_unlock(sipa->main_loop);

    return 0;
}

static void recording_stream_state_callback(pa_stream *stream, void *userdata) {
    SoundIoInStreamPrivate *is = (SoundIoInStreamPrivate*)userdata;
    SoundIoInStreamPulseAudio *ispa = &is->backend_data.pulseaudio;
    SoundIoInStream *instream = &is->pub;
    SoundIo *soundio = instream->device->soundio;
    SoundIoPrivate *si = (SoundIoPrivate *)soundio;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_READY:
            ispa->stream_ready = true;
            pa_threaded_mainloop_signal(sipa->main_loop, 0);
            break;
        case PA_STREAM_FAILED:
            soundio_panic("pulseaudio stream error: %s",
                    pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
            break;
    }
}

static void recording_stream_read_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    SoundIoInStreamPrivate *is = (SoundIoInStreamPrivate*)userdata;
    SoundIoInStream *instream = &is->pub;
    assert(nbytes % instream->bytes_per_frame == 0);
    assert(nbytes > 0);
    int available_frame_count = nbytes / instream->bytes_per_frame;
    instream->read_callback(instream, available_frame_count);
}

static void instream_destroy_pa(SoundIoPrivate *si, SoundIoInStreamPrivate *is) {
    SoundIoInStreamPulseAudio *ispa = &is->backend_data.pulseaudio;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    pa_stream *stream = ispa->stream;
    if (stream) {
        pa_threaded_mainloop_lock(sipa->main_loop);

        pa_stream_set_state_callback(stream, nullptr, nullptr);
        pa_stream_set_read_callback(stream, nullptr, nullptr);
        pa_stream_disconnect(stream);
        pa_stream_unref(stream);

        pa_threaded_mainloop_unlock(sipa->main_loop);

        ispa->stream = nullptr;
    }
}

static int instream_open_pa(SoundIoPrivate *si, SoundIoInStreamPrivate *is) {
    SoundIoInStreamPulseAudio *ispa = &is->backend_data.pulseaudio;
    SoundIoInStream *instream = &is->pub;

    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    ispa->stream_ready = false;

    pa_threaded_mainloop_lock(sipa->main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_format(instream->format);
    sample_spec.rate = instream->sample_rate;
    sample_spec.channels = instream->layout.channel_count;

    pa_channel_map channel_map = to_pulseaudio_channel_map(&instream->layout);

    ispa->stream = pa_stream_new(sipa->pulse_context, instream->name, &sample_spec, &channel_map);
    if (!ispa->stream) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        instream_destroy_pa(si, is);
        return SoundIoErrorNoMem;
    }

    pa_stream *stream = ispa->stream;

    pa_stream_set_state_callback(stream, recording_stream_state_callback, is);
    pa_stream_set_read_callback(stream, recording_stream_read_callback, is);

    ispa->buffer_attr.maxlength = UINT32_MAX;
    ispa->buffer_attr.tlength = UINT32_MAX;
    ispa->buffer_attr.prebuf = 0;
    ispa->buffer_attr.minreq = UINT32_MAX;
    ispa->buffer_attr.fragsize = UINT32_MAX;

    int bytes_per_second = instream->bytes_per_frame * instream->sample_rate;
    if (instream->buffer_duration > 0.0) {
        int buffer_length = instream->bytes_per_frame *
            ceil(instream->buffer_duration * bytes_per_second / (double)instream->bytes_per_frame);
        ispa->buffer_attr.maxlength = buffer_length;
    }

    if (instream->period_duration > 0.0) {
        int buffer_length = instream->bytes_per_frame *
            ceil(instream->period_duration * bytes_per_second / (double)instream->bytes_per_frame);
        ispa->buffer_attr.fragsize = buffer_length;
    }

    pa_threaded_mainloop_unlock(sipa->main_loop);

    return 0;
}

static int instream_start_pa(SoundIoPrivate *si, SoundIoInStreamPrivate *is) {
    SoundIoInStream *instream = &is->pub;
    SoundIoInStreamPulseAudio *ispa = &is->backend_data.pulseaudio;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    pa_threaded_mainloop_lock(sipa->main_loop);

    pa_stream_flags_t flags = (instream->period_duration > 0.0) ? PA_STREAM_ADJUST_LATENCY : PA_STREAM_NOFLAGS;

    int err = pa_stream_connect_record(ispa->stream,
            instream->device->name,
            &ispa->buffer_attr, flags);
    if (err) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        return SoundIoErrorOpeningDevice;
    }

    while (!ispa->stream_ready)
        pa_threaded_mainloop_wait(sipa->main_loop);

    const pa_buffer_attr *attr = pa_stream_get_buffer_attr(ispa->stream);
    instream->buffer_duration = (attr->maxlength /
        (double)instream->bytes_per_frame) / (double)instream->sample_rate;
    instream->period_duration = (attr->fragsize /
        (double)instream->bytes_per_frame) / (double)instream->sample_rate;

    pa_threaded_mainloop_unlock(sipa->main_loop);
    return 0;
}

static int instream_begin_read_pa(SoundIoPrivate *si,
        SoundIoInStreamPrivate *is, SoundIoChannelArea **out_areas, int *frame_count)
{
    SoundIoInStream *instream = &is->pub;
    SoundIoInStreamPulseAudio *ispa = &is->backend_data.pulseaudio;
    pa_stream *stream = ispa->stream;

    assert(ispa->stream_ready);

    char *data;
    size_t nbytes = *frame_count * instream->bytes_per_frame;
    if (pa_stream_peek(stream, (const void **)&data, &nbytes)) {
        *out_areas = nullptr;
        *frame_count = 0;
        return SoundIoErrorStreaming;
    }

    if (data) {
        for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
            ispa->areas[ch].ptr = data + instream->bytes_per_sample * ch;
            ispa->areas[ch].step = instream->bytes_per_frame;
        }

        *frame_count = nbytes / instream->bytes_per_frame;
        *out_areas = ispa->areas;
    } else {
        *frame_count = nbytes / instream->bytes_per_frame;
        *out_areas = nullptr;
    }

    return 0;
}

static int instream_end_read_pa(SoundIoPrivate *si, SoundIoInStreamPrivate *is) {
    SoundIoInStreamPulseAudio *ispa = &is->backend_data.pulseaudio;
    pa_stream *stream = ispa->stream;
    if (pa_stream_drop(stream))
        return SoundIoErrorStreaming;

    return 0;
}

static int instream_pause_pa(SoundIoPrivate *si, SoundIoInStreamPrivate *is, bool pause) {
    SoundIoInStreamPulseAudio *ispa = &is->backend_data.pulseaudio;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    pa_threaded_mainloop_lock(sipa->main_loop);

    if (pause != pa_stream_is_corked(ispa->stream)) {
        pa_operation *op = pa_stream_cork(ispa->stream, pause, NULL, NULL);
        if (!op)
            return SoundIoErrorStreaming;
        pa_operation_unref(op);
    }

    pa_threaded_mainloop_unlock(sipa->main_loop);

    return 0;
}

int soundio_pulseaudio_init(SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    sipa->connection_refused = false;
    sipa->device_scan_queued = false;
    sipa->ready_flag = false;
    sipa->have_devices_flag = false;

    sipa->main_loop = pa_threaded_mainloop_new();
    if (!sipa->main_loop) {
        destroy_pa(si);
        return SoundIoErrorNoMem;
    }

    pa_mainloop_api *main_loop_api = pa_threaded_mainloop_get_api(sipa->main_loop);

    sipa->props = pa_proplist_new();
    if (!sipa->props) {
        destroy_pa(si);
        return SoundIoErrorNoMem;
    }

    sipa->pulse_context = pa_context_new_with_proplist(main_loop_api, soundio->app_name, sipa->props);
    if (!sipa->pulse_context) {
        destroy_pa(si);
        return SoundIoErrorNoMem;
    }

    pa_context_set_subscribe_callback(sipa->pulse_context, subscribe_callback, si);
    pa_context_set_state_callback(sipa->pulse_context, context_state_callback, si);

    int err = pa_context_connect(sipa->pulse_context, NULL, (pa_context_flags_t)0, NULL);
    if (err) {
        destroy_pa(si);
        return SoundIoErrorInitAudioBackend;
    }

    if (sipa->connection_refused) {
        destroy_pa(si);
        return SoundIoErrorInitAudioBackend;
    }

    if (pa_threaded_mainloop_start(sipa->main_loop)) {
        destroy_pa(si);
        return SoundIoErrorNoMem;
    }

    si->destroy = destroy_pa;
    si->flush_events = flush_events;
    si->wait_events = wait_events;
    si->wakeup = wakeup;

    si->outstream_open = outstream_open_pa;
    si->outstream_destroy = outstream_destroy_pa;
    si->outstream_start = outstream_start_pa;
    si->outstream_begin_write = outstream_begin_write_pa;
    si->outstream_end_write = outstream_end_write_pa;
    si->outstream_clear_buffer = outstream_clear_buffer_pa;
    si->outstream_pause = outstream_pause_pa;

    si->instream_open = instream_open_pa;
    si->instream_destroy = instream_destroy_pa;
    si->instream_start = instream_start_pa;
    si->instream_begin_read = instream_begin_read_pa;
    si->instream_end_read = instream_end_read_pa;
    si->instream_pause = instream_pause_pa;

    return 0;
}
