/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_WASAPI_H
#define SOUNDIO_WASAPI_H

#include "soundio_internal.h"
#include "os.h"
#include "list.h"
#include "atomics.h"

#define CINTERFACE
#define COBJMACROS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiosessiontypes.h>
#include <audiopolicy.h>

struct SoundIoPrivate;
int soundio_wasapi_init(struct SoundIoPrivate *si);

struct SoundIoDeviceWasapi {
    double period_duration;
    IMMDevice *mm_device;
};

struct SoundIoWasapi {
    struct SoundIoOsMutex *mutex;
    struct SoundIoOsCond *cond;
    struct SoundIoOsCond *scan_devices_cond;
    struct SoundIoOsMutex *scan_devices_mutex;
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
    struct SoundIoOsThread *thread;
    struct SoundIoOsMutex *mutex;
    struct SoundIoOsCond *cond;
    struct SoundIoOsCond *start_cond;
    struct SoundIoAtomicFlag thread_exit_flag;
    bool is_raw;
    int writable_frame_count;
    UINT32 buffer_frame_count;
    int write_frame_count;
    HANDLE h_event;
    struct SoundIoAtomicBool desired_pause_state;
    struct SoundIoAtomicFlag pause_resume_flag;
    struct SoundIoAtomicFlag clear_buffer_flag;
    bool is_paused;
    bool open_complete;
    int open_err;
    bool started;
    UINT32 min_padding_frames;
    struct SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

struct SoundIoInStreamWasapi {
    IAudioClient *audio_client;
    IAudioCaptureClient *audio_capture_client;
    IAudioSessionControl *audio_session_control;
    LPWSTR stream_name;
    struct SoundIoOsThread *thread;
    struct SoundIoOsMutex *mutex;
    struct SoundIoOsCond *cond;
    struct SoundIoOsCond *start_cond;
    struct SoundIoAtomicFlag thread_exit_flag;
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
	int opened_buf_frames;
    struct SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

#endif
