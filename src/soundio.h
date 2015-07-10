/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_SOUNDIO_H
#define SOUNDIO_SOUNDIO_H

#include "config.h"
#include <stdbool.h>

#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)

#define SOUNDIO_OS_BIG_ENDIAN

#elif (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || \
    defined(_WIN32)

#define SOUNDIO_OS_LITTLE_ENDIAN

#else
#error unknown byte order
#endif


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

struct SoundIo;
struct SoundIoDevicesInfo;

enum SoundIoError {
    SoundIoErrorNone,
    SoundIoErrorNoMem,
    SoundIoErrorInitAudioBackend,
    SoundIoErrorSystemResources,
    SoundIoErrorOpeningDevice,
};

enum SoundIoChannelId {
    SoundIoChannelIdInvalid,
    SoundIoChannelIdFrontLeft,
    SoundIoChannelIdFrontRight,
    SoundIoChannelIdFrontCenter,
    SoundIoChannelIdLfe,
    SoundIoChannelIdBackLeft,
    SoundIoChannelIdBackRight,
    SoundIoChannelIdFrontLeftCenter,
    SoundIoChannelIdFrontRightCenter,
    SoundIoChannelIdBackCenter,
    SoundIoChannelIdSideLeft,
    SoundIoChannelIdSideRight,
    SoundIoChannelIdTopCenter,
    SoundIoChannelIdTopFrontLeft,
    SoundIoChannelIdTopFrontCenter,
    SoundIoChannelIdTopFrontRight,
    SoundIoChannelIdTopBackLeft,
    SoundIoChannelIdTopBackCenter,
    SoundIoChannelIdTopBackRight,

    SoundIoChannelIdBackLeftCenter,
    SoundIoChannelIdBackRightCenter,
    SoundIoChannelIdFrontLeftWide,
    SoundIoChannelIdFrontRightWide,
    SoundIoChannelIdFrontLeftHigh,
    SoundIoChannelIdFrontCenterHigh,
    SoundIoChannelIdFrontRightHigh,
    SoundIoChannelIdTopFrontLeftCenter,
    SoundIoChannelIdTopFrontRightCenter,
    SoundIoChannelIdTopSideLeft,
    SoundIoChannelIdTopSideRight,
    SoundIoChannelIdLeftLfe,
    SoundIoChannelIdRightLfe,
    SoundIoChannelIdBottomCenter,
    SoundIoChannelIdBottomLeftCenter,
    SoundIoChannelIdBottomRightCenter,

    SoundIoChannelIdCount,
};

#define SOUNDIO_MAX_CHANNELS 32
struct SoundIoChannelLayout {
    const char *name;
    int channel_count;
    enum SoundIoChannelId channels[SOUNDIO_MAX_CHANNELS];
};

enum SoundIoChannelLayoutId {
    SoundIoChannelLayoutIdMono,
    SoundIoChannelLayoutIdStereo,
    SoundIoChannelLayoutId2Point1,
    SoundIoChannelLayoutId3Point0,
    SoundIoChannelLayoutId3Point0Back,
    SoundIoChannelLayoutId3Point1,
    SoundIoChannelLayoutId4Point0,
    SoundIoChannelLayoutId4Point1,
    SoundIoChannelLayoutIdQuad,
    SoundIoChannelLayoutIdQuadSide,
    SoundIoChannelLayoutId5Point0,
    SoundIoChannelLayoutId5Point0Back,
    SoundIoChannelLayoutId5Point1,
    SoundIoChannelLayoutId5Point1Back,
    SoundIoChannelLayoutId6Point0,
    SoundIoChannelLayoutId6Point0Front,
    SoundIoChannelLayoutIdHexagonal,
    SoundIoChannelLayoutId6Point1,
    SoundIoChannelLayoutId6Point1Back,
    SoundIoChannelLayoutId6Point1Front,
    SoundIoChannelLayoutId7Point0,
    SoundIoChannelLayoutId7Point0Front,
    SoundIoChannelLayoutId7Point1,
    SoundIoChannelLayoutId7Point1Wide,
    SoundIoChannelLayoutId7Point1WideBack,
    SoundIoChannelLayoutIdOctagonal,
};

enum SoundIoBackend {
    SoundIoBackendNone,
    SoundIoBackendPulseAudio,
    SoundIoBackendAlsa,
    SoundIoBackendDummy,
};

enum SoundIoDevicePurpose {
    SoundIoDevicePurposeInput,
    SoundIoDevicePurposeOutput,
};

enum SoundIoFormat {
    SoundIoFormatInvalid,
    SoundIoFormatS8,          // Signed 8 bit
    SoundIoFormatU8,          // Unsigned 8 bit
    SoundIoFormatS16LE,       // Signed 16 bit Little Endian
    SoundIoFormatS16BE,       // Signed 16 bit Big Endian
    SoundIoFormatU16LE,       // Unsigned 16 bit Little Endian
    SoundIoFormatU16BE,       // Unsigned 16 bit Little Endian
    SoundIoFormatS24LE,       // Signed 24 bit Little Endian using low three bytes in 32-bit word
    SoundIoFormatS24BE,       // Signed 24 bit Big Endian using low three bytes in 32-bit word
    SoundIoFormatU24LE,       // Unsigned 24 bit Little Endian using low three bytes in 32-bit word
    SoundIoFormatU24BE,       // Unsigned 24 bit Big Endian using low three bytes in 32-bit word
    SoundIoFormatS32LE,       // Signed 32 bit Little Endian
    SoundIoFormatS32BE,       // Signed 32 bit Big Endian
    SoundIoFormatU32LE,       // Unsigned 32 bit Little Endian
    SoundIoFormatU32BE,       // Unsigned 32 bit Big Endian
    SoundIoFormatFloat32LE,   // Float 32 bit Little Endian, Range -1.0 to 1.0
    SoundIoFormatFloat32BE,   // Float 32 bit Big Endian, Range -1.0 to 1.0
    SoundIoFormatFloat64LE,   // Float 64 bit Little Endian, Range -1.0 to 1.0
    SoundIoFormatFloat64BE,   // Float 64 bit Big Endian, Range -1.0 to 1.0
};

#if defined(SOUNDIO_OS_BIG_ENDIAN)
#define SoundIoFormatS16NE SoundIoFormatS16BE
#define SoundIoFormatU16NE SoundIoFormatU16BE
#define SoundIoFormatS24NE SoundIoFormatS24BE
#define SoundIoFormatU24NE SoundIoFormatU24BE
#define SoundIoFormatS32NE SoundIoFormatS32BE
#define SoundIoFormatU32NE SoundIoFormatU32BE
#define SoundIoFormatFloat32NE SoundIoFormatFloat32BE
#define SoundIoFormatFloat64NE SoundIoFormatFloat64BE
#elif defined(SOUNDIO_OS_LITTLE_ENDIAN)
#define SoundIoFormatS16NE SoundIoFormatS16LE
#define SoundIoFormatU16NE SoundIoFormatU16LE
#define SoundIoFormatS24NE SoundIoFormatS24LE
#define SoundIoFormatU24NE SoundIoFormatU24LE
#define SoundIoFormatS32NE SoundIoFormatS32LE
#define SoundIoFormatU32NE SoundIoFormatU32LE
#define SoundIoFormatFloat32NE SoundIoFormatFloat32LE
#define SoundIoFormatFloat64NE SoundIoFormatFloat64LE
#endif

struct SoundIoDevice {
    struct SoundIo *soundio;

    // `name` uniquely identifies this device. `description` is user-friendly
    // text to describe the device. These fields are UTF-8 encoded.
    char *name;
    char *description;

    // If this information is missing due to a `probe_error`, the number of
    // channels will be zero.
    struct SoundIoChannelLayout channel_layout;

    // A device is either a raw device or it is a virtual device that is
    // provided by a software mixing service such as dmix or PulseAudio (see
    // `is_raw`). If it is a raw device, `current_format` is meaningless;
    // the device has no current format until you open it. On the other hand,
    // if it is a virtual device, `current_format` describes the destination
    // sample format that your audio will be converted to. Or, if you're the
    // lucky first application to open the device, you might cause the
    // `current_format` to change to your format. Generally, you want to
    // ignore `current_format` and use whatever format is most convenient
    // for you which is supported by the device, because when you are the only
    // application left, the mixer might decide to switch `current_format` to
    // yours. You can learn the supported formats via `formats` and
    // `format_count`. If this information is missing due to a probe error,
    // `formats` will be `NULL`. If `current_format` is unavailable, it will be
    // set to `SoundIoFormatInvalid`.
    enum SoundIoFormat *formats;
    int format_count;
    enum SoundIoFormat current_format;

    // Sample rate is handled very similar to sample format; see those docs.
    // If sample rate information is missing due to a probe error, the field
    // will be set to zero.
    int sample_rate_min;
    int sample_rate_max;
    int sample_rate_current;

    double default_latency;

    // Tells whether this device is an input device or an output device.
    enum SoundIoDevicePurpose purpose;

    // raw means that you are directly opening the hardware device and not
    // going through a proxy such as dmix or PulseAudio. When you open a raw
    // device, other applications on the computer are not able to
    // simultaneously access the device. Raw devices do not perform automatic
    // resampling and thus tend to have fewer formats available.
    bool is_raw;

    // Devices are reference counted. See `soundio_device_ref` and
    // `soundio_device_unref`.
    int ref_count;

    // This is set to a SoundIoError representing the result of the device
    // probe. Ideally this will be SoundIoErrorNone in which case all the
    // fields of the device will be populated. If there is an error code here
    // then information about formats, sample rates, and channel layouts might
    // be missing.
    int probe_error;
};

struct SoundIoOutStream {
    void *backend_data;
    struct SoundIoDevice *device;
    enum SoundIoFormat format;
    int sample_rate;
    double latency;
    int bytes_per_frame;

    void *userdata;
    void (*underrun_callback)(struct SoundIoOutStream *);
    void (*write_callback)(struct SoundIoOutStream *, int frame_count);
};

struct SoundIoInStream {
    void *backend_data;
    struct SoundIoDevice *device;
    enum SoundIoFormat format;
    int sample_rate;
    double latency;
    int bytes_per_frame;

    void *userdata;
    void (*read_callback)(struct SoundIoInStream *);
};

struct SoundIo {
    enum SoundIoBackend current_backend;

    // safe to read without a mutex from a single thread
    struct SoundIoDevicesInfo *safe_devices_info;

    void *userdata;
    void (*on_devices_change)(struct SoundIo *);
    void (*on_events_signal)(struct SoundIo *);


    void *backend_data;
    void (*destroy)(struct SoundIo *);
    void (*flush_events)(struct SoundIo *);
    void (*wait_events)(struct SoundIo *);
    void (*wakeup)(struct SoundIo *);

    int (*out_stream_init)(struct SoundIo *, struct SoundIoOutStream *);
    void (*out_stream_destroy)(struct SoundIo *, struct SoundIoOutStream *);
    int (*out_stream_start)(struct SoundIo *, struct SoundIoOutStream *);
    int (*out_stream_free_count)(struct SoundIo *, struct SoundIoOutStream *);
    void (*out_stream_begin_write)(struct SoundIo *, struct SoundIoOutStream *,
            char **data, int *frame_count);
    void (*out_stream_write)(struct SoundIo *, struct SoundIoOutStream *,
            char *data, int frame_count);
    void (*out_stream_clear_buffer)(struct SoundIo *, struct SoundIoOutStream *);


    int (*in_stream_init)(struct SoundIo *, struct SoundIoInStream *);
    void (*in_stream_destroy)(struct SoundIo *, struct SoundIoInStream *);
    int (*in_stream_start)(struct SoundIo *, struct SoundIoInStream *);
    void (*in_stream_peek)(struct SoundIo *, struct SoundIoInStream *,
            const char **data, int *frame_count);
    void (*in_stream_drop)(struct SoundIo *, struct SoundIoInStream *);
    void (*in_stream_clear_buffer)(struct SoundIo *, struct SoundIoInStream *);
};

// Main Context

// Create a SoundIo context.
// Returns an error code.
struct SoundIo * soundio_create(void);
void soundio_destroy(struct SoundIo *soundio);

int soundio_connect(struct SoundIo *soundio);
void soundio_disconnect(struct SoundIo *soundio);

const char *soundio_strerror(int error);
const char *soundio_backend_name(enum SoundIoBackend backend);

// when you call this, the on_devices_change and on_events_signal callbacks
// might be called. This is the only time those functions will be called.
void soundio_flush_events(struct SoundIo *soundio);

// flushes events as they occur, blocks until you call soundio_wakeup
// be ready for spurious wakeups
void soundio_wait_events(struct SoundIo *soundio);

// makes soundio_wait_events stop blocking
void soundio_wakeup(struct SoundIo *soundio);



// Channel Layouts

bool soundio_channel_layout_equal(const struct SoundIoChannelLayout *a,
        const struct SoundIoChannelLayout *b);

const char *soundio_get_channel_name(enum SoundIoChannelId id);

int soundio_channel_layout_builtin_count(void);
const struct SoundIoChannelLayout *soundio_channel_layout_get_builtin(int index);

void soundio_debug_print_channel_layout(const struct SoundIoChannelLayout *layout);

int soundio_channel_layout_find_channel(
        const struct SoundIoChannelLayout *layout, enum SoundIoChannelId channel);

// merely populates the name field of layout if it matches a builtin one.
// returns whether it found a match
bool soundio_channel_layout_detect_builtin(struct SoundIoChannelLayout *layout);



// Sample Formats

int soundio_get_bytes_per_sample(enum SoundIoFormat format);

static inline int soundio_get_bytes_per_frame(enum SoundIoFormat format, int channel_count) {
    return soundio_get_bytes_per_sample(format) * channel_count;
}

static inline int soundio_get_bytes_per_second(enum SoundIoFormat format,
        int channel_count, int sample_rate)
{
    return soundio_get_bytes_per_frame(format, channel_count) * sample_rate;
}

const char * soundio_format_string(enum SoundIoFormat format);




// Devices

// returns -1 on error
int soundio_get_input_device_count(struct SoundIo *soundio);
int soundio_get_output_device_count(struct SoundIo *soundio);

// returns NULL on error
// call soundio_device_unref when you no longer have a reference to the pointer.
struct SoundIoDevice *soundio_get_input_device(struct SoundIo *soundio, int index);
struct SoundIoDevice *soundio_get_output_device(struct SoundIo *soundio, int index);

// returns the index of the default input device, or -1 on error
int soundio_get_default_input_device_index(struct SoundIo *soundio);

// returns the index of the default output device, or -1 on error
int soundio_get_default_output_device_index(struct SoundIo *soundio);

void soundio_device_ref(struct SoundIoDevice *device);
void soundio_device_unref(struct SoundIoDevice *device);

bool soundio_device_equal(
        const struct SoundIoDevice *a,
        const struct SoundIoDevice *b);
enum SoundIoDevicePurpose soundio_device_purpose(const struct SoundIoDevice *device);



// Output Devices

int soundio_out_stream_create(struct SoundIoDevice *device,
        enum SoundIoFormat format, int sample_rate,
        double latency, void *userdata,
        void (*write_callback)(struct SoundIoOutStream *, int frame_count),
        void (*underrun_callback)(struct SoundIoOutStream *),
        struct SoundIoOutStream **out_out_stream);
void soundio_out_stream_destroy(struct SoundIoOutStream *out_stream);

int soundio_out_stream_start(struct SoundIoOutStream *out_stream);

void soundio_out_stream_fill_with_silence(struct SoundIoOutStream *out_stream);


// number of frames available to write
int soundio_out_stream_free_count(struct SoundIoOutStream *out_stream);
void soundio_out_stream_begin_write(struct SoundIoOutStream *out_stream,
        char **data, int *frame_count);
void soundio_out_stream_write(struct SoundIoOutStream *out_stream,
        char *data, int frame_count);

void soundio_out_stream_clear_buffer(struct SoundIoOutStream *out_stream);



// Input Devices

int soundio_in_stream_create(struct SoundIoDevice *device,
        enum SoundIoFormat format, int sample_rate,
        double latency, void *userdata,
        void (*read_callback)(struct SoundIoInStream *),
        struct SoundIoInStream **out_in_stream);
void soundio_in_stream_destroy(struct SoundIoInStream *in_stream);

int soundio_in_stream_start(struct SoundIoInStream *in_stream);

void soundio_in_stream_peek(struct SoundIoInStream *in_stream,
        const char **data, int *out_frame_count);
// this will drop all of the frames from when you called soundio_in_stream_peek
void soundio_in_stream_drop(struct SoundIoInStream *in_stream);

void soundio_in_stream_clear_buffer(struct SoundIoInStream *in_stream);


// Ring Buffer
struct SoundIoRingBuffer;
struct SoundIoRingBuffer *soundio_ring_buffer_create(struct SoundIo *soundio, int requested_capacity);
void soundio_ring_buffer_destroy(struct SoundIoRingBuffer *ring_buffer);
int soundio_ring_buffer_capacity(struct SoundIoRingBuffer *ring_buffer);

// don't write more than capacity
char *soundio_ring_buffer_write_ptr(struct SoundIoRingBuffer *ring_buffer);
void soundio_ring_buffer_advance_write_ptr(struct SoundIoRingBuffer *ring_buffer, int count);

// don't read more than capacity
char *soundio_ring_buffer_read_ptr(struct SoundIoRingBuffer *ring_buffer);
void soundio_ring_buffer_advance_read_ptr(struct SoundIoRingBuffer *ring_buffer, int count);

// how much of the buffer is used, ready for reading
int soundio_ring_buffer_fill_count(struct SoundIoRingBuffer *ring_buffer);

// how much is available, ready for writing
int soundio_ring_buffer_free_count(struct SoundIoRingBuffer *ring_buffer);

// must be called by the writer
void soundio_ring_buffer_clear(struct SoundIoRingBuffer *ring_buffer);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
