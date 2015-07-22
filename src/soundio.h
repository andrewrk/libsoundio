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

#ifdef __cplusplus
extern "C"
{
#endif

enum SoundIoError {
    SoundIoErrorNone,
    SoundIoErrorNoMem,
    SoundIoErrorInitAudioBackend,
    SoundIoErrorSystemResources,
    SoundIoErrorOpeningDevice,
    SoundIoErrorInvalid,
    SoundIoErrorBackendUnavailable,
    SoundIoErrorUnderflow,
    SoundIoErrorStreaming,
    SoundIoErrorIncompatibleDevice,
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

// For your convenience, Native Endian and Foreign Endian constants are defined
// which point to the respective SoundIoFormat values.

#if defined(SOUNDIO_OS_BIG_ENDIAN)
#define SoundIoFormatS16NE SoundIoFormatS16BE
#define SoundIoFormatU16NE SoundIoFormatU16BE
#define SoundIoFormatS24NE SoundIoFormatS24BE
#define SoundIoFormatU24NE SoundIoFormatU24BE
#define SoundIoFormatS32NE SoundIoFormatS32BE
#define SoundIoFormatU32NE SoundIoFormatU32BE
#define SoundIoFormatFloat32NE SoundIoFormatFloat32BE
#define SoundIoFormatFloat64NE SoundIoFormatFloat64BE

#define SoundIoFormatS16FE SoundIoFormatS16LE
#define SoundIoFormatU16FE SoundIoFormatU16LE
#define SoundIoFormatS24FE SoundIoFormatS24LE
#define SoundIoFormatU24FE SoundIoFormatU24LE
#define SoundIoFormatS32FE SoundIoFormatS32LE
#define SoundIoFormatU32FE SoundIoFormatU32LE
#define SoundIoFormatFloat32FE SoundIoFormatFloat32LE
#define SoundIoFormatFloat64FE SoundIoFormatFloat64LE

#elif defined(SOUNDIO_OS_LITTLE_ENDIAN)

#define SoundIoFormatS16NE SoundIoFormatS16LE
#define SoundIoFormatU16NE SoundIoFormatU16LE
#define SoundIoFormatS24NE SoundIoFormatS24LE
#define SoundIoFormatU24NE SoundIoFormatU24LE
#define SoundIoFormatS32NE SoundIoFormatS32LE
#define SoundIoFormatU32NE SoundIoFormatU32LE
#define SoundIoFormatFloat32NE SoundIoFormatFloat32LE
#define SoundIoFormatFloat64NE SoundIoFormatFloat64LE

#define SoundIoFormatS16FE SoundIoFormatS16BE
#define SoundIoFormatU16FE SoundIoFormatU16BE
#define SoundIoFormatS24FE SoundIoFormatS24BE
#define SoundIoFormatU24FE SoundIoFormatU24BE
#define SoundIoFormatS32FE SoundIoFormatS32BE
#define SoundIoFormatU32FE SoundIoFormatU32BE
#define SoundIoFormatFloat32FE SoundIoFormatFloat32BE
#define SoundIoFormatFloat64FE SoundIoFormatFloat64BE

#else
#error unknown byte order
#endif

// The size of this struct is OK to use.
#define SOUNDIO_MAX_CHANNELS 24
struct SoundIoChannelLayout {
    const char *name;
    int channel_count;
    enum SoundIoChannelId channels[SOUNDIO_MAX_CHANNELS];
};

// The size of this struct is not part of the API or ABI.
struct SoundIoChannelArea {
    // Base address of buffer.
    char *ptr;
    // How many bytes it takes to get from the beginning of one sample to
    // the beginning of the next sample.
    int step;
};

// The size of this struct is not part of the API or ABI.
struct SoundIo {
    // Defaults to NULL. Put whatever you want here.
    void *userdata;
    // Optional callback. Called when the list of devices change. Only called
    // during a call to soundio_flush_events or soundio_wait_events.
    void (*on_devices_change)(struct SoundIo *);
    // Optional callback. Called from an unknown thread that you should not use
    // to call any soundio functions. You may use this to signal a condition
    // variable to wake up. Called when soundio_wait_events would be woken up.
    void (*on_events_signal)(struct SoundIo *);

    // Optional: Application name. Used by PulseAudio. Defaults to "SoundIo".
    const char *app_name;
};

// The size of this struct is not part of the API or ABI.
struct SoundIoDevice {
    // Read-only. Set automatically.
    struct SoundIo *soundio;

    // `name` uniquely identifies this device. `description` is user-friendly
    // text to describe the device. These fields are UTF-8 encoded.
    char *name;
    char *description;

    // Channel layouts are handled similarly to sample format; see those docs.
    // If this information is missing due to a `probe_error`, `layouts`
    // will be NULL. It's OK to modify this data, for example calling
    // soundio_sort_channel_layouts on it.
    // Devices are guaranteed to have at least 1 channel layout.
    struct SoundIoChannelLayout *layouts;
    int layout_count;
    struct SoundIoChannelLayout current_layout;

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
    // Devices are guaranteed to have at least 1 format available.
    enum SoundIoFormat *formats;
    int format_count;
    enum SoundIoFormat current_format;

    // Sample rate is handled very similar to sample format; see those docs.
    // If sample rate information is missing due to a probe error, the field
    // will be set to zero.
    // Devices are guaranteed to have at least 1 sample rate available.
    int sample_rate_min;
    int sample_rate_max;
    int sample_rate_current;

    // Buffer duration in seconds. If any values are unknown, they are set to
    // 0.0. These values are unknown for PulseAudio.
    double buffer_duration_min;
    double buffer_duration_max;
    double buffer_duration_current;

    // Period duration in seconds. After this much time passes, write_callback
    // is called. If values are unknown, they are set to 0.0. These values are
    // unknown for PulseAudio.
    double period_duration_min;
    double period_duration_max;
    double period_duration_current;

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

// The size of this struct is not part of the API or ABI.
struct SoundIoOutStream {
    // Populated automatically when you call soundio_outstream_create.
    struct SoundIoDevice *device;

    // Defaults to SoundIoFormatFloat32NE, followed by the first one supported.
    enum SoundIoFormat format;

    // Defaults to 48000 (and then clamped into range).
    int sample_rate;

    // Defaults to Stereo, if available, followed by the first layout supported.
    struct SoundIoChannelLayout layout;

    // Buffer duration in seconds.
    // After you call soundio_outstream_open this value is replaced with the
    // actual duration, as near to this value as possible.
    // Defaults to 1 second (and then clamped into range).
    // If the device has unknown buffer duration min and max values, you may
    // still set this. If you set this and the backend is PulseAudio, it
    // sets `PA_STREAM_ADJUST_LATENCY` and is the value used for `maxlength`
    // and `tlength`.
    double buffer_duration;

    // `period_duration` is the latency; how much time it takes
    // for a sample put in the buffer to get played.
    // After you call `soundio_outstream_open` this value is replaced with the
    // actual period duration, as near to this value as possible.
    // Defaults to `buffer_duration / 2` (and then clamped into range).
    // If the device has unknown period duration min and max values, you may
    // still set this. If you set this and the backend is PulseAudio, it
    // sets `PA_STREAM_ADJUST_LATENCY` and is the value used for `fragsize`.
    double period_duration;

    // Defaults to NULL. Put whatever you want here.
    void *userdata;
    // `err` is SoundIoErrorUnderflow or SoundIoErrorStreaming.
    // SoundIoErrorUnderflow means that the sound device ran out of buffered
    // audio data to play. You must write more data to the buffer to recover.
    // SoundIoErrorStreaming is an unrecoverable error. The stream is in an
    // invalid state and must be destroyed.
    void (*error_callback)(struct SoundIoOutStream *, int err);
    void (*write_callback)(struct SoundIoOutStream *, int requested_frame_count);

    // Name of the stream. This is used by PulseAudio. Defaults to "SoundIo".
    const char *name;


    // computed automatically when you call soundio_outstream_open
    int bytes_per_frame;
    int bytes_per_sample;

    // If setting the channel layout fails for some reason, this field is set
    // to an error code. Possible error codes are: SoundIoErrorIncompatibleDevice
    int layout_error;
};

// The size of this struct is not part of the API or ABI.
struct SoundIoInStream {
    // Populated automatically when you call soundio_outstream_create.
    struct SoundIoDevice *device;

    // Defaults to SoundIoFormatFloat32NE, followed by the first one supported.
    enum SoundIoFormat format;

    // Defaults to max(sample_rate_min, min(sample_rate_max, 48000))
    int sample_rate;

    // Defaults to Stereo, if available, followed by the first layout supported.
    struct SoundIoChannelLayout layout;

    // Buffer duration in seconds. If the captured audio frames exceeds this
    // before they are read, a buffer overrun occurs and the frames are lost.
    // Defaults to 1 second (and then clamped into range).
    double buffer_duration;

    // The latency of the captured audio. After this many seconds pass,
    // `read_callback` is called.
    // Defaults to `buffer_duration / 8`.
    double period_duration;

    // Defaults to NULL. Put whatever you want here.
    void *userdata;
    void (*read_callback)(struct SoundIoInStream *);

    // Name of the stream. This is used by PulseAudio. Defaults to "SoundIo".
    const char *name;

    // computed automatically when you call soundio_instream_open
    int bytes_per_frame;
    int bytes_per_sample;
};

// Main Context

// Create a SoundIo context. You may create multiple instances of this to
// connect to multiple backends.
struct SoundIo * soundio_create(void);
void soundio_destroy(struct SoundIo *soundio);


// Provided these backends were compiled in, this tries JACK, then PulseAudio,
// then ALSA, then CoreAudio, then ASIO, then DirectSound, then OSS, then Dummy.
int soundio_connect(struct SoundIo *soundio);
// Instead of calling `soundio_connect` you may call this function to try a
// specific backend.
int soundio_connect_backend(struct SoundIo *soundio, enum SoundIoBackend backend);
void soundio_disconnect(struct SoundIo *soundio);

const char *soundio_strerror(int error);
const char *soundio_backend_name(enum SoundIoBackend backend);

// return the number of available backends
int soundio_backend_count(struct SoundIo *soundio);
// get the available backend at the specified index (0 <= index < soundio_backend_count)
enum SoundIoBackend soundio_get_backend(struct SoundIo *soundio, int index);

// when you call this, the on_devices_change and on_events_signal callbacks
// might be called. This is the only time those callbacks will be called.
void soundio_flush_events(struct SoundIo *soundio);

// flushes events as they occur, blocks until you call soundio_wakeup
// be ready for spurious wakeups
void soundio_wait_events(struct SoundIo *soundio);

// makes soundio_wait_events stop blocking
void soundio_wakeup(struct SoundIo *soundio);



// Channel Layouts

// Returns whether the channel count field and each channel id matches in
// the supplied channel layouts.
bool soundio_channel_layout_equal(
        const struct SoundIoChannelLayout *a,
        const struct SoundIoChannelLayout *b);

const char *soundio_get_channel_name(enum SoundIoChannelId id);

int soundio_channel_layout_builtin_count(void);
const struct SoundIoChannelLayout *soundio_channel_layout_get_builtin(int index);

int soundio_channel_layout_find_channel(
        const struct SoundIoChannelLayout *layout, enum SoundIoChannelId channel);

// Populates the name field of layout if it matches a builtin one.
// returns whether it found a match
bool soundio_channel_layout_detect_builtin(struct SoundIoChannelLayout *layout);

// Iterates over preferred_layouts. Returns the first channel layout in
// preferred_layouts which matches one of the channel layouts in
// available_layouts. Returns NULL if none matches.
const struct SoundIoChannelLayout *soundio_best_matching_channel_layout(
        const struct SoundIoChannelLayout *preferred_layouts, int preferred_layout_count,
        const struct SoundIoChannelLayout *available_layouts, int available_layout_count);

// Sorts by channel count, descending.
void soundio_sort_channel_layouts(struct SoundIoChannelLayout *layouts, int layout_count);


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

int soundio_get_input_device_count(struct SoundIo *soundio);
int soundio_get_output_device_count(struct SoundIo *soundio);

// returns NULL on error
// call soundio_device_unref when you no longer have a reference to the pointer.
struct SoundIoDevice *soundio_get_input_device(struct SoundIo *soundio, int index);
struct SoundIoDevice *soundio_get_output_device(struct SoundIo *soundio, int index);

// returns the index of the default input device
// returns -1 if there are no devices.
int soundio_get_default_input_device_index(struct SoundIo *soundio);

// returns the index of the default output device
// returns -1 if there are no devices.
int soundio_get_default_output_device_index(struct SoundIo *soundio);

void soundio_device_ref(struct SoundIoDevice *device);
void soundio_device_unref(struct SoundIoDevice *device);

bool soundio_device_equal(
        const struct SoundIoDevice *a,
        const struct SoundIoDevice *b);
enum SoundIoDevicePurpose soundio_device_purpose(const struct SoundIoDevice *device);

// Sorts channel layouts by channel count, descending.
void soundio_device_sort_channel_layouts(struct SoundIoDevice *device);

// Convenience function. Returns whether `format` is included in the device's
// supported formats.
bool soundio_device_supports_format(struct SoundIoDevice *device,
        enum SoundIoFormat format);

// Convenience function. Returns whether `layout` is included in the device's
// supported channel layouts.
bool soundio_device_supports_layout(struct SoundIoDevice *device,
        const struct SoundIoChannelLayout *layout);



// Output Streams
// allocates memory and sets defaults. Next you should fill out the struct fields
// and then call soundio_outstream_open
struct SoundIoOutStream *soundio_outstream_create(struct SoundIoDevice *device);

int soundio_outstream_open(struct SoundIoOutStream *outstream);

void soundio_outstream_destroy(struct SoundIoOutStream *outstream);

int soundio_outstream_start(struct SoundIoOutStream *outstream);

int soundio_outstream_fill_with_silence(struct SoundIoOutStream *outstream);


// number of frames available to write
int soundio_outstream_free_count(struct SoundIoOutStream *outstream);

// Call this function when you are ready to begin writing to the device buffer.
//  * `outstream` - (in) The output stream you want to write to.
//  * `areas` - (out) The memory addresses you can write data to. It is OK to
//     modify the pointers if that helps you iterate.
//  * `frame_count` - (in/out) Provide the number of frames you want to write.
//    Returned will be the number of frames you actually can write.
// It is your responsibility to call this function no more and no fewer than the
// correct number of times as determined by `requested_frame_count` from
// `write_callback`. See sine.c for an example.
int soundio_outstream_begin_write(struct SoundIoOutStream *outstream,
        struct SoundIoChannelArea **areas, int *frame_count);

int soundio_outstream_write(struct SoundIoOutStream *outstream, int frame_count);

void soundio_outstream_clear_buffer(struct SoundIoOutStream *outstream);

// If the underyling device supports pausing, this pauses the stream and
// prevents `write_callback` from being called. Otherwise this returns
// `SoundIoErrorIncompatibleDevice`.
int soundio_outstream_pause(struct SoundIoOutStream *outstream, bool pause);




// Input Streams
// allocates memory and sets defaults. Next you should fill out the struct fields
// and then call soundio_instream_open
struct SoundIoInStream *soundio_instream_create(struct SoundIoDevice *device);
void soundio_instream_destroy(struct SoundIoInStream *instream);

int soundio_instream_open(struct SoundIoInStream *instream);

int soundio_instream_start(struct SoundIoInStream *instream);

void soundio_instream_peek(struct SoundIoInStream *instream,
        const char **data, int *out_frame_count);
// this will drop all of the frames from when you called soundio_instream_peek
void soundio_instream_drop(struct SoundIoInStream *instream);

void soundio_instream_clear_buffer(struct SoundIoInStream *instream);

// If the underyling device supports pausing, this pauses the stream and
// prevents `read_callback` from being called. Otherwise this returns
// `SoundIoErrorIncompatibleDevice`.
int soundio_instream_pause(struct SoundIoInStream *instream, bool pause);


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
#endif

#endif
