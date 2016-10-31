/*
 * Copyright (c) 2016 libsoundio
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "android.h"
#include "soundio_private.h"
#include "util.h"

static void destroy_android(struct SoundIoPrivate *si) {
    struct SoundIoAndroid *sia = &si->backend_data.android;

    if (sia->cond) {
        soundio_os_cond_destroy(sia->cond);
        sia->cond = NULL;
    }

    if (sia->engineObject) {
        (*sia->engineObject)->Destroy(sia->engineObject);
        sia->engineObject = NULL;
    }
}

static void flush_events_android(struct SoundIoPrivate *si) {
    struct SoundIo *soundio = &si->pub;
    struct SoundIoAndroid *sia = &si->backend_data.android;
    if (sia->devices_emitted)
        return;
    sia->devices_emitted = true;
    soundio->on_devices_change(soundio);
}

static void wait_events_android(struct SoundIoPrivate *si) {
    struct SoundIoAndroid *sia = &si->backend_data.android;
    flush_events_android(si);
    soundio_os_cond_wait(sia->cond, NULL);
}

static void wakeup_android(struct SoundIoPrivate *si) {
    struct SoundIoAndroid *sia = &si->backend_data.android;
    soundio_os_cond_signal(sia->cond, NULL);
}

static void force_device_scan_android(struct SoundIoPrivate *si) {
    // nothing to do; Android devices never change
}

static void write_callback_android(SLAndroidSimpleBufferQueueItf bq, void* context) {
    struct SoundIoOutStreamPrivate *os = context;
    struct SoundIoOutStreamAndroid *osa = &os->backend_data.android;
    struct SoundIoOutStream *outstream = &os->pub;

    osa->write_frame_count = 0;
    outstream->write_callback(outstream, 1, osa->bytes_per_buffer /
        outstream->bytes_per_frame);

    // Sometimes write callbacks return without writing to any buffers. They're
    // supposed to honor min_frames, but if they don't, give them a more informative
    // error message and keep the stream alive.
    if (osa->write_frame_count == 0) {
        outstream->error_callback(outstream, SoundIoErrorInvalid);
        osa->write_frame_count = 1;
        osa->buffers[osa->curBuffer][0] = 0;
    }

    if (SL_RESULT_SUCCESS != (*osa->playerBufferQueue)->Enqueue(osa->playerBufferQueue,
        osa->buffers[osa->curBuffer], osa->write_frame_count * outstream->bytes_per_frame))
    {
        outstream->error_callback(outstream, SoundIoErrorStreaming);
    }

    osa->curBuffer ^= 1;
}

static void outstream_destroy_android(struct SoundIoPrivate *si,
    struct SoundIoOutStreamPrivate *os)
{
    struct SoundIoOutStreamAndroid *osa = &os->backend_data.android;

    if (osa->playerObject)
        (*osa->playerObject)->Destroy(osa->playerObject);

    if (osa->outputMixObject)
        (*osa->outputMixObject)->Destroy(osa->outputMixObject);

    if (osa->buffers[0])
        free(osa->buffers[0]);
}

static enum SoundIoError outstream_open_android(struct SoundIoPrivate *si,
    struct SoundIoOutStreamPrivate *os)
{
    struct SoundIoOutStreamAndroid *osa = &os->backend_data.android;
    struct SoundIoAndroid *sia = &si->backend_data.android;
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoDevice *device = outstream->device;

    if (outstream->software_latency == 0.0) {
        outstream->software_latency = soundio_double_clamp(
                device->software_latency_min, 1.0, device->software_latency_max);
    }

    if (SL_RESULT_SUCCESS != (*sia->engineEngine)->CreateOutputMix(sia->engineEngine,
        &osa->outputMixObject, 0, 0, 0))
    {
        return SoundIoErrorOpeningDevice;
    }

    if(SL_RESULT_SUCCESS != (*osa->outputMixObject)->Realize(osa->outputMixObject,
        SL_BOOLEAN_FALSE))
    {
        outstream_destroy_android(si, os);
        return SoundIoErrorOpeningDevice;
    }

    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
        // TODO: The minimum buffer count is 1. When would you want to have
        // a single buffer?
        2
    };

    SLAndroidDataFormat_PCM_EX format_pcm = {
        SL_ANDROID_DATAFORMAT_PCM_EX,
        outstream->layout.channel_count,
        outstream->sample_rate * 1000,
        outstream->bytes_per_sample * 8,
        outstream->bytes_per_sample * 8,
        outstream->layout.channel_count == 1 ? SL_SPEAKER_FRONT_CENTER :
            (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
        SL_BYTEORDER_LITTLEENDIAN,
        outstream->format == SoundIoFormatFloat32LE ?
            SL_ANDROID_PCM_REPRESENTATION_FLOAT :
            outstream->format == SoundIoFormatS16LE ?
            SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT :
            SL_ANDROID_PCM_REPRESENTATION_UNSIGNED_INT
    };

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};
    SLDataLocator_OutputMix loc_outmix = {
        SL_DATALOCATOR_OUTPUTMIX,
        osa->outputMixObject
    };
    SLDataSink audioSnk = {&loc_outmix, NULL};
    SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    if (SL_RESULT_SUCCESS != (*sia->engineEngine)->CreateAudioPlayer(sia->engineEngine,
        &osa->playerObject, &audioSrc, &audioSnk, 1, ids, req))
    {
        outstream_destroy_android(si, os);
        return SoundIoErrorOpeningDevice;
    }
    if (SL_RESULT_SUCCESS != (*osa->playerObject)->Realize(osa->playerObject,
        SL_BOOLEAN_FALSE))
    {
        outstream_destroy_android(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if (SL_RESULT_SUCCESS != (*osa->playerObject)->GetInterface(osa->playerObject,
        SL_IID_PLAY, &osa->playerPlay))
    {
        outstream_destroy_android(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if (SL_RESULT_SUCCESS != (*osa->playerObject)->GetInterface(osa->playerObject,
        SL_IID_BUFFERQUEUE, &osa->playerBufferQueue))
    {
        outstream_destroy_android(si, os);
        return SoundIoErrorOpeningDevice;
    }
    if (SL_RESULT_SUCCESS != (*osa->playerBufferQueue)->RegisterCallback(
        osa->playerBufferQueue, write_callback_android, os))
    {
        outstream_destroy_android(si, os);
        return SoundIoErrorOpeningDevice;
    }

    // TODO: An audio stream's latency and jitter can be reduced by using the
    // native buffer size. Unfortunately, there is no officially supported way
    // to get the native buffer size without access to a JVM.
    osa->bytes_per_buffer = outstream->bytes_per_frame * outstream->sample_rate
        * outstream->software_latency / 2;
    size_t samples_per_buffer = osa->bytes_per_buffer / outstream->bytes_per_sample;
    osa->buffers[0] = calloc(samples_per_buffer * 2, outstream->bytes_per_sample);
    if (!osa->buffers[0]) {
        outstream_destroy_android(si, os);
        return SoundIoErrorNoMem;
    }
    osa->buffers[1] = &osa->buffers[0][osa->bytes_per_buffer];

    return 0;
}

static enum SoundIoError outstream_pause_android(struct SoundIoPrivate *si,
    struct SoundIoOutStreamPrivate *os, bool pause)
{
    struct SoundIoOutStreamAndroid *osa = &os->backend_data.android;

    if (SL_RESULT_SUCCESS != (*osa->playerPlay)->SetPlayState(osa->playerPlay,
        pause ? SL_PLAYSTATE_PAUSED : SL_PLAYSTATE_PLAYING))
    {
        return SoundIoErrorInvalid;
    }

    return 0;
}

static enum SoundIoError outstream_start_android(struct SoundIoPrivate *si,
    struct SoundIoOutStreamPrivate *os)
{
    struct SoundIoOutStreamAndroid *osa = &os->backend_data.android;

    if (SL_RESULT_SUCCESS != (*osa->playerPlay)->SetPlayState(osa->playerPlay,
        SL_PLAYSTATE_PLAYING))
    {
        return SoundIoErrorInvalid;
    }
    write_callback_android(osa->playerBufferQueue, os);
    write_callback_android(osa->playerBufferQueue, os);

    return 0;
}

static enum SoundIoError outstream_begin_write_android(struct SoundIoPrivate *si,
    struct SoundIoOutStreamPrivate *os, struct SoundIoChannelArea **out_areas,
    int *frame_count)
{
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoOutStreamAndroid *osa = &os->backend_data.android;

    if (*frame_count > osa->bytes_per_buffer / outstream->bytes_per_frame)
        return SoundIoErrorInvalid;

    char *write_ptr = osa->buffers[osa->curBuffer];
    for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
        osa->areas[ch].ptr = write_ptr + outstream->bytes_per_sample * ch;
        osa->areas[ch].step = outstream->bytes_per_frame;
    }
    osa->write_frame_count = *frame_count;
    *out_areas = osa->areas;

    return 0;
}

static enum SoundIoError outstream_end_write_android(struct SoundIoPrivate *si,
    struct SoundIoOutStreamPrivate *os)
{
    return 0;
}

static enum SoundIoError outstream_clear_buffer_android(struct SoundIoPrivate *si,
    struct SoundIoOutStreamPrivate *os)
{
    // TODO: This might be supported, actually.
    return SoundIoErrorIncompatibleBackend;
}

static enum SoundIoError outstream_get_latency_android(struct SoundIoPrivate *si,
    struct SoundIoOutStreamPrivate *os, double *out_latency)
{
    struct SoundIoOutStream *outstream = &os->pub;
    *out_latency = outstream->software_latency;
    return 0;
}

enum SoundIoError soundio_android_init(struct SoundIoPrivate *si) {
    struct SoundIo *soundio = &si->pub;
    struct SoundIoAndroid *sia = &si->backend_data.android;

    if (SL_RESULT_SUCCESS != slCreateEngine(&sia->engineObject, 0, NULL, 0, NULL,
        NULL))
    {
        return SoundIoErrorInitAudioBackend;
    }

    if (SL_RESULT_SUCCESS != (*sia->engineObject)->Realize(sia->engineObject,
        SL_BOOLEAN_FALSE))
    {
        destroy_android(si);
        return SoundIoErrorInitAudioBackend;
    }

    if (SL_RESULT_SUCCESS != (*sia->engineObject)->GetInterface(sia->engineObject,
        SL_IID_ENGINE, &sia->engineEngine))
    {
        destroy_android(si);
        return SoundIoErrorInitAudioBackend;
    }

    sia->cond = soundio_os_cond_create();
    if (!sia->cond) {
        destroy_android(si);
        return SoundIoErrorNoMem;
    }

    si->safe_devices_info = ALLOCATE(struct SoundIoDevicesInfo, 1);
    if (!si->safe_devices_info) {
        destroy_android(si);
        return SoundIoErrorNoMem;
    }

    // TODO: no input devices yet
    si->safe_devices_info->default_input_index = -1;
    si->safe_devices_info->default_output_index = 0;
    si->destroy = destroy_android;
    si->flush_events = flush_events_android;
    si->wait_events = wait_events_android;
    si->wakeup = wakeup_android;
    si->force_device_scan = force_device_scan_android;

    si->outstream_open = outstream_open_android;
    si->outstream_destroy = outstream_destroy_android;
    si->outstream_start = outstream_start_android;
    si->outstream_begin_write = outstream_begin_write_android;
    si->outstream_end_write = outstream_end_write_android;
    si->outstream_clear_buffer = outstream_clear_buffer_android;
    si->outstream_pause = outstream_pause_android;
    si->outstream_get_latency = outstream_get_latency_android;

    // create output device
    {
        struct SoundIoDevicePrivate *dev = ALLOCATE(struct SoundIoDevicePrivate, 1);
        if (!dev) {
            destroy_android(si);
            return SoundIoErrorNoMem;
        }
        struct SoundIoDevice *device = &dev->pub;

        device->ref_count = 1;
        device->aim = SoundIoDeviceAimOutput;
        device->soundio = soundio;
        device->id = strdup("android-out");
        device->name = strdup("Android Output");
        if (!device->id || !device->name) {
            soundio_device_unref(device);
            destroy_android(si);
            return SoundIoErrorNoMem;
        }

        // The following information is according to:
        // https://developer.android.com/ndk/guides/audio/opensl-for-android.html

        // TODO: could be prealloc'd?
        device->layout_count = 2;
        device->layouts = ALLOCATE(struct SoundIoChannelLayout, device->layout_count);
        if (!device->layouts) {
            soundio_device_unref(device);
            destroy_android(si);
            return SoundIoErrorNoMem;
        }
        device->layouts[0] =
            *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);
        device->layouts[1] =
            *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);

        // Might support additional formats as well? But there's no way to query
        // for them.
        // TODO: Could be prealloc'd?
        device->format_count = 3;
        device->formats = ALLOCATE(enum SoundIoFormat, device->format_count);
        if (!device->formats) {
            soundio_device_unref(device);
            destroy_android(si);
            return SoundIoErrorNoMem;
        }
        device->formats[0] = SoundIoFormatU8;
        device->formats[1] = SoundIoFormatS16LE;
        // Floating-point was added in API level 21 / Android 5.0 Lollipop
        device->formats[2] = SoundIoFormatFloat32LE;

        // TODO: Could be prealloc'd?
        device->sample_rate_count = 9;
        device->sample_rates = ALLOCATE(struct SoundIoSampleRateRange,
            device->sample_rate_count);
        if (!device->sample_rates) {
            soundio_device_unref(device);
            destroy_android(si);
            return SoundIoErrorNoMem;
        }
        device->sample_rates[0].min = device->sample_rates[0].max = 8000;
        device->sample_rates[1].min = device->sample_rates[1].max = 11025;
        device->sample_rates[2].min = device->sample_rates[2].max = 12000;
        device->sample_rates[3].min = device->sample_rates[3].max = 16000;
        device->sample_rates[4].min = device->sample_rates[4].max = 22050;
        device->sample_rates[5].min = device->sample_rates[5].max = 24000;
        device->sample_rates[6].min = device->sample_rates[6].max = 32000;
        device->sample_rates[7].min = device->sample_rates[7].max = 44100;
        device->sample_rates[8].min = device->sample_rates[8].max = 48000;

        // TODO: An audio stream's latency can be greatly reduced by using the
        // native sampling rate (and thus bypassing software remixing in AudioFlinger
        // (a.k.a. the "fast track")). Unfortunately, there is no officially
        // supported way to get the native sampling rate without access to a JVM.
        device->sample_rate_current = 48000;

        // In general, Android makes no guarantees about audio latency. There
        // are hardware feature flags that do make guarantees:
        // - android.hardware.audio.low_latency: output latency of <=45ms
        // - android.hardware.audio.pro: round-trip latency of <=20ms
        // There is currently no way to check these flags without access to a
        // JVM. The following number is a very optimistic lower-bound.
        device->software_latency_min = 0.01;

        // TODO: These numbers are completely made up
        device->software_latency_current = 0.1;
        device->software_latency_max = 4.0;

        if (SoundIoListDevicePtr_append(&si->safe_devices_info->output_devices,
            device))
        {
            soundio_device_unref(device);
            destroy_android(si);
            return SoundIoErrorNoMem;
        }
    }

    return SoundIoErrorNone;
}
