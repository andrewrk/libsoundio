/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "pulseaudio.hpp"
#include "soundio.hpp"
#include "atomics.hpp"

#include <string.h>
#include <math.h>

#include <pulse/pulseaudio.h>

struct SoundIoOutStreamPulseAudio {
    pa_stream *stream;
    atomic_bool stream_ready;
    pa_buffer_attr buffer_attr;
};

struct SoundIoInStreamPulseAudio {
    pa_stream *stream;
    atomic_bool stream_ready;
    pa_buffer_attr buffer_attr;
};

struct SoundIoPulseAudio {
    bool connection_refused;

    pa_context *pulse_context;
    atomic_bool device_scan_queued;

    // the one that we're working on building
    struct SoundIoDevicesInfo *current_devices_info;
    char * default_sink_name;
    char * default_source_name;

    // this one is ready to be read with flush_events. protected by mutex
    struct SoundIoDevicesInfo *ready_devices_info;

    bool have_sink_list;
    bool have_source_list;
    bool have_default_sink;

    atomic_bool ready_flag;
    atomic_bool have_devices_flag;

    pa_threaded_mainloop *main_loop;
    pa_proplist *props;
};


static void subscribe_callback(pa_context *context,
        pa_subscription_event_type_t event_bits, uint32_t index, void *userdata)
{
    SoundIo *soundio = (SoundIo *)userdata;
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    sipa->device_scan_queued = true;
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

static void subscribe_to_events(SoundIo *soundio) {
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    pa_subscription_mask_t events = (pa_subscription_mask_t)(
            PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE|PA_SUBSCRIPTION_MASK_SERVER);
    pa_operation *subscribe_op = pa_context_subscribe(sipa->pulse_context,
            events, nullptr, soundio);
    if (!subscribe_op)
        soundio_panic("pa_context_subscribe failed: %s", pa_strerror(pa_context_errno(sipa->pulse_context)));
    pa_operation_unref(subscribe_op);
}

static void context_state_callback(pa_context *context, void *userdata) {
    SoundIo *soundio = (SoundIo *)userdata;
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;

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
        subscribe_to_events(soundio);
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

static void destroy_pa(SoundIo *soundio) {
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    if (!sipa)
        return;

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

    destroy(sipa);
    soundio->backend_data = nullptr;
}

static double usec_to_sec(pa_usec_t usec) {
    return (double)usec / (double)PA_USEC_PER_SEC;
}


static SoundIoFormat format_from_pulseaudio(pa_sample_spec sample_spec) {
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

static int sample_rate_from_pulseaudio(pa_sample_spec sample_spec) {
    return sample_spec.rate;
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

static int perform_operation(SoundIo *soundio, pa_operation *op) {
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
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

static void finish_device_query(SoundIo *soundio) {
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;

    if (!sipa->have_sink_list ||
        !sipa->have_source_list ||
        !sipa->have_default_sink)
    {
        return;
    }

    // based on the default sink name, figure out the default output index
    sipa->current_devices_info->default_output_index = -1;
    sipa->current_devices_info->default_input_index = -1;
    for (int i = 0; i < sipa->current_devices_info->input_devices.length; i += 1) {
        SoundIoDevice *device = sipa->current_devices_info->input_devices.at(i);
        assert(device->purpose == SoundIoDevicePurposeInput);
        if (strcmp(device->name, sipa->default_source_name) == 0) {
            sipa->current_devices_info->default_input_index = i;
        }
    }
    for (int i = 0; i < sipa->current_devices_info->output_devices.length; i += 1) {
        SoundIoDevice *device = sipa->current_devices_info->output_devices.at(i);
        assert(device->purpose == SoundIoDevicePurposeOutput);
        if (strcmp(device->name, sipa->default_sink_name) == 0) {
            sipa->current_devices_info->default_output_index = i;
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
    SoundIo *soundio = (SoundIo *)userdata;
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    if (eol) {
        sipa->have_sink_list = true;
        finish_device_query(soundio);
    } else {
        SoundIoDevice *device = create<SoundIoDevice>();
        if (!device)
            soundio_panic("out of memory");

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup(info->name);
        device->description = strdup(info->description);
        if (!device->name || !device->description)
            soundio_panic("out of memory");
        set_from_pulseaudio_channel_map(info->channel_map, &device->channel_layout);
        // TODO determine the list of supported formats and the min and max sample rate
        device->current_format = format_from_pulseaudio(info->sample_spec);
        device->default_latency = usec_to_sec(info->configured_latency);
        device->sample_rate_current = sample_rate_from_pulseaudio(info->sample_spec);
        device->purpose = SoundIoDevicePurposeOutput;

        if (sipa->current_devices_info->output_devices.append(device))
            soundio_panic("out of memory");
    }
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

static void source_info_callback(pa_context *pulse_context, const pa_source_info *info, int eol, void *userdata) {
    SoundIo *soundio = (SoundIo *)userdata;
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    if (eol) {
        sipa->have_source_list = true;
        finish_device_query(soundio);
    } else {
        SoundIoDevice *device = create<SoundIoDevice>();
        if (!device)
            soundio_panic("out of memory");

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup(info->name);
        device->description = strdup(info->description);
        if (!device->name || !device->description)
            soundio_panic("out of memory");
        set_from_pulseaudio_channel_map(info->channel_map, &device->channel_layout);
        // TODO determine the list of supported formats and the min and max sample rate
        device->current_format = format_from_pulseaudio(info->sample_spec);
        device->default_latency = usec_to_sec(info->configured_latency);
        device->sample_rate_current = sample_rate_from_pulseaudio(info->sample_spec);
        device->purpose = SoundIoDevicePurposeInput;

        if (sipa->current_devices_info->input_devices.append(device))
            soundio_panic("out of memory");
    }
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

static void server_info_callback(pa_context *pulse_context, const pa_server_info *info, void *userdata) {
    SoundIo *soundio = (SoundIo *)userdata;
    assert(soundio);
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;

    free(sipa->default_sink_name);
    free(sipa->default_source_name);

    sipa->default_sink_name = strdup(info->default_sink_name);
    sipa->default_source_name = strdup(info->default_source_name);

    if (!sipa->default_sink_name || !sipa->default_source_name)
        soundio_panic("out of memory");

    sipa->have_default_sink = true;
    finish_device_query(soundio);
    pa_threaded_mainloop_signal(sipa->main_loop, 0);
}

static void scan_devices(SoundIo *soundio) {
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;

    sipa->have_sink_list = false;
    sipa->have_default_sink = false;
    sipa->have_source_list = false;

    soundio_destroy_devices_info(sipa->current_devices_info);
    sipa->current_devices_info = create<SoundIoDevicesInfo>();
    if (!sipa->current_devices_info)
        soundio_panic("out of memory");

    pa_threaded_mainloop_lock(sipa->main_loop);

    pa_operation *list_sink_op = pa_context_get_sink_info_list(sipa->pulse_context,
            sink_info_callback, soundio);
    pa_operation *list_source_op = pa_context_get_source_info_list(sipa->pulse_context,
            source_info_callback, soundio);
    pa_operation *server_info_op = pa_context_get_server_info(sipa->pulse_context,
            server_info_callback, soundio);

    if (perform_operation(soundio, list_sink_op))
        soundio_panic("list sinks failed");
    if (perform_operation(soundio, list_source_op))
        soundio_panic("list sources failed");
    if (perform_operation(soundio, server_info_op))
        soundio_panic("get server info failed");

    pa_threaded_mainloop_signal(sipa->main_loop, 0);

    pa_threaded_mainloop_unlock(sipa->main_loop);
}

static void block_until_have_devices(SoundIo *soundio) {
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    if (sipa->have_devices_flag)
        return;
    pa_threaded_mainloop_lock(sipa->main_loop);
    while (!sipa->have_devices_flag) {
        pa_threaded_mainloop_wait(sipa->main_loop);
    }
    pa_threaded_mainloop_unlock(sipa->main_loop);
}

static void block_until_ready(SoundIo *soundio) {
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    if (sipa->ready_flag)
        return;
    pa_threaded_mainloop_lock(sipa->main_loop);
    while (!sipa->ready_flag) {
        pa_threaded_mainloop_wait(sipa->main_loop);
    }
    pa_threaded_mainloop_unlock(sipa->main_loop);
}

static void flush_events(SoundIo *soundio) {
    block_until_ready(soundio);

    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;

    if (sipa->device_scan_queued) {
        sipa->device_scan_queued = false;
        scan_devices(soundio);
    }

    SoundIoDevicesInfo *old_devices_info = nullptr;
    bool change = false;

    pa_threaded_mainloop_lock(sipa->main_loop);

    if (sipa->ready_devices_info) {
        old_devices_info = soundio->safe_devices_info;
        soundio->safe_devices_info = sipa->ready_devices_info;
        sipa->ready_devices_info = nullptr;
        change = true;
    }

    pa_threaded_mainloop_unlock(sipa->main_loop);

    if (change)
        soundio->on_devices_change(soundio);

    soundio_destroy_devices_info(old_devices_info);

    block_until_have_devices(soundio);
}

static void wait_events(SoundIo *soundio) {
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    flush_events(soundio);
    pa_threaded_mainloop_wait(sipa->main_loop);
}

static void wakeup(SoundIo *soundio) {
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
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

    case SoundIoChannelIdCount:
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
    SoundIoOutStream *out_stream = (SoundIoOutStream*) userdata;
    SoundIo *soundio = out_stream->device->soundio;
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    SoundIoOutStreamPulseAudio *ospa = (SoundIoOutStreamPulseAudio *)out_stream->backend_data;
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
    SoundIoOutStream *out_stream = (SoundIoOutStream*)userdata;
    out_stream->underrun_callback(out_stream);
}


static void playback_stream_write_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    SoundIoOutStream *out_stream = (SoundIoOutStream*)(userdata);
    int frame_count = ((int)nbytes) / out_stream->bytes_per_frame;
    out_stream->write_callback(out_stream, frame_count);
}

static void out_stream_destroy_pa(SoundIo *soundio,
        SoundIoOutStream *out_stream)
{
    SoundIoOutStreamPulseAudio *ospa = (SoundIoOutStreamPulseAudio *)out_stream->backend_data;
    if (!ospa)
        return;

    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
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

    destroy(ospa);
    out_stream->backend_data = nullptr;
}

static int out_stream_init_pa(SoundIo *soundio,
        SoundIoOutStream *out_stream)
{
    SoundIoOutStreamPulseAudio *ospa = create<SoundIoOutStreamPulseAudio>();
    if (!ospa) {
        out_stream_destroy_pa(soundio, out_stream);
        return SoundIoErrorNoMem;
    }
    out_stream->backend_data = ospa;

    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    SoundIoDevice *device = out_stream->device; 
    ospa->stream_ready = false;

    assert(sipa->pulse_context);

    pa_threaded_mainloop_lock(sipa->main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_format(out_stream->format);
    sample_spec.rate = out_stream->sample_rate;
    sample_spec.channels = device->channel_layout.channel_count;
    pa_channel_map channel_map = to_pulseaudio_channel_map(&device->channel_layout);

    ospa->stream = pa_stream_new(sipa->pulse_context, "SoundIo", &sample_spec, &channel_map);
    if (!ospa->stream) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        out_stream_destroy_pa(soundio, out_stream);
        return SoundIoErrorNoMem;
    }
    pa_stream_set_state_callback(ospa->stream, playback_stream_state_callback, out_stream);
    pa_stream_set_write_callback(ospa->stream, playback_stream_write_callback, out_stream);
    pa_stream_set_underflow_callback(ospa->stream, playback_stream_underflow_callback, out_stream);

    int bytes_per_second = out_stream->bytes_per_frame * out_stream->sample_rate;
    int buffer_length = out_stream->bytes_per_frame *
        ceil(out_stream->latency * bytes_per_second / (double)out_stream->bytes_per_frame);

    ospa->buffer_attr.maxlength = buffer_length;
    ospa->buffer_attr.tlength = buffer_length;
    ospa->buffer_attr.prebuf = 0;
    ospa->buffer_attr.minreq = UINT32_MAX;
    ospa->buffer_attr.fragsize = UINT32_MAX;

    pa_threaded_mainloop_unlock(sipa->main_loop);

    return 0;
}

static int out_stream_start_pa(SoundIo *soundio,
        SoundIoOutStream *out_stream)
{
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    SoundIoOutStreamPulseAudio *ospa = (SoundIoOutStreamPulseAudio *)out_stream->backend_data;

    pa_threaded_mainloop_lock(sipa->main_loop);


    int err = pa_stream_connect_playback(ospa->stream,
            out_stream->device->name, &ospa->buffer_attr,
            PA_STREAM_ADJUST_LATENCY, nullptr, nullptr);
    if (err) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        return SoundIoErrorOpeningDevice;
    }

    while (!ospa->stream_ready)
        pa_threaded_mainloop_wait(sipa->main_loop);

    soundio_out_stream_fill_with_silence(out_stream);

    pa_threaded_mainloop_unlock(sipa->main_loop);

    return 0;
}

static int out_stream_free_count_pa(SoundIo *soundio,
        SoundIoOutStream *out_stream)
{
    SoundIoOutStreamPulseAudio *ospa = (SoundIoOutStreamPulseAudio *)out_stream->backend_data;
    return pa_stream_writable_size(ospa->stream) / out_stream->bytes_per_frame;
}


static void out_stream_begin_write_pa(SoundIo *soundio,
        SoundIoOutStream *out_stream, char **data, int *frame_count)
{
    SoundIoOutStreamPulseAudio *ospa = (SoundIoOutStreamPulseAudio *)out_stream->backend_data;
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    pa_stream *stream = ospa->stream;
    size_t byte_count = *frame_count * out_stream->bytes_per_frame;
    if (pa_stream_begin_write(stream, (void**)data, &byte_count))
        soundio_panic("pa_stream_begin_write error: %s", pa_strerror(pa_context_errno(sipa->pulse_context)));

    *frame_count = byte_count / out_stream->bytes_per_frame;
}

static void out_stream_write_pa(SoundIo *soundio,
        SoundIoOutStream *out_stream, char *data, int frame_count)
{
    SoundIoOutStreamPulseAudio *ospa = (SoundIoOutStreamPulseAudio *)out_stream->backend_data;
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    pa_stream *stream = ospa->stream;
    size_t byte_count = frame_count * out_stream->bytes_per_frame;
    if (pa_stream_write(stream, data, byte_count, NULL, 0, PA_SEEK_RELATIVE))
        soundio_panic("pa_stream_write error: %s", pa_strerror(pa_context_errno(sipa->pulse_context)));
}

static void out_stream_clear_buffer_pa(SoundIo *soundio,
        SoundIoOutStream *out_stream)
{
    SoundIoOutStreamPulseAudio *ospa = (SoundIoOutStreamPulseAudio *)out_stream->backend_data;
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    pa_stream *stream = ospa->stream;
    pa_threaded_mainloop_lock(sipa->main_loop);
    pa_operation *op = pa_stream_flush(stream, NULL, NULL);
    if (!op)
        soundio_panic("pa_stream_flush failed: %s", pa_strerror(pa_context_errno(sipa->pulse_context)));
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(sipa->main_loop);
}

static void recording_stream_state_callback(pa_stream *stream, void *userdata) {
    SoundIoInStream *in_stream = (SoundIoInStream*)userdata;
    SoundIoInStreamPulseAudio *ispa = (SoundIoInStreamPulseAudio *)in_stream->backend_data;
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_READY:
            ispa->stream_ready = true;
            break;
        case PA_STREAM_FAILED:
            soundio_panic("pulseaudio stream error: %s",
                    pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
            break;
    }
}

static void recording_stream_read_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    SoundIoInStream *in_stream = (SoundIoInStream*)userdata;
    in_stream->read_callback(in_stream);
}

static void in_stream_destroy_pa(SoundIo *soundio,
        SoundIoInStream *in_stream)
{
    SoundIoInStreamPulseAudio *ispa = (SoundIoInStreamPulseAudio *)in_stream->backend_data;
    if (!ispa)
        return;

    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
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

static int in_stream_init_pa(SoundIo *soundio,
        SoundIoInStream *in_stream)
{
    SoundIoInStreamPulseAudio *ispa = create<SoundIoInStreamPulseAudio>();
    if (!ispa) {
        in_stream_destroy_pa(soundio, in_stream);
        return SoundIoErrorNoMem;
    }
    in_stream->backend_data = ispa;

    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    SoundIoDevice *device = in_stream->device;
    ispa->stream_ready = false;

    pa_threaded_mainloop_lock(sipa->main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_format(in_stream->format);
    sample_spec.rate = in_stream->sample_rate;
    sample_spec.channels = device->channel_layout.channel_count;

    pa_channel_map channel_map = to_pulseaudio_channel_map(&device->channel_layout);

    ispa->stream = pa_stream_new(sipa->pulse_context, "SoundIo", &sample_spec, &channel_map);
    if (!in_stream) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        in_stream_destroy_pa(soundio, in_stream);
        return SoundIoErrorNoMem;
    }

    pa_stream *stream = ispa->stream;

    pa_stream_set_state_callback(stream, recording_stream_state_callback, in_stream);
    pa_stream_set_read_callback(stream, recording_stream_read_callback, in_stream);

    int bytes_per_second = in_stream->bytes_per_frame * in_stream->sample_rate;
    int buffer_length = in_stream->bytes_per_frame *
        ceil(in_stream->latency * bytes_per_second / (double)in_stream->bytes_per_frame);

    ispa->buffer_attr.maxlength = UINT32_MAX;
    ispa->buffer_attr.tlength = UINT32_MAX;
    ispa->buffer_attr.prebuf = 0;
    ispa->buffer_attr.minreq = UINT32_MAX;
    ispa->buffer_attr.fragsize = buffer_length;

    pa_threaded_mainloop_unlock(sipa->main_loop);

    return 0;
}

static int in_stream_start_pa(SoundIo *soundio,
        SoundIoInStream *in_stream)
{
    SoundIoInStreamPulseAudio *ispa = (SoundIoInStreamPulseAudio *)in_stream->backend_data;
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;
    pa_threaded_mainloop_lock(sipa->main_loop);

    int err = pa_stream_connect_record(ispa->stream,
            in_stream->device->name,
            &ispa->buffer_attr, PA_STREAM_ADJUST_LATENCY);
    if (err) {
        pa_threaded_mainloop_unlock(sipa->main_loop);
        return SoundIoErrorOpeningDevice;
    }

    pa_threaded_mainloop_unlock(sipa->main_loop);
    return 0;
}

static void in_stream_peek_pa(SoundIo *soundio,
        SoundIoInStream *in_stream, const char **data, int *frame_count)
{
    SoundIoInStreamPulseAudio *ispa = (SoundIoInStreamPulseAudio *)in_stream->backend_data;
    pa_stream *stream = ispa->stream;
    if (ispa->stream_ready) {
        size_t nbytes;
        if (pa_stream_peek(stream, (const void **)data, &nbytes))
            soundio_panic("pa_stream_peek error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
        *frame_count = ((int)nbytes) / in_stream->bytes_per_frame;
    } else {
        *data = nullptr;
        *frame_count = 0;
    }
}

static void in_stream_drop_pa(SoundIo *soundio,
        SoundIoInStream *in_stream)
{
    SoundIoInStreamPulseAudio *ispa = (SoundIoInStreamPulseAudio *)in_stream->backend_data;
    pa_stream *stream = ispa->stream;
    if (pa_stream_drop(stream))
        soundio_panic("pa_stream_drop error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
}

static void in_stream_clear_buffer_pa(SoundIo *soundio,
        SoundIoInStream *in_stream)
{
    SoundIoInStreamPulseAudio *ispa = (SoundIoInStreamPulseAudio *)in_stream->backend_data;
    if (!ispa->stream_ready)
        return;

    pa_stream *stream = ispa->stream;
    SoundIoPulseAudio *sipa = (SoundIoPulseAudio *)soundio->backend_data;

    pa_threaded_mainloop_lock(sipa->main_loop);

    for (;;) {
        const char *data;
        size_t nbytes;
        if (pa_stream_peek(stream, (const void **)&data, &nbytes))
            soundio_panic("pa_stream_peek error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));

        if (nbytes == 0)
            break;

        if (pa_stream_drop(stream))
            soundio_panic("pa_stream_drop error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
    }

    pa_threaded_mainloop_unlock(sipa->main_loop);
}

int soundio_pulseaudio_init(SoundIo *soundio) {
    assert(!soundio->backend_data);
    SoundIoPulseAudio *sipa = create<SoundIoPulseAudio>();
    if (!sipa) {
        destroy_pa(soundio);
        return SoundIoErrorNoMem;
    }
    soundio->backend_data = sipa;

    sipa->connection_refused = false;
    sipa->device_scan_queued = false;
    sipa->ready_flag = false;
    sipa->have_devices_flag = false;

    sipa->main_loop = pa_threaded_mainloop_new();
    if (!sipa->main_loop) {
        destroy_pa(soundio);
        return SoundIoErrorNoMem;
    }

    pa_mainloop_api *main_loop_api = pa_threaded_mainloop_get_api(sipa->main_loop);

    sipa->props = pa_proplist_new();
    if (!sipa->props) {
        destroy_pa(soundio);
        return SoundIoErrorNoMem;
    }

    // TODO let the API specify this
    pa_proplist_sets(sipa->props, PA_PROP_APPLICATION_NAME, "libsoundio");
    pa_proplist_sets(sipa->props, PA_PROP_APPLICATION_VERSION, SOUNDIO_VERSION_STRING);
    pa_proplist_sets(sipa->props, PA_PROP_APPLICATION_ID, "me.andrewkelley.libsoundio");

    sipa->pulse_context = pa_context_new_with_proplist(main_loop_api, "SoundIo", sipa->props);
    if (!sipa->pulse_context) {
        destroy_pa(soundio);
        return SoundIoErrorNoMem;
    }

    pa_context_set_subscribe_callback(sipa->pulse_context, subscribe_callback, soundio);
    pa_context_set_state_callback(sipa->pulse_context, context_state_callback, soundio);

    int err = pa_context_connect(sipa->pulse_context, NULL, (pa_context_flags_t)0, NULL);
    if (err) {
        destroy_pa(soundio);
        return SoundIoErrorInitAudioBackend;
    }

    if (sipa->connection_refused) {
        destroy_pa(soundio);
        return SoundIoErrorInitAudioBackend;
    }

    if (pa_threaded_mainloop_start(sipa->main_loop)) {
        destroy_pa(soundio);
        return SoundIoErrorNoMem;
    }

    soundio->destroy = destroy_pa;
    soundio->flush_events = flush_events;
    soundio->wait_events = wait_events;
    soundio->wakeup = wakeup;

    soundio->out_stream_init = out_stream_init_pa;
    soundio->out_stream_destroy = out_stream_destroy_pa;
    soundio->out_stream_start = out_stream_start_pa;
    soundio->out_stream_free_count = out_stream_free_count_pa;
    soundio->out_stream_begin_write = out_stream_begin_write_pa;
    soundio->out_stream_write = out_stream_write_pa;
    soundio->out_stream_clear_buffer = out_stream_clear_buffer_pa;

    soundio->in_stream_init = in_stream_init_pa;
    soundio->in_stream_destroy = in_stream_destroy_pa;
    soundio->in_stream_start = in_stream_start_pa;
    soundio->in_stream_peek = in_stream_peek_pa;
    soundio->in_stream_drop = in_stream_drop_pa;
    soundio->in_stream_clear_buffer = in_stream_clear_buffer_pa;

    return 0;
}
