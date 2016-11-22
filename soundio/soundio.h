/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_SOUNDIO_H
#define SOUNDIO_SOUNDIO_H

#include "endian.h"
#include <stdbool.h>

/// \cond
#ifdef __cplusplus
#define SOUNDIO_EXTERN_C extern "C"
#else
#define SOUNDIO_EXTERN_C
#endif

#if defined(SOUNDIO_STATIC_LIBRARY)
# define SOUNDIO_EXPORT SOUNDIO_EXTERN_C
#else
# if defined(_WIN32)
#  if defined(SOUNDIO_BUILDING_LIBRARY)
#   define SOUNDIO_EXPORT SOUNDIO_EXTERN_C __declspec(dllexport)
#  else
#   define SOUNDIO_EXPORT SOUNDIO_EXTERN_C __declspec(dllimport)
#  endif
# else
#  define SOUNDIO_EXPORT SOUNDIO_EXTERN_C __attribute__((visibility ("default")))
# endif
#endif
/// \endcond

/** \mainpage
 *
 * \section intro_sec Overview
 *
 * libsoundio is a C library for cross-platform audio input and output. It is
 * suitable for real-time and consumer software.
 *
 * Documentation: soundio.h
 */


/** \example sio_list_devices.c
 * List the available input and output devices on the system and their
 * properties. Supports watching for changes and specifying backend to use.
 */

/** \example sio_sine.c
 * Play a sine wave over the default output device.
 * Supports specifying device and backend to use.
 */

/** \example sio_record.c
 * Record audio to an output file.
 * Supports specifying device and backend to use.
 */

/** \example sio_microphone.c
 * Stream the default input device over the default output device.
 * Supports specifying device and backend to use.
 */

/** \example backend_disconnect_recover.c
 * Demonstrates recovering from a backend disconnecting.
 */

/// See also ::soundio_error_name
enum SoundIoError {
    SoundIoErrorNone,
    /// Out of memory.
    SoundIoErrorNoMem,
    /// The backend does not appear to be active or running.
    SoundIoErrorInitAudioBackend,
    /// A system resource other than memory was not available.
    SoundIoErrorSystemResources,
    /// Attempted to open a device and failed.
    SoundIoErrorOpeningDevice,
    SoundIoErrorNoSuchDevice,
    /// The programmer did not comply with the API.
    SoundIoErrorInvalid,
    /// libsoundio was compiled without support for that backend.
    SoundIoErrorBackendUnavailable,
    /// An open stream had an error that can only be recovered from by
    /// destroying the stream and creating it again.
    SoundIoErrorStreaming,
    /// Attempted to use a device with parameters it cannot support.
    SoundIoErrorIncompatibleDevice,
    /// When JACK returns `JackNoSuchClient`
    SoundIoErrorNoSuchClient,
    /// Attempted to use parameters that the backend cannot support.
    SoundIoErrorIncompatibleBackend,
    /// Backend server shutdown or became inactive.
    SoundIoErrorBackendDisconnected,
    SoundIoErrorInterrupted,
    /// Buffer underrun occurred.
    SoundIoErrorUnderflow,
    /// Unable to convert to or from UTF-8 to the native string format.
    SoundIoErrorEncodingString,
};

/// Specifies where a channel is physically located.
enum SoundIoChannelId {
    SoundIoChannelIdInvalid,

    SoundIoChannelIdFrontLeft, ///< First of the more commonly supported ids.
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
    SoundIoChannelIdTopBackRight, ///< Last of the more commonly supported ids.

    SoundIoChannelIdBackLeftCenter, ///< First of the less commonly supported ids.
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
    SoundIoChannelIdLfe2,
    SoundIoChannelIdBottomCenter,
    SoundIoChannelIdBottomLeftCenter,
    SoundIoChannelIdBottomRightCenter,

    /// Mid/side recording
    SoundIoChannelIdMsMid,
    SoundIoChannelIdMsSide,

    /// first order ambisonic channels
    SoundIoChannelIdAmbisonicW,
    SoundIoChannelIdAmbisonicX,
    SoundIoChannelIdAmbisonicY,
    SoundIoChannelIdAmbisonicZ,

    /// X-Y Recording
    SoundIoChannelIdXyX,
    SoundIoChannelIdXyY,

    SoundIoChannelIdHeadphonesLeft, ///< First of the "other" channel ids
    SoundIoChannelIdHeadphonesRight,
    SoundIoChannelIdClickTrack,
    SoundIoChannelIdForeignLanguage,
    SoundIoChannelIdHearingImpaired,
    SoundIoChannelIdNarration,
    SoundIoChannelIdHaptic,
    SoundIoChannelIdDialogCentricMix, ///< Last of the "other" channel ids

    SoundIoChannelIdAux,
    SoundIoChannelIdAux0,
    SoundIoChannelIdAux1,
    SoundIoChannelIdAux2,
    SoundIoChannelIdAux3,
    SoundIoChannelIdAux4,
    SoundIoChannelIdAux5,
    SoundIoChannelIdAux6,
    SoundIoChannelIdAux7,
    SoundIoChannelIdAux8,
    SoundIoChannelIdAux9,
    SoundIoChannelIdAux10,
    SoundIoChannelIdAux11,
    SoundIoChannelIdAux12,
    SoundIoChannelIdAux13,
    SoundIoChannelIdAux14,
    SoundIoChannelIdAux15,
};

/// Built-in channel layouts for convenience.
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
    SoundIoBackendDummy,
    SoundIoBackendJack,
    SoundIoBackendPulseAudio,
    SoundIoBackendAlsa,
    SoundIoBackendCoreAudio,
    SoundIoBackendWasapi,
};

enum SoundIoDeviceAim {
    SoundIoDeviceAimInput,  ///< capture / recording
    SoundIoDeviceAimOutput, ///< playback
};

/// For your convenience, Native Endian and Foreign Endian constants are defined
/// which point to the respective SoundIoFormat values.
enum SoundIoFormat {
    SoundIoFormatInvalid,
    SoundIoFormatS8,          ///< Signed 8 bit
    SoundIoFormatU8,          ///< Unsigned 8 bit
    SoundIoFormatS16LE,       ///< Signed 16 bit Little Endian
    SoundIoFormatS16BE,       ///< Signed 16 bit Big Endian
    SoundIoFormatU16LE,       ///< Unsigned 16 bit Little Endian
    SoundIoFormatU16BE,       ///< Unsigned 16 bit Little Endian
    SoundIoFormatS24LE,       ///< Signed 24 bit Little Endian using low three bytes in 32-bit word
    SoundIoFormatS24BE,       ///< Signed 24 bit Big Endian using low three bytes in 32-bit word
    SoundIoFormatU24LE,       ///< Unsigned 24 bit Little Endian using low three bytes in 32-bit word
    SoundIoFormatU24BE,       ///< Unsigned 24 bit Big Endian using low three bytes in 32-bit word
    SoundIoFormatS24PackedLE, ///< Signed 24 bit Little Endian using three bytes
    SoundIoFormatS24PackedBE, ///< Signed 24 bit Big Endian using three bytes
    SoundIoFormatU24PackedLE, ///< Unsigned 24 bit Little Endian using three bytes
    SoundIoFormatU24PackedBE, ///< Unsigned 24 bit Big Endian using three bytes
    SoundIoFormatS32LE,       ///< Signed 32 bit Little Endian
    SoundIoFormatS32BE,       ///< Signed 32 bit Big Endian
    SoundIoFormatU32LE,       ///< Unsigned 32 bit Little Endian
    SoundIoFormatU32BE,       ///< Unsigned 32 bit Big Endian
    SoundIoFormatFloat32LE,   ///< Float 32 bit Little Endian, Range -1.0 to 1.0
    SoundIoFormatFloat32BE,   ///< Float 32 bit Big Endian, Range -1.0 to 1.0
    SoundIoFormatFloat64LE,   ///< Float 64 bit Little Endian, Range -1.0 to 1.0
    SoundIoFormatFloat64BE,   ///< Float 64 bit Big Endian, Range -1.0 to 1.0
};

#if defined(SOUNDIO_OS_BIG_ENDIAN)
#define SoundIoFormatS16NE SoundIoFormatS16BE
#define SoundIoFormatU16NE SoundIoFormatU16BE
#define SoundIoFormatS24NE SoundIoFormatS24BE
#define SoundIoFormatU24NE SoundIoFormatU24BE
#define SoundIoFormatS24PackedNE SoundIoFormatS24PackedBE
#define SoundIoFormatU24PackedNE SoundIoFormatU24PackedBE
#define SoundIoFormatS32NE SoundIoFormatS32BE
#define SoundIoFormatU32NE SoundIoFormatU32BE
#define SoundIoFormatFloat32NE SoundIoFormatFloat32BE
#define SoundIoFormatFloat64NE SoundIoFormatFloat64BE

#define SoundIoFormatS16FE SoundIoFormatS16LE
#define SoundIoFormatU16FE SoundIoFormatU16LE
#define SoundIoFormatS24FE SoundIoFormatS24LE
#define SoundIoFormatU24FE SoundIoFormatU24LE
#define SoundIoFormatS24PackedFE SoundIoFormatS24PackedLE
#define SoundIoFormatU24PackedFE SoundIoFormatU24PackedLE
#define SoundIoFormatS32FE SoundIoFormatS32LE
#define SoundIoFormatU32FE SoundIoFormatU32LE
#define SoundIoFormatFloat32FE SoundIoFormatFloat32LE
#define SoundIoFormatFloat64FE SoundIoFormatFloat64LE

#elif defined(SOUNDIO_OS_LITTLE_ENDIAN)

/// Note that we build the documentation in Little Endian mode,
/// so all the "NE" macros in the docs point to "LE" and
/// "FE" macros point to "BE". On a Big Endian system it is the
/// other way around.
#define SoundIoFormatS16NE SoundIoFormatS16LE
#define SoundIoFormatU16NE SoundIoFormatU16LE
#define SoundIoFormatS24NE SoundIoFormatS24LE
#define SoundIoFormatU24NE SoundIoFormatU24LE
#define SoundIoFormatS24PackedNE SoundIoFormatS24PackedLE
#define SoundIoFormatU24PackedNE SoundIoFormatU24PackedLE
#define SoundIoFormatS32NE SoundIoFormatS32LE
#define SoundIoFormatU32NE SoundIoFormatU32LE
#define SoundIoFormatFloat32NE SoundIoFormatFloat32LE
#define SoundIoFormatFloat64NE SoundIoFormatFloat64LE

#define SoundIoFormatS16FE SoundIoFormatS16BE
#define SoundIoFormatU16FE SoundIoFormatU16BE
#define SoundIoFormatS24FE SoundIoFormatS24BE
#define SoundIoFormatU24FE SoundIoFormatU24BE
#define SoundIoFormatS24PackedFE SoundIoFormatS24PackedBE
#define SoundIoFormatU24PackedFE SoundIoFormatU24PackedBE
#define SoundIoFormatS32FE SoundIoFormatS32BE
#define SoundIoFormatU32FE SoundIoFormatU32BE
#define SoundIoFormatFloat32FE SoundIoFormatFloat32BE
#define SoundIoFormatFloat64FE SoundIoFormatFloat64BE

#else
#error unknown byte order
#endif

#define SOUNDIO_MAX_CHANNELS 24
/// The size of this struct is OK to use.
struct SoundIoChannelLayout {
    const char *name;
    int channel_count;
    enum SoundIoChannelId channels[SOUNDIO_MAX_CHANNELS];
};

/// The size of this struct is OK to use.
struct SoundIoSampleRateRange {
    int min;
    int max;
};

/// The size of this struct is OK to use.
struct SoundIoChannelArea {
    /// Base address of buffer.
    char *ptr;
    /// How many bytes it takes to get from the beginning of one sample to
    /// the beginning of the next sample.
    int step;
};

/// The size of this struct is not part of the API or ABI.
struct SoundIo {
    /// Optional. Put whatever you want here. Defaults to NULL.
    void *userdata;
    /// Optional callback. Called when the list of devices change. Only called
    /// during a call to ::soundio_flush_events or ::soundio_wait_events.
    void (*on_devices_change)(struct SoundIo *);
    /// Optional callback. Called when the backend disconnects. For example,
    /// when the JACK server shuts down. When this happens, listing devices
    /// and opening streams will always fail with
    /// SoundIoErrorBackendDisconnected. This callback is only called during a
    /// call to ::soundio_flush_events or ::soundio_wait_events.
    /// If you do not supply a callback, the default will crash your program
    /// with an error message. This callback is also called when the thread
    /// that retrieves device information runs into an unrecoverable condition
    /// such as running out of memory.
    ///
    /// Possible errors:
    /// * #SoundIoErrorBackendDisconnected
    /// * #SoundIoErrorNoMem
    /// * #SoundIoErrorSystemResources
    /// * #SoundIoErrorOpeningDevice - unexpected problem accessing device
    ///   information
    void (*on_backend_disconnect)(struct SoundIo *, enum SoundIoError err);
    /// Optional callback. Called from an unknown thread that you should not use
    /// to call any soundio functions. You may use this to signal a condition
    /// variable to wake up. Called when ::soundio_wait_events would be woken up.
    void (*on_events_signal)(struct SoundIo *);

    /// Read-only. After calling ::soundio_connect or ::soundio_connect_backend,
    /// this field tells which backend is currently connected.
    enum SoundIoBackend current_backend;

    /// Optional: Application name.
    /// PulseAudio uses this for "application name".
    /// JACK uses this for `client_name`.
    /// Must not contain a colon (":").
    const char *app_name;

    /// Optional: Real time priority warning.
    /// This callback is fired when making thread real-time priority failed. By
    /// default, it will print to stderr only the first time it is called
    /// a message instructing the user how to configure their system to allow
    /// real-time priority threads. This must be set to a function not NULL.
    /// To silence the warning, assign this to a function that does nothing.
    void (*emit_rtprio_warning)(struct SoundIo *);

    /// Optional: JACK info callback.
    /// By default, libsoundio sets this to an empty function in order to
    /// silence stdio messages from JACK. You may override the behavior by
    /// setting this to `NULL` or providing your own function. This is
    /// registered with JACK regardless of whether ::soundio_connect_backend
    /// succeeds.
    /// These functions are called globally from JACK and there is not a way
    /// to provide access to the SoundIo instance. For more details, see
    /// https://github.com/jackaudio/jack2/issues/235
    void (*jack_info_callback)(const char *msg);
    /// Optional: JACK error callback.
    /// See SoundIo::jack_info_callback
    void (*jack_error_callback)(const char *msg);
};

/// The size of this struct is not part of the API or ABI.
struct SoundIoDevice {
    /// Read-only. Set automatically.
    struct SoundIo *soundio;

    /// A string of bytes that uniquely identifies this device.
    /// If the same physical device supports both input and output, that makes
    /// one SoundIoDevice for the input and one SoundIoDevice for the output.
    /// In this case, the id of each SoundIoDevice will be the same, and
    /// SoundIoDevice::aim will be different. Additionally, if the device
    /// supports raw mode, there may be up to four devices with the same id:
    /// one for each value of SoundIoDevice::is_raw and one for each value of
    /// SoundIoDevice::aim.
    char *id;
    /// User-friendly UTF-8 encoded text to describe the device.
    char *name;

    /// Tells whether this device is an input device or an output device.
    enum SoundIoDeviceAim aim;

    /// Channel layouts are handled similarly to SoundIoDevice::formats.
    /// If this information is missing due to a SoundIoDevice::probe_error,
    /// layouts will be NULL. It's OK to modify this data, for example calling
    /// ::soundio_sort_channel_layouts on it.
    /// Devices are guaranteed to have at least 1 channel layout.
    struct SoundIoChannelLayout *layouts;
    int layout_count;
    /// See SoundIoDevice::current_format
    struct SoundIoChannelLayout current_layout;

    /// List of formats this device supports. See also
    /// SoundIoDevice::current_format.
    enum SoundIoFormat *formats;
    /// How many formats are available in SoundIoDevice::formats.
    int format_count;
    /// A device is either a raw device or it is a virtual device that is
    /// provided by a software mixing service such as dmix or PulseAudio (see
    /// SoundIoDevice::is_raw). If it is a raw device,
    /// current_format is meaningless;
    /// the device has no current format until you open it. On the other hand,
    /// if it is a virtual device, current_format describes the
    /// destination sample format that your audio will be converted to. Or,
    /// if you're the lucky first application to open the device, you might
    /// cause the current_format to change to your format.
    /// Generally, you want to ignore current_format and use
    /// whatever format is most convenient
    /// for you which is supported by the device, because when you are the only
    /// application left, the mixer might decide to switch
    /// current_format to yours. You can learn the supported formats via
    /// formats and SoundIoDevice::format_count. If this information is missing
    /// due to a probe error, formats will be `NULL`. If current_format is
    /// unavailable, it will be set to #SoundIoFormatInvalid.
    /// Devices are guaranteed to have at least 1 format available.
    enum SoundIoFormat current_format;

    /// Sample rate is the number of frames per second.
    /// Sample rate is handled very similar to SoundIoDevice::formats.
    /// If sample rate information is missing due to a probe error, the field
    /// will be set to NULL.
    /// Devices which have SoundIoDevice::probe_error set to #SoundIoErrorNone are
    /// guaranteed to have at least 1 sample rate available.
    struct SoundIoSampleRateRange *sample_rates;
    /// How many sample rate ranges are available in
    /// SoundIoDevice::sample_rates. 0 if sample rate information is missing
    /// due to a probe error.
    int sample_rate_count;
    /// See SoundIoDevice::current_format
    /// 0 if sample rate information is missing due to a probe error.
    int sample_rate_current;

    /// Software latency minimum in seconds. If this value is unknown or
    /// irrelevant, it is set to 0.0.
    /// For PulseAudio and WASAPI this value is unknown until you open a
    /// stream.
    double software_latency_min;
    /// Software latency maximum in seconds. If this value is unknown or
    /// irrelevant, it is set to 0.0.
    /// For PulseAudio and WASAPI this value is unknown until you open a
    /// stream.
    double software_latency_max;
    /// Software latency in seconds. If this value is unknown or
    /// irrelevant, it is set to 0.0.
    /// For PulseAudio and WASAPI this value is unknown until you open a
    /// stream.
    /// See SoundIoDevice::current_format
    double software_latency_current;

    /// Raw means that you are directly opening the hardware device and not
    /// going through a proxy such as dmix, PulseAudio, or JACK. When you open a
    /// raw device, other applications on the computer are not able to
    /// simultaneously access the device. Raw devices do not perform automatic
    /// resampling and thus tend to have fewer formats available.
    bool is_raw;

    /// Devices are reference counted. See ::soundio_device_ref and
    /// ::soundio_device_unref.
    int ref_count;

    /// This is set to a SoundIoError representing the result of the device
    /// probe. Ideally this will be SoundIoErrorNone in which case all the
    /// fields of the device will be populated. If there is an error code here
    /// then information about formats, sample rates, and channel layouts might
    /// be missing.
    ///
    /// Possible errors:
    /// * #SoundIoErrorOpeningDevice
    /// * #SoundIoErrorNoMem
    enum SoundIoError probe_error;
};

/// The size of this struct is not part of the API or ABI.
struct SoundIoOutStream {
    /// Populated automatically when you call ::soundio_outstream_create.
    struct SoundIoDevice *device;

    /// Defaults to #SoundIoFormatFloat32NE, followed by the first one
    /// supported.
    enum SoundIoFormat format;

    /// Sample rate is the number of frames per second.
    /// Defaults to 48000 (and then clamped into range).
    int sample_rate;

    /// Defaults to Stereo, if available, followed by the first layout
    /// supported.
    struct SoundIoChannelLayout layout;

    /// Ignoring hardware latency, this is the number of seconds it takes for
    /// the last sample in a full buffer to be played.
    /// After you call ::soundio_outstream_open, this value is replaced with the
    /// actual software latency, as near to this value as possible.
    /// On systems that support clearing the buffer, this defaults to a large
    /// latency, potentially upwards of 2 seconds, with the understanding that
    /// you will call ::soundio_outstream_clear_buffer when you want to reduce
    /// the latency to 0. On systems that do not support clearing the buffer,
    /// this defaults to a reasonable lower latency value.
    ///
    /// On backends with high latencies (such as 2 seconds), `frame_count_min`
    /// will be 0, meaning you don't have to fill the entire buffer. In this
    /// case, the large buffer is there if you want it; you only have to fill
    /// as much as you want. On backends like JACK, `frame_count_min` will be
    /// equal to `frame_count_max` and if you don't fill that many frames, you
    /// will get glitches.
    ///
    /// If the device has unknown software latency min and max values, you may
    /// still set this, but you might not get the value you requested.
    /// For PulseAudio, if you set this value to non-default, it sets
    /// `PA_STREAM_ADJUST_LATENCY` and is the value used for `maxlength` and
    /// `tlength`.
    ///
    /// For JACK, this value is always equal to
    /// SoundIoDevice::software_latency_current of the device.
    double software_latency;

    /// Defaults to NULL. Put whatever you want here.
    void *userdata;
    /// In this callback, you call ::soundio_outstream_begin_write and
    /// ::soundio_outstream_end_write as many times as necessary to write
    /// at minimum `frame_count_min` frames and at maximum `frame_count_max`
    /// frames. `frame_count_max` will always be greater than 0. Note that you
    /// should write as many frames as you can; `frame_count_min` might be 0 and
    /// you can still get a buffer underflow if you always write
    /// `frame_count_min` frames.
    ///
    /// For Dummy, ALSA, and PulseAudio, `frame_count_min` will be 0. For JACK
    /// and CoreAudio `frame_count_min` will be equal to `frame_count_max`.
    ///
    /// The code in the supplied function must be suitable for real-time
    /// execution. That means that it cannot call functions that might block
    /// for a long time. This includes all I/O functions (disk, TTY, network),
    /// malloc, free, printf, pthread_mutex_lock, sleep, wait, poll, select,
    /// pthread_join, pthread_cond_wait, etc.
    void (*write_callback)(struct SoundIoOutStream *,
            int frame_count_min, int frame_count_max);
    /// This optional callback happens when the sound device runs out of
    /// buffered audio data to play. After this occurs, the outstream waits
    /// until the buffer is full to resume playback.
    /// This is called from the SoundIoOutStream::write_callback thread context.
    void (*underflow_callback)(struct SoundIoOutStream *);
    /// Optional callback. `err` is always SoundIoErrorStreaming.
    /// SoundIoErrorStreaming is an unrecoverable error. The stream is in an
    /// invalid state and must be destroyed.
    /// If you do not supply error_callback, the default callback will print
    /// a message to stderr and then call `abort`.
    /// This is called from the SoundIoOutStream::write_callback thread context.
    void (*error_callback)(struct SoundIoOutStream *, enum SoundIoError err);

    /// Optional: Name of the stream. Defaults to "SoundIoOutStream"
    /// PulseAudio uses this for the stream name.
    /// JACK uses this for the client name of the client that connects when you
    /// open the stream.
    /// WASAPI uses this for the session display name.
    /// Must not contain a colon (":").
    const char *name;

    /// Optional: Hint that this output stream is nonterminal. This is used by
    /// JACK and it means that the output stream data originates from an input
    /// stream. Defaults to `false`.
    bool non_terminal_hint;


    /// computed automatically when you call ::soundio_outstream_open
    int bytes_per_frame;
    /// computed automatically when you call ::soundio_outstream_open
    int bytes_per_sample;

    /// If setting the channel layout fails for some reason, this field is set
    /// to an error code. Possible error codes are:
    /// * #SoundIoErrorIncompatibleDevice
    enum SoundIoError layout_error;

    /// Optional: Whether to leave the software outputs unconnected.
    /// If this is set to `true`, JACK will not immediately connect the output
    /// of this stream to the output ports of the sound card.
    /// Defaults to `false`.
    bool unconnected;
};

/// The size of this struct is not part of the API or ABI.
struct SoundIoInStream {
    /// Populated automatically when you call ::soundio_outstream_create.
    struct SoundIoDevice *device;

    /// Defaults to #SoundIoFormatFloat32NE, followed by the first one
    /// supported.
    enum SoundIoFormat format;

    /// Sample rate is the number of frames per second.
    /// Defaults to max(sample_rate_min, min(sample_rate_max, 48000))
    int sample_rate;

    /// Defaults to Stereo, if available, followed by the first layout
    /// supported.
    struct SoundIoChannelLayout layout;

    /// Ignoring hardware latency, this is the number of seconds it takes for a
    /// captured sample to become available for reading.
    /// After you call ::soundio_instream_open, this value is replaced with the
    /// actual software latency, as near to this value as possible.
    /// A higher value means less CPU usage. Defaults to a large value,
    /// potentially upwards of 2 seconds.
    /// If the device has unknown software latency min and max values, you may
    /// still set this, but you might not get the value you requested.
    /// For PulseAudio, if you set this value to non-default, it sets
    /// `PA_STREAM_ADJUST_LATENCY` and is the value used for `fragsize`.
    /// For JACK, this value is always equal to
    /// SoundIoDevice::software_latency_current
    double software_latency;

    /// Defaults to NULL. Put whatever you want here.
    void *userdata;
    /// In this function call ::soundio_instream_begin_read and
    /// ::soundio_instream_end_read as many times as necessary to read at
    /// minimum `frame_count_min` frames and at maximum `frame_count_max`
    /// frames. If you return from read_callback without having read
    /// `frame_count_min`, the frames will be dropped. `frame_count_max` is how
    /// many frames are available to read.
    ///
    /// The code in the supplied function must be suitable for real-time
    /// execution. That means that it cannot call functions that might block
    /// for a long time. This includes all I/O functions (disk, TTY, network),
    /// malloc, free, printf, pthread_mutex_lock, sleep, wait, poll, select,
    /// pthread_join, pthread_cond_wait, etc.
    void (*read_callback)(struct SoundIoInStream *, int frame_count_min, int frame_count_max);
    /// This optional callback happens when the sound device buffer is full,
    /// yet there is more captured audio to put in it.
    /// This is never fired for PulseAudio.
    /// This is called from the SoundIoInStream::read_callback thread context.
    void (*overflow_callback)(struct SoundIoInStream *);
    /// Optional callback. `err` is always SoundIoErrorStreaming.
    /// SoundIoErrorStreaming is an unrecoverable error. The stream is in an
    /// invalid state and must be destroyed.
    /// If you do not supply `error_callback`, the default callback will print
    /// a message to stderr and then abort().
    /// This is called from the SoundIoInStream::read_callback thread context.
    void (*error_callback)(struct SoundIoInStream *, enum SoundIoError err);

    /// Optional: Name of the stream. Defaults to "SoundIoInStream";
    /// PulseAudio uses this for the stream name.
    /// JACK uses this for the client name of the client that connects when you
    /// open the stream.
    /// WASAPI uses this for the session display name.
    /// Must not contain a colon (":").
    const char *name;

    /// Optional: Hint that this input stream is nonterminal. This is used by
    /// JACK and it means that the data received by the stream will be
    /// passed on or made available to another stream. Defaults to `false`.
    bool non_terminal_hint;

    /// computed automatically when you call ::soundio_instream_open
    int bytes_per_frame;
    /// computed automatically when you call ::soundio_instream_open
    int bytes_per_sample;

    /// If setting the channel layout fails for some reason, this field is set
    /// to an error code. Possible error codes are: #SoundIoErrorIncompatibleDevice
    enum SoundIoError layout_error;

    /// Optional: Whether to leave the software inputs unconnected.
    /// If this is set to `true`, JACK will not immediately connect the input
    /// of this stream to the input ports of the sound card.
    /// Defaults to `false`.
    bool unconnected;
};

/// See also ::soundio_version_major, ::soundio_version_minor, ::soundio_version_patch
SOUNDIO_EXPORT const char *soundio_version_string(void);
/// See also ::soundio_version_string, ::soundio_version_minor, ::soundio_version_patch
SOUNDIO_EXPORT int soundio_version_major(void);
/// See also ::soundio_version_major, ::soundio_version_string, ::soundio_version_patch
SOUNDIO_EXPORT int soundio_version_minor(void);
/// See also ::soundio_version_major, ::soundio_version_minor, ::soundio_version_string
SOUNDIO_EXPORT int soundio_version_patch(void);

/// Create a SoundIo context. You may create multiple instances of this to
/// connect to multiple backends. Sets all fields to defaults.
/// Returns `NULL` if and only if memory could not be allocated.
/// See also ::soundio_destroy
SOUNDIO_EXPORT struct SoundIo *soundio_create(void);
SOUNDIO_EXPORT void soundio_destroy(struct SoundIo *soundio);


/// Tries ::soundio_connect_backend on all available backends in order.
/// Possible errors:
/// * #SoundIoErrorInvalid - already connected
/// * #SoundIoErrorNoMem
/// * #SoundIoErrorSystemResources
/// * #SoundIoErrorNoSuchClient - when JACK returns `JackNoSuchClient`
/// See also ::soundio_disconnect
SOUNDIO_EXPORT enum SoundIoError soundio_connect(struct SoundIo *soundio);
/// Instead of calling ::soundio_connect you may call this function to try a
/// specific backend.
/// Possible errors:
/// * #SoundIoErrorInvalid - already connected or invalid backend parameter
/// * #SoundIoErrorNoMem
/// * #SoundIoErrorBackendUnavailable - backend was not compiled in
/// * #SoundIoErrorSystemResources
/// * #SoundIoErrorNoSuchClient - when JACK returns `JackNoSuchClient`
/// * #SoundIoErrorInitAudioBackend - requested `backend` is not active
/// * #SoundIoErrorBackendDisconnected - backend disconnected while connecting
/// See also ::soundio_disconnect
SOUNDIO_EXPORT enum SoundIoError soundio_connect_backend(struct SoundIo *soundio, enum SoundIoBackend backend);
SOUNDIO_EXPORT void soundio_disconnect(struct SoundIo *soundio);

/// Get a string representation of a #SoundIoError
SOUNDIO_EXPORT const char *soundio_error_name(enum SoundIoError error);
/// Get a string representation of a #SoundIoBackend
SOUNDIO_EXPORT const char *soundio_backend_name(enum SoundIoBackend backend);

/// Returns the number of available backends.
SOUNDIO_EXPORT int soundio_backend_count(void);
/// get the available backend at the specified index
/// (0 <= index < ::soundio_backend_count)
SOUNDIO_EXPORT enum SoundIoBackend soundio_get_backend(int index);

/// Returns whether libsoundio was compiled with backend.
SOUNDIO_EXPORT bool soundio_have_backend(enum SoundIoBackend backend);

/// Atomically update information for all connected devices. Note that calling
/// this function merely flips a pointer; the actual work of collecting device
/// information is done elsewhere. It is performant to call this function many
/// times per second.
///
/// When you call this, the following callbacks might be called:
/// * SoundIo::on_devices_change
/// * SoundIo::on_backend_disconnect
/// This is the only time those callbacks can be called.
///
/// This must be called from the same thread as the thread in which you call
/// these functions:
/// * ::soundio_input_device_count
/// * ::soundio_output_device_count
/// * ::soundio_get_input_device
/// * ::soundio_get_output_device
/// * ::soundio_default_input_device_index
/// * ::soundio_default_output_device_index
///
/// Note that if you do not care about learning about updated devices, you
/// might call this function only once ever and never call
/// ::soundio_wait_events.
SOUNDIO_EXPORT void soundio_flush_events(struct SoundIo *soundio);

/// This function calls ::soundio_flush_events then blocks until another event
/// is ready or you call ::soundio_wakeup. Be ready for spurious wakeups.
SOUNDIO_EXPORT void soundio_wait_events(struct SoundIo *soundio);

/// Makes ::soundio_wait_events stop blocking.
SOUNDIO_EXPORT void soundio_wakeup(struct SoundIo *soundio);


/// If necessary you can manually trigger a device rescan. Normally you will
/// not ever have to call this function, as libsoundio listens to system events
/// for device changes and responds to them by rescanning devices and preparing
/// the new device information for you to be atomically replaced when you call
/// ::soundio_flush_events. However you might run into cases where you want to
/// force trigger a device rescan, for example if an ALSA device has a
/// SoundIoDevice::probe_error.
///
/// After you call this you still have to use ::soundio_flush_events or
/// ::soundio_wait_events and then wait for the
/// SoundIo::on_devices_change callback.
///
/// This can be called from any thread context except for
/// SoundIoOutStream::write_callback and SoundIoInStream::read_callback
SOUNDIO_EXPORT void soundio_force_device_scan(struct SoundIo *soundio);


// Channel Layouts

/// Returns whether the channel count field and each channel id matches in
/// the supplied channel layouts.
SOUNDIO_EXPORT bool soundio_channel_layout_equal(
        const struct SoundIoChannelLayout *a,
        const struct SoundIoChannelLayout *b);

SOUNDIO_EXPORT const char *soundio_get_channel_name(enum SoundIoChannelId id);
/// Given UTF-8 encoded text which is the name of a channel such as
/// "Front Left", "FL", or "front-left", return the corresponding
/// SoundIoChannelId. Returns SoundIoChannelIdInvalid for no match.
SOUNDIO_EXPORT enum SoundIoChannelId soundio_parse_channel_id(const char *str, int str_len);

/// Returns the number of builtin channel layouts.
SOUNDIO_EXPORT int soundio_channel_layout_builtin_count(void);
/// Returns a builtin channel layout. 0 <= `index` < ::soundio_channel_layout_builtin_count
///
/// Although `index` is of type `int`, it should be a valid
/// #SoundIoChannelLayoutId enum value.
SOUNDIO_EXPORT const struct SoundIoChannelLayout *soundio_channel_layout_get_builtin(int index);

/// Get the default builtin channel layout for the given number of channels.
SOUNDIO_EXPORT const struct SoundIoChannelLayout *soundio_channel_layout_get_default(int channel_count);

/// Return the index of `channel` in `layout`, or `-1` if not found.
SOUNDIO_EXPORT int soundio_channel_layout_find_channel(
        const struct SoundIoChannelLayout *layout, enum SoundIoChannelId channel);

/// Populates the name field of layout if it matches a builtin one.
/// returns whether it found a match
SOUNDIO_EXPORT bool soundio_channel_layout_detect_builtin(struct SoundIoChannelLayout *layout);

/// Iterates over preferred_layouts. Returns the first channel layout in
/// preferred_layouts which matches one of the channel layouts in
/// available_layouts. Returns NULL if none matches.
SOUNDIO_EXPORT const struct SoundIoChannelLayout *soundio_best_matching_channel_layout(
        const struct SoundIoChannelLayout *preferred_layouts, int preferred_layout_count,
        const struct SoundIoChannelLayout *available_layouts, int available_layout_count);

/// Sorts by channel count, descending.
SOUNDIO_EXPORT void soundio_sort_channel_layouts(struct SoundIoChannelLayout *layouts, int layout_count);


// Sample Formats

/// Returns -1 on invalid format.
SOUNDIO_EXPORT int soundio_get_bytes_per_sample(enum SoundIoFormat format);

/// A frame is one sample per channel.
static inline int soundio_get_bytes_per_frame(enum SoundIoFormat format, int channel_count) {
    return soundio_get_bytes_per_sample(format) * channel_count;
}

/// Sample rate is the number of frames per second.
static inline int soundio_get_bytes_per_second(enum SoundIoFormat format,
        int channel_count, int sample_rate)
{
    return soundio_get_bytes_per_frame(format, channel_count) * sample_rate;
}

/// Returns string representation of `format`.
SOUNDIO_EXPORT const char * soundio_format_name(enum SoundIoFormat format);




// Devices

/// When you call ::soundio_flush_events, a snapshot of all device state is
/// saved and these functions merely access the snapshot data. When you want
/// to check for new devices, call ::soundio_flush_events. Or you can call
/// ::soundio_wait_events to block until devices change. If an error occurs
/// scanning devices in a background thread, SoundIo::on_backend_disconnect is called
/// with the error code.

/// Get the number of input devices.
/// Returns -1 if you never called ::soundio_flush_events.
SOUNDIO_EXPORT int soundio_input_device_count(struct SoundIo *soundio);
/// Get the number of output devices.
/// Returns -1 if you never called ::soundio_flush_events.
SOUNDIO_EXPORT int soundio_output_device_count(struct SoundIo *soundio);

/// Always returns a device. Call ::soundio_device_unref when done.
/// `index` must be 0 <= index < ::soundio_input_device_count
/// Returns NULL if you never called ::soundio_flush_events or if you provide
/// invalid parameter values.
SOUNDIO_EXPORT struct SoundIoDevice *soundio_get_input_device(struct SoundIo *soundio, int index);
/// Always returns a device. Call ::soundio_device_unref when done.
/// `index` must be 0 <= index < ::soundio_output_device_count
/// Returns NULL if you never called ::soundio_flush_events or if you provide
/// invalid parameter values.
SOUNDIO_EXPORT struct SoundIoDevice *soundio_get_output_device(struct SoundIo *soundio, int index);

/// returns the index of the default input device
/// returns -1 if there are no devices or if you never called
/// ::soundio_flush_events.
SOUNDIO_EXPORT int soundio_default_input_device_index(struct SoundIo *soundio);

/// returns the index of the default output device
/// returns -1 if there are no devices or if you never called
/// ::soundio_flush_events.
SOUNDIO_EXPORT int soundio_default_output_device_index(struct SoundIo *soundio);

/// Add 1 to the reference count of `device`.
SOUNDIO_EXPORT void soundio_device_ref(struct SoundIoDevice *device);
/// Remove 1 to the reference count of `device`. Clean up if it was the last
/// reference.
SOUNDIO_EXPORT void soundio_device_unref(struct SoundIoDevice *device);

/// Return `true` if and only if the devices have the same SoundIoDevice::id,
/// SoundIoDevice::is_raw, and SoundIoDevice::aim are the same.
SOUNDIO_EXPORT bool soundio_device_equal(
        const struct SoundIoDevice *a,
        const struct SoundIoDevice *b);

/// Sorts channel layouts by channel count, descending.
SOUNDIO_EXPORT void soundio_device_sort_channel_layouts(struct SoundIoDevice *device);

/// Convenience function. Returns whether `format` is included in the device's
/// supported formats.
SOUNDIO_EXPORT bool soundio_device_supports_format(struct SoundIoDevice *device,
        enum SoundIoFormat format);

/// Convenience function. Returns whether `layout` is included in the device's
/// supported channel layouts.
SOUNDIO_EXPORT bool soundio_device_supports_layout(struct SoundIoDevice *device,
        const struct SoundIoChannelLayout *layout);

/// Convenience function. Returns whether `sample_rate` is included in the
/// device's supported sample rates.
SOUNDIO_EXPORT bool soundio_device_supports_sample_rate(struct SoundIoDevice *device,
        int sample_rate);

/// Convenience function. Returns the available sample rate nearest to
/// `sample_rate`, rounding up.
SOUNDIO_EXPORT int soundio_device_nearest_sample_rate(struct SoundIoDevice *device,
        int sample_rate);



// Output Streams
/// Allocates memory and sets defaults. Next you should fill out the struct fields
/// and then call ::soundio_outstream_open. Sets all fields to defaults.
/// Returns `NULL` if and only if memory could not be allocated.
/// See also ::soundio_outstream_destroy
SOUNDIO_EXPORT struct SoundIoOutStream *soundio_outstream_create(struct SoundIoDevice *device);
/// You may not call this function from the SoundIoOutStream::write_callback thread context.
SOUNDIO_EXPORT void soundio_outstream_destroy(struct SoundIoOutStream *outstream);

/// After you call this function, SoundIoOutStream::software_latency is set to
/// the correct value.
///
/// The next thing to do is call ::soundio_outstream_start.
/// If this function returns an error, the outstream is in an invalid state and
/// you must call ::soundio_outstream_destroy on it.
///
/// Possible errors:
/// * #SoundIoErrorInvalid
///   * SoundIoDevice::aim is not #SoundIoDeviceAimOutput
///   * SoundIoOutStream::format is not valid
///   * SoundIoOutStream::channel_count is greater than #SOUNDIO_MAX_CHANNELS
/// * #SoundIoErrorNoMem
/// * #SoundIoErrorOpeningDevice
/// * #SoundIoErrorBackendDisconnected
/// * #SoundIoErrorSystemResources
/// * #SoundIoErrorNoSuchClient - when JACK returns `JackNoSuchClient`
/// * #SoundIoErrorIncompatibleBackend - SoundIoOutStream::channel_count is
///   greater than the number of channels the backend can handle.
/// * #SoundIoErrorIncompatibleDevice - stream parameters requested are not
///   compatible with the chosen device.
SOUNDIO_EXPORT enum SoundIoError soundio_outstream_open(struct SoundIoOutStream *outstream);

/// After you call this function, SoundIoOutStream::write_callback will be called.
///
/// This function might directly call SoundIoOutStream::write_callback.
///
/// Possible errors:
/// * #SoundIoErrorStreaming
/// * #SoundIoErrorNoMem
/// * #SoundIoErrorSystemResources
/// * #SoundIoErrorBackendDisconnected
SOUNDIO_EXPORT enum SoundIoError soundio_outstream_start(struct SoundIoOutStream *outstream);

/// Call this function when you are ready to begin writing to the device buffer.
///  * `outstream` - (in) The output stream you want to write to.
///  * `areas` - (out) The memory addresses you can write data to, one per
///    channel. It is OK to modify the pointers if that helps you iterate.
///  * `frame_count` - (in/out) Provide the number of frames you want to write.
///    Returned will be the number of frames you can actually write, which is
///    also the number of frames that will be written when you call
///    ::soundio_outstream_end_write. The value returned will always be less
///    than or equal to the value provided.
/// It is your responsibility to call this function exactly as many times as
/// necessary to meet the `frame_count_min` and `frame_count_max` criteria from
/// SoundIoOutStream::write_callback.
/// You must call this function only from the SoundIoOutStream::write_callback thread context.
/// After calling this function, write data to `areas` and then call
/// ::soundio_outstream_end_write.
/// If this function returns an error, do not call ::soundio_outstream_end_write.
///
/// Possible errors:
/// * #SoundIoErrorInvalid
///   * `*frame_count` <= 0
///   * `*frame_count` < `frame_count_min` or `*frame_count` > `frame_count_max`
///   * function called too many times without respecting `frame_count_max`
/// * #SoundIoErrorStreaming
/// * #SoundIoErrorUnderflow - an underflow caused this call to fail. You might
///   also get a SoundIoOutStream::underflow_callback, and you might not get
///   this error code when an underflow occurs. Unlike #SoundIoErrorStreaming,
///   the outstream is still in a valid state and streaming can continue.
/// * #SoundIoErrorIncompatibleDevice - in rare cases it might just now
///   be discovered that the device uses non-byte-aligned access, in which
///   case this error code is returned.
SOUNDIO_EXPORT enum SoundIoError soundio_outstream_begin_write(struct SoundIoOutStream *outstream,
        struct SoundIoChannelArea **areas, int *frame_count);

/// Commits the write that you began with ::soundio_outstream_begin_write.
/// You must call this function only from the SoundIoOutStream::write_callback thread context.
///
/// Possible errors:
/// * #SoundIoErrorStreaming
/// * #SoundIoErrorUnderflow - an underflow caused this call to fail. You might
///   also get a SoundIoOutStream::underflow_callback, and you might not get
///   this error code when an underflow occurs. Unlike #SoundIoErrorStreaming,
///   the outstream is still in a valid state and streaming can continue.
SOUNDIO_EXPORT enum SoundIoError soundio_outstream_end_write(struct SoundIoOutStream *outstream);

/// Clears the output stream buffer.
/// This function can be called from any thread.
/// This function can be called regardless of whether the outstream is paused
/// or not.
/// Some backends do not support clearing the buffer. On these backends this
/// function will return SoundIoErrorIncompatibleBackend.
/// Some devices do not support clearing the buffer. On these devices this
/// function might return SoundIoErrorIncompatibleDevice.
/// Possible errors:
///
/// * #SoundIoErrorStreaming
/// * #SoundIoErrorIncompatibleBackend
/// * #SoundIoErrorIncompatibleDevice
SOUNDIO_EXPORT enum SoundIoError soundio_outstream_clear_buffer(struct SoundIoOutStream *outstream);

/// If the underlying backend and device support pausing, this pauses the
/// stream. SoundIoOutStream::write_callback may be called a few more times if
/// the buffer is not full.
/// Pausing might put the hardware into a low power state which is ideal if your
/// software is silent for some time.
/// This function may be called from any thread context, including
/// SoundIoOutStream::write_callback.
/// Pausing when already paused or unpausing when already unpaused has no
/// effect and returns #SoundIoErrorNone.
///
/// Possible errors:
/// * #SoundIoErrorBackendDisconnected
/// * #SoundIoErrorStreaming
/// * #SoundIoErrorIncompatibleDevice - device does not support
///   pausing/unpausing. This error code might not be returned even if the
///   device does not support pausing/unpausing.
/// * #SoundIoErrorIncompatibleBackend - backend does not support
///   pausing/unpausing.
/// * #SoundIoErrorInvalid - outstream not opened and started
SOUNDIO_EXPORT enum SoundIoError soundio_outstream_pause(struct SoundIoOutStream *outstream, bool pause);

/// Obtain the total number of seconds that the next frame written after the
/// last frame written with ::soundio_outstream_end_write will take to become
/// audible. This includes both software and hardware latency. In other words,
/// if you call this function directly after calling ::soundio_outstream_end_write,
/// this gives you the number of seconds that the next frame written will take
/// to become audible.
///
/// This function must be called only from within SoundIoOutStream::write_callback.
///
/// Possible errors:
/// * #SoundIoErrorStreaming
SOUNDIO_EXPORT enum SoundIoError soundio_outstream_get_latency(struct SoundIoOutStream *outstream,
        double *out_latency);



// Input Streams
/// Allocates memory and sets defaults. Next you should fill out the struct fields
/// and then call ::soundio_instream_open. Sets all fields to defaults.
/// Returns `NULL` if and only if memory could not be allocated.
/// See also ::soundio_instream_destroy
SOUNDIO_EXPORT struct SoundIoInStream *soundio_instream_create(struct SoundIoDevice *device);
/// You may not call this function from SoundIoInStream::read_callback.
SOUNDIO_EXPORT void soundio_instream_destroy(struct SoundIoInStream *instream);

/// After you call this function, SoundIoInStream::software_latency is set to the correct
/// value.
/// The next thing to do is call ::soundio_instream_start.
/// If this function returns an error, the instream is in an invalid state and
/// you must call ::soundio_instream_destroy on it.
///
/// Possible errors:
/// * #SoundIoErrorInvalid
///   * device aim is not #SoundIoDeviceAimInput
///   * format is not valid
///   * requested layout channel count > #SOUNDIO_MAX_CHANNELS
/// * #SoundIoErrorOpeningDevice
/// * #SoundIoErrorNoMem
/// * #SoundIoErrorBackendDisconnected
/// * #SoundIoErrorSystemResources
/// * #SoundIoErrorNoSuchClient
/// * #SoundIoErrorIncompatibleBackend
/// * #SoundIoErrorIncompatibleDevice
SOUNDIO_EXPORT enum SoundIoError soundio_instream_open(struct SoundIoInStream *instream);

/// After you call this function, SoundIoInStream::read_callback will be called.
///
/// Possible errors:
/// * #SoundIoErrorBackendDisconnected
/// * #SoundIoErrorStreaming
/// * #SoundIoErrorOpeningDevice
/// * #SoundIoErrorSystemResources
SOUNDIO_EXPORT enum SoundIoError soundio_instream_start(struct SoundIoInStream *instream);

/// Call this function when you are ready to begin reading from the device
/// buffer.
/// * `instream` - (in) The input stream you want to read from.
/// * `areas` - (out) The memory addresses you can read data from. It is OK
///   to modify the pointers if that helps you iterate. There might be a "hole"
///   in the buffer. To indicate this, `areas` will be `NULL` and `frame_count`
///   tells how big the hole is in frames.
/// * `frame_count` - (in/out) - Provide the number of frames you want to read;
///   returns the number of frames you can actually read. The returned value
///   will always be less than or equal to the provided value. If the provided
///   value is less than `frame_count_min` from SoundIoInStream::read_callback this function
///   returns with #SoundIoErrorInvalid.
/// It is your responsibility to call this function no more and no fewer than the
/// correct number of times according to the `frame_count_min` and
/// `frame_count_max` criteria from SoundIoInStream::read_callback.
/// You must call this function only from the SoundIoInStream::read_callback thread context.
/// After calling this function, read data from `areas` and then use
/// ::soundio_instream_end_read` to actually remove the data from the buffer
/// and move the read index forward. ::soundio_instream_end_read should not be
/// called if the buffer is empty (`frame_count` == 0), but it should be called
/// if there is a hole.
///
/// Possible errors:
/// * #SoundIoErrorInvalid
///   * `*frame_count` < `frame_count_min` or `*frame_count` > `frame_count_max`
/// * #SoundIoErrorStreaming
/// * #SoundIoErrorIncompatibleDevice - in rare cases it might just now
///   be discovered that the device uses non-byte-aligned access, in which
///   case this error code is returned.
SOUNDIO_EXPORT enum SoundIoError soundio_instream_begin_read(struct SoundIoInStream *instream,
        struct SoundIoChannelArea **areas, int *frame_count);
/// This will drop all of the frames from when you called
/// ::soundio_instream_begin_read.
/// You must call this function only from the SoundIoInStream::read_callback thread context.
/// You must call this function only after a successful call to
/// ::soundio_instream_begin_read.
///
/// Possible errors:
/// * #SoundIoErrorStreaming
SOUNDIO_EXPORT enum SoundIoError soundio_instream_end_read(struct SoundIoInStream *instream);

/// If the underyling device supports pausing, this pauses the stream and
/// prevents SoundIoInStream::read_callback from being called. Otherwise this returns
/// #SoundIoErrorIncompatibleDevice.
/// This function may be called from any thread.
/// Pausing when already paused or unpausing when already unpaused has no
/// effect and always returns #SoundIoErrorNone.
///
/// Possible errors:
/// * #SoundIoErrorBackendDisconnected
/// * #SoundIoErrorStreaming
/// * #SoundIoErrorIncompatibleDevice - device does not support pausing/unpausing
SOUNDIO_EXPORT enum SoundIoError soundio_instream_pause(struct SoundIoInStream *instream, bool pause);

/// Obtain the number of seconds that the next frame of sound being
/// captured will take to arrive in the buffer, plus the amount of time that is
/// represented in the buffer. This includes both software and hardware latency.
///
/// This function must be called only from within SoundIoInStream::read_callback.
///
/// Possible errors:
/// * #SoundIoErrorStreaming
SOUNDIO_EXPORT enum SoundIoError soundio_instream_get_latency(struct SoundIoInStream *instream,
        double *out_latency);


struct SoundIoRingBuffer;

/// A ring buffer is a single-reader single-writer lock-free fixed-size queue.
/// libsoundio ring buffers use memory mapping techniques to enable a
/// contiguous buffer when reading or writing across the boundary of the ring
/// buffer's capacity.
/// `requested_capacity` in bytes.
/// Returns `NULL` if and only if memory could not be allocated.
/// Use ::soundio_ring_buffer_capacity to get the actual capacity, which might
/// be greater for alignment purposes.
/// See also ::soundio_ring_buffer_destroy
SOUNDIO_EXPORT struct SoundIoRingBuffer *soundio_ring_buffer_create(struct SoundIo *soundio, int requested_capacity);
SOUNDIO_EXPORT void soundio_ring_buffer_destroy(struct SoundIoRingBuffer *ring_buffer);

/// When you create a ring buffer, capacity might be more than the requested
/// capacity for alignment purposes. This function returns the actual capacity.
SOUNDIO_EXPORT int soundio_ring_buffer_capacity(struct SoundIoRingBuffer *ring_buffer);

/// Do not write more than capacity.
SOUNDIO_EXPORT char *soundio_ring_buffer_write_ptr(struct SoundIoRingBuffer *ring_buffer);
/// `count` in bytes.
SOUNDIO_EXPORT void soundio_ring_buffer_advance_write_ptr(struct SoundIoRingBuffer *ring_buffer, int count);

/// Do not read more than capacity.
SOUNDIO_EXPORT char *soundio_ring_buffer_read_ptr(struct SoundIoRingBuffer *ring_buffer);
/// `count` in bytes.
SOUNDIO_EXPORT void soundio_ring_buffer_advance_read_ptr(struct SoundIoRingBuffer *ring_buffer, int count);

/// Returns how many bytes of the buffer is used, ready for reading.
SOUNDIO_EXPORT int soundio_ring_buffer_fill_count(struct SoundIoRingBuffer *ring_buffer);

/// Returns how many bytes of the buffer is free, ready for writing.
SOUNDIO_EXPORT int soundio_ring_buffer_free_count(struct SoundIoRingBuffer *ring_buffer);

/// Must be called by the writer.
SOUNDIO_EXPORT void soundio_ring_buffer_clear(struct SoundIoRingBuffer *ring_buffer);

#endif
