#include "coreaudio.hpp"
#include "soundio.hpp"

static void destroy_ca(struct SoundIoPrivate *) {
    soundio_panic("TODO");
}

static void flush_events_ca(struct SoundIoPrivate *) {
    soundio_panic("TODO");
}

static void wait_events_ca(struct SoundIoPrivate *) {
    soundio_panic("TODO");
}

static void wakeup_ca(struct SoundIoPrivate *) {
    soundio_panic("TODO");
}


static int outstream_open_ca(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *) {
    soundio_panic("TODO");
}

static void outstream_destroy_ca(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *) {
    soundio_panic("TODO");
}

static int outstream_start_ca(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *) {
    soundio_panic("TODO");
}

static int outstream_begin_write_ca(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    soundio_panic("TODO");
}

static int outstream_end_write_ca(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *, int frame_count) {
    soundio_panic("TODO");
}

static int outstream_clear_buffer_ca(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *) {
    soundio_panic("TODO");
}

static int outstream_pause_ca(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *, bool pause) {
    soundio_panic("TODO");
}



static int instream_open_ca(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *) {
    soundio_panic("TODO");
}

static void instream_destroy_ca(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *) {
    soundio_panic("TODO");
}

static int instream_start_ca(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *) {
    soundio_panic("TODO");
}

static int instream_begin_read_ca(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *,
        SoundIoChannelArea **out_areas, int *frame_count)
{
    soundio_panic("TODO");
}

static int instream_end_read_ca(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *) {
    soundio_panic("TODO");
}

static int instream_pause_ca(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *, bool pause) {
    soundio_panic("TODO");
}


int soundio_coreaudio_init(SoundIoPrivate *si) {
    si->destroy = destroy_ca;
    si->flush_events = flush_events_ca;
    si->wait_events = wait_events_ca;
    si->wakeup = wakeup_ca;

    si->outstream_open = outstream_open_ca;
    si->outstream_destroy = outstream_destroy_ca;
    si->outstream_start = outstream_start_ca;
    si->outstream_begin_write = outstream_begin_write_ca;
    si->outstream_end_write = outstream_end_write_ca;
    si->outstream_clear_buffer = outstream_clear_buffer_ca;
    si->outstream_pause = outstream_pause_ca;

    si->instream_open = instream_open_ca;
    si->instream_destroy = instream_destroy_ca;
    si->instream_start = instream_start_ca;
    si->instream_begin_read = instream_begin_read_ca;
    si->instream_end_read = instream_end_read_ca;
    si->instream_pause = instream_pause_ca;

    return 0;
}
