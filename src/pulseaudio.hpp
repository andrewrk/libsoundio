/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_PULSEAUDIO_HPP
#define SOUNDIO_PULSEAUDIO_HPP

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
    struct SoundIoDevicesInfo *current_audio_devices_info;
    char * default_sink_name;
    char * default_source_name;

    // this one is ready to be read with flush_events. protected by mutex
    struct SoundIoDevicesInfo *ready_audio_devices_info;

    bool have_sink_list;
    bool have_source_list;
    bool have_default_sink;

    atomic_bool ready_flag;
    atomic_bool have_devices_flag;

    pa_threaded_mainloop *main_loop;
    pa_proplist *props;
};

int soundio_pulseaudio_init(struct SoundIo *soundio);

#endif
