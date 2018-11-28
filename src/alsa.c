/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#define _GNU_SOURCE
#include "alsa.h"
#include "soundio_private.h"

#include <sys/inotify.h>
#include <fcntl.h>
#include <unistd.h>

static snd_pcm_stream_t stream_types[] = {SND_PCM_STREAM_PLAYBACK, SND_PCM_STREAM_CAPTURE};

static snd_pcm_access_t prioritized_access_types[] = {
    SND_PCM_ACCESS_MMAP_INTERLEAVED,
    SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
    SND_PCM_ACCESS_MMAP_COMPLEX,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    SND_PCM_ACCESS_RW_NONINTERLEAVED,
};

SOUNDIO_MAKE_LIST_DEF(struct SoundIoAlsaPendingFile, SoundIoListAlsaPendingFile, SOUNDIO_LIST_STATIC)

static void wakeup_device_poll(struct SoundIoAlsa *sia) {
    ssize_t amt = write(sia->notify_pipe_fd[1], "a", 1);
    if (amt == -1) {
        assert(errno != EBADF);
        assert(errno != EIO);
        assert(errno != ENOSPC);
        assert(errno != EPERM);
        assert(errno != EPIPE);
    }
}

static void wakeup_outstream_poll(struct SoundIoOutStreamAlsa *osa) {
    ssize_t amt = write(osa->poll_exit_pipe_fd[1], "a", 1);
    if (amt == -1) {
        assert(errno != EBADF);
        assert(errno != EIO);
        assert(errno != ENOSPC);
        assert(errno != EPERM);
        assert(errno != EPIPE);
    }
}

static void destroy_alsa(struct SoundIoPrivate *si) {
    struct SoundIoAlsa *sia = &si->backend_data.alsa;

    if (sia->thread) {
        SOUNDIO_ATOMIC_FLAG_CLEAR(sia->abort_flag);
        wakeup_device_poll(sia);
        soundio_os_thread_destroy(sia->thread);
    }

    SoundIoListAlsaPendingFile_deinit(&sia->pending_files);

    if (sia->cond)
        soundio_os_cond_destroy(sia->cond);

    if (sia->mutex)
        soundio_os_mutex_destroy(sia->mutex);

    soundio_destroy_devices_info(sia->ready_devices_info);



    close(sia->notify_pipe_fd[0]);
    close(sia->notify_pipe_fd[1]);
    close(sia->notify_fd);
}

static inline snd_pcm_uframes_t ceil_dbl_to_uframes(double x) {
    const double truncation = (snd_pcm_uframes_t)x;
    return truncation + (truncation < x);
}

static char * str_partition_on_char(char *str, char c) {
    if (!str)
        return NULL;
    while (*str) {
        if (*str == c) {
            *str = 0;
            return str + 1;
        }
        str += 1;
    }
    return NULL;
}

static snd_pcm_stream_t aim_to_stream(enum SoundIoDeviceAim aim) {
    switch (aim) {
        case SoundIoDeviceAimOutput: return SND_PCM_STREAM_PLAYBACK;
        case SoundIoDeviceAimInput: return SND_PCM_STREAM_CAPTURE;
    }
    assert(0); // Invalid aim
    return SND_PCM_STREAM_PLAYBACK;
}

static enum SoundIoChannelId from_alsa_chmap_pos(unsigned int pos) {
    switch ((enum snd_pcm_chmap_position)pos) {
        case SND_CHMAP_UNKNOWN: return SoundIoChannelIdInvalid;
        case SND_CHMAP_NA:      return SoundIoChannelIdInvalid;
        case SND_CHMAP_MONO:    return SoundIoChannelIdFrontCenter;
        case SND_CHMAP_FL:      return SoundIoChannelIdFrontLeft; // front left
        case SND_CHMAP_FR:      return SoundIoChannelIdFrontRight; // front right
        case SND_CHMAP_RL:      return SoundIoChannelIdBackLeft; // rear left
        case SND_CHMAP_RR:      return SoundIoChannelIdBackRight; // rear right
        case SND_CHMAP_FC:      return SoundIoChannelIdFrontCenter; // front center
        case SND_CHMAP_LFE:     return SoundIoChannelIdLfe; // LFE
        case SND_CHMAP_SL:      return SoundIoChannelIdSideLeft; // side left
        case SND_CHMAP_SR:      return SoundIoChannelIdSideRight; // side right
        case SND_CHMAP_RC:      return SoundIoChannelIdBackCenter; // rear center
        case SND_CHMAP_FLC:     return SoundIoChannelIdFrontLeftCenter; // front left center
        case SND_CHMAP_FRC:     return SoundIoChannelIdFrontRightCenter; // front right center
        case SND_CHMAP_RLC:     return SoundIoChannelIdBackLeftCenter; // rear left center
        case SND_CHMAP_RRC:     return SoundIoChannelIdBackRightCenter; // rear right center
        case SND_CHMAP_FLW:     return SoundIoChannelIdFrontLeftWide; // front left wide
        case SND_CHMAP_FRW:     return SoundIoChannelIdFrontRightWide; // front right wide
        case SND_CHMAP_FLH:     return SoundIoChannelIdFrontLeftHigh; // front left high
        case SND_CHMAP_FCH:     return SoundIoChannelIdFrontCenterHigh; // front center high
        case SND_CHMAP_FRH:     return SoundIoChannelIdFrontRightHigh; // front right high
        case SND_CHMAP_TC:      return SoundIoChannelIdTopCenter; // top center
        case SND_CHMAP_TFL:     return SoundIoChannelIdTopFrontLeft; // top front left
        case SND_CHMAP_TFR:     return SoundIoChannelIdTopFrontRight; // top front right
        case SND_CHMAP_TFC:     return SoundIoChannelIdTopFrontCenter; // top front center
        case SND_CHMAP_TRL:     return SoundIoChannelIdTopBackLeft; // top rear left
        case SND_CHMAP_TRR:     return SoundIoChannelIdTopBackRight; // top rear right
        case SND_CHMAP_TRC:     return SoundIoChannelIdTopBackCenter; // top rear center
        case SND_CHMAP_TFLC:    return SoundIoChannelIdTopFrontLeftCenter; // top front left center
        case SND_CHMAP_TFRC:    return SoundIoChannelIdTopFrontRightCenter; // top front right center
        case SND_CHMAP_TSL:     return SoundIoChannelIdTopSideLeft; // top side left
        case SND_CHMAP_TSR:     return SoundIoChannelIdTopSideRight; // top side right
        case SND_CHMAP_LLFE:    return SoundIoChannelIdLeftLfe; // left LFE
        case SND_CHMAP_RLFE:    return SoundIoChannelIdRightLfe; // right LFE
        case SND_CHMAP_BC:      return SoundIoChannelIdBottomCenter; // bottom center
        case SND_CHMAP_BLC:     return SoundIoChannelIdBottomLeftCenter; // bottom left center
        case SND_CHMAP_BRC:     return SoundIoChannelIdBottomRightCenter; // bottom right center
    }
    return SoundIoChannelIdInvalid;
}

static int to_alsa_chmap_pos(enum SoundIoChannelId channel_id) {
    switch (channel_id) {
        case SoundIoChannelIdFrontLeft:             return SND_CHMAP_FL;
        case SoundIoChannelIdFrontRight:            return SND_CHMAP_FR;
        case SoundIoChannelIdBackLeft:              return SND_CHMAP_RL;
        case SoundIoChannelIdBackRight:             return SND_CHMAP_RR;
        case SoundIoChannelIdFrontCenter:           return SND_CHMAP_FC;
        case SoundIoChannelIdLfe:                   return SND_CHMAP_LFE;
        case SoundIoChannelIdSideLeft:              return SND_CHMAP_SL;
        case SoundIoChannelIdSideRight:             return SND_CHMAP_SR;
        case SoundIoChannelIdBackCenter:            return SND_CHMAP_RC;
        case SoundIoChannelIdFrontLeftCenter:       return SND_CHMAP_FLC;
        case SoundIoChannelIdFrontRightCenter:      return SND_CHMAP_FRC;
        case SoundIoChannelIdBackLeftCenter:        return SND_CHMAP_RLC;
        case SoundIoChannelIdBackRightCenter:       return SND_CHMAP_RRC;
        case SoundIoChannelIdFrontLeftWide:         return SND_CHMAP_FLW;
        case SoundIoChannelIdFrontRightWide:        return SND_CHMAP_FRW;
        case SoundIoChannelIdFrontLeftHigh:         return SND_CHMAP_FLH;
        case SoundIoChannelIdFrontCenterHigh:       return SND_CHMAP_FCH;
        case SoundIoChannelIdFrontRightHigh:        return SND_CHMAP_FRH;
        case SoundIoChannelIdTopCenter:             return SND_CHMAP_TC;
        case SoundIoChannelIdTopFrontLeft:          return SND_CHMAP_TFL;
        case SoundIoChannelIdTopFrontRight:         return SND_CHMAP_TFR;
        case SoundIoChannelIdTopFrontCenter:        return SND_CHMAP_TFC;
        case SoundIoChannelIdTopBackLeft:           return SND_CHMAP_TRL;
        case SoundIoChannelIdTopBackRight:          return SND_CHMAP_TRR;
        case SoundIoChannelIdTopBackCenter:         return SND_CHMAP_TRC;
        case SoundIoChannelIdTopFrontLeftCenter:    return SND_CHMAP_TFLC;
        case SoundIoChannelIdTopFrontRightCenter:   return SND_CHMAP_TFRC;
        case SoundIoChannelIdTopSideLeft:           return SND_CHMAP_TSL;
        case SoundIoChannelIdTopSideRight:          return SND_CHMAP_TSR;
        case SoundIoChannelIdLeftLfe:               return SND_CHMAP_LLFE;
        case SoundIoChannelIdRightLfe:              return SND_CHMAP_RLFE;
        case SoundIoChannelIdBottomCenter:          return SND_CHMAP_BC;
        case SoundIoChannelIdBottomLeftCenter:      return SND_CHMAP_BLC;
        case SoundIoChannelIdBottomRightCenter:     return SND_CHMAP_BRC;

        default:
            return SND_CHMAP_UNKNOWN;
    }
}

static void get_channel_layout(struct SoundIoChannelLayout *dest, snd_pcm_chmap_t *chmap) {
    int channel_count = soundio_int_min(SOUNDIO_MAX_CHANNELS, chmap->channels);
    dest->channel_count = channel_count;
    for (int i = 0; i < channel_count; i += 1) {
        dest->channels[i] = from_alsa_chmap_pos(chmap->pos[i]);
    }
    soundio_channel_layout_detect_builtin(dest);
}

static int handle_channel_maps(struct SoundIoDevice *device, snd_pcm_chmap_query_t **maps) {
    if (!maps)
        return 0;

    snd_pcm_chmap_query_t **p;
    snd_pcm_chmap_query_t *v;

    // one iteration to count
    int layout_count = 0;
    for (p = maps; (v = *p) && layout_count < SOUNDIO_MAX_CHANNELS; p += 1, layout_count += 1) { }
    device->layouts = ALLOCATE(struct SoundIoChannelLayout, layout_count);
    if (!device->layouts) {
        snd_pcm_free_chmaps(maps);
        return SoundIoErrorNoMem;
    }
    device->layout_count = layout_count;

    // iterate again to collect data
    int layout_index;
    for (p = maps, layout_index = 0;
        (v = *p) && layout_index < layout_count;
        p += 1, layout_index += 1)
    {
        get_channel_layout(&device->layouts[layout_index], &v->map);
    }
    snd_pcm_free_chmaps(maps);

    return 0;
}

static snd_pcm_format_t to_alsa_fmt(enum SoundIoFormat fmt) {
    switch (fmt) {
    case SoundIoFormatS8:           return SND_PCM_FORMAT_S8;
    case SoundIoFormatU8:           return SND_PCM_FORMAT_U8;
    case SoundIoFormatS16LE:        return SND_PCM_FORMAT_S16_LE;
    case SoundIoFormatS16BE:        return SND_PCM_FORMAT_S16_BE;
    case SoundIoFormatU16LE:        return SND_PCM_FORMAT_U16_LE;
    case SoundIoFormatU16BE:        return SND_PCM_FORMAT_U16_BE;
    case SoundIoFormatS24LE:        return SND_PCM_FORMAT_S24_LE;
    case SoundIoFormatS24BE:        return SND_PCM_FORMAT_S24_BE;
    case SoundIoFormatU24LE:        return SND_PCM_FORMAT_U24_LE;
    case SoundIoFormatU24BE:        return SND_PCM_FORMAT_U24_BE;
    case SoundIoFormatS32LE:        return SND_PCM_FORMAT_S32_LE;
    case SoundIoFormatS32BE:        return SND_PCM_FORMAT_S32_BE;
    case SoundIoFormatU32LE:        return SND_PCM_FORMAT_U32_LE;
    case SoundIoFormatU32BE:        return SND_PCM_FORMAT_U32_BE;
    case SoundIoFormatFloat32LE:    return SND_PCM_FORMAT_FLOAT_LE;
    case SoundIoFormatFloat32BE:    return SND_PCM_FORMAT_FLOAT_BE;
    case SoundIoFormatFloat64LE:    return SND_PCM_FORMAT_FLOAT64_LE;
    case SoundIoFormatFloat64BE:    return SND_PCM_FORMAT_FLOAT64_BE;

    case SoundIoFormatInvalid:
        return SND_PCM_FORMAT_UNKNOWN;
    }
    return SND_PCM_FORMAT_UNKNOWN;
}

static void test_fmt_mask(struct SoundIoDevice *device, const snd_pcm_format_mask_t *fmt_mask,
        enum SoundIoFormat fmt)
{
    if (snd_pcm_format_mask_test(fmt_mask, to_alsa_fmt(fmt))) {
        device->formats[device->format_count] = fmt;
        device->format_count += 1;
    }
}

static int set_access(snd_pcm_t *handle, snd_pcm_hw_params_t *hwparams, snd_pcm_access_t *out_access) {
    for (int i = 0; i < ARRAY_LENGTH(prioritized_access_types); i += 1) {
        snd_pcm_access_t access = prioritized_access_types[i];
        int err = snd_pcm_hw_params_set_access(handle, hwparams, access);
        if (err >= 0) {
            if (out_access)
                *out_access = access;
            return 0;
        }
    }
    return SoundIoErrorOpeningDevice;
}

// this function does not override device->formats, so if you want it to, deallocate and set it to NULL
static int probe_open_device(struct SoundIoDevice *device, snd_pcm_t *handle, int resample,
        int *out_channels_min, int *out_channels_max)
{
    struct SoundIoDevicePrivate *dev = (struct SoundIoDevicePrivate *)device;
    int err;

    snd_pcm_hw_params_t *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);

    if ((err = snd_pcm_hw_params_any(handle, hwparams)) < 0)
        return SoundIoErrorOpeningDevice;

    if ((err = snd_pcm_hw_params_set_rate_resample(handle, hwparams, resample)) < 0)
        return SoundIoErrorOpeningDevice;

    if ((err = set_access(handle, hwparams, NULL)))
        return err;

    unsigned int channels_min;
    unsigned int channels_max;

    if ((err = snd_pcm_hw_params_get_channels_min(hwparams, &channels_min)) < 0)
        return SoundIoErrorOpeningDevice;
    if ((err = snd_pcm_hw_params_set_channels_last(handle, hwparams, &channels_max)) < 0)
        return SoundIoErrorOpeningDevice;

    *out_channels_min = channels_min;
    *out_channels_max = channels_max;

    unsigned int rate_min;
    unsigned int rate_max;

    if ((err = snd_pcm_hw_params_get_rate_min(hwparams, &rate_min, NULL)) < 0)
        return SoundIoErrorOpeningDevice;

    if ((err = snd_pcm_hw_params_set_rate_last(handle, hwparams, &rate_max, NULL)) < 0)
        return SoundIoErrorOpeningDevice;

    device->sample_rate_count = 1;
    device->sample_rates = &dev->prealloc_sample_rate_range;
    device->sample_rates[0].min = rate_min;
    device->sample_rates[0].max = rate_max;

    double one_over_actual_rate = 1.0 / (double)rate_max;

    // Purposefully leave the parameters with the highest rate, highest channel count.

    snd_pcm_uframes_t min_frames;
    snd_pcm_uframes_t max_frames;


    if ((err = snd_pcm_hw_params_get_buffer_size_min(hwparams, &min_frames)) < 0)
        return SoundIoErrorOpeningDevice;
    if ((err = snd_pcm_hw_params_get_buffer_size_max(hwparams, &max_frames)) < 0)
        return SoundIoErrorOpeningDevice;

    device->software_latency_min = min_frames * one_over_actual_rate;
    device->software_latency_max = max_frames * one_over_actual_rate;

    if ((err = snd_pcm_hw_params_set_buffer_size_first(handle, hwparams, &min_frames)) < 0)
        return SoundIoErrorOpeningDevice;


    snd_pcm_format_mask_t *fmt_mask;
    snd_pcm_format_mask_alloca(&fmt_mask);
    snd_pcm_format_mask_none(fmt_mask);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_S8);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_U8);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_S16_LE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_S16_BE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_U16_LE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_U16_BE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_S24_LE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_S24_BE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_U24_LE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_U24_BE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_S32_LE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_S32_BE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_U32_LE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_U32_BE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_FLOAT_LE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_FLOAT_BE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_FLOAT64_LE);
    snd_pcm_format_mask_set(fmt_mask, SND_PCM_FORMAT_FLOAT64_BE);

    if ((err = snd_pcm_hw_params_set_format_mask(handle, hwparams, fmt_mask)) < 0)
        return SoundIoErrorOpeningDevice;

    if (!device->formats) {
        snd_pcm_hw_params_get_format_mask(hwparams, fmt_mask);
        device->formats = ALLOCATE(enum SoundIoFormat, 18);
        if (!device->formats)
            return SoundIoErrorNoMem;

        device->format_count = 0;
        test_fmt_mask(device, fmt_mask, SoundIoFormatS8);
        test_fmt_mask(device, fmt_mask, SoundIoFormatU8);
        test_fmt_mask(device, fmt_mask, SoundIoFormatS16LE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatS16BE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatU16LE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatU16BE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatS24LE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatS24BE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatU24LE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatU24BE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatS32LE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatS32BE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatU32LE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatU32BE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatFloat32LE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatFloat32BE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatFloat64LE);
        test_fmt_mask(device, fmt_mask, SoundIoFormatFloat64BE);
    }

    return 0;
}

static int probe_device(struct SoundIoDevice *device, snd_pcm_chmap_query_t **maps) {
    int err;
    snd_pcm_t *handle;

    snd_pcm_stream_t stream = aim_to_stream(device->aim);

    if ((err = snd_pcm_open(&handle, device->id, stream, 0)) < 0) {
        handle_channel_maps(device, maps);
        return SoundIoErrorOpeningDevice;
    }

    int channels_min, channels_max;
    if ((err = probe_open_device(device, handle, 0, &channels_min, &channels_max))) {
        handle_channel_maps(device, maps);
        snd_pcm_close(handle);
        return err;
    }

    if (!maps) {
        maps = snd_pcm_query_chmaps(handle);
        if (!maps) {
            // device gave us no channel maps. we're forced to conclude that
            // the min and max channel counts are correct.
            int layout_count = 0;
            for (int i = 0; i < soundio_channel_layout_builtin_count(); i += 1) {
                const struct SoundIoChannelLayout *layout = soundio_channel_layout_get_builtin(i);
                if (layout->channel_count >= channels_min && layout->channel_count <= channels_max) {
                    layout_count += 1;
                }
            }
            device->layout_count = layout_count;
            device->layouts = ALLOCATE(struct SoundIoChannelLayout, device->layout_count);
            if (!device->layouts) {
                snd_pcm_close(handle);
                return SoundIoErrorNoMem;
            }
            int layout_index = 0;
            for (int i = 0; i < soundio_channel_layout_builtin_count(); i += 1) {
                const struct SoundIoChannelLayout *layout = soundio_channel_layout_get_builtin(i);
                if (layout->channel_count >= channels_min && layout->channel_count <= channels_max) {
                    device->layouts[layout_index++] = *soundio_channel_layout_get_builtin(i);
                }
            }
        }
    }

    snd_pcm_chmap_t *chmap = snd_pcm_get_chmap(handle);
    if (chmap) {
        get_channel_layout(&device->current_layout, chmap);
        free(chmap);
    }
    if ((err = handle_channel_maps(device, maps))) {
        snd_pcm_close(handle);
        return err;
    }
    maps = NULL;

    if (!device->is_raw) {
        if (device->sample_rates[0].min == device->sample_rates[0].max)
            device->sample_rate_current = device->sample_rates[0].min;

        if (device->software_latency_min == device->software_latency_max)
            device->software_latency_current = device->software_latency_min;

        // now say that resampling is OK and see what the real min and max is.
        if ((err = probe_open_device(device, handle, 1, &channels_min, &channels_max)) < 0) {
            snd_pcm_close(handle);
            return SoundIoErrorOpeningDevice;
        }
    }

    snd_pcm_close(handle);
    return 0;
}

static inline bool str_has_prefix(const char *big_str, const char *prefix) {
    return strncmp(big_str, prefix, strlen(prefix)) == 0;
}

static int refresh_devices(struct SoundIoPrivate *si) {
    struct SoundIo *soundio = &si->pub;
    struct SoundIoAlsa *sia = &si->backend_data.alsa;

    int err;
    if ((err = snd_config_update_free_global()) < 0)
        return SoundIoErrorSystemResources;
    if ((err = snd_config_update()) < 0)
        return SoundIoErrorSystemResources;

    struct SoundIoDevicesInfo *devices_info = ALLOCATE(struct SoundIoDevicesInfo, 1);
    if (!devices_info)
        return SoundIoErrorNoMem;
    devices_info->default_output_index = -1;
    devices_info->default_input_index = -1;

    void **hints;
    if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
        soundio_destroy_devices_info(devices_info);
        return SoundIoErrorNoMem;
    }

    int default_output_index = -1;
    int sysdefault_output_index = -1;
    int default_input_index = -1;
    int sysdefault_input_index = -1;

    for (void **hint_ptr = hints; *hint_ptr; hint_ptr += 1) {
        char *name = snd_device_name_get_hint(*hint_ptr, "NAME");
        // null - libsoundio has its own dummy backend. API clients should use
        // that instead of alsa null device.
        if (strcmp(name, "null") == 0 ||
            // all these surround devices are clutter
            str_has_prefix(name, "front:") ||
            str_has_prefix(name, "surround21:") ||
            str_has_prefix(name, "surround40:") ||
            str_has_prefix(name, "surround41:") ||
            str_has_prefix(name, "surround50:") ||
            str_has_prefix(name, "surround51:") ||
            str_has_prefix(name, "surround71:"))
        {
            free(name);
            continue;
        }

        // One or both of descr and descr1 can be NULL.
        char *descr = snd_device_name_get_hint(*hint_ptr, "DESC");
        char *descr1 = str_partition_on_char(descr, '\n');

        char *io = snd_device_name_get_hint(*hint_ptr, "IOID");
        bool is_playback;
        bool is_capture;

        // Workaround for Raspberry Pi driver bug, reporting itself as output
        // when really it is input.
        if (descr && strcmp(descr, "bcm2835 ALSA, bcm2835 ALSA") == 0 &&
            descr1 && strcmp(descr1, "Direct sample snooping device") == 0)
        {
            is_playback = false;
            is_capture = true;
        } else if (descr && strcmp(descr, "bcm2835 ALSA, bcm2835 IEC958/HDMI") == 0 &&
                   descr1 && strcmp(descr1, "Direct sample snooping device") == 0)
        {
            is_playback = false;
            is_capture = true;
        } else if (io) {
            if (strcmp(io, "Input") == 0) {
                is_playback = false;
                is_capture = true;
            } else {
                assert(strcmp(io, "Output") == 0);
                is_playback = true;
                is_capture = false;
            }
            free(io);
        } else {
            is_playback = true;
            is_capture = true;
        }

        for (int stream_type_i = 0; stream_type_i < ARRAY_LENGTH(stream_types); stream_type_i += 1) {
            snd_pcm_stream_t stream = stream_types[stream_type_i];
            if (stream == SND_PCM_STREAM_PLAYBACK && !is_playback) continue;
            if (stream == SND_PCM_STREAM_CAPTURE && !is_capture) continue;
            if (stream == SND_PCM_STREAM_CAPTURE && descr1 &&
                (strstr(descr1, "Output") || strstr(descr1, "output")))
            {
                continue;
            }


            struct SoundIoDevicePrivate *dev = ALLOCATE(struct SoundIoDevicePrivate, 1);
            if (!dev) {
                free(name);
                free(descr);
                soundio_destroy_devices_info(devices_info);
                snd_device_name_free_hint(hints);
                return SoundIoErrorNoMem;
            }
            struct SoundIoDevice *device = &dev->pub;
            device->ref_count = 1;
            device->soundio = soundio;
            device->is_raw = false;
            device->id = strdup(name);
            if (descr1) {
                device->name = soundio_alloc_sprintf(NULL, "%s: %s", descr, descr1);
            } else if (descr) {
                device->name = strdup(descr);
            } else {
                device->name = strdup(name);
            }

            if (!device->id || !device->name) {
                soundio_device_unref(device);
                free(name);
                free(descr);
                soundio_destroy_devices_info(devices_info);
                snd_device_name_free_hint(hints);
                return SoundIoErrorNoMem;
            }

            struct SoundIoListDevicePtr *device_list;
            bool is_default = str_has_prefix(name, "default:") || strcmp(name, "default") == 0;
            bool is_sysdefault = str_has_prefix(name, "sysdefault:") || strcmp(name, "sysdefault") == 0;

            if (stream == SND_PCM_STREAM_PLAYBACK) {
                device->aim = SoundIoDeviceAimOutput;
                device_list = &devices_info->output_devices;
                if (is_default)
                    default_output_index = device_list->length;
                if (is_sysdefault)
                    sysdefault_output_index = device_list->length;
                if (devices_info->default_output_index == -1)
                    devices_info->default_output_index = device_list->length;
            } else {
                assert(stream == SND_PCM_STREAM_CAPTURE);
                device->aim = SoundIoDeviceAimInput;
                device_list = &devices_info->input_devices;
                if (is_default)
                    default_input_index = device_list->length;
                if (is_sysdefault)
                    sysdefault_input_index = device_list->length;
                if (devices_info->default_input_index == -1)
                    devices_info->default_input_index = device_list->length;
            }

            device->probe_error = probe_device(device, NULL);

            if (SoundIoListDevicePtr_append(device_list, device)) {
                soundio_device_unref(device);
                free(name);
                free(descr);
                soundio_destroy_devices_info(devices_info);
                snd_device_name_free_hint(hints);
                return SoundIoErrorNoMem;
            }
        }

        free(name);
        free(descr);
    }

    if (default_input_index >= 0) {
        devices_info->default_input_index = default_input_index;
    } else if (sysdefault_input_index >= 0) {
        devices_info->default_input_index = sysdefault_input_index;
    }

    if (default_output_index >= 0) {
        devices_info->default_output_index = default_output_index;
    } else if (sysdefault_output_index >= 0) {
        devices_info->default_output_index = sysdefault_output_index;
    }

    snd_device_name_free_hint(hints);

    int card_index = -1;

    if (snd_card_next(&card_index) < 0)
        return SoundIoErrorSystemResources;

    snd_ctl_card_info_t *card_info;
    snd_ctl_card_info_alloca(&card_info);

    snd_pcm_info_t *pcm_info;
    snd_pcm_info_alloca(&pcm_info);

    while (card_index >= 0) {
        int err;
        snd_ctl_t *handle;
        char name[32];
        sprintf(name, "hw:%d", card_index);
        if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
            if (err == -ENOENT) {
                break;
            } else {
                soundio_destroy_devices_info(devices_info);
                return SoundIoErrorOpeningDevice;
            }
        }

        if ((err = snd_ctl_card_info(handle, card_info)) < 0) {
            snd_ctl_close(handle);
            soundio_destroy_devices_info(devices_info);
            return SoundIoErrorSystemResources;
        }
        const char *card_name = snd_ctl_card_info_get_name(card_info);

        int device_index = -1;
        for (;;) {
            if (snd_ctl_pcm_next_device(handle, &device_index) < 0) {
                snd_ctl_close(handle);
                soundio_destroy_devices_info(devices_info);
                return SoundIoErrorSystemResources;
            }
            if (device_index < 0)
                break;

            snd_pcm_info_set_device(pcm_info, device_index);
            snd_pcm_info_set_subdevice(pcm_info, 0);

            for (int stream_type_i = 0; stream_type_i < ARRAY_LENGTH(stream_types); stream_type_i += 1) {
                snd_pcm_stream_t stream = stream_types[stream_type_i];
                snd_pcm_info_set_stream(pcm_info, stream);

                if ((err = snd_ctl_pcm_info(handle, pcm_info)) < 0) {
                    if (err == -ENOENT) {
                        continue;
                    } else {
                        snd_ctl_close(handle);
                        soundio_destroy_devices_info(devices_info);
                        return SoundIoErrorSystemResources;
                    }
                }

                const char *device_name = snd_pcm_info_get_name(pcm_info);

                struct SoundIoDevicePrivate *dev = ALLOCATE(struct SoundIoDevicePrivate, 1);
                if (!dev) {
                    snd_ctl_close(handle);
                    soundio_destroy_devices_info(devices_info);
                    return SoundIoErrorNoMem;
                }
                struct SoundIoDevice *device = &dev->pub;
                device->ref_count = 1;
                device->soundio = soundio;
                device->id = soundio_alloc_sprintf(NULL, "hw:%d,%d", card_index, device_index);
                device->name = soundio_alloc_sprintf(NULL, "%s %s", card_name, device_name);
                device->is_raw = true;

                if (!device->id || !device->name) {
                    soundio_device_unref(device);
                    snd_ctl_close(handle);
                    soundio_destroy_devices_info(devices_info);
                    return SoundIoErrorNoMem;
                }

                struct SoundIoListDevicePtr *device_list;
                if (stream == SND_PCM_STREAM_PLAYBACK) {
                    device->aim = SoundIoDeviceAimOutput;
                    device_list = &devices_info->output_devices;
                } else {
                    assert(stream == SND_PCM_STREAM_CAPTURE);
                    device->aim = SoundIoDeviceAimInput;
                    device_list = &devices_info->input_devices;
                }

                snd_pcm_chmap_query_t **maps = snd_pcm_query_chmaps_from_hw(card_index, device_index, -1, stream);
                device->probe_error = probe_device(device, maps);

                if (SoundIoListDevicePtr_append(device_list, device)) {
                    soundio_device_unref(device);
                    soundio_destroy_devices_info(devices_info);
                    return SoundIoErrorNoMem;
                }
            }
        }
        snd_ctl_close(handle);
        if (snd_card_next(&card_index) < 0) {
            soundio_destroy_devices_info(devices_info);
            return SoundIoErrorSystemResources;
        }
    }

    soundio_os_mutex_lock(sia->mutex);
    soundio_destroy_devices_info(sia->ready_devices_info);
    sia->ready_devices_info = devices_info;
    sia->have_devices_flag = true;
    soundio_os_cond_signal(sia->cond, sia->mutex);
    soundio->on_events_signal(soundio);
    soundio_os_mutex_unlock(sia->mutex);
    return 0;
}

static void shutdown_backend(struct SoundIoPrivate *si, int err) {
    struct SoundIo *soundio = &si->pub;
    struct SoundIoAlsa *sia = &si->backend_data.alsa;
    soundio_os_mutex_lock(sia->mutex);
    sia->shutdown_err = err;
    soundio_os_cond_signal(sia->cond, sia->mutex);
    soundio->on_events_signal(soundio);
    soundio_os_mutex_unlock(sia->mutex);
}

static bool copy_str(char *dest, const char *src, int buf_len) {
    for (;;) {
        buf_len -= 1;
        if (buf_len <= 0)
            return false;
        *dest = *src;
        dest += 1;
        src += 1;
        if (!*src)
            break;
    }
    *dest = '\0';
    return true;
}

static void device_thread_run(void *arg) {
    struct SoundIoPrivate *si = (struct SoundIoPrivate *)arg;
    struct SoundIoAlsa *sia = &si->backend_data.alsa;

    // Some systems cannot read integer variables if they are not
    // properly aligned. On other systems, incorrect alignment may
    // decrease performance. Hence, the buffer used for reading from
    // the inotify file descriptor should have the same alignment as
    // struct inotify_event.
    char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;

    struct pollfd fds[2];
    fds[0].fd = sia->notify_fd;
    fds[0].events = POLLIN;

    fds[1].fd = sia->notify_pipe_fd[0];
    fds[1].events = POLLIN;

    int err;
    for (;;) {
        int poll_num = poll(fds, 2, -1);
        if (!SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(sia->abort_flag))
            break;
        if (poll_num == -1) {
            if (errno == EINTR)
                continue;
            assert(errno != EFAULT);
            assert(errno != EINVAL);
            assert(errno == ENOMEM);
            // Kernel ran out of polling memory.
            shutdown_backend(si, SoundIoErrorSystemResources);
            return;
        }
        if (poll_num <= 0)
            continue;
        bool got_rescan_event = false;
        if (fds[0].revents & POLLIN) {
            for (;;) {
                ssize_t len = read(sia->notify_fd, buf, sizeof(buf));
                if (len == -1) {
                    assert(errno != EBADF);
                    assert(errno != EFAULT);
                    assert(errno != EINVAL);
                    assert(errno != EIO);
                    assert(errno != EISDIR);
                    if (errno == EBADF || errno == EFAULT || errno == EINVAL ||
                        errno == EIO || errno == EISDIR)
                    {
                        shutdown_backend(si, SoundIoErrorSystemResources);
                        return;
                    }
                }

                // catches EINTR and EAGAIN
                if (len <= 0)
                    break;

                // loop over all events in the buffer
                for (char *ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
                    event = (const struct inotify_event *) ptr;

                    if (!((event->mask & IN_CLOSE_WRITE) || (event->mask & IN_DELETE) || (event->mask & IN_CREATE)))
                        continue;
                    if (event->mask & IN_ISDIR)
                        continue;
                    if (!event->len || event->len < 8)
                        continue;
                    if (strncmp(event->name, "controlC", 8) != 0) {
                        continue;
                    }
                    if (event->mask & IN_CREATE) {
                        if ((err = SoundIoListAlsaPendingFile_add_one(&sia->pending_files))) {
                            shutdown_backend(si, SoundIoErrorNoMem);
                            return;
                        }
                        struct SoundIoAlsaPendingFile *pending_file =
                            SoundIoListAlsaPendingFile_last_ptr(&sia->pending_files);
                        if (!copy_str(pending_file->name, event->name, SOUNDIO_MAX_ALSA_SND_FILE_LEN)) {
                            SoundIoListAlsaPendingFile_pop(&sia->pending_files);
                        }
                        continue;
                    }
                    if (sia->pending_files.length > 0) {
                        // At this point ignore IN_DELETE in favor of waiting until the files
                        // opened with IN_CREATE have their IN_CLOSE_WRITE event.
                        if (!(event->mask & IN_CLOSE_WRITE))
                            continue;
                        for (int i = 0; i < sia->pending_files.length; i += 1) {
                            struct SoundIoAlsaPendingFile *pending_file =
                                SoundIoListAlsaPendingFile_ptr_at(&sia->pending_files, i);
                            if (strcmp(pending_file->name, event->name) == 0) {
                                SoundIoListAlsaPendingFile_swap_remove(&sia->pending_files, i);
                                if (sia->pending_files.length == 0) {
                                    got_rescan_event = true;
                                }
                                break;
                            }
                        }
                    } else if (event->mask & IN_DELETE) {
                        // We are not waiting on created files to be closed, so when
                        // a delete happens we act on it.
                        got_rescan_event = true;
                    }
                }
            }
        }
        if (fds[1].revents & POLLIN) {
            got_rescan_event = true;
            for (;;) {
                ssize_t len = read(sia->notify_pipe_fd[0], buf, sizeof(buf));
                if (len == -1) {
                    assert(errno != EBADF);
                    assert(errno != EFAULT);
                    assert(errno != EINVAL);
                    assert(errno != EIO);
                    assert(errno != EISDIR);
                    if (errno == EBADF || errno == EFAULT || errno == EINVAL ||
                        errno == EIO || errno == EISDIR)
                    {
                        shutdown_backend(si, SoundIoErrorSystemResources);
                        return;
                    }
                }
                if (len <= 0)
                    break;
            }
        }
        if (got_rescan_event) {
            if ((err = refresh_devices(si))) {
                shutdown_backend(si, err);
                return;
            }
        }
    }
}

static void my_flush_events(struct SoundIoPrivate *si, bool wait) {
    struct SoundIo *soundio = &si->pub;
    struct SoundIoAlsa *sia = &si->backend_data.alsa;

    bool change = false;
    bool cb_shutdown = false;
    struct SoundIoDevicesInfo *old_devices_info = NULL;

    soundio_os_mutex_lock(sia->mutex);

    // block until have devices
    while (wait || (!sia->have_devices_flag && !sia->shutdown_err)) {
        soundio_os_cond_wait(sia->cond, sia->mutex);
        wait = false;
    }

    if (sia->shutdown_err && !sia->emitted_shutdown_cb) {
        sia->emitted_shutdown_cb = true;
        cb_shutdown = true;
    } else if (sia->ready_devices_info) {
        old_devices_info = si->safe_devices_info;
        si->safe_devices_info = sia->ready_devices_info;
        sia->ready_devices_info = NULL;
        change = true;
    }

    soundio_os_mutex_unlock(sia->mutex);

    if (cb_shutdown)
        soundio->on_backend_disconnect(soundio, sia->shutdown_err);
    else if (change)
        soundio->on_devices_change(soundio);

    soundio_destroy_devices_info(old_devices_info);
}

static void flush_events_alsa(struct SoundIoPrivate *si) {
    my_flush_events(si, false);
}

static void wait_events_alsa(struct SoundIoPrivate *si) {
    my_flush_events(si, false);
    my_flush_events(si, true);
}

static void wakeup_alsa(struct SoundIoPrivate *si) {
    struct SoundIoAlsa *sia = &si->backend_data.alsa;
    soundio_os_mutex_lock(sia->mutex);
    soundio_os_cond_signal(sia->cond, sia->mutex);
    soundio_os_mutex_unlock(sia->mutex);
}

static void force_device_scan_alsa(struct SoundIoPrivate *si) {
    struct SoundIoAlsa *sia = &si->backend_data.alsa;
    wakeup_device_poll(sia);
}

static void outstream_destroy_alsa(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;

    if (osa->thread) {
        SOUNDIO_ATOMIC_FLAG_CLEAR(osa->thread_exit_flag);
        wakeup_outstream_poll(osa);
        soundio_os_thread_destroy(osa->thread);
        osa->thread = NULL;
    }

    if (osa->handle) {
        snd_pcm_close(osa->handle);
        osa->handle = NULL;
    }

    free(osa->poll_fds);
    osa->poll_fds = NULL;

    free(osa->chmap);
    osa->chmap = NULL;

    free(osa->sample_buffer);
    osa->sample_buffer = NULL;
}

static int outstream_xrun_recovery(struct SoundIoOutStreamPrivate *os, int err) {
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;
    if (err == -EPIPE) {
        err = snd_pcm_prepare(osa->handle);
        if (err >= 0)
            outstream->underflow_callback(outstream);
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(osa->handle)) == -EAGAIN) {
            // wait until suspend flag is released
            poll(NULL, 0, 1);
        }
        if (err < 0)
            err = snd_pcm_prepare(osa->handle);
        if (err >= 0)
            outstream->underflow_callback(outstream);
    }
    return err;
}

static int instream_xrun_recovery(struct SoundIoInStreamPrivate *is, int err) {
    struct SoundIoInStream *instream = &is->pub;
    struct SoundIoInStreamAlsa *isa = &is->backend_data.alsa;
    if (err == -EPIPE) {
        err = snd_pcm_prepare(isa->handle);
        if (err >= 0)
            instream->overflow_callback(instream);
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(isa->handle)) == -EAGAIN) {
            // wait until suspend flag is released
            poll(NULL, 0, 1);
        }
        if (err < 0)
            err = snd_pcm_prepare(isa->handle);
        if (err >= 0)
            instream->overflow_callback(instream);
    }
    return err;
}

static int outstream_wait_for_poll(struct SoundIoOutStreamPrivate *os) {
    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;
    int err;
    unsigned short revents;
    for (;;) {
        if ((err = poll(osa->poll_fds, osa->poll_fd_count_with_extra, -1)) < 0) {
            return SoundIoErrorStreaming;
        }
        if (!SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(osa->thread_exit_flag))
            return SoundIoErrorInterrupted;
        if ((err = snd_pcm_poll_descriptors_revents(osa->handle,
                        osa->poll_fds, osa->poll_fd_count, &revents)) < 0)
        {
            return SoundIoErrorStreaming;
        }
        if (revents & (POLLERR|POLLNVAL|POLLHUP)) {
            return 0;
        }
        if (revents & POLLOUT)
            return 0;
    }
}

static int instream_wait_for_poll(struct SoundIoInStreamPrivate *is) {
    struct SoundIoInStreamAlsa *isa = &is->backend_data.alsa;
    int err;
    unsigned short revents;
    for (;;) {
        if ((err = poll(isa->poll_fds, isa->poll_fd_count, -1)) < 0) {
            return err;
        }
        if ((err = snd_pcm_poll_descriptors_revents(isa->handle,
                        isa->poll_fds, isa->poll_fd_count, &revents)) < 0)
        {
            return err;
        }
        if (revents & (POLLERR|POLLNVAL|POLLHUP)) {
            return 0;
        }
        if (revents & POLLIN)
            return 0;
    }
}

static void outstream_thread_run(void *arg) {
    struct SoundIoOutStreamPrivate *os = (struct SoundIoOutStreamPrivate *) arg;
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;

    int err;

    for (;;) {
        snd_pcm_state_t state = snd_pcm_state(osa->handle);
        switch (state) {
            case SND_PCM_STATE_SETUP:
            {
                if ((err = snd_pcm_prepare(osa->handle)) < 0) {
                    outstream->error_callback(outstream, SoundIoErrorStreaming);
                    return;
                }
                continue;
            }
            case SND_PCM_STATE_PREPARED:
            {
                snd_pcm_sframes_t avail = snd_pcm_avail(osa->handle);
                if (avail < 0) {
                    outstream->error_callback(outstream, SoundIoErrorStreaming);
                    return;
                }

                if ((snd_pcm_uframes_t)avail == osa->buffer_size_frames) {
                    outstream->write_callback(outstream, 0, avail);
                    if (!SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(osa->thread_exit_flag))
                        return;
                    continue;
                }

                if ((err = snd_pcm_start(osa->handle)) < 0) {
                    outstream->error_callback(outstream, SoundIoErrorStreaming);
                    return;
                }
                continue;
            }
            case SND_PCM_STATE_RUNNING:
            case SND_PCM_STATE_PAUSED:
            {
                if ((err = outstream_wait_for_poll(os))) {
                    if (err == SoundIoErrorInterrupted)
                        return;
                    outstream->error_callback(outstream, err);
                    return;
                }
                if (!SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(osa->thread_exit_flag))
                    return;
                if (!SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(osa->clear_buffer_flag)) {
                    if ((err = snd_pcm_drop(osa->handle)) < 0) {
                        outstream->error_callback(outstream, SoundIoErrorStreaming);
                        return;
                    }
                    if ((err = snd_pcm_reset(osa->handle)) < 0) {
                        if (err == -EBADFD) {
                            // If this happens the snd_pcm_drop will have done
                            // the function of the reset so it's ok that this
                            // did not work.
                        } else {
                            outstream->error_callback(outstream, SoundIoErrorStreaming);
                            return;
                        }
                    }
                    continue;
                }

                snd_pcm_sframes_t avail = snd_pcm_avail_update(osa->handle);
                if (avail < 0) {
                    if ((err = outstream_xrun_recovery(os, avail)) < 0) {
                        outstream->error_callback(outstream, SoundIoErrorStreaming);
                        return;
                    }
                    continue;
                }

                if (avail > 0)
                    outstream->write_callback(outstream, 0, avail);
                continue;
            }
            case SND_PCM_STATE_XRUN:
                if ((err = outstream_xrun_recovery(os, -EPIPE)) < 0) {
                    outstream->error_callback(outstream, SoundIoErrorStreaming);
                    return;
                }
                continue;
            case SND_PCM_STATE_SUSPENDED:
                if ((err = outstream_xrun_recovery(os, -ESTRPIPE)) < 0) {
                    outstream->error_callback(outstream, SoundIoErrorStreaming);
                    return;
                }
                continue;
            case SND_PCM_STATE_OPEN:
            case SND_PCM_STATE_DRAINING:
            case SND_PCM_STATE_DISCONNECTED:
                outstream->error_callback(outstream, SoundIoErrorStreaming);
                return;
            default:
                continue;
        }
    }
}

static void instream_thread_run(void *arg) {
    struct SoundIoInStreamPrivate *is = (struct SoundIoInStreamPrivate *) arg;
    struct SoundIoInStream *instream = &is->pub;
    struct SoundIoInStreamAlsa *isa = &is->backend_data.alsa;

    int err;

    for (;;) {
        snd_pcm_state_t state = snd_pcm_state(isa->handle);
        switch (state) {
            case SND_PCM_STATE_SETUP:
                if ((err = snd_pcm_prepare(isa->handle)) < 0) {
                    instream->error_callback(instream, SoundIoErrorStreaming);
                    return;
                }
                continue;
            case SND_PCM_STATE_PREPARED:
                if ((err = snd_pcm_start(isa->handle)) < 0) {
                    instream->error_callback(instream, SoundIoErrorStreaming);
                    return;
                }
                continue;
            case SND_PCM_STATE_RUNNING:
            case SND_PCM_STATE_PAUSED:
            {
                if ((err = instream_wait_for_poll(is)) < 0) {
                    if (!SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(isa->thread_exit_flag))
                        return;
                    instream->error_callback(instream, SoundIoErrorStreaming);
                    return;
                }
                if (!SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(isa->thread_exit_flag))
                    return;

                snd_pcm_sframes_t avail = snd_pcm_avail_update(isa->handle);

                if (avail < 0) {
                    if ((err = instream_xrun_recovery(is, avail)) < 0) {
                        instream->error_callback(instream, SoundIoErrorStreaming);
                        return;
                    }
                    continue;
                }

                if (avail > 0)
                    instream->read_callback(instream, 0, avail);
                continue;
            }
            case SND_PCM_STATE_XRUN:
                if ((err = instream_xrun_recovery(is, -EPIPE)) < 0) {
                    instream->error_callback(instream, SoundIoErrorStreaming);
                    return;
                }
                continue;
            case SND_PCM_STATE_SUSPENDED:
                if ((err = instream_xrun_recovery(is, -ESTRPIPE)) < 0) {
                    instream->error_callback(instream, SoundIoErrorStreaming);
                    return;
                }
                continue;
            case SND_PCM_STATE_OPEN:
            case SND_PCM_STATE_DRAINING:
            case SND_PCM_STATE_DISCONNECTED:
                instream->error_callback(instream, SoundIoErrorStreaming);
                return;
            default:
                continue;
        }
    }
}

static int outstream_open_alsa(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoDevice *device = outstream->device;

    SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(osa->clear_buffer_flag);

    if (outstream->software_latency == 0.0)
        outstream->software_latency = 1.0;
    outstream->software_latency = soundio_double_clamp(device->software_latency_min, outstream->software_latency, device->software_latency_max);

    int ch_count = outstream->layout.channel_count;

    osa->chmap_size = sizeof(int) + sizeof(int) * ch_count;
    osa->chmap = (snd_pcm_chmap_t *)ALLOCATE(char, osa->chmap_size);
    if (!osa->chmap) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorNoMem;
    }

    int err;

    snd_pcm_hw_params_t *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);

    snd_pcm_stream_t stream = aim_to_stream(outstream->device->aim);

    if ((err = snd_pcm_open(&osa->handle, outstream->device->id, stream, 0)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if ((err = snd_pcm_hw_params_any(osa->handle, hwparams)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    int want_resample = !outstream->device->is_raw;
    if ((err = snd_pcm_hw_params_set_rate_resample(osa->handle, hwparams, want_resample)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if ((err = set_access(osa->handle, hwparams, &osa->access))) {
        outstream_destroy_alsa(si, os);
        return err;
    }

    if ((err = snd_pcm_hw_params_set_channels(osa->handle, hwparams, ch_count)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if ((err = snd_pcm_hw_params_set_rate(osa->handle, hwparams, outstream->sample_rate, 0)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    snd_pcm_format_t format = to_alsa_fmt(outstream->format);
    int phys_bits_per_sample = snd_pcm_format_physical_width(format);
    if (phys_bits_per_sample % 8 != 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorIncompatibleDevice;
    }
    int phys_bytes_per_sample = phys_bits_per_sample / 8;
    if ((err = snd_pcm_hw_params_set_format(osa->handle, hwparams, format)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    osa->buffer_size_frames = outstream->software_latency * outstream->sample_rate;
    if ((err = snd_pcm_hw_params_set_buffer_size_near(osa->handle, hwparams, &osa->buffer_size_frames)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }
    outstream->software_latency = ((double)osa->buffer_size_frames) / (double)outstream->sample_rate;

    // write the hardware parameters to device
    if ((err = snd_pcm_hw_params(osa->handle, hwparams)) < 0) {
        outstream_destroy_alsa(si, os);
        return (err == -EINVAL) ? SoundIoErrorIncompatibleDevice : SoundIoErrorOpeningDevice;
    }

    if ((snd_pcm_hw_params_get_period_size(hwparams, &osa->period_size, NULL)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }


    // set channel map
    osa->chmap->channels = ch_count;
    for (int i = 0; i < ch_count; i += 1) {
        osa->chmap->pos[i] = to_alsa_chmap_pos(outstream->layout.channels[i]);
    }
    if ((err = snd_pcm_set_chmap(osa->handle, osa->chmap)) < 0)
        outstream->layout_error = SoundIoErrorIncompatibleDevice;

    // get current swparams
    snd_pcm_sw_params_t *swparams;
    snd_pcm_sw_params_alloca(&swparams);

    if ((err = snd_pcm_sw_params_current(osa->handle, swparams)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if ((err = snd_pcm_sw_params_set_start_threshold(osa->handle, swparams, 0)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    if ((err = snd_pcm_sw_params_set_avail_min(osa->handle, swparams, osa->period_size)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    // write the software parameters to device
    if ((err = snd_pcm_sw_params(osa->handle, swparams)) < 0) {
        outstream_destroy_alsa(si, os);
        return (err == -EINVAL) ? SoundIoErrorIncompatibleDevice : SoundIoErrorOpeningDevice;
    }

    if (osa->access == SND_PCM_ACCESS_RW_INTERLEAVED || osa->access == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
        osa->sample_buffer_size = ch_count * osa->period_size * phys_bytes_per_sample;
        osa->sample_buffer = ALLOCATE_NONZERO(char, osa->sample_buffer_size);
        if (!osa->sample_buffer) {
            outstream_destroy_alsa(si, os);
            return SoundIoErrorNoMem;
        }
    }

    osa->poll_fd_count = snd_pcm_poll_descriptors_count(osa->handle);
    if (osa->poll_fd_count <= 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    osa->poll_fd_count_with_extra = osa->poll_fd_count + 1;
    osa->poll_fds = ALLOCATE(struct pollfd, osa->poll_fd_count_with_extra);
    if (!osa->poll_fds) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorNoMem;
    }

    if ((err = snd_pcm_poll_descriptors(osa->handle, osa->poll_fds, osa->poll_fd_count)) < 0) {
        outstream_destroy_alsa(si, os);
        return SoundIoErrorOpeningDevice;
    }

    struct pollfd *extra_fd = &osa->poll_fds[osa->poll_fd_count];
    if (pipe2(osa->poll_exit_pipe_fd, O_NONBLOCK)) {
        assert(errno != EFAULT);
        assert(errno != EINVAL);
        assert(errno == EMFILE || errno == ENFILE);
        outstream_destroy_alsa(si, os);
        return SoundIoErrorSystemResources;
    }
    extra_fd->fd = osa->poll_exit_pipe_fd[0];
    extra_fd->events = POLLIN;

    return 0;
}

static int outstream_start_alsa(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;
    struct SoundIo *soundio = &si->pub;

    assert(!osa->thread);

    int err;
    SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(osa->thread_exit_flag);
    if ((err = soundio_os_thread_create(outstream_thread_run, os, soundio->emit_rtprio_warning, &osa->thread)))
        return err;

    return 0;
}

static int outstream_begin_write_alsa(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os,
        struct SoundIoChannelArea **out_areas, int *frame_count)
{
    *out_areas = NULL;
    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;
    struct SoundIoOutStream *outstream = &os->pub;

    if (osa->access == SND_PCM_ACCESS_RW_INTERLEAVED) {
        for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
            osa->areas[ch].ptr = osa->sample_buffer + ch * outstream->bytes_per_sample;
            osa->areas[ch].step = outstream->bytes_per_frame;
        }

        osa->write_frame_count = soundio_int_min(*frame_count, osa->period_size);
        *frame_count = osa->write_frame_count;
    } else if (osa->access == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
        for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
            osa->areas[ch].ptr = osa->sample_buffer + ch * outstream->bytes_per_sample * osa->period_size;
            osa->areas[ch].step = outstream->bytes_per_sample;
        }

        osa->write_frame_count = soundio_int_min(*frame_count, osa->period_size);
        *frame_count = osa->write_frame_count;
    } else {
        const snd_pcm_channel_area_t *areas;
        snd_pcm_uframes_t frames = *frame_count;
        int err;

        if ((err = snd_pcm_mmap_begin(osa->handle, &areas, &osa->offset, &frames)) < 0) {
            if (err == -EPIPE || err == -ESTRPIPE)
                return SoundIoErrorUnderflow;
            else
                return SoundIoErrorStreaming;
        }

        for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
            if ((areas[ch].first % 8 != 0) || (areas[ch].step % 8 != 0))
                return SoundIoErrorIncompatibleDevice;
            osa->areas[ch].step = areas[ch].step / 8;
            osa->areas[ch].ptr = ((char *)areas[ch].addr) + (areas[ch].first / 8) +
                (osa->areas[ch].step * osa->offset);
        }

        osa->write_frame_count = frames;
        *frame_count = osa->write_frame_count;
    }

    *out_areas = osa->areas;
    return 0;
}

static int outstream_end_write_alsa(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;
    struct SoundIoOutStream *outstream = &os->pub;

    snd_pcm_sframes_t commitres;
    if (osa->access == SND_PCM_ACCESS_RW_INTERLEAVED) {
        commitres = snd_pcm_writei(osa->handle, osa->sample_buffer, osa->write_frame_count);
    } else if (osa->access == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
        char *ptrs[SOUNDIO_MAX_CHANNELS];
        for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
            ptrs[ch] = osa->sample_buffer + ch * outstream->bytes_per_sample * osa->period_size;
        }
        commitres = snd_pcm_writen(osa->handle, (void**)ptrs, osa->write_frame_count);
    } else {
        commitres = snd_pcm_mmap_commit(osa->handle, osa->offset, osa->write_frame_count);
    }

    if (commitres < 0 || commitres != osa->write_frame_count) {
        int err = (commitres >= 0) ? -EPIPE : commitres;
        if (err == -EPIPE || err == -ESTRPIPE)
            return SoundIoErrorUnderflow;
        else
            return SoundIoErrorStreaming;
    }
    return 0;
}

static int outstream_clear_buffer_alsa(struct SoundIoPrivate *si,
        struct SoundIoOutStreamPrivate *os)
{
    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;
    SOUNDIO_ATOMIC_FLAG_CLEAR(osa->clear_buffer_flag);
    return 0;
}

static int outstream_pause_alsa(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os, bool pause) {
    if (!si)
        return SoundIoErrorInvalid;

    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;

    if (!osa->handle)
        return SoundIoErrorInvalid;

    if (osa->is_paused == pause)
        return 0;

    int err;
    if ((err = snd_pcm_pause(osa->handle, pause)) < 0) {
        return SoundIoErrorIncompatibleDevice;
    }

    osa->is_paused = pause;
    return 0;
}

static int outstream_get_latency_alsa(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os,
        double *out_latency)
{
    struct SoundIoOutStream *outstream = &os->pub;
    struct SoundIoOutStreamAlsa *osa = &os->backend_data.alsa;
    int err;

    snd_pcm_sframes_t delay;
    if ((err = snd_pcm_delay(osa->handle, &delay)) < 0) {
        return SoundIoErrorStreaming;
    }

    *out_latency = delay / (double)outstream->sample_rate;
    return 0;
}

static void instream_destroy_alsa(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    struct SoundIoInStreamAlsa *isa = &is->backend_data.alsa;

    if (isa->thread) {
        SOUNDIO_ATOMIC_FLAG_CLEAR(isa->thread_exit_flag);
        soundio_os_thread_destroy(isa->thread);
        isa->thread = NULL;
    }

    if (isa->handle) {
        snd_pcm_close(isa->handle);
        isa->handle = NULL;
    }

    free(isa->poll_fds);
    isa->poll_fds = NULL;

    free(isa->chmap);
    isa->chmap = NULL;

    free(isa->sample_buffer);
    isa->sample_buffer = NULL;
}

static int instream_open_alsa(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    struct SoundIoInStreamAlsa *isa = &is->backend_data.alsa;
    struct SoundIoInStream *instream = &is->pub;
    struct SoundIoDevice *device = instream->device;

    if (instream->software_latency == 0.0)
        instream->software_latency = 1.0;
    instream->software_latency = soundio_double_clamp(device->software_latency_min, instream->software_latency, device->software_latency_max);

    int ch_count = instream->layout.channel_count;

    isa->chmap_size = sizeof(int) + sizeof(int) * ch_count;
    isa->chmap = (snd_pcm_chmap_t *)ALLOCATE(char, isa->chmap_size);
    if (!isa->chmap) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorNoMem;
    }

    int err;

    snd_pcm_hw_params_t *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);

    snd_pcm_stream_t stream = aim_to_stream(instream->device->aim);

    if ((err = snd_pcm_open(&isa->handle, instream->device->id, stream, 0)) < 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }

    if ((err = snd_pcm_hw_params_any(isa->handle, hwparams)) < 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }

    int want_resample = !instream->device->is_raw;
    if ((err = snd_pcm_hw_params_set_rate_resample(isa->handle, hwparams, want_resample)) < 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }

    if ((err = set_access(isa->handle, hwparams, &isa->access))) {
        instream_destroy_alsa(si, is);
        return err;
    }

    if ((err = snd_pcm_hw_params_set_channels(isa->handle, hwparams, ch_count)) < 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }

    if ((err = snd_pcm_hw_params_set_rate(isa->handle, hwparams, instream->sample_rate, 0)) < 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }

    snd_pcm_format_t format = to_alsa_fmt(instream->format);
    int phys_bits_per_sample = snd_pcm_format_physical_width(format);
    if (phys_bits_per_sample % 8 != 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorIncompatibleDevice;
    }
    int phys_bytes_per_sample = phys_bits_per_sample / 8;
    if ((err = snd_pcm_hw_params_set_format(isa->handle, hwparams, format)) < 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }

    snd_pcm_uframes_t period_frames = ceil_dbl_to_uframes(0.5 * instream->software_latency * (double)instream->sample_rate);
    if ((err = snd_pcm_hw_params_set_period_size_near(isa->handle, hwparams, &period_frames, NULL)) < 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }
    instream->software_latency = ((double)period_frames) / (double)instream->sample_rate;
    isa->period_size = period_frames;


    snd_pcm_uframes_t buffer_size_frames;
    if ((err = snd_pcm_hw_params_set_buffer_size_last(isa->handle, hwparams, &buffer_size_frames)) < 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }

    // write the hardware parameters to device
    if ((err = snd_pcm_hw_params(isa->handle, hwparams)) < 0) {
        instream_destroy_alsa(si, is);
        return (err == -EINVAL) ? SoundIoErrorIncompatibleDevice : SoundIoErrorOpeningDevice;
    }

    // set channel map
    isa->chmap->channels = ch_count;
    for (int i = 0; i < ch_count; i += 1) {
        isa->chmap->pos[i] = to_alsa_chmap_pos(instream->layout.channels[i]);
    }
    if ((err = snd_pcm_set_chmap(isa->handle, isa->chmap)) < 0)
        instream->layout_error = SoundIoErrorIncompatibleDevice;

    // get current swparams
    snd_pcm_sw_params_t *swparams;
    snd_pcm_sw_params_alloca(&swparams);

    if ((err = snd_pcm_sw_params_current(isa->handle, swparams)) < 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }

    // write the software parameters to device
    if ((err = snd_pcm_sw_params(isa->handle, swparams)) < 0) {
        instream_destroy_alsa(si, is);
        return (err == -EINVAL) ? SoundIoErrorIncompatibleDevice : SoundIoErrorOpeningDevice;
    }

    if (isa->access == SND_PCM_ACCESS_RW_INTERLEAVED || isa->access == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
        isa->sample_buffer_size = ch_count * isa->period_size * phys_bytes_per_sample;
        isa->sample_buffer = ALLOCATE_NONZERO(char, isa->sample_buffer_size);
        if (!isa->sample_buffer) {
            instream_destroy_alsa(si, is);
            return SoundIoErrorNoMem;
        }
    }

    isa->poll_fd_count = snd_pcm_poll_descriptors_count(isa->handle);
    if (isa->poll_fd_count <= 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }

    isa->poll_fds = ALLOCATE(struct pollfd, isa->poll_fd_count);
    if (!isa->poll_fds) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorNoMem;
    }

    if ((err = snd_pcm_poll_descriptors(isa->handle, isa->poll_fds, isa->poll_fd_count)) < 0) {
        instream_destroy_alsa(si, is);
        return SoundIoErrorOpeningDevice;
    }

    return 0;
}

static int instream_start_alsa(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    struct SoundIoInStreamAlsa *isa = &is->backend_data.alsa;
    struct SoundIo *soundio = &si->pub;

    assert(!isa->thread);

    SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(isa->thread_exit_flag);
    int err;
    if ((err = soundio_os_thread_create(instream_thread_run, is, soundio->emit_rtprio_warning, &isa->thread))) {
        instream_destroy_alsa(si, is);
        return err;
    }

    return 0;
}

static int instream_begin_read_alsa(struct SoundIoPrivate *si,
        struct SoundIoInStreamPrivate *is, struct SoundIoChannelArea **out_areas, int *frame_count)
{
    *out_areas = NULL;
    struct SoundIoInStreamAlsa *isa = &is->backend_data.alsa;
    struct SoundIoInStream *instream = &is->pub;

    if (isa->access == SND_PCM_ACCESS_RW_INTERLEAVED) {
        for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
            isa->areas[ch].ptr = isa->sample_buffer + ch * instream->bytes_per_sample;
            isa->areas[ch].step = instream->bytes_per_frame;
        }

        isa->read_frame_count = soundio_int_min(*frame_count, isa->period_size);
        *frame_count = isa->read_frame_count;

        snd_pcm_sframes_t commitres = snd_pcm_readi(isa->handle, isa->sample_buffer, isa->read_frame_count);
        if (commitres < 0 || commitres != isa->read_frame_count) {
            int err = (commitres >= 0) ? -EPIPE : commitres;
            if ((err = instream_xrun_recovery(is, err)) < 0)
                return SoundIoErrorStreaming;
        }
    } else if (isa->access == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
        char *ptrs[SOUNDIO_MAX_CHANNELS];
        for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
            isa->areas[ch].ptr = isa->sample_buffer + ch * instream->bytes_per_sample * isa->period_size;
            isa->areas[ch].step = instream->bytes_per_sample;
            ptrs[ch] = isa->areas[ch].ptr;
        }

        isa->read_frame_count = soundio_int_min(*frame_count, isa->period_size);
        *frame_count = isa->read_frame_count;

        snd_pcm_sframes_t commitres = snd_pcm_readn(isa->handle, (void**)ptrs, isa->read_frame_count);
        if (commitres < 0 || commitres != isa->read_frame_count) {
            int err = (commitres >= 0) ? -EPIPE : commitres;
            if ((err = instream_xrun_recovery(is, err)) < 0)
                return SoundIoErrorStreaming;
        }
    } else {
        const snd_pcm_channel_area_t *areas;
        snd_pcm_uframes_t frames = *frame_count;
        int err;

        if ((err = snd_pcm_mmap_begin(isa->handle, &areas, &isa->offset, &frames)) < 0) {
            if ((err = instream_xrun_recovery(is, err)) < 0)
                return SoundIoErrorStreaming;
        }

        for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
            if ((areas[ch].first % 8 != 0) || (areas[ch].step % 8 != 0))
                return SoundIoErrorIncompatibleDevice;
            isa->areas[ch].step = areas[ch].step / 8;
            isa->areas[ch].ptr = ((char *)areas[ch].addr) + (areas[ch].first / 8) +
                (isa->areas[ch].step * isa->offset);
        }

        isa->read_frame_count = frames;
        *frame_count = isa->read_frame_count;
    }

    *out_areas = isa->areas;
    return 0;
}

static int instream_end_read_alsa(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    struct SoundIoInStreamAlsa *isa = &is->backend_data.alsa;

    if (isa->access == SND_PCM_ACCESS_RW_INTERLEAVED) {
        // nothing to do
    } else if (isa->access == SND_PCM_ACCESS_RW_NONINTERLEAVED) {
        // nothing to do
    } else {
        snd_pcm_sframes_t commitres = snd_pcm_mmap_commit(isa->handle, isa->offset, isa->read_frame_count);
        if (commitres < 0 || commitres != isa->read_frame_count) {
            int err = (commitres >= 0) ? -EPIPE : commitres;
            if ((err = instream_xrun_recovery(is, err)) < 0)
                return SoundIoErrorStreaming;
        }
    }

    return 0;
}

static int instream_pause_alsa(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is, bool pause) {
    struct SoundIoInStreamAlsa *isa = &is->backend_data.alsa;

    if (isa->is_paused == pause)
        return 0;

    int err;
    if ((err = snd_pcm_pause(isa->handle, pause)) < 0)
        return SoundIoErrorIncompatibleDevice;

    isa->is_paused = pause;
    return 0;
}

static int instream_get_latency_alsa(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is,
        double *out_latency)
{
    struct SoundIoInStream *instream = &is->pub;
    struct SoundIoInStreamAlsa *isa = &is->backend_data.alsa;
    int err;

    snd_pcm_sframes_t delay;
    if ((err = snd_pcm_delay(isa->handle, &delay)) < 0) {
        return SoundIoErrorStreaming;
    }

    *out_latency = delay / (double)instream->sample_rate;
    return 0;
}

int soundio_alsa_init(struct SoundIoPrivate *si) {
    struct SoundIoAlsa *sia = &si->backend_data.alsa;
    int err;

    sia->notify_fd = -1;
    sia->notify_wd = -1;
    SOUNDIO_ATOMIC_FLAG_TEST_AND_SET(sia->abort_flag);

    sia->mutex = soundio_os_mutex_create();
    if (!sia->mutex) {
        destroy_alsa(si);
        return SoundIoErrorNoMem;
    }

    sia->cond = soundio_os_cond_create();
    if (!sia->cond) {
        destroy_alsa(si);
        return SoundIoErrorNoMem;
    }


    // set up inotify to watch /dev/snd for devices added or removed
    sia->notify_fd = inotify_init1(IN_NONBLOCK);
    if (sia->notify_fd == -1) {
        err = errno;
        assert(err != EINVAL);
        destroy_alsa(si);
        if (err == EMFILE || err == ENFILE) {
            return SoundIoErrorSystemResources;
        } else {
            assert(err == ENOMEM);
            return SoundIoErrorNoMem;
        }
    }

    sia->notify_wd = inotify_add_watch(sia->notify_fd, "/dev/snd", IN_CREATE | IN_CLOSE_WRITE | IN_DELETE);
    if (sia->notify_wd == -1) {
        err = errno;
        assert(err != EACCES);
        assert(err != EBADF);
        assert(err != EFAULT);
        assert(err != EINVAL);
        assert(err != ENAMETOOLONG);
        destroy_alsa(si);
        if (err == ENOSPC) {
            return SoundIoErrorSystemResources;
        } else if (err == ENOMEM) {
            return SoundIoErrorNoMem;
        } else {
            // Kernel must not have ALSA support.
            return SoundIoErrorInitAudioBackend;
        }
    }

    if (pipe2(sia->notify_pipe_fd, O_NONBLOCK)) {
        assert(errno != EFAULT);
        assert(errno != EINVAL);
        assert(errno == EMFILE || errno == ENFILE);
        return SoundIoErrorSystemResources;
    }

    wakeup_device_poll(sia);

    if ((err = soundio_os_thread_create(device_thread_run, si, NULL, &sia->thread))) {
        destroy_alsa(si);
        return err;
    }

    si->destroy = destroy_alsa;
    si->flush_events = flush_events_alsa;
    si->wait_events = wait_events_alsa;
    si->wakeup = wakeup_alsa;
    si->force_device_scan = force_device_scan_alsa;

    si->outstream_open = outstream_open_alsa;
    si->outstream_destroy = outstream_destroy_alsa;
    si->outstream_start = outstream_start_alsa;
    si->outstream_begin_write = outstream_begin_write_alsa;
    si->outstream_end_write = outstream_end_write_alsa;
    si->outstream_clear_buffer = outstream_clear_buffer_alsa;
    si->outstream_pause = outstream_pause_alsa;
    si->outstream_get_latency = outstream_get_latency_alsa;

    si->instream_open = instream_open_alsa;
    si->instream_destroy = instream_destroy_alsa;
    si->instream_start = instream_start_alsa;
    si->instream_begin_read = instream_begin_read_alsa;
    si->instream_end_read = instream_end_read_alsa;
    si->instream_pause = instream_pause_alsa;
    si->instream_get_latency = instream_get_latency_alsa;

    return 0;
}
