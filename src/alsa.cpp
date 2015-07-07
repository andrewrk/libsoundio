/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "alsa.hpp"
#include "soundio.hpp"
#include "os.hpp"
#include "atomics.hpp"

#include <alsa/asoundlib.h>
#include <sys/inotify.h>

static snd_pcm_stream_t stream_types[] = {SND_PCM_STREAM_PLAYBACK, SND_PCM_STREAM_CAPTURE};

struct SoundIoAlsa {
    SoundIoOsMutex *mutex;
    SoundIoOsCond *cond;

    struct SoundIoOsThread *thread;
    atomic_flag abort_flag;
    int notify_fd;
    int notify_wd;
    atomic_bool have_devices_flag;
    int notify_pipe_fd[2];

    // this one is ready to be read with flush_events. protected by mutex
    struct SoundIoDevicesInfo *ready_devices_info;
};

static void wakeup_device_poll(SoundIoAlsa *sia) {
    ssize_t amt = write(sia->notify_pipe_fd[1], "a", 1);
    if (amt == -1) {
        assert(errno != EBADF);
        assert(errno != EIO);
        assert(errno != ENOSPC);
        assert(errno != EPERM);
        assert(errno != EPIPE);
    }
}

static void destroy_alsa(SoundIo *soundio) {
    SoundIoAlsa *sia = (SoundIoAlsa *)soundio->backend_data;
    if (!sia)
        return;

    if (sia->thread) {
        sia->abort_flag.clear();
        wakeup_device_poll(sia);
        soundio_os_thread_destroy(sia->thread);
    }

    if (sia->cond)
        soundio_os_cond_destroy(sia->cond);

    if (sia->mutex)
        soundio_os_mutex_destroy(sia->mutex);

    soundio_destroy_devices_info(sia->ready_devices_info);



    close(sia->notify_pipe_fd[0]);
    close(sia->notify_pipe_fd[1]);
    close(sia->notify_fd);

    destroy(sia);
    soundio->backend_data = nullptr;
}

static int refresh_devices(SoundIo *soundio) {
    SoundIoAlsa *sia = (SoundIoAlsa *)soundio->backend_data;
    int card_index = -1;

    if (snd_card_next(&card_index) < 0)
        return SoundIoErrorSystemResources;

    snd_ctl_card_info_t *card_info;
    snd_ctl_card_info_alloca(&card_info);

    snd_pcm_info_t *pcm_info;
    snd_pcm_info_alloca(&pcm_info);

    SoundIoDevicesInfo *devices_info = create<SoundIoDevicesInfo>();
    if (!devices_info)
        return SoundIoErrorNoMem;

    while (card_index >= 0) {
        int err;
        snd_ctl_t *handle;
        char name[32];
        sprintf(name, "hw:%d", card_index);
        if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
            if (err == -ENOENT) {
                break;
            } else {
                destroy(devices_info);
                return SoundIoErrorOpeningDevice;
            }
        }

        if ((err = snd_ctl_card_info(handle, card_info)) < 0) {
            snd_ctl_close(handle);
            destroy(devices_info);
            return SoundIoErrorSystemResources;
        }
        const char *card_name = snd_ctl_card_info_get_name(card_info);

        int device_index = -1;
        for (;;) {
            if (snd_ctl_pcm_next_device(handle, &device_index) < 0) {
                snd_ctl_close(handle);
                destroy(devices_info);
                return SoundIoErrorSystemResources;
            }
            if (device_index < 0)
                break;

            snd_pcm_info_set_device(pcm_info, device_index);
            snd_pcm_info_set_subdevice(pcm_info, 0);

            for (int stream_type_i = 0; stream_type_i < array_length(stream_types); stream_type_i += 1) {
                snd_pcm_stream_t stream = stream_types[stream_type_i];
                snd_pcm_info_set_stream(pcm_info, stream);

                if ((err = snd_ctl_pcm_info(handle, pcm_info)) < 0) {
                    if (err == -ENOENT) {
                        continue;
                    } else {
                        snd_ctl_close(handle);
                        destroy(devices_info);
                        return SoundIoErrorSystemResources;
                    }
                }

                const char *device_name = snd_pcm_info_get_name(pcm_info);

                SoundIoDevice *device = create<SoundIoDevice>();
                if (!device) {
                    snd_ctl_close(handle);
                    destroy(devices_info);
                    return SoundIoErrorNoMem;
                }
                device->ref_count = 1;
                device->soundio = soundio;
                device->name = soundio_alloc_sprintf(nullptr, "hw:%d,%d,%d", card_index, device_index, 0);
                device->description = soundio_alloc_sprintf(nullptr, "%s %s", card_name, device_name);

                if (!device->name || !device->description) {
                    soundio_device_unref(device);
                    snd_ctl_close(handle);
                    destroy(devices_info);
                    return SoundIoErrorNoMem;
                }

                // TODO: device->channel_layout
                // TODO: device->default_sample_format
                // TODO: device->default_latency
                // TODO: device->default_sample_rate

                SoundIoList<SoundIoDevice *> *device_list;
                if (stream == SND_PCM_STREAM_PLAYBACK) {
                    device->purpose = SoundIoDevicePurposeOutput;
                    device_list = &devices_info->output_devices;
                } else {
                    assert(stream == SND_PCM_STREAM_CAPTURE);
                    device->purpose = SoundIoDevicePurposeInput;
                    device_list = &devices_info->input_devices;
                }

                if (device_list->append(device)) {
                    soundio_device_unref(device);
                    destroy(devices_info);
                    return SoundIoErrorNoMem;
                }
            }
        }
        snd_ctl_close(handle);
        if (snd_card_next(&card_index) < 0) {
            destroy(devices_info);
            return SoundIoErrorSystemResources;
        }
    }

    soundio_os_mutex_lock(sia->mutex);
    soundio_destroy_devices_info(sia->ready_devices_info);
    sia->ready_devices_info = devices_info;
    sia->have_devices_flag.store(true);
    soundio_os_cond_signal(sia->cond, sia->mutex);
    soundio_os_mutex_unlock(sia->mutex);
    return 0;
}

static void device_thread_run(void *arg) {
    SoundIo *soundio = (SoundIo *)arg;
    SoundIoAlsa *sia = (SoundIoAlsa *)soundio->backend_data;

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
        if (!sia->abort_flag.test_and_set())
            break;
        if (poll_num == -1) {
            if (errno == EINTR)
                continue;
            assert(errno != EFAULT);
            assert(errno != EFAULT);
            assert(errno != EINVAL);
            assert(errno == ENOMEM);
            soundio_panic("kernel ran out of polling memory");
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
                }

                // catches EINTR and EAGAIN
                if (len <= 0)
                    break;

                // loop over all events in the buffer
                for (char *ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
                    event = (const struct inotify_event *) ptr;

                    if (!((event->mask & IN_CREATE) || (event->mask & IN_DELETE)))
                        continue;
                    if (event->mask & IN_ISDIR)
                        continue;
                    if (!event->len || event->len < 8)
                        continue;
                    if (event->name[0] != 'p' ||
                        event->name[1] != 'c' ||
                        event->name[2] != 'm')
                    {
                        continue;
                    }
                    got_rescan_event = true;
                    break;
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
                }
                if (len <= 0)
                    break;
            }
        }
        if (got_rescan_event) {
            if ((err = refresh_devices(soundio)))
                soundio_panic("error refreshing devices: %s", soundio_error_string(err));
        }
    }
}

static void block_until_have_devices(SoundIoAlsa *sia) {
    if (sia->have_devices_flag.load())
        return;
    soundio_os_mutex_lock(sia->mutex);
    while (!sia->have_devices_flag.load())
        soundio_os_cond_wait(sia->cond, sia->mutex);
    soundio_os_mutex_unlock(sia->mutex);
}

static void flush_events(SoundIo *soundio) {
    SoundIoAlsa *sia = (SoundIoAlsa *)soundio->backend_data;
    block_until_have_devices(sia);

    bool change = false;
    SoundIoDevicesInfo *old_devices_info = nullptr;

    soundio_os_mutex_lock(sia->mutex);

    if (sia->ready_devices_info) {
        old_devices_info = soundio->safe_devices_info;
        soundio->safe_devices_info = sia->ready_devices_info;
        sia->ready_devices_info = nullptr;
        change = true;
    }

    soundio_os_mutex_unlock(sia->mutex);

    if (change)
        soundio->on_devices_change(soundio);

    soundio_destroy_devices_info(old_devices_info);
}

static void wait_events(SoundIo *soundio) {
    SoundIoAlsa *sia = (SoundIoAlsa *)soundio->backend_data;
    flush_events(soundio);
    soundio_os_mutex_lock(sia->mutex);
    soundio_os_cond_wait(sia->cond, sia->mutex);
    soundio_os_mutex_unlock(sia->mutex);
}

static void wakeup(SoundIo *soundio) {
    SoundIoAlsa *sia = (SoundIoAlsa *)soundio->backend_data;
    soundio_os_mutex_lock(sia->mutex);
    soundio_os_cond_signal(sia->cond, sia->mutex);
    soundio_os_mutex_unlock(sia->mutex);
}

static void output_device_destroy_alsa(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    soundio_panic("TODO");
}

static int output_device_init_alsa(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    soundio_panic("TODO");
}

static int output_device_start_alsa(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    soundio_panic("TODO");
}

static int output_device_free_count_alsa(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    soundio_panic("TODO");
}

static void output_device_begin_write_alsa(SoundIo *soundio,
        SoundIoOutputDevice *output_device, char **data, int *frame_count)
{
    soundio_panic("TODO");
}

static void output_device_write_alsa(SoundIo *soundio,
        SoundIoOutputDevice *output_device, char *data, int frame_count)
{
    soundio_panic("TODO");
}

static void output_device_clear_buffer_alsa(SoundIo *soundio,
        SoundIoOutputDevice *output_device)
{
    soundio_panic("TODO");
}

static int input_device_init_alsa(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    soundio_panic("TODO");
}

static void input_device_destroy_alsa(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    soundio_panic("TODO");
}

static int input_device_start_alsa(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    soundio_panic("TODO");
}

static void input_device_peek_alsa(SoundIo *soundio,
        SoundIoInputDevice *input_device, const char **data, int *frame_count)
{
    soundio_panic("TODO");
}

static void input_device_drop_alsa(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    soundio_panic("TODO");
}

static void input_device_clear_buffer_alsa(SoundIo *soundio,
        SoundIoInputDevice *input_device)
{
    soundio_panic("TODO");
}

int soundio_alsa_init(SoundIo *soundio) {
    int err;

    assert(!soundio->backend_data);
    SoundIoAlsa *sia = create<SoundIoAlsa>();
    if (!sia) {
        destroy_alsa(soundio);
        return SoundIoErrorNoMem;
    }
    soundio->backend_data = sia;
    sia->notify_fd = -1;
    sia->notify_wd = -1;
    sia->have_devices_flag.store(false);
    sia->abort_flag.test_and_set();

    sia->mutex = soundio_os_mutex_create();
    if (!sia->mutex) {
        destroy_alsa(soundio);
        return SoundIoErrorNoMem;
    }

    sia->cond = soundio_os_cond_create();
    if (!sia->cond) {
        destroy_alsa(soundio);
        return SoundIoErrorNoMem;
    }


    // set up inotify to watch /dev/snd for devices added or removed
    sia->notify_fd = inotify_init1(IN_NONBLOCK);
    if (sia->notify_fd == -1) {
        err = errno;
        assert(err != EINVAL);
        destroy_alsa(soundio);
        if (err == EMFILE || err == ENFILE) {
            return SoundIoErrorSystemResources;
        } else {
            assert(err == ENOMEM);
            return SoundIoErrorNoMem;
        }
    }

    sia->notify_wd = inotify_add_watch(sia->notify_fd, "/dev/snd", IN_CREATE | IN_DELETE);
    if (sia->notify_wd == -1) {
        err = errno;
        assert(err != EACCES);
        assert(err != EBADF);
        assert(err != EFAULT);
        assert(err != EINVAL);
        assert(err != ENAMETOOLONG);
        assert(err != ENOENT);
        destroy_alsa(soundio);
        if (err == ENOSPC) {
            return SoundIoErrorSystemResources;
        } else {
            assert(err == ENOMEM);
            return SoundIoErrorNoMem;
        }
    }

    if (pipe2(sia->notify_pipe_fd, O_NONBLOCK)) {
        assert(errno != EFAULT);
        assert(errno != EINVAL);
        assert(errno == EMFILE || errno == ENFILE);
        return SoundIoErrorSystemResources;
    }

    wakeup_device_poll(sia);

    if ((err = soundio_os_thread_create(device_thread_run, soundio, false, &sia->thread))) {
        destroy_alsa(soundio);
        return err;
    }

    soundio->destroy = destroy_alsa;
    soundio->flush_events = flush_events;
    soundio->wait_events = wait_events;
    soundio->wakeup = wakeup;

    soundio->output_device_init = output_device_init_alsa;
    soundio->output_device_destroy = output_device_destroy_alsa;
    soundio->output_device_start = output_device_start_alsa;
    soundio->output_device_free_count = output_device_free_count_alsa;
    soundio->output_device_begin_write = output_device_begin_write_alsa;
    soundio->output_device_write = output_device_write_alsa;
    soundio->output_device_clear_buffer = output_device_clear_buffer_alsa;

    soundio->input_device_init = input_device_init_alsa;
    soundio->input_device_destroy = input_device_destroy_alsa;
    soundio->input_device_start = input_device_start_alsa;
    soundio->input_device_peek = input_device_peek_alsa;
    soundio->input_device_drop = input_device_drop_alsa;
    soundio->input_device_clear_buffer = input_device_clear_buffer_alsa;

    return 0;
}
