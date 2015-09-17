/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "pulseaudio.hpp"
#include "soundio.hpp"

#include <string.h>
#include <stdio.h>


static void subscribe_callback(pa_context *context,
        pa_subscription_event_type_t event_bits, uint32_t index, void *userdata)
{
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    sipa->device_scan_queued = true;
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
    soundio->on_events_signal(soundio);
}

static int subscribe_to_events(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    pa_subscription_mask_t events = (pa_subscription_mask_t)(
            PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE|PA_SUBSCRIPTION_MASK_SERVER);
    pa_operation *subscribe_op = pa_context_subscribe(sipa->pulse_context, events, nullptr, si);
    if (!subscribe_op)
        return SoundIoErrorNoMem;
    pa_operation_unref(subscribe_op);
    return 0;
}

static void context_state_callback(pa_context *context, void *userdata) {
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    SoundIo *soundio = &si->pub;

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
        sipa->ready_flag = true;
        pa_threaded_mainloop_signal(sipa->main_loop, 0);
        return;
    case PA_CONTEXT_TERMINATED: // The connection was terminated cleanly.
        pa_threaded_mainloop_signal(sipa->main_loop, 0);
        return;
    case PA_CONTEXT_FAILED: // The connection failed or was disconnected.
        if (sipa->ready_flag) {
            sipa->connection_err = SoundIoErrorBackendDisconnected;
        } else {
            sipa->connection_err = SoundIoErrorInitAudioBackend;
            sipa->ready_flag = true;
        }
        pa_threaded_mainloop_signal(sipa->main_loop, 0);
        soundio->on_events_signal(soundio);
        return;
    }
}

static void destroy_pa(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    if (sipa->main_loop)
        pa_threaded_mainloop_stop(sipa->main_loop);

    pa_context_disconnect(sipa->pulse_context);
    pa_context_unref(sipa->pulse_context);

    soundio_destroy_devices_info(sipa->current_devices_info);
    soundio_destroy_devices_info(sipa->ready_devices_info);

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

    case PA_CHANNEL_POSITION_AUX0: return SoundIoChannelIdAux0;
    case PA_CHANNEL_POSITION_AUX1: return SoundIoChannelIdAux1;
    case PA_CHANNEL_POSITION_AUX2: return SoundIoChannelIdAux2;
    case PA_CHANNEL_POSITION_AUX3: return SoundIoChannelIdAux3;
    case PA_CHANNEL_POSITION_AUX4: return SoundIoChannelIdAux4;
    case PA_CHANNEL_POSITION_AUX5: return SoundIoChannelIdAux5;
    case PA_CHANNEL_POSITION_AUX6: return SoundIoChannelIdAux6;
    case PA_CHANNEL_POSITION_AUX7: return SoundIoChannelIdAux7;
    case PA_CHANNEL_POSITION_AUX8: return SoundIoChannelIdAux8;
    case PA_CHANNEL_POSITION_AUX9: return SoundIoChannelIdAux9;
    case PA_CHANNEL_POSITION_AUX10: return SoundIoChannelIdAux10;
    case PA_CHANNEL_POSITION_AUX11: return SoundIoChannelIdAux11;
    case PA_CHANNEL_POSITION_AUX12: return SoundIoChannelIdAux12;
    case PA_CHANNEL_POSITION_AUX13: return SoundIoChannelIdAux13;
    case PA_CHANNEL_POSITION_AUX14: return SoundIoChannelIdAux14;
    case PA_CHANNEL_POSITION_AUX15: return SoundIoChannelIdAux15;

    default: return SoundIoChannelIdInvalid;
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
    if (!op)
        return SoundIoErrorNoMem;
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
            return SoundIoErrorInterrupted;
        }
    }
}

static void sink_info_callback(pa_context *pulse_context, const pa_sink_info *info, int eol, void *userdata) {
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    int err;
    if (eol) {
        pa_threaded_mainloop_signal(sipa->main_loop, 0);
        return;
    }
    if (sipa->device_query_err)
        return;

    SoundIoDevicePrivate *dev = allocate<SoundIoDevicePrivate>(1);
    if (!dev) {
        sipa->device_query_err = SoundIoErrorNoMem;
        return;
    }
    SoundIoDevice *device = &dev->pub;

    device->ref_count = 1;
    device->soundio = soundio;
    device->id = strdup(info->name);
    device->name = strdup(info->description);
    if (!device->id || !device->name) {
        soundio_device_unref(device);
        sipa->device_query_err = SoundIoErrorNoMem;
        return;
    }

    device->sample_rate_current = info->sample_spec.rate;
    // PulseAudio performs resampling, so any value is valid. Let's pick
    // some reasonable min and max values.
    device->sample_rate_count = 1;
    device->sample_rates = &dev->prealloc_sample_rate_range;
    device->sample_rates[0].min = min(SOUNDIO_MIN_SAMPLE_RATE, device->sample_rate_current);
    device->sample_rates[0].max = max(SOUNDIO_MAX_SAMPLE_RATE, device->sample_rate_current);

    device->current_format = from_pulseaudio_format(info->sample_spec);
    // PulseAudio performs sample format conversion, so any PulseAudio
    // value is valid.
    if ((err = set_all_device_formats(device))) {
        soundio_device_unref(device);
        sipa->device_query_err = SoundIoErrorNoMem;
        return;
    }

    set_from_pulseaudio_channel_map(info->channel_map, &device->current_layout);
    // PulseAudio does channel layout remapping, so any channel layout is valid.
    if ((err = set_all_device_channel_layouts(device))) {
        soundio_device_unref(device);
        sipa->device_query_err = SoundIoErrorNoMem;
        return;
    }

    device->aim = SoundIoDeviceAimOutput;

    if (sipa->current_devices_info->output_devices.append(device)) {
        soundio_device_unref(device);
        sipa->device_query_err = SoundIoErrorNoMem;
        return;
    }
}

static void source_info_callback(pa_context *pulse_context, const pa_source_info *info, int eol, void *userdata) {
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    int err;

    if (eol) {
        pa_threaded_mainloop_signal(sipa->main_loop, 0);
        return;
    }
    if (sipa->device_query_err)
        return;

    SoundIoDevicePrivate *dev = allocate<SoundIoDevicePrivate>(1);
    if (!dev) {
        sipa->device_query_err = SoundIoErrorNoMem;
        return;
    }
    SoundIoDevice *device = &dev->pub;

    device->ref_count = 1;
    device->soundio = soundio;
    device->id = strdup(info->name);
    device->name = strdup(info->description);
    if (!device->id || !device->name) {
        soundio_device_unref(device);
        sipa->device_query_err = SoundIoErrorNoMem;
        return;
    }

    device->sample_rate_current = info->sample_spec.rate;
    // PulseAudio performs resampling, so any value is valid. Let's pick
    // some reasonable min and max values.
    device->sample_rate_count = 1;
    device->sample_rates = &dev->prealloc_sample_rate_range;
    device->sample_rates[0].min = min(8000, device->sample_rate_current);
    device->sample_rates[0].max = max(5644800, device->sample_rate_current);

    device->current_format = from_pulseaudio_format(info->sample_spec);
    // PulseAudio performs sample format conversion, so any PulseAudio
    // value is valid.
    if ((err = set_all_device_formats(device))) {
        soundio_device_unref(device);
        sipa->device_query_err = SoundIoErrorNoMem;
        return;
    }

    set_from_pulseaudio_channel_map(info->channel_map, &device->current_layout);
    // PulseAudio does channel layout remapping, so any channel layout is valid.
    if ((err = set_all_device_channel_layouts(device))) {
        soundio_device_unref(device);
        sipa->device_query_err = SoundIoErrorNoMem;
        return;
    }

    device->aim = SoundIoDeviceAimInput;

    if (sipa->current_devices_info->input_devices.append(device)) {
        soundio_device_unref(device);
        sipa->device_query_err = SoundIoErrorNoMem;
        return;
    }
}

static void server_info_callback(pa_context *pulse_context, const pa_server_info *info, void *userdata) {
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    assert(si);
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    assert(!sipa->default_sink_name);
    assert(!sipa->default_source_name);

    sipa->default_sink_name = strdup(info->default_sink_name);
    sipa->default_source_name = strdup(info->default_source_name);

    if (!sipa->default_sink_name || !sipa->default_source_name)
        sipa->device_query_err = SoundIoErrorNoMem;

    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

// always called even when refresh_devices succeeds
static void cleanup_refresh_devices(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    soundio_destroy_devices_info(sipa->current_devices_info);
    sipa->current_devices_info = nullptr;

    free(sipa->default_sink_name);
    sipa->default_sink_name = nullptr;

    free(sipa->default_source_name);
    sipa->default_source_name = nullptr;
}

// call this while holding the main loop lock
static int refresh_devices(SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    assert(!sipa->current_devices_info);
    sipa->current_devices_info = allocate<SoundIoDevicesInfo>(1);
    if (!sipa->current_devices_info)
        return SoundIoErrorNoMem;

    pa_operation *list_sink_op = pa_context_get_sink_info_list(sipa->pulse_context, sink_info_callback, si);
    pa_operation *list_source_op = pa_context_get_source_info_list(sipa->pulse_context, source_info_callback, si);
    pa_operation *server_info_op = pa_context_get_server_info(sipa->pulse_context, server_info_callback, si);

    int err;
    if ((err = perform_operation(si, list_sink_op))) {
        return err;
    }
    if ((err = perform_operation(si, list_source_op))) {
        return err;
    }
    if ((err = perform_operation(si, server_info_op))) {
        return err;
    }

    if (sipa->device_query_err) {
        return sipa->device_query_err;
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
            assert(device->aim == SoundIoDeviceAimInput);
            if (strcmp(device->id, sipa->default_source_name) == 0) {
                sipa->current_devices_info->default_input_index = i;
            }
        }
    }

    if (sipa->current_devices_info->output_devices.length > 0) {
        sipa->current_devices_info->default_output_index = 0;
        for (int i = 0; i < sipa->current_devices_info->output_devices.length; i += 1) {
            SoundIoDevice *device = sipa->current_devices_info->output_devices.at(i);
            assert(device->aim == SoundIoDeviceAimOutput);
            if (strcmp(device->id, sipa->default_sink_name) == 0) {
                sipa->current_devices_info->default_output_index = i;
            }
        }
    }

    soundio_destroy_devices_info(sipa->ready_devices_info);
    sipa->ready_devices_info = sipa->current_devices_info;
    sipa->current_devices_info = nullptr;
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
    soundio->on_events_signal(soundio);

    return 0;
}

static void my_flush_events(SoundIoPrivate *si, bool wait) {
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    bool change = false;
    bool cb_shutdown = false;
    SoundIoDevicesInfo *old_devices_info = nullptr;

    pa_threaded_mainloop_lock(sipa->main_loop);

    if (wait)
        pa_threaded_mainloop_wait(sipa->main_loop);

    if (sipa->device_scan_queued && !sipa->connection_err) {
        sipa->device_scan_queued = false;
        sipa->connection_err = refresh_devices(si);
        cleanup_refresh_devices(si);
    }

    if (sipa->connection_err && !sipa->emitted_shutdown_cb) {
        sipa->emitted_shutdown_cb = true;
        cb_shutdown = true;
    } else if (sipa->ready_devices_info) {
        old_devices_info = si->safe_devices_info;
        si->safe_devices_info = sipa->ready_devices_info;
        sipa->ready_devices_info = nullptr;
        change = true;
    }

    pa_threaded_mainloop_unlock(sipa->main_loop);

    if (cb_shutdown)
        soundio->on_backend_disconnect(soundio, sipa->connection_err);
    else if (change)
        soundio->on_devices_change(soundio);

    soundio_destroy_devices_info(old_devices_info);
}

static void flush_events_pa(SoundIoPrivate *si) {
    my_flush_events(si, false);
}

static void wait_events_pa(SoundIoPrivate *si) {
    my_flush_events(si, false);
    my_flush_events(si, true);
}

static void wakeup_pa(SoundIoPrivate *si) {
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    pa_threaded_mainloop_lock(sipa->main_loop);
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
    pa_threaded_mainloop_unlock(sipa->main_loop);
}

static void force_device_scan_pa(SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    pa_threaded_mainloop_lock(sipa->main_loop);
    sipa->device_scan_queued = true;
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
    soundio->on_events_signal(soundio);
    pa_threaded_mainloop_unlock(sipa->main_loop);
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

    case SoundIoChannelIdAux0: return PA_CHANNEL_POSITION_AUX0;
    case SoundIoChannelIdAux1: return PA_CHANNEL_POSITION_AUX1;
    case SoundIoChannelIdAux2: return PA_CHANNEL_POSITION_AUX2;
    case SoundIoChannelIdAux3: return PA_CHANNEL_POSITION_AUX3;
    case SoundIoChannelIdAux4: return PA_CHANNEL_POSITION_AUX4;
    case SoundIoChannelIdAux5: return PA_CHANNEL_POSITION_AUX5;
    case SoundIoChannelIdAux6: return PA_CHANNEL_POSITION_AUX6;
    case SoundIoChannelIdAux7: return PA_CHANNEL_POSITION_AUX7;
    case SoundIoChannelIdAux8: return PA_CHANNEL_POSITION_AUX8;
    case SoundIoChannelIdAux9: return PA_CHANNEL_POSITION_AUX9;
    case SoundIoChannelIdAux10: return PA_CHANNEL_POSITION_AUX10;
    case SoundIoChannelIdAux11: return PA_CHANNEL_POSITION_AUX11;
    case SoundIoChannelIdAux12: return PA_CHANNEL_POSITION_AUX12;
    case SoundIoChannelIdAux13: return PA_CHANNEL_POSITION_AUX13;
    case SoundIoChannelIdAux14: return PA_CHANNEL_POSITION_AUX14;
    case SoundIoChannelIdAux15: return PA_CHANNEL_POSITION_AUX15;

    default:
        return PA_CHANNEL_POSITION_INVALID;
    }
}

static pa_channel_map to_pulseaudio_channel_map(const SoundIoChannelLayout *channel_layout) {
    pa_channel_map channel_map;
    channel_map.channels = channel_layout->channel_count;

    assert((unsigned)channel_layout->channel_count <= PA_CHANNELS_MAX);

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
            outstream->error_callback(outstream, SoundIoErrorStreaming);
            break;
    }
}

static void playback_stream_underflow_callback(pa_stream *stream, void *userdata) {
    SoundIoOutStream *outstream = (SoundIoOutStream*)userdata;
    outstream->underflow_callback(outstream);
}

static void playback_stream_write_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    SoundIoOutStreamPrivate *os = (SoundIoOutStreamPrivate*)(userdata);
    SoundIoOutStream *outstream = &os->pub;
    int frame_count = nbytes / outstream->bytes_per_frame;
    outstream->write_callback(outstream, 0, frame_count);
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
        pa_stream_set_overflow_callback(stream, nullptr, nullptr);
        pa_stream_disconnect(stream);

        pa_stream_unref(stream);

        pa_threaded_mainloop_unlock(sipa->main_loop);

        ospa->stream = nullptr;
    }
}

static void timing_update_callback(pa_stream *stream, int success, void *userdata) {
    SoundIoPrivate *si = (SoundIoPrivate *)userdata;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

static int outstream_open_pa(SoundIoPrivate *si, SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    SoundIoOutStream *outstream = &os->pub;

    if ((unsigned)outstream->layout.channel_count > PA_CHANNELS_MAX)
        return SoundIoErrorIncompatibleBackend;

    if (!outstream->name)
        outstream->name = "SoundIoOutStream";

    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    ospa->stream_ready.store(false);
    ospa->clear_buffer_flag.test_and_set();

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

    ospa->buffer_attr.maxlength = UINT32_MAX;
    ospa->buffer_attr.tlength = UINT32_MAX;
    ospa->buffer_attr.prebuf = 0;
    ospa->buffer_attr.minreq = UINT32_MAX;
    ospa->buffer_attr.fragsize = UINT32_MAX;

    int bytes_per_second = outstream->bytes_per_frame * outstream->sample_rate;
    if (outstream->software_latency > 0.0) {
        int buffer_length = outstream->bytes_per_frame *
            ceil_dbl_to_int(outstream->software_latency * bytes_per_second / (double)outstream->bytes_per_frame);

        ospa->buffer_attr.maxlength = buffer_length;
        ospa->buffer_attr.tlength = buffer_length;
    }

    pa_stream_flags_t flags = (pa_stream_flags_t)(PA_STREAM_START_CORKED|PA_STREAM_AUTO_TIMING_UPDATE);
    if (outstream->software_latency > 0.0)
        flags = (pa_stream_flags_t) (flags | PA_STREAM_ADJUST_LATENCY);

    int err = pa_stream_connect_playback(ospa->stream,
            outstream->device->id, &ospa->buffer_attr,
            flags, nullptr, nullptr);
    if (err) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        return SoundIoErrorOpeningDevice;
    }

    while (!ospa->stream_ready.load())
        pa_threaded_mainloop_wait(sipa->main_loop);

    pa_operation *update_timing_info_op = pa_stream_update_timing_info(ospa->stream, timing_update_callback, si);
    if ((err = perform_operation(si, update_timing_info_op))) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        return err;
    }

    size_t writable_size = pa_stream_writable_size(ospa->stream);
    outstream->software_latency = writable_size / bytes_per_second;

    pa_threaded_mainloop_unlock(sipa->main_loop);

    return 0;
}

static int outstream_start_pa(SoundIoPrivate *si, SoundIoOutStreamPrivate *os) {
    SoundIoOutStream *outstream = &os->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;

    pa_threaded_mainloop_lock(sipa->main_loop);

    ospa->write_byte_count = pa_stream_writable_size(ospa->stream);
    int frame_count = ospa->write_byte_count / outstream->bytes_per_frame;
    outstream->write_callback(outstream, 0, frame_count);

    pa_operation *op = pa_stream_cork(ospa->stream, false, nullptr, nullptr);
    if (!op) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        return SoundIoErrorStreaming;
    }
    pa_operation_unref(op);
    pa_stream_set_write_callback(ospa->stream, playback_stream_write_callback, os);
    pa_stream_set_underflow_callback(ospa->stream, playback_stream_underflow_callback, outstream);
    pa_stream_set_overflow_callback(ospa->stream, playback_stream_underflow_callback, outstream);

    pa_threaded_mainloop_unlock(sipa->main_loop);

    return 0;
}

static int outstream_begin_write_pa(SoundIoPrivate *si,
        SoundIoOutStreamPrivate *os, SoundIoChannelArea **out_areas, int *frame_count)
{
    SoundIoOutStream *outstream = &os->pub;
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    pa_stream *stream = ospa->stream;

    ospa->write_byte_count = *frame_count * outstream->bytes_per_frame;
    if (pa_stream_begin_write(stream, (void**)&ospa->write_ptr, &ospa->write_byte_count))
        return SoundIoErrorStreaming;

    for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
        ospa->areas[ch].ptr = ospa->write_ptr + outstream->bytes_per_sample * ch;
        ospa->areas[ch].step = outstream->bytes_per_frame;
    }

    *frame_count = ospa->write_byte_count / outstream->bytes_per_frame;
    *out_areas = ospa->areas;

    return 0;
}

static int outstream_end_write_pa(SoundIoPrivate *si, SoundIoOutStreamPrivate *os) {
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    pa_stream *stream = ospa->stream;

    pa_seek_mode_t seek_mode = ospa->clear_buffer_flag.test_and_set() ? PA_SEEK_RELATIVE : PA_SEEK_RELATIVE_ON_READ;
    if (pa_stream_write(stream, ospa->write_ptr, ospa->write_byte_count, nullptr, 0, seek_mode))
        return SoundIoErrorStreaming;

    return 0;
}

static int outstream_clear_buffer_pa(SoundIoPrivate *si,
        SoundIoOutStreamPrivate *os)
{
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    ospa->clear_buffer_flag.clear();
    return 0;
}

static int outstream_pause_pa(SoundIoPrivate *si, SoundIoOutStreamPrivate *os, bool pause) {
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    if (!pa_threaded_mainloop_in_thread(sipa->main_loop)) {
        pa_threaded_mainloop_lock(sipa->main_loop);
    }

    if (pause != pa_stream_is_corked(ospa->stream)) {
        pa_operation *op = pa_stream_cork(ospa->stream, pause, NULL, NULL);
        if (!op) {
            pa_threaded_mainloop_unlock(sipa->main_loop);
            return SoundIoErrorStreaming;
        }
        pa_operation_unref(op);
    }

    if (!pa_threaded_mainloop_in_thread(sipa->main_loop)) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
    }

    return 0;
}

static int outstream_get_latency_pa(SoundIoPrivate *si, SoundIoOutStreamPrivate *os, double *out_latency) {
    SoundIoOutStreamPulseAudio *ospa = &os->backend_data.pulseaudio;

    int err;
    pa_usec_t r_usec;
    int negative;
    if ((err = pa_stream_get_latency(ospa->stream, &r_usec, &negative))) {
        return SoundIoErrorStreaming;
    }
    *out_latency = r_usec / 1000000.0;
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
            instream->error_callback(instream, SoundIoErrorStreaming);
            break;
    }
}

static void recording_stream_read_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    SoundIoInStreamPrivate *is = (SoundIoInStreamPrivate*)userdata;
    SoundIoInStream *instream = &is->pub;
    assert(nbytes % instream->bytes_per_frame == 0);
    assert(nbytes > 0);
    int available_frame_count = nbytes / instream->bytes_per_frame;
    instream->read_callback(instream, 0, available_frame_count);
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

    if ((unsigned)instream->layout.channel_count > PA_CHANNELS_MAX)
        return SoundIoErrorIncompatibleBackend;
    if (!instream->name)
        instream->name = "SoundIoInStream";

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

    if (instream->software_latency > 0.0) {
        int bytes_per_second = instream->bytes_per_frame * instream->sample_rate;
        int buffer_length = instream->bytes_per_frame *
            ceil_dbl_to_int(instream->software_latency * bytes_per_second / (double)instream->bytes_per_frame);
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

    pa_stream_flags_t flags = PA_STREAM_AUTO_TIMING_UPDATE;
    if (instream->software_latency > 0.0)
        flags = (pa_stream_flags_t) (flags|PA_STREAM_ADJUST_LATENCY);

    int err = pa_stream_connect_record(ispa->stream,
            instream->device->id,
            &ispa->buffer_attr, flags);
    if (err) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        return SoundIoErrorOpeningDevice;
    }

    while (!ispa->stream_ready)
        pa_threaded_mainloop_wait(sipa->main_loop);

    pa_operation *update_timing_info_op = pa_stream_update_timing_info(ispa->stream, timing_update_callback, si);
    if ((err = perform_operation(si, update_timing_info_op))) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        return err;
    }


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

    if (!ispa->peek_buf) {
        if (pa_stream_peek(stream, (const void **)&ispa->peek_buf, &ispa->peek_buf_size))
            return SoundIoErrorStreaming;

        ispa->peek_buf_frames_left = ispa->peek_buf_size / instream->bytes_per_frame;
        ispa->peek_buf_index = 0;

        // hole
        if (!ispa->peek_buf) {
            *frame_count = ispa->peek_buf_frames_left;
            *out_areas = nullptr;
            return 0;
        }
    }

    ispa->read_frame_count = min(*frame_count, ispa->peek_buf_frames_left);
    *frame_count = ispa->read_frame_count;
    for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
        ispa->areas[ch].ptr = ispa->peek_buf + ispa->peek_buf_index + instream->bytes_per_sample * ch;
        ispa->areas[ch].step = instream->bytes_per_frame;
    }

    *out_areas = ispa->areas;

    return 0;
}

static int instream_end_read_pa(SoundIoPrivate *si, SoundIoInStreamPrivate *is) {
    SoundIoInStream *instream = &is->pub;
    SoundIoInStreamPulseAudio *ispa = &is->backend_data.pulseaudio;
    pa_stream *stream = ispa->stream;

    // hole
    if (!ispa->peek_buf) {
        if (pa_stream_drop(stream))
            return SoundIoErrorStreaming;
        return 0;
    }

    size_t advance_bytes = ispa->read_frame_count * instream->bytes_per_frame;
    ispa->peek_buf_index += advance_bytes;
    ispa->peek_buf_frames_left -= ispa->read_frame_count;

    if (ispa->peek_buf_index >= ispa->peek_buf_size) {
        if (pa_stream_drop(stream))
            return SoundIoErrorStreaming;
        ispa->peek_buf = nullptr;
    }

    return 0;
}

static int instream_pause_pa(SoundIoPrivate *si, SoundIoInStreamPrivate *is, bool pause) {
    SoundIoInStreamPulseAudio *ispa = &is->backend_data.pulseaudio;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    if (!pa_threaded_mainloop_in_thread(sipa->main_loop)) {
        pa_threaded_mainloop_lock(sipa->main_loop);
    }

    if (pause != pa_stream_is_corked(ispa->stream)) {
        pa_operation *op = pa_stream_cork(ispa->stream, pause, NULL, NULL);
        if (!op)
            return SoundIoErrorStreaming;
        pa_operation_unref(op);
    }

    if (!pa_threaded_mainloop_in_thread(sipa->main_loop)) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
    }

    return 0;
}

static int instream_get_latency_pa(SoundIoPrivate *si, SoundIoInStreamPrivate *is, double *out_latency) {
    SoundIoInStreamPulseAudio *ispa = &is->backend_data.pulseaudio;

    int err;
    pa_usec_t r_usec;
    int negative;
    if ((err = pa_stream_get_latency(ispa->stream, &r_usec, &negative))) {
        return SoundIoErrorStreaming;
    }
    *out_latency = r_usec / 1000000.0;
    return 0;
}

int soundio_pulseaudio_init(SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;
    SoundIoPulseAudio *sipa = &si->backend_data.pulseaudio;

    sipa->device_scan_queued = true;

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

    if (pa_threaded_mainloop_start(sipa->main_loop)) {
        destroy_pa(si);
        return SoundIoErrorNoMem;
    }

    pa_threaded_mainloop_lock(sipa->main_loop);

    // block until ready
    while (!sipa->ready_flag)
        pa_threaded_mainloop_wait(sipa->main_loop);

    if (sipa->connection_err) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        destroy_pa(si);
        return sipa->connection_err;
    }

    if ((err = subscribe_to_events(si))) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        destroy_pa(si);
        return err;
    }

    pa_threaded_mainloop_unlock(sipa->main_loop);

    si->destroy = destroy_pa;
    si->flush_events = flush_events_pa;
    si->wait_events = wait_events_pa;
    si->wakeup = wakeup_pa;
    si->force_device_scan = force_device_scan_pa;

    si->outstream_open = outstream_open_pa;
    si->outstream_destroy = outstream_destroy_pa;
    si->outstream_start = outstream_start_pa;
    si->outstream_begin_write = outstream_begin_write_pa;
    si->outstream_end_write = outstream_end_write_pa;
    si->outstream_clear_buffer = outstream_clear_buffer_pa;
    si->outstream_pause = outstream_pause_pa;
    si->outstream_get_latency = outstream_get_latency_pa;

    si->instream_open = instream_open_pa;
    si->instream_destroy = instream_destroy_pa;
    si->instream_start = instream_start_pa;
    si->instream_begin_read = instream_begin_read_pa;
    si->instream_end_read = instream_end_read_pa;
    si->instream_pause = instream_pause_pa;
    si->instream_get_latency = instream_get_latency_pa;

    return 0;
}
