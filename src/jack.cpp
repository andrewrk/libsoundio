/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "jack.hpp"
#include "soundio.hpp"
#include "atomics.hpp"

#include <jack/jack.h>
#include <stdio.h>

static atomic_flag global_msg_callback_flag = ATOMIC_FLAG_INIT;

struct SoundIoJack {
    jack_client_t *client;
};

static void flush_events_jack(struct SoundIoPrivate *) {
    soundio_panic("TODO");
}

static void wait_events_jack(struct SoundIoPrivate *) {
    soundio_panic("TODO");
}

static void wakeup_jack(struct SoundIoPrivate *) {
    soundio_panic("TODO");
}


static int outstream_open_jack(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *) {
    soundio_panic("TODO");
}

static void outstream_destroy_jack(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *) {
    soundio_panic("TODO");
}

static int outstream_start_jack(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *) {
    soundio_panic("TODO");
}

static int outstream_begin_write_jack(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    soundio_panic("TODO");
}

static int outstream_end_write_jack(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *, int frame_count) {
    soundio_panic("TODO");
}

static int outstream_clear_buffer_jack(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *) {
    soundio_panic("TODO");
}

static int outstream_pause_jack(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *, bool pause) {
    soundio_panic("TODO");
}



static int instream_open_jack(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *) {
    soundio_panic("TODO");
}

static void instream_destroy_jack(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *) {
    soundio_panic("TODO");
}

static int instream_start_jack(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *) {
    soundio_panic("TODO");
}

static int instream_begin_read_jack(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    soundio_panic("TODO");
}

static int instream_end_read_jack(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *) {
    soundio_panic("TODO");
}

static int instream_pause_jack(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *, bool pause) {
    soundio_panic("TODO");
}


static void destroy_jack(SoundIoPrivate *si) {
    SoundIoJack *sij = (SoundIoJack *)si->backend_data;
    if (!sij)
        return;

    destroy(sij);
    si->backend_data = nullptr;
}

int soundio_jack_init(struct SoundIoPrivate *si) {
    SoundIo *soundio = &si->pub;

    if (!global_msg_callback_flag.test_and_set()) {
        if (soundio->jack_error_callback)
            jack_set_error_function(soundio->jack_error_callback);
        if (soundio->jack_info_callback)
            jack_set_info_function(soundio->jack_info_callback);
        global_msg_callback_flag.clear();
    }

    assert(!si->backend_data);
    SoundIoJack *sij = create<SoundIoJack>();
    if (!sij) {
        destroy_jack(si);
        return SoundIoErrorNoMem;
    }
    si->backend_data = sij;

    jack_status_t status;
    sij->client = jack_client_open(soundio->app_name, JackNoStartServer, &status);
    if (!sij->client) {
        destroy_jack(si);
        assert(!(status & JackInvalidOption));
        if (status & JackNameNotUnique)
            return SoundIoErrorNameNotUnique;
        if (status & JackShmFailure)
            return SoundIoErrorSystemResources;
        if (status & JackNoSuchClient)
            return SoundIoErrorNoSuchClient;

        return SoundIoErrorInitAudioBackend;
    }


    si->destroy = destroy_jack;
    si->flush_events = flush_events_jack;
    si->wait_events = wait_events_jack;
    si->wakeup = wakeup_jack;

    si->outstream_open = outstream_open_jack;
    si->outstream_destroy = outstream_destroy_jack;
    si->outstream_start = outstream_start_jack;
    si->outstream_begin_write = outstream_begin_write_jack;
    si->outstream_end_write = outstream_end_write_jack;
    si->outstream_clear_buffer = outstream_clear_buffer_jack;
    si->outstream_pause = outstream_pause_jack;

    si->instream_open = instream_open_jack;
    si->instream_destroy = instream_destroy_jack;
    si->instream_start = instream_start_jack;
    si->instream_begin_read = instream_begin_read_jack;
    si->instream_end_read = instream_end_read_jack;
    si->instream_pause = instream_pause_jack;

    return 0;
}
