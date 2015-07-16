/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_SOUNDIO_HPP
#define SOUNDIO_SOUNDIO_HPP

#include "soundio.h"
#include "list.hpp"

struct SoundIoDevicesInfo {
    SoundIoList<SoundIoDevice *> input_devices;
    SoundIoList<SoundIoDevice *> output_devices;
    // can be -1 when default device is unknown
    int default_output_index;
    int default_input_index;
};

struct SoundIoOutStreamPrivate {
    struct SoundIoOutStream pub;
    void *backend_data;
};

struct SoundIoInStreamPrivate {
    struct SoundIoInStream pub;
    void *backend_data;
};

struct SoundIoPrivate {
    struct SoundIo pub;

    enum SoundIoBackend current_backend;

    // safe to read without a mutex from a single thread
    struct SoundIoDevicesInfo *safe_devices_info;

    void *backend_data;
    void (*destroy)(struct SoundIoPrivate *);
    void (*flush_events)(struct SoundIoPrivate *);
    void (*wait_events)(struct SoundIoPrivate *);
    void (*wakeup)(struct SoundIoPrivate *);

    int (*outstream_open)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    void (*outstream_destroy)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    int (*outstream_start)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    int (*outstream_free_count)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    int (*outstream_begin_write)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *,
            SoundIoChannelArea **out_areas, int *frame_count);
    int (*outstream_write)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *, int frame_count);
    void (*outstream_clear_buffer)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *);
    int (*outstream_pause)(struct SoundIoPrivate *, struct SoundIoOutStreamPrivate *, bool pause);


    int (*instream_open)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    void (*instream_destroy)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    int (*instream_start)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    void (*instream_peek)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *,
            const char **data, int *frame_count);
    void (*instream_drop)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    void (*instream_clear_buffer)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *);
    int (*instream_pause)(struct SoundIoPrivate *, struct SoundIoInStreamPrivate *, bool pause);
};

void soundio_destroy_devices_info(struct SoundIoDevicesInfo *devices_info);


#endif
