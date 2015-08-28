/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_PULSEAUDIO_HPP
#define SOUNDIO_PULSEAUDIO_HPP

#include "soundio_private.h"
#include "atomics.hpp"

#include <pulse/pulseaudio.h>

int soundio_pulseaudio_init(struct SoundIoPrivate *si);

struct SoundIoDevicePulseAudio { };

struct SoundIoPulseAudio {
    int device_query_err;
    int connection_err;
    bool emitted_shutdown_cb;

    pa_context *pulse_context;
    bool device_scan_queued;

    // the one that we're working on building
    struct SoundIoDevicesInfo *current_devices_info;
    char *default_sink_name;
    char *default_source_name;

    // this one is ready to be read with flush_events. protected by mutex
    struct SoundIoDevicesInfo *ready_devices_info;

    bool ready_flag;

    pa_threaded_mainloop *main_loop;
    pa_proplist *props;
};

struct SoundIoOutStreamPulseAudio {
    pa_stream *stream;
    atomic_bool stream_ready;
    pa_buffer_attr buffer_attr;
    char *write_ptr;
    size_t write_byte_count;
    SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

struct SoundIoInStreamPulseAudio {
    pa_stream *stream;
    atomic_bool stream_ready;
    pa_buffer_attr buffer_attr;
    char *peek_buf;
    size_t peek_buf_index;
    size_t peek_buf_size;
    int peek_buf_frames_left;
    int read_frame_count;
    SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

#endif
