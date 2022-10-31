/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_COREAUDIO_H
#define SOUNDIO_COREAUDIO_H

#include "soundio_internal.h"
#include "os.h"
#include "list.h"
#include "atomics.h"

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

struct SoundIoPrivate;
int soundio_coreaudio_init(struct SoundIoPrivate *si);

struct SoundIoDeviceCoreAudio {
    AudioDeviceID device_id;
    UInt32 latency_frames;
};

SOUNDIO_MAKE_LIST_STRUCT(AudioDeviceID, SoundIoListAudioDeviceID, SOUNDIO_LIST_STATIC)

struct SoundIoCoreAudio {
    struct SoundIoOsMutex *mutex;
    struct SoundIoOsCond *cond;
    struct SoundIoOsThread *thread;
    struct SoundIoAtomicFlag abort_flag;

    // this one is ready to be read with flush_events. protected by mutex
    struct SoundIoDevicesInfo *ready_devices_info;
    struct SoundIoAtomicBool have_devices_flag;
    struct SoundIoOsCond *have_devices_cond;
    struct SoundIoOsCond *scan_devices_cond;
    struct SoundIoListAudioDeviceID registered_listeners;

    struct SoundIoAtomicBool device_scan_queued;
    struct SoundIoAtomicBool service_restarted;
    int shutdown_err;
    bool emitted_shutdown_cb;
};

struct SoundIoOutStreamCoreAudio {
    AudioComponentInstance instance;
    AudioDeviceIOProcID io_proc_id;
    AudioStreamID raw_stream_id;
    AudioStreamBasicDescription hardware_format;
    AudioStreamBasicDescription previous_hardware_format;
    AudioBufferList *io_data;
    int buffer_index;
    int frames_left;
    int write_frame_count;
    double hardware_latency;
    float volume;
    struct SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
    struct SoundIoAtomicBool output_format_match;
    bool revert_format;
};

struct SoundIoInStreamCoreAudio {
    AudioComponentInstance instance;
    AudioBufferList *buffer_list;
    int frames_left;
    double hardware_latency;
    struct SoundIoChannelArea areas[SOUNDIO_MAX_CHANNELS];
};

#endif
