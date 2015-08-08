#include "wasapi.hpp"
#include "soundio.hpp"

static void flush_events_wasapi(struct SoundIoPrivate *si) {
    soundio_panic("TODO");
}

static void wait_events_wasapi(struct SoundIoPrivate *si) {
    soundio_panic("TODO");
}

static void wakeup_wasapi(struct SoundIoPrivate *si) {
    soundio_panic("TODO");
}

static void outstream_destroy_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static int outstream_open_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static int outstream_pause_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os, bool pause) {
    soundio_panic("TODO");
}

static int outstream_start_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static int outstream_begin_write_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    soundio_panic("TODO");
}

static int outstream_end_write_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}

static int outstream_clear_buffer_wasapi(struct SoundIoPrivate *si, struct SoundIoOutStreamPrivate *os) {
    soundio_panic("TODO");
}



static void instream_destroy_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int instream_open_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int instream_pause_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is, bool pause) {
    soundio_panic("TODO");
}

static int instream_start_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}

static int instream_begin_read_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    soundio_panic("TODO");
}

static int instream_end_read_wasapi(struct SoundIoPrivate *si, struct SoundIoInStreamPrivate *is) {
    soundio_panic("TODO");
}


static void destroy_wasapi(struct SoundIoPrivate *si) {
    soundio_panic("TODO");
}

int soundio_wasapi_init(SoundIoPrivate *si) {
    si->destroy = destroy_wasapi;
    si->flush_events = flush_events_wasapi;
    si->wait_events = wait_events_wasapi;
    si->wakeup = wakeup_wasapi;

    si->outstream_open = outstream_open_wasapi;
    si->outstream_destroy = outstream_destroy_wasapi;
    si->outstream_start = outstream_start_wasapi;
    si->outstream_begin_write = outstream_begin_write_wasapi;
    si->outstream_end_write = outstream_end_write_wasapi;
    si->outstream_clear_buffer = outstream_clear_buffer_wasapi;
    si->outstream_pause = outstream_pause_wasapi;

    si->instream_open = instream_open_wasapi;
    si->instream_destroy = instream_destroy_wasapi;
    si->instream_start = instream_start_wasapi;
    si->instream_begin_read = instream_begin_read_wasapi;
    si->instream_end_read = instream_end_read_wasapi;
    si->instream_pause = instream_pause_wasapi;

    return 0;
}
