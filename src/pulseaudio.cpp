#include "pulseaudio.hpp"
#include "soundio.hpp"

#include <string.h>
#include <math.h>

#include <pulse/pulseaudio.h>
#include <atomic>
using std::atomic_bool;

struct SoundIoOutputDevicePulseAudio {
    pa_stream *stream;
    atomic_bool stream_ready;
    pa_buffer_attr buffer_attr;
};

struct SoundIoInputDevicePulseAudio {
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
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    ah->device_scan_queued = true;
    pa_threaded_mainloop_signal(ah->main_loop, 0);
}

static void subscribe_to_events(SoundIo *soundio) {
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    pa_subscription_mask_t events = (pa_subscription_mask_t)(
            PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE|PA_SUBSCRIPTION_MASK_SERVER);
    pa_operation *subscribe_op = pa_context_subscribe(ah->pulse_context,
            events, nullptr, soundio);
    if (!subscribe_op)
        panic("pa_context_subscribe failed: %s", pa_strerror(pa_context_errno(ah->pulse_context)));
    pa_operation_unref(subscribe_op);
}

static void context_state_callback(pa_context *context, void *userdata) {
    SoundIo *soundio = (SoundIo *)userdata;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;

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
        ah->device_scan_queued = true;
        subscribe_to_events(soundio);
        ah->ready_flag = true;
        pa_threaded_mainloop_signal(ah->main_loop, 0);
        return;
    case PA_CONTEXT_TERMINATED: // The connection was terminated cleanly.
        pa_threaded_mainloop_signal(ah->main_loop, 0);
        return;
    case PA_CONTEXT_FAILED: // The connection failed or was disconnected.
        {
            int err_number = pa_context_errno(context);
            if (err_number == PA_ERR_CONNECTIONREFUSED) {
                ah->connection_refused = true;
            } else {
                panic("pulseaudio connect failure: %s", pa_strerror(pa_context_errno(context)));
            }
            return;
        }
    }
}

static void destroy_current_devices_info(SoundIo *soundio) {
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    if (ah->current_devices_info) {
        for (int i = 0; i < ah->current_devices_info->input_devices.length; i += 1)
            soundio_device_unref(ah->current_devices_info->input_devices.at(i));
        for (int i = 0; i < ah->current_devices_info->output_devices.length; i += 1)
            soundio_device_unref(ah->current_devices_info->output_devices.at(i));

        destroy(ah->current_devices_info);
        ah->current_devices_info = nullptr;
    }
}

static void destroy_ready_devices_info(SoundIo *soundio) {
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    if (ah->ready_devices_info) {
        for (int i = 0; i < ah->ready_devices_info->input_devices.length; i += 1)
            soundio_device_unref(ah->ready_devices_info->input_devices.at(i));
        for (int i = 0; i < ah->ready_devices_info->output_devices.length; i += 1)
            soundio_device_unref(ah->ready_devices_info->output_devices.at(i));
        destroy(ah->ready_devices_info);
        ah->ready_devices_info = nullptr;
    }
}


static void destroy_pa(SoundIo *soundio) {
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    if (!ah)
        return;

    if (ah->main_loop)
        pa_threaded_mainloop_stop(ah->main_loop);

    destroy_current_devices_info(soundio);
    destroy_ready_devices_info(soundio);

    pa_context_disconnect(ah->pulse_context);
    pa_context_unref(ah->pulse_context);

    if (ah->main_loop)
        pa_threaded_mainloop_free(ah->main_loop);

    if (ah->props)
        pa_proplist_free(ah->props);

    free(ah->default_sink_name);
    free(ah->default_source_name);

    destroy(ah);
    soundio->backend_data = nullptr;
}

static double usec_to_sec(pa_usec_t usec) {
    return (double)usec / (double)PA_USEC_PER_SEC;
}

static SoundIoSampleFormat sample_format_from_pulseaudio(pa_sample_spec sample_spec) {
    switch (sample_spec.format) {
    case PA_SAMPLE_U8:        return SoundIoSampleFormatUInt8;
    case PA_SAMPLE_S16NE:     return SoundIoSampleFormatInt16;
    case PA_SAMPLE_S32NE:     return SoundIoSampleFormatInt32;
    case PA_SAMPLE_FLOAT32NE: return SoundIoSampleFormatFloat;
    default:                  return SoundIoSampleFormatInvalid;
    }
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
    case PA_CHANNEL_POSITION_LFE: return SoundIoChannelIdLowFrequency;
    case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER: return SoundIoChannelIdFrontLeftOfCenter;
    case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER: return SoundIoChannelIdFrontRightOfCenter;
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
        panic("cannot map pulseaudio channel to libsoundio");
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
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    for (;;) {
        switch (pa_operation_get_state(op)) {
        case PA_OPERATION_RUNNING:
            pa_threaded_mainloop_wait(ah->main_loop);
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
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;

    if (!ah->have_sink_list ||
        !ah->have_source_list ||
        !ah->have_default_sink)
    {
        return;
    }

    // based on the default sink name, figure out the default output index
    ah->current_devices_info->default_output_index = -1;
    ah->current_devices_info->default_input_index = -1;
    for (int i = 0; i < ah->current_devices_info->input_devices.length; i += 1) {
        SoundIoDevice *device = ah->current_devices_info->input_devices.at(i);
        assert(device->purpose == SoundIoDevicePurposeInput);
        if (strcmp(device->name, ah->default_source_name) == 0) {
            ah->current_devices_info->default_input_index = i;
        }
    }
    for (int i = 0; i < ah->current_devices_info->output_devices.length; i += 1) {
        SoundIoDevice *device = ah->current_devices_info->output_devices.at(i);
        assert(device->purpose == SoundIoDevicePurposeOutput);
        if (strcmp(device->name, ah->default_sink_name) == 0) {
            ah->current_devices_info->default_output_index = i;
        }
    }

    destroy_ready_devices_info(soundio);
    ah->ready_devices_info = ah->current_devices_info;
    ah->current_devices_info = NULL;
    ah->have_devices_flag = true;
    pa_threaded_mainloop_signal(ah->main_loop, 0);
    soundio->on_events_signal(soundio);
}

static void sink_info_callback(pa_context *pulse_context, const pa_sink_info *info, int eol, void *userdata) {
    SoundIo *soundio = (SoundIo *)userdata;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    if (eol) {
        ah->have_sink_list = true;
        finish_device_query(soundio);
    } else {
        SoundIoDevice *device = create<SoundIoDevice>();
        if (!device)
            panic("out of memory");

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup(info->name);
        device->description = strdup(info->description);
        if (!device->name || !device->description)
            panic("out of memory");
        set_from_pulseaudio_channel_map(info->channel_map, &device->channel_layout);
        device->default_sample_format = sample_format_from_pulseaudio(info->sample_spec);
        device->default_latency = usec_to_sec(info->configured_latency);
        device->default_sample_rate = sample_rate_from_pulseaudio(info->sample_spec);
        device->purpose = SoundIoDevicePurposeOutput;

        if (ah->current_devices_info->output_devices.append(device))
            panic("out of memory");
    }
    pa_threaded_mainloop_signal(ah->main_loop, 0);
}

static void source_info_callback(pa_context *pulse_context, const pa_source_info *info, int eol, void *userdata) {
    SoundIo *soundio = (SoundIo *)userdata;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    if (eol) {
        ah->have_source_list = true;
        finish_device_query(soundio);
    } else {
        SoundIoDevice *device = create<SoundIoDevice>();
        if (!device)
            panic("out of memory");

        device->ref_count = 1;
        device->soundio = soundio;
        device->name = strdup(info->name);
        device->description = strdup(info->description);
        if (!device->name || !device->description)
            panic("out of memory");
        set_from_pulseaudio_channel_map(info->channel_map, &device->channel_layout);
        device->default_sample_format = sample_format_from_pulseaudio(info->sample_spec);
        device->default_latency = usec_to_sec(info->configured_latency);
        device->default_sample_rate = sample_rate_from_pulseaudio(info->sample_spec);
        device->purpose = SoundIoDevicePurposeInput;

        if (ah->current_devices_info->input_devices.append(device))
            panic("out of memory");
    }
    pa_threaded_mainloop_signal(ah->main_loop, 0);
}

static void server_info_callback(pa_context *pulse_context, const pa_server_info *info, void *userdata) {
    SoundIo *soundio = (SoundIo *)userdata;
    assert(soundio);
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;

    free(ah->default_sink_name);
    free(ah->default_source_name);

    ah->default_sink_name = strdup(info->default_sink_name);
    ah->default_source_name = strdup(info->default_source_name);

    if (!ah->default_sink_name || !ah->default_source_name)
        panic("out of memory");

    ah->have_default_sink = true;
    finish_device_query(soundio);
    pa_threaded_mainloop_signal(ah->main_loop, 0);
}

static void scan_devices(SoundIo *soundio) {
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;

    ah->have_sink_list = false;
    ah->have_default_sink = false;
    ah->have_source_list = false;

    destroy_current_devices_info(soundio);
    ah->current_devices_info = create<SoundIoDevicesInfo>();
    if (!ah->current_devices_info)
        panic("out of memory");

    pa_threaded_mainloop_lock(ah->main_loop);

    pa_operation *list_sink_op = pa_context_get_sink_info_list(ah->pulse_context,
            sink_info_callback, soundio);
    pa_operation *list_source_op = pa_context_get_source_info_list(ah->pulse_context,
            source_info_callback, soundio);
    pa_operation *server_info_op = pa_context_get_server_info(ah->pulse_context,
            server_info_callback, soundio);

    if (perform_operation(soundio, list_sink_op))
        panic("list sinks failed");
    if (perform_operation(soundio, list_source_op))
        panic("list sources failed");
    if (perform_operation(soundio, server_info_op))
        panic("get server info failed");

    pa_threaded_mainloop_signal(ah->main_loop, 0);

    pa_threaded_mainloop_unlock(ah->main_loop);
}

static void flush_events(SoundIo *soundio) {
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;

    if (ah->device_scan_queued) {
        ah->device_scan_queued = false;
        scan_devices(soundio);
    }

    SoundIoDevicesInfo *old_devices_info = nullptr;
    bool change = false;

    pa_threaded_mainloop_lock(ah->main_loop);

    if (ah->ready_devices_info) {
        old_devices_info = soundio->safe_devices_info;
        soundio->safe_devices_info = ah->ready_devices_info;
        ah->ready_devices_info = nullptr;
        change = true;
    }

    pa_threaded_mainloop_unlock(ah->main_loop);

    if (change)
        soundio->on_devices_change(soundio);

    if (old_devices_info) {
        for (int i = 0; i < old_devices_info->input_devices.length; i += 1)
            soundio_device_unref(old_devices_info->input_devices.at(i));
        for (int i = 0; i < old_devices_info->output_devices.length; i += 1)
            soundio_device_unref(old_devices_info->output_devices.at(i));
        destroy(old_devices_info);
    }
}

static void block_until_ready(SoundIo *soundio) {
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    if (ah->ready_flag)
        return;
    pa_threaded_mainloop_lock(ah->main_loop);
    while (!ah->ready_flag) {
        pa_threaded_mainloop_wait(ah->main_loop);
    }
    pa_threaded_mainloop_unlock(ah->main_loop);
}

static void block_until_have_devices(SoundIo *soundio) {
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    if (ah->have_devices_flag)
        return;
    pa_threaded_mainloop_lock(ah->main_loop);
    while (!ah->have_devices_flag) {
        pa_threaded_mainloop_wait(ah->main_loop);
    }
    pa_threaded_mainloop_unlock(ah->main_loop);
}

static pa_sample_format_t to_pulseaudio_sample_format(SoundIoSampleFormat sample_format) {
    switch (sample_format) {
    case SoundIoSampleFormatUInt8:
        return PA_SAMPLE_U8;
    case SoundIoSampleFormatInt16:
        return PA_SAMPLE_S16NE;
    case SoundIoSampleFormatInt24:
        return PA_SAMPLE_S24NE;
    case SoundIoSampleFormatInt32:
        return PA_SAMPLE_S32NE;
    case SoundIoSampleFormatFloat:
        return PA_SAMPLE_FLOAT32NE;
    case SoundIoSampleFormatDouble:
        panic("cannot use double sample format with pulseaudio");
    case SoundIoSampleFormatInvalid:
        panic("invalid sample format");
    }
    panic("invalid sample format");
}

static pa_channel_position_t to_pulseaudio_channel_pos(SoundIoChannelId channel_id) {
    switch (channel_id) {
    case SoundIoChannelIdInvalid:
    case SoundIoChannelIdCount:
        panic("invalid channel id");
    case SoundIoChannelIdFrontLeft:
        return PA_CHANNEL_POSITION_FRONT_LEFT;
    case SoundIoChannelIdFrontRight:
        return PA_CHANNEL_POSITION_FRONT_RIGHT;
    case SoundIoChannelIdFrontCenter:
        return PA_CHANNEL_POSITION_FRONT_CENTER;
    case SoundIoChannelIdLowFrequency:
        return PA_CHANNEL_POSITION_LFE;
    case SoundIoChannelIdBackLeft:
        return PA_CHANNEL_POSITION_REAR_LEFT;
    case SoundIoChannelIdBackRight:
        return PA_CHANNEL_POSITION_REAR_RIGHT;
    case SoundIoChannelIdFrontLeftOfCenter:
        return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
    case SoundIoChannelIdFrontRightOfCenter:
        return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
    case SoundIoChannelIdBackCenter:
        return PA_CHANNEL_POSITION_REAR_CENTER;
    case SoundIoChannelIdSideLeft:
        return PA_CHANNEL_POSITION_SIDE_LEFT;
    case SoundIoChannelIdSideRight:
        return PA_CHANNEL_POSITION_SIDE_RIGHT;
    case SoundIoChannelIdTopCenter:
        return PA_CHANNEL_POSITION_TOP_CENTER;
    case SoundIoChannelIdTopFrontLeft:
        return PA_CHANNEL_POSITION_TOP_FRONT_LEFT;
    case SoundIoChannelIdTopFrontCenter:
        return PA_CHANNEL_POSITION_TOP_FRONT_CENTER;
    case SoundIoChannelIdTopFrontRight:
        return PA_CHANNEL_POSITION_TOP_FRONT_RIGHT;
    case SoundIoChannelIdTopBackLeft:
        return PA_CHANNEL_POSITION_TOP_REAR_LEFT;
    case SoundIoChannelIdTopBackCenter:
        return PA_CHANNEL_POSITION_TOP_REAR_CENTER;
    case SoundIoChannelIdTopBackRight:
        return PA_CHANNEL_POSITION_TOP_REAR_RIGHT;
    }
    panic("invalid channel id");
}

static pa_channel_map to_pulseaudio_channel_map(const SoundIoChannelLayout *channel_layout) {
    pa_channel_map channel_map;
    channel_map.channels = channel_layout->channel_count;

    if ((unsigned)channel_layout->channel_count > PA_CHANNELS_MAX)
        panic("channel layout greater than pulseaudio max channels");

    for (int i = 0; i < channel_layout->channel_count; i += 1)
        channel_map.map[i] = to_pulseaudio_channel_pos(channel_layout->channels[i]);

    return channel_map;
}

static void playback_stream_state_callback(pa_stream *stream, void *userdata) {
    SoundIoOutputDevice *output_device = (SoundIoOutputDevice*) userdata;
    SoundIo *soundio = output_device->device->soundio;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    SoundIoOutputDevicePulseAudio *opd = (SoundIoOutputDevicePulseAudio *)output_device->backend_data;
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_READY:
            opd->stream_ready = true;
            pa_threaded_mainloop_signal(ah->main_loop, 0);
            break;
        case PA_STREAM_FAILED:
            panic("pulseaudio stream error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
            break;
    }
}

static void playback_stream_underflow_callback(pa_stream *stream, void *userdata) {
    SoundIoOutputDevice *output_device = (SoundIoOutputDevice*)userdata;
    output_device->underrun_callback(output_device);
}


static void playback_stream_write_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    SoundIoOutputDevice *output_device = (SoundIoOutputDevice*)(userdata);
    int frame_count = ((int)nbytes) / output_device->bytes_per_frame;
    output_device->write_callback(output_device, frame_count);
}

static int output_device_init_pa(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    SoundIoOutputDevicePulseAudio *opd = (SoundIoOutputDevicePulseAudio *)output_device->backend_data;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    SoundIoDevice *device = output_device->device; 
    opd->stream_ready = false;

    assert(ah->pulse_context);

    pa_threaded_mainloop_lock(ah->main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_sample_format(output_device->sample_format);
    sample_spec.rate = device->default_sample_rate;
    sample_spec.channels = device->channel_layout.channel_count;
    pa_channel_map channel_map = to_pulseaudio_channel_map(&device->channel_layout);

    opd->stream = pa_stream_new(ah->pulse_context, "SoundIo", &sample_spec, &channel_map);
    if (!opd->stream) {
        pa_threaded_mainloop_unlock(ah->main_loop);
        return SoundIoErrorNoMem;
    }
    pa_stream_set_state_callback(opd->stream, playback_stream_state_callback, output_device);
    pa_stream_set_write_callback(opd->stream, playback_stream_write_callback, output_device);
    pa_stream_set_underflow_callback(opd->stream, playback_stream_underflow_callback, output_device);

    int bytes_per_second = output_device->bytes_per_frame * device->default_sample_rate;
    int buffer_length = output_device->bytes_per_frame *
        ceil(output_device->latency * bytes_per_second / (double)output_device->bytes_per_frame);

    opd->buffer_attr.maxlength = buffer_length;
    opd->buffer_attr.tlength = buffer_length;
    opd->buffer_attr.prebuf = 0;
    opd->buffer_attr.minreq = UINT32_MAX;
    opd->buffer_attr.fragsize = UINT32_MAX;

    pa_threaded_mainloop_unlock(ah->main_loop);

    return 0;
}

static void output_device_destroy_pa(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    SoundIoOutputDevicePulseAudio *opd = (SoundIoOutputDevicePulseAudio *)output_device->backend_data;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    pa_stream *stream = opd->stream;
    if (stream) {
        pa_threaded_mainloop_lock(ah->main_loop);

        pa_stream_set_write_callback(stream, nullptr, nullptr);
        pa_stream_set_state_callback(stream, nullptr, nullptr);
        pa_stream_set_underflow_callback(stream, nullptr, nullptr);
        pa_stream_disconnect(stream);

        pa_stream_unref(stream);

        pa_threaded_mainloop_unlock(ah->main_loop);

        opd->stream = nullptr;
    }
}

static int output_device_start_pa(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    SoundIoOutputDevicePulseAudio *opd = (SoundIoOutputDevicePulseAudio *)output_device->backend_data;

    pa_threaded_mainloop_lock(ah->main_loop);


    int err = pa_stream_connect_playback(opd->stream,
            output_device->device->name, &opd->buffer_attr,
            PA_STREAM_ADJUST_LATENCY, nullptr, nullptr);
    if (err) {
        pa_threaded_mainloop_unlock(ah->main_loop);
        return SoundIoErrorOpeningDevice;
    }

    while (!opd->stream_ready)
        pa_threaded_mainloop_wait(ah->main_loop);

    soundio_output_device_fill_with_silence(output_device);

    pa_threaded_mainloop_unlock(ah->main_loop);

    return 0;
}

static int output_device_free_count_pa(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    SoundIoOutputDevicePulseAudio *opd = (SoundIoOutputDevicePulseAudio *)output_device->backend_data;
    return pa_stream_writable_size(opd->stream) / output_device->bytes_per_frame;
}


static void output_device_begin_write_pa(SoundIo *soundio,
        SoundIoOutputDevice *output_device, char **data, int *frame_count)
{
    SoundIoOutputDevicePulseAudio *opd = (SoundIoOutputDevicePulseAudio *)output_device->backend_data;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    pa_stream *stream = opd->stream;
    size_t byte_count = *frame_count * output_device->bytes_per_frame;
    if (pa_stream_begin_write(stream, (void**)data, &byte_count))
        panic("pa_stream_begin_write error: %s", pa_strerror(pa_context_errno(ah->pulse_context)));

    *frame_count = byte_count / output_device->bytes_per_frame;
}

static void output_device_write_pa(SoundIo *soundio,
        SoundIoOutputDevice *output_device, char *data, int frame_count)
{
    SoundIoOutputDevicePulseAudio *opd = (SoundIoOutputDevicePulseAudio *)output_device->backend_data;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    pa_stream *stream = opd->stream;
    size_t byte_count = frame_count * output_device->bytes_per_frame;
    if (pa_stream_write(stream, data, byte_count, NULL, 0, PA_SEEK_RELATIVE))
        panic("pa_stream_write error: %s", pa_strerror(pa_context_errno(ah->pulse_context)));
}

static void output_device_clear_buffer_pa(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    SoundIoOutputDevicePulseAudio *opd = (SoundIoOutputDevicePulseAudio *)output_device->backend_data;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    pa_stream *stream = opd->stream;
    pa_threaded_mainloop_lock(ah->main_loop);
    pa_operation *op = pa_stream_flush(stream, NULL, NULL);
    if (!op)
        panic("pa_stream_flush failed: %s", pa_strerror(pa_context_errno(ah->pulse_context)));
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(ah->main_loop);
}

static void recording_stream_state_callback(pa_stream *stream, void *userdata) {
    SoundIoInputDevice *input_device = (SoundIoInputDevice*)userdata;
    SoundIoInputDevicePulseAudio *ord = (SoundIoInputDevicePulseAudio *)input_device->backend_data;
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_READY:
            ord->stream_ready = true;
            break;
        case PA_STREAM_FAILED:
            panic("pulseaudio stream error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
            break;
    }
}

static void recording_stream_read_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    SoundIoInputDevice *input_device = (SoundIoInputDevice*)userdata;
    input_device->read_callback(input_device);
}

static int input_device_init_pa(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    SoundIoInputDevicePulseAudio *ord = (SoundIoInputDevicePulseAudio *)input_device->backend_data;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    SoundIoDevice *device = input_device->device;
    ord->stream_ready = false;

    pa_threaded_mainloop_lock(ah->main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_sample_format(input_device->sample_format);
    sample_spec.rate = device->default_sample_rate;
    sample_spec.channels = device->channel_layout.channel_count;

    pa_channel_map channel_map = to_pulseaudio_channel_map(&device->channel_layout);

    ord->stream = pa_stream_new(ah->pulse_context, "SoundIo", &sample_spec, &channel_map);
    if (!input_device) {
        pa_threaded_mainloop_unlock(ah->main_loop);
        return SoundIoErrorNoMem;
    }

    pa_stream *stream = ord->stream;

    pa_stream_set_state_callback(stream, recording_stream_state_callback, input_device);
    pa_stream_set_read_callback(stream, recording_stream_read_callback, input_device);

    int bytes_per_second = input_device->bytes_per_frame * device->default_sample_rate;
    int buffer_length = input_device->bytes_per_frame *
        ceil(input_device->latency * bytes_per_second / (double)input_device->bytes_per_frame);

    ord->buffer_attr.maxlength = UINT32_MAX;
    ord->buffer_attr.tlength = UINT32_MAX;
    ord->buffer_attr.prebuf = 0;
    ord->buffer_attr.minreq = UINT32_MAX;
    ord->buffer_attr.fragsize = buffer_length;

    pa_threaded_mainloop_unlock(ah->main_loop);

    return 0;
}

static void input_device_destroy_pa(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    SoundIoInputDevicePulseAudio *ord = (SoundIoInputDevicePulseAudio *)input_device->backend_data;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    pa_stream *stream = ord->stream;
    if (stream) {
        pa_threaded_mainloop_lock(ah->main_loop);

        pa_stream_set_state_callback(stream, nullptr, nullptr);
        pa_stream_set_read_callback(stream, nullptr, nullptr);
        pa_stream_disconnect(stream);
        pa_stream_unref(stream);

        pa_threaded_mainloop_unlock(ah->main_loop);

        ord->stream = nullptr;
    }
}

static int input_device_start_pa(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    SoundIoInputDevicePulseAudio *ord = (SoundIoInputDevicePulseAudio *)input_device->backend_data;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;
    pa_threaded_mainloop_lock(ah->main_loop);

    int err = pa_stream_connect_record(ord->stream,
            input_device->device->name,
            &ord->buffer_attr, PA_STREAM_ADJUST_LATENCY);
    if (err) {
        pa_threaded_mainloop_unlock(ah->main_loop);
        return SoundIoErrorOpeningDevice;
    }

    pa_threaded_mainloop_unlock(ah->main_loop);
    return 0;
}

static void input_device_peek_pa(SoundIo *soundio,
        SoundIoInputDevice *input_device, const char **data, int *frame_count)
{
    SoundIoInputDevicePulseAudio *ord = (SoundIoInputDevicePulseAudio *)input_device->backend_data;
    pa_stream *stream = ord->stream;
    if (ord->stream_ready) {
        size_t nbytes;
        if (pa_stream_peek(stream, (const void **)data, &nbytes))
            panic("pa_stream_peek error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
        *frame_count = ((int)nbytes) / input_device->bytes_per_frame;
    } else {
        *data = nullptr;
        *frame_count = 0;
    }
}

static void input_device_drop_pa(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    SoundIoInputDevicePulseAudio *ord = (SoundIoInputDevicePulseAudio *)input_device->backend_data;
    pa_stream *stream = ord->stream;
    if (pa_stream_drop(stream))
        panic("pa_stream_drop error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
}

static void input_device_clear_buffer_pa(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    SoundIoInputDevicePulseAudio *ord = (SoundIoInputDevicePulseAudio *)input_device->backend_data;
    if (!ord->stream_ready)
        return;

    pa_stream *stream = ord->stream;
    SoundIoPulseAudio *ah = (SoundIoPulseAudio *)soundio->backend_data;

    pa_threaded_mainloop_lock(ah->main_loop);

    for (;;) {
        const char *data;
        size_t nbytes;
        if (pa_stream_peek(stream, (const void **)&data, &nbytes))
            panic("pa_stream_peek error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));

        if (nbytes == 0)
            break;

        if (pa_stream_drop(stream))
            panic("pa_stream_drop error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
    }

    pa_threaded_mainloop_unlock(ah->main_loop);
}

static void refresh_devices(SoundIo *soundio) {
    block_until_ready(soundio);
    soundio_flush_events(soundio);
    block_until_have_devices(soundio);
}

int soundio_pulseaudio_init(SoundIo *soundio) {
    assert(!soundio->backend_data);
    SoundIoPulseAudio *ah = create<SoundIoPulseAudio>();
    if (!ah) {
        destroy_pa(soundio);
        return SoundIoErrorNoMem;
    }
    soundio->backend_data = ah;

    ah->connection_refused = false;
    ah->device_scan_queued = false;
    ah->ready_flag = false;
    ah->have_devices_flag = false;

    ah->main_loop = pa_threaded_mainloop_new();
    if (!ah->main_loop) {
        destroy_pa(soundio);
        return SoundIoErrorNoMem;
    }

    pa_mainloop_api *main_loop_api = pa_threaded_mainloop_get_api(ah->main_loop);

    ah->props = pa_proplist_new();
    if (!ah->props) {
        destroy_pa(soundio);
        return SoundIoErrorNoMem;
    }

    // TODO let the API specify this
    pa_proplist_sets(ah->props, PA_PROP_APPLICATION_NAME, "libsoundio");
    pa_proplist_sets(ah->props, PA_PROP_APPLICATION_VERSION, SOUNDIO_VERSION_STRING);
    pa_proplist_sets(ah->props, PA_PROP_APPLICATION_ID, "me.andrewkelley.libsoundio");

    ah->pulse_context = pa_context_new_with_proplist(main_loop_api, "SoundIo", ah->props);
    if (!ah->pulse_context) {
        destroy_pa(soundio);
        return SoundIoErrorNoMem;
    }

    pa_context_set_subscribe_callback(ah->pulse_context, subscribe_callback, soundio);
    pa_context_set_state_callback(ah->pulse_context, context_state_callback, soundio);

    int err = pa_context_connect(ah->pulse_context, NULL, (pa_context_flags_t)0, NULL);
    if (err) {
        destroy_pa(soundio);
        return SoundIoErrorInitAudioBackend;
    }

    if (ah->connection_refused) {
        destroy_pa(soundio);
        return SoundIoErrorInitAudioBackend;
    }

    if (pa_threaded_mainloop_start(ah->main_loop)) {
        destroy_pa(soundio);
        return SoundIoErrorNoMem;
    }

    soundio->destroy = destroy_pa;
    soundio->flush_events = flush_events;
    soundio->refresh_devices = refresh_devices;

    soundio->output_device_init = output_device_init_pa;
    soundio->output_device_destroy = output_device_destroy_pa;
    soundio->output_device_start = output_device_start_pa;
    soundio->output_device_free_count = output_device_free_count_pa;
    soundio->output_device_begin_write = output_device_begin_write_pa;
    soundio->output_device_write = output_device_write_pa;
    soundio->output_device_clear_buffer = output_device_clear_buffer_pa;

    soundio->input_device_init = input_device_init_pa;
    soundio->input_device_destroy = input_device_destroy_pa;
    soundio->input_device_start = input_device_start_pa;
    soundio->input_device_peek = input_device_peek_pa;
    soundio->input_device_drop = input_device_drop_pa;
    soundio->input_device_clear_buffer = input_device_clear_buffer_pa;

    return 0;
}


