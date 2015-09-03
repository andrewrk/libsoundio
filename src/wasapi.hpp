/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_WASAPI_HPP
#define SOUNDIO_WASAPI_HPP

#include "soundio_private.h"
#include "os.h"
#include "atomics.hpp"
#include "list.hpp"

#define INITGUID
#define CINTERFACE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmreg.h>
#include <audioclient.h>
#include <audiosessiontypes.h>
#include <audiopolicy.h>

int soundio_wasapi_init(struct SoundIoPrivate *si);

struct SoundIoDeviceWasapi {
    double period_duration;
    IMMDevice *mm_device;
};

struct SoundIoWasapi {
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;
    SoundIoOsCond *scan_devices_cond;
    SoundIoOsMutex *scan_devices_mutex;
    struct SoundIoOsThread *thread;
    bool abort_flag;
    // this one is ready to be read with flush_events. protected by mutex
    struct SoundIoDevicesInfo *ready_devices_info;
    bool have_devices_flag;
    bool device_scan_queued;
    int shutdown_err;
    bool emitted_shutdown_cb;

    IMMDeviceEnumerator* device_enumerator;
    IMMNotificationClient device_events;
    LONG device_events_refs;
};

struct SoundIoOutStreamWasapi {
    IAudioClient *audio_client;
    IAudioClockAdjustment *audio_clock_adjustment;
    IAudioRenderClient *audio_render_client;
    IAudioSessionControl *audio_session_control;
    LPWSTR stream_name;
    bool need_resample;
    SoundIoOsThread *thread;
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;
    SoundIoOsCond *start_cond;
    atomic_flag thread_exit_flag;
    bool is_raw;
    int writable_frame_count;
    UINT32 buffer_frame_count;
    int write_frame_count;
    HANDLE h_event;
    atomic_bool desired_pause_state;
    atomic_flag pause_resume_flag;
    atomic_flag clear_buffer_flag;
    bool is_paused;
    bool open_complete;
    int open_err;
    bool started;
    UINT32 min_padding_frames;
    SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

struct SoundIoInStreamWasapi {
    IAudioClient *audio_client;
    IAudioCaptureClient *audio_capture_client;
    IAudioSessionControl *audio_session_control;
    LPWSTR stream_name;
    SoundIoOsThread *thread;
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;
    SoundIoOsCond *start_cond;
    atomic_flag thread_exit_flag;
    bool is_raw;
    int readable_frame_count;
    UINT32 buffer_frame_count;
    int read_frame_count;
    HANDLE h_event;
    bool is_paused;
    bool open_complete;
    int open_err;
    bool started;
    char *read_buf;
    int read_buf_frames_left;
    SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

#endif
