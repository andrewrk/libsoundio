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
    SoundIoErrorStreaming,
    SoundIoErrorIncompatibleDevice,
    SoundIoErrorNoSuchClient,
    SoundIoErrorIncompatibleBackend,
    SoundIoErrorBackendDisconnected,
    SoundIoErrorInterrupted,
    SoundIoErrorUnderflow,
};

enum SoundIoChannelId {
    // These channel ids are more commonly supported.
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

    // These channel ids are less commonly supported.
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
    SoundIoChannelLayoutIdQuad,
    SoundIoChannelLayoutIdQuadSide,
    SoundIoChannelLayoutId4Point1,
    SoundIoChannelLayoutId5Point0Back,
    SoundIoChannelLayoutId5Point0Side,
    SoundIoChannelLayoutId5Point1,
    SoundIoChannelLayoutId5Point1Back,
    SoundIoChannelLayoutId6Point0Side,
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
    SoundIoBackendJack,
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
    SoundIoFormatS8,        // Signed 8 bit
    SoundIoFormatU8,        // Unsigned 8 bit
    SoundIoFormatS16LE,     // Signed 16 bit Little Endian
    SoundIoFormatS16BE,     // Signed 16 bit Big Endian
    SoundIoFormatU16LE,     // Unsigned 16 bit Little Endian
    SoundIoFormatU16BE,     // Unsigned 16 bit Little Endian
    SoundIoFormatS24LE,     // Signed 24 bit Little Endian using low three bytes in 32-bit word
    SoundIoFormatS24BE,     // Signed 24 bit Big Endian using low three bytes in 32-bit word
    SoundIoFormatU24LE,     // Unsigned 24 bit Little Endian using low three bytes in 32-bit word
    SoundIoFormatU24BE,     // Unsigned 24 bit Big Endian using low three bytes in 32-bit word
    SoundIoFormatS32LE,     // Signed 32 bit Little Endian
    SoundIoFormatS32BE,     // Signed 32 bit Big Endian
    SoundIoFormatU32LE,     // Unsigned 32 bit Little Endian
    SoundIoFormatU32BE,     // Unsigned 32 bit Big Endian
    SoundIoFormatFloat32LE, // Float 32 bit Little Endian, Range -1.0 to 1.0
    SoundIoFormatFloat32BE, // Float 32 bit Big Endian, Range -1.0 to 1.0
    SoundIoFormatFloat64LE, // Float 64 bit Little Endian, Range -1.0 to 1.0
    SoundIoFormatFloat64BE, // Float 64 bit Big Endian, Range -1.0 to 1.0
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
    // Optional. Put whatever you want here. Defaults to NULL.
    void *userdata;
    // Optional callback. Called when the list of devices change. Only called
    // during a call to soundio_flush_events or soundio_wait_events.
    void (*on_devices_change)(struct SoundIo *);
    // Optional callback. Called when the backend disconnects. For example,
    // when the JACK server shuts down. When this happens, listing devices
    // and opening streams will always fail with
    // SoundIoErrorBackendDisconnected. This callback is only called during a
    // call to soundio_flush_events or soundio_wait_events.
    void (*on_backend_disconnect)(struct SoundIo *, int err);
    // Optional callback. Called from an unknown thread that you should not use
    // to call any soundio functions. You may use this to signal a condition
    // variable to wake up. Called when soundio_wait_events would be woken up.
    void (*on_events_signal)(struct SoundIo *);

    // Optional: Application name.
    // PulseAudio uses this for "application name".
    // JACK uses this for `client_name`.
    // Must not contain a colon (":").
    const char *app_name;

    // Optional: JACK info and error callbacks.
    // By default, libsoundio sets these to empty functions in order to
    // silence stdio messages from JACK. You may override the behavior by
    // setting these to `NULL` or providing your own function. These are
    // registered with JACK regardless of whether `soundio_connect_backend`
    // succeeds.
    void (*jack_info_callback)(const char *msg);
    void (*jack_error_callback)(const char *msg);

    // Read-only. After calling `soundio_connect` or `soundio_connect_backend`,
    // this field tells which backend is currently connected.
    enum SoundIoBackend current_backend;
};

// The size of this struct is not part of the API or ABI.
struct SoundIoDevice {
    // Read-only. Set automatically.
    struct SoundIo *soundio;

    // `id` is a string of bytes that uniquely identifies this device.
    // `name` is user-friendly UTF-8 encoded text to describe the device.
    char *id;
    char *name;

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

    // Sample rate is the number of frames per second.
    // Sample rate is handled very similar to sample format; see those docs.
    // If sample rate information is missing due to a probe error, the field
    // will be set to zero.
    // Devices are guaranteed to have at least 1 sample rate available.
    int sample_rate_min;
    int sample_rate_max;
    int sample_rate_current;

    // Buffer duration in seconds. If `buffer_duration_current` is unknown or
    // irrelevant, it is set to 0.0.
    // PulseAudio allows any value and so reasonable min/max of 0.10 and 4.0
    // are used. You may check that the current backend is PulseAudio and
    // ignore these min/max values.
    // For JACK, buffer duration and period duration are the same.
    double buffer_duration_min;
    double buffer_duration_max;
    double buffer_duration_current;

    // Period duration in seconds. After this much time passes, write_callback
    // is called. If values are unknown, they are set to 0.0. These values are
    // meaningless for PulseAudio. For JACK, buffer duration and period duration
    // are the same.
    double period_duration_min;
    double period_duration_max;
    double period_duration_current;

    // Tells whether this device is an input device or an output device.
    enum SoundIoDevicePurpose purpose;

    // Raw means that you are directly opening the hardware device and not
    // going through a proxy such as dmix, PulseAudio, or JACK. When you open a
    // raw device, other applications on the computer are not able to
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

    // Sample rate is the number of frames per second.
    // Defaults to 48000 (and then clamped into range).
    int sample_rate;

    // Defaults to Stereo, if available, followed by the first layout supported.
    struct SoundIoChannelLayout layout;

    // Buffer duration in seconds.
    // After you call `soundio_outstream_open` this value is replaced with the
    // actual duration, as near to this value as possible.
    // Defaults to 1 second (and then clamped into range).
    // If the device has unknown buffer duration min and max values, you may
    // still set this. If you set this and the backend is PulseAudio, it
    // sets `PA_STREAM_ADJUST_LATENCY` and is the value used for `maxlength`
    // and `tlength`. With PulseAudio, this value is not replaced with the
    // actual duration until `soundio_outstream_start`.
    double buffer_duration;

    // `period_duration` is the latency; how much time it takes
    // for a sample put in the buffer to get played.
    // After you call `soundio_outstream_open` this value is replaced with the
    // actual period duration, as near to this value as possible.
    // Defaults to `buffer_duration / 2` (and then clamped into range).
    // If the device has unknown period duration min and max values, you may
    // still set this. This value is meaningless for PulseAudio.
    double period_duration;

    // Defaults to NULL. Put whatever you want here.
    void *userdata;
    // In this callback, you call `soundio_outstream_begin_write` and
    // `soundio_outstream_end_write`. `requested_frame_count` will always be
    // greater than 0.
    void (*write_callback)(struct SoundIoOutStream *, int requested_frame_count);
    // This optional callback happens when the sound device runs out of buffered
    // audio data to play. After this occurs, the outstream waits until the
    // buffer is full to resume playback.
    // This is called from the `write_callback` thread context.
    void (*underflow_callback)(struct SoundIoOutStream *);
    // Optional callback. `err` is always SoundIoErrorStreaming.
    // SoundIoErrorStreaming is an unrecoverable error. The stream is in an
    // invalid state and must be destroyed.
    // If you do not supply `error_callback`, the default callback will print
    // a message to stderr and then call `abort`.
    // This is called from the `write_callback` thread context.
    void (*error_callback)(struct SoundIoOutStream *, int err);

    // Optional: Name of the stream. Defaults to "SoundIoOutStream"
    // PulseAudio uses this for the stream name.
    // JACK uses this for the client name of the client that connects when you
    // open the stream.
    // Must not contain a colon (":").
    const char *name;

    // Optional: Hint that this output stream is nonterminal. This is used by
    // JACK and it means that the output stream data originates from an input
    // stream. Defaults to `false`.
    bool non_terminal_hint;


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

    // Sample rate is the number of frames per second.
    // Defaults to max(sample_rate_min, min(sample_rate_max, 48000))
    int sample_rate;

    // Defaults to Stereo, if available, followed by the first layout supported.
    struct SoundIoChannelLayout layout;

    // Buffer duration in seconds.
    // After you call `soundio_instream_open` this value is replaced with the
    // actual duration, as near to this value as possible.
    // If the captured audio frames exceeds this before they are read, a buffer
    // overrun occurs and the frames are lost.
    // Defaults to 1 second (and then clamped into range). For PulseAudio,
    // defaults to PulseAudio's default value, usually large. If you set this
    // and the backend is PulseAudio, it sets `PA_STREAM_ADJUST_LATENCY` and
    // is the value used for `maxlength`. With PulseAudio, this value is not
    // replaced with the actual duration until `soundio_instream_start`.
    double buffer_duration;

    // The latency of the captured audio.
    // After you call `soundio_instream_open` this value is replaced with the
    // actual duration, as near to this value as possible.
    // After this many seconds pass, `read_callback` is called.
    // Defaults to `buffer_duration / 8`.
    // If you set this and the backend is PulseAudio, it sets
    // `PA_STREAM_ADJUST_LATENCY` and is the value used for `fragsize`.
    // With PulseAudio, this value is not replaced with the actual duration
    // until `soundio_instream_start`.
    double period_duration;

    // Defaults to NULL. Put whatever you want here.
    void *userdata;
    // In this function call `soundio_instream_begin_read` and
    // `soundio_instream_end_read`.
    void (*read_callback)(struct SoundIoInStream *, int available_frame_count);
    // Optional callback. `err` is always SoundIoErrorStreaming.
    // SoundIoErrorStreaming is an unrecoverable error. The stream is in an
    // invalid state and must be destroyed.
    // If you do not supply `error_callback`, the default callback will print
    // a message to stderr and then abort().
    // This is called from the `read_callback` thread context.
    void (*error_callback)(struct SoundIoInStream *, int err);

    // Optional: Name of the stream. Defaults to "SoundIoInStream";
    // PulseAudio uses this for the stream name.
    // JACK uses this for the client name of the client that connects when you
    // open the stream.
    // Must not contain a colon (":").
    const char *name;

    // Optional: Hint that this input stream is nonterminal. This is used by
    // JACK and it means that the data received by the stream will be
    // passed on or made available to another stream. Defaults to `false`.
    // stream. Defaults to `false`.
    bool non_terminal_hint;

    // computed automatically when you call soundio_instream_open
    int bytes_per_frame;
    int bytes_per_sample;

    // If setting the channel layout fails for some reason, this field is set
    // to an error code. Possible error codes are: SoundIoErrorIncompatibleDevice
    int layout_error;
};

// Main Context

// Create a SoundIo context. You may create multiple instances of this to
// connect to multiple backends.
struct SoundIo * soundio_create(void);
void soundio_destroy(struct SoundIo *soundio);


// This is a convenience function you could implement yourself if you wanted
// to. It tries `soundio_connect_backend` on all available backends in order.
int soundio_connect(struct SoundIo *soundio);
// Instead of calling `soundio_connect` you may call this function to try a
int soundio_connect_backend(struct SoundIo *soundio, enum SoundIoBackend backend);
void soundio_disconnect(struct SoundIo *soundio);

const char *soundio_strerror(int error);
const char *soundio_backend_name(enum SoundIoBackend backend);

// Returns the number of available backends.
int soundio_backend_count(struct SoundIo *soundio);
// get the available backend at the specified index
// (0 <= index < `soundio_backend_count`)
enum SoundIoBackend soundio_get_backend(struct SoundIo *soundio, int index);

// Returns whether libsoundio was compiled with `backend`.
bool soundio_have_backend(enum SoundIoBackend backend);

// When you call this, the `on_devices_change` and `on_events_signal` callbacks
// might be called. This is the only time those callbacks will be called.
// This must be called from the same thread as the thread in which you call
// these functions:
// * `soundio_input_device_count`
// * `soundio_output_device_count`
// * `soundio_get_input_device`
// * `soundio_get_output_device`
// * `soundio_default_input_device_index`
// * `soundio_default_output_device_index`
void soundio_flush_events(struct SoundIo *soundio);

// This function calls `soundio_flush_events` then blocks until another event
// is ready or you call `soundio_wakeup`. Be ready for spurious wakeups.
void soundio_wait_events(struct SoundIo *soundio);

// Makes `soundio_wait_events` stop blocking.
void soundio_wakeup(struct SoundIo *soundio);



// Channel Layouts

// Returns whether the channel count field and each channel id matches in
// the supplied channel layouts.
bool soundio_channel_layout_equal(
        const struct SoundIoChannelLayout *a,
        const struct SoundIoChannelLayout *b);

const char *soundio_get_channel_name(enum SoundIoChannelId id);
// Given UTF-8 encoded text which is the name of a channel such as
// "Front Left", "FL", or "front-left", return the corresponding
// SoundIoChannelId. Returns SoundIoChannelIdInvalid for no match.
enum SoundIoChannelId soundio_parse_channel_id(const char *str, int str_len);

int soundio_channel_layout_builtin_count(void);
const struct SoundIoChannelLayout *soundio_channel_layout_get_builtin(int index);

// Get the default builtin channel layout for the given number of channels.
const struct SoundIoChannelLayout *soundio_channel_layout_get_default(int channel_count);

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

// Returns -1 on invalid format.
int soundio_get_bytes_per_sample(enum SoundIoFormat format);

static inline int soundio_get_bytes_per_frame(enum SoundIoFormat format, int channel_count) {
    return soundio_get_bytes_per_sample(format) * channel_count;
}

// Sample rate is the number of frames per second.
static inline int soundio_get_bytes_per_second(enum SoundIoFormat format,
        int channel_count, int sample_rate)
{
    return soundio_get_bytes_per_frame(format, channel_count) * sample_rate;
}

const char * soundio_format_string(enum SoundIoFormat format);




// Devices

int soundio_input_device_count(struct SoundIo *soundio);
int soundio_output_device_count(struct SoundIo *soundio);

// Always returns a device. Call soundio_device_unref when done.
// `index` must be 0 <= index < soundio_input_device_count
struct SoundIoDevice *soundio_get_input_device(struct SoundIo *soundio, int index);
// Always returns a device. Call soundio_device_unref when done.
// `index` must be 0 <= index < soundio_output_device_count
struct SoundIoDevice *soundio_get_output_device(struct SoundIo *soundio, int index);

// returns the index of the default input device
// returns -1 if there are no devices.
int soundio_default_input_device_index(struct SoundIo *soundio);

// returns the index of the default output device
// returns -1 if there are no devices.
int soundio_default_output_device_index(struct SoundIo *soundio);

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
// Allocates memory and sets defaults. Next you should fill out the struct fields
// and then call `soundio_outstream_open`.
struct SoundIoOutStream *soundio_outstream_create(struct SoundIoDevice *device);
// You may not call this function from the `write_callback` thread context.
void soundio_outstream_destroy(struct SoundIoOutStream *outstream);

// After you call this function, `buffer_duration` and `period_duration` are
// set to the correct values, if available.
// The next thing to do is call `soundio_instream_start`.
int soundio_outstream_open(struct SoundIoOutStream *outstream);

// After you call this function, `write_callback` will be called.
int soundio_outstream_start(struct SoundIoOutStream *outstream);

// Call this function when you are ready to begin writing to the device buffer.
//  * `outstream` - (in) The output stream you want to write to.
//  * `areas` - (out) The memory addresses you can write data to. It is OK to
//     modify the pointers if that helps you iterate.
//  * `frame_count` - (in/out) Provide the number of frames you want to write.
//    Returned will be the number of frames you actually can write. Must be
//    greater than 0 frames.
// It is your responsibility to call this function no more and no fewer than the
// correct number of times as determined by `requested_frame_count` from
// `write_callback`. See sine.c for an example.
// You must call this function only from the `write_callback` thread context.
// After calling this function, write data to `areas` and then call `soundio_outstream_end_write`.
int soundio_outstream_begin_write(struct SoundIoOutStream *outstream,
        struct SoundIoChannelArea **areas, int *frame_count);

// Commits the write that you began with `soundio_outstream_begin_write`.
// You must call this function only from the `write_callback` thread context.
// This function might return `SoundIoErrorUnderflow` but don't count on it.
int soundio_outstream_end_write(struct SoundIoOutStream *outstream, int frame_count);

// Clears the output stream buffer.
// You must call this function only from the `write_callback` thread context.
int soundio_outstream_clear_buffer(struct SoundIoOutStream *outstream);

// If the underyling device supports pausing, this pauses the stream and
// prevents `write_callback` from being called. Otherwise this returns
// `SoundIoErrorIncompatibleDevice`.
// You must call this function only from the `write_callback` thread context.
int soundio_outstream_pause(struct SoundIoOutStream *outstream, bool pause);




// Input Streams
// Allocates memory and sets defaults. Next you should fill out the struct fields
// and then call `soundio_instream_open`.
struct SoundIoInStream *soundio_instream_create(struct SoundIoDevice *device);
// You may not call this function from `read_callback`.
void soundio_instream_destroy(struct SoundIoInStream *instream);

// After you call this function, `buffer_duration` and `period_duration` are
// set to the correct values, if available.
// The next thing to do is call `soundio_instream_start`.
int soundio_instream_open(struct SoundIoInStream *instream);

// After you call this function, `read_callback` will be called.
int soundio_instream_start(struct SoundIoInStream *instream);

// Call this function when you are ready to begin reading from the device
// buffer.
// * `instream` - (in) The input stream you want to read from.
// * `areas` - (out) The memory addresses you can read data from. It is OK
//   to modify the pointers if that helps you iterate. If a buffer overflow
//   occurred, there will be a "hole" in the buffer. To indicate this,
//   `areas` will be `NULL` and `frame_count` tells how big the hole is in
//   frames.
// * `frame_count` - (in/out) - Provide the number of frames you want to read.
//   Returned will be the number of frames you can actually read.
// It is your responsibility to call this function no more and no fewer than the
// correct number of times as determined by `available_frame_count` from
// `read_callback`. See microphone.c for an example.
// You must call this function only from the `read_callback` thread context.
// After calling this function, read data from `areas` and then use
// `soundio_instream_end_read` to actually remove the data from the buffer
// and move the read index forward. `soundio_instream_end_read` should not be
// called if the buffer is empty (`frame_count` == 0), but it should be called
// if there is a hole.
int soundio_instream_begin_read(struct SoundIoInStream *instream,
        struct SoundIoChannelArea **areas, int *frame_count);
// This will drop all of the frames from when you called
// `soundio_instream_begin_read`.
// You must call this function only from the `read_callback` thread context.
// You must call this function only after a successful call to
// `soundio_instream_begin_read`.
int soundio_instream_end_read(struct SoundIoInStream *instream);

// If the underyling device supports pausing, this pauses the stream and
// prevents `read_callback` from being called. Otherwise this returns
// `SoundIoErrorIncompatibleDevice`.
// You must call this function only from the `read_callback` thread context.
int soundio_instream_pause(struct SoundIoInStream *instream, bool pause);


// Ring Buffer
struct SoundIoRingBuffer;
// `requested_capacity` in bytes.
struct SoundIoRingBuffer *soundio_ring_buffer_create(struct SoundIo *soundio, int requested_capacity);
void soundio_ring_buffer_destroy(struct SoundIoRingBuffer *ring_buffer);
int soundio_ring_buffer_capacity(struct SoundIoRingBuffer *ring_buffer);

// don't write more than capacity
char *soundio_ring_buffer_write_ptr(struct SoundIoRingBuffer *ring_buffer);
// `count` in bytes.
void soundio_ring_buffer_advance_write_ptr(struct SoundIoRingBuffer *ring_buffer, int count);

// don't read more than capacity
char *soundio_ring_buffer_read_ptr(struct SoundIoRingBuffer *ring_buffer);
// `count` in bytes.
void soundio_ring_buffer_advance_read_ptr(struct SoundIoRingBuffer *ring_buffer, int count);

// Returns how many bytes of the buffer is used, ready for reading.
int soundio_ring_buffer_fill_count(struct SoundIoRingBuffer *ring_buffer);

// Returns how many bytes of the buffer is free, ready for writing.
int soundio_ring_buffer_free_count(struct SoundIoRingBuffer *ring_buffer);

// Must be called by the writer.
void soundio_ring_buffer_clear(struct SoundIoRingBuffer *ring_buffer);


#ifdef __cplusplus
}
#endif

#endif
