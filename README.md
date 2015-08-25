# libsoundio

C99 library providing cross-platform audio input and output. The API is
suitable for real-time software such as digital audio workstations as well
as consumer software such as music players.

This library is an abstraction; however in the delicate balance between
performance and power, and API convenience, the scale is tipped closer to
the former. Features that only exist in some sound backends are exposed.

The goal of this library is to be the only resource needed to implement top
quality audio playback and capture on desktop and laptop systems. This
includes detailed documentation explaining how audio works on each supported
backend, how they are abstracted to provide the libsoundio API, and what
assumptions you can and cannot make in order to guarantee consistent, reliable
behavior on every platform.

**This project is a work-in-progress.**

## Features and Limitations

 * Supported backends:
   - [JACK](http://jackaudio.org/)
   - [PulseAudio](http://www.freedesktop.org/wiki/Software/PulseAudio/)
   - [ALSA](http://www.alsa-project.org/)
   - [CoreAudio](https://developer.apple.com/library/mac/documentation/MusicAudio/Conceptual/CoreAudioOverview/Introduction/Introduction.html)
   - [WASAPI](https://msdn.microsoft.com/en-us/library/windows/desktop/dd371455%28v=vs.85%29.aspx)
   - Dummy (silence)
 * Exposes both raw devices and shared devices. Raw devices give you the best
   performance but prevent other applications from using them. Shared devices
   are default and usually provide sample rate conversion and format
   conversion.
 * Exposes both device id and friendly name. id you could save in a config file
   because it persists between devices becoming plugged and unplugged, while
   friendly name is suitable for exposing to users.
 * Supports optimal usage of each supported backend. The same API does the
   right thing whether the backend has a fixed buffer size, such as on JACK and
   CoreAudio, or whether it allows directly managing the buffer, such as on
   ALSA, PulseAudio, and WASAPI.
 * C library. Depends only on the respective backend API libraries and libc.
   Does *not* depend on libstdc++, and does *not* have exceptions, run-time type
   information, or [setjmp](http://latentcontent.net/2007/12/05/libpng-worst-api-ever/).
 * Errors are communicated via return codes, not logging to stdio.
 * Supports channel layouts (also known as channel maps), important for
   surround sound applications.
 * Ability to monitor devices and get an event when available devices change.
 * Ability to get an event when the backend is disconnected, for example when
   the JACK server or PulseAudio server shuts down.
 * Detects which input device is default and which output device is default.
 * Ability to connect to multiple backends at once. For example you could have
   an ALSA device open and a JACK device open at the same time.
 * Meticulously checks all return codes and memory allocations and uses
   meaningful error codes.
 * Exposes extra API that is only available on some backends. For example you
   can provide application name and stream names which is used by JACK and
   PulseAudio.

## Synopsis

Complete program to emit a sine wave over the default device using the best
backend:

```c
#include <soundio/soundio.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void panic(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}

static const float PI = 3.1415926535f;
static float seconds_offset = 0.0f;
static void write_callback(struct SoundIoOutStream *outstream,
        int frame_count_min, int frame_count_max)
{
    const struct SoundIoChannelLayout *layout = &outstream->layout;
    float float_sample_rate = outstream->sample_rate;
    float seconds_per_frame = 1.0f / float_sample_rate;
    struct SoundIoChannelArea *areas;
    int frames_left = frame_count_max;
    int err;

    while (frames_left > 0) {
        int frame_count = frames_left;

        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count)))
            panic("%s", soundio_strerror(err));

        if (!frame_count)
            break;

        float pitch = 440.0f;
        float radians_per_second = pitch * 2.0f * PI;
        for (int frame = 0; frame < frame_count; frame += 1) {
            float sample = sinf((seconds_offset + frame * seconds_per_frame) * radians_per_second);
            for (int channel = 0; channel < layout->channel_count; channel += 1) {
                float *ptr = (float*)(areas[channel].ptr + areas[channel].step * frame);
                *ptr = sample;
            }
        }
        seconds_offset += seconds_per_frame * frame_count;

        if ((err = soundio_outstream_end_write(outstream)))
            panic("%s", soundio_strerror(err));

        frames_left -= frame_count;
    }
}

int main(int argc, char **argv) {
    int err;
    struct SoundIo *soundio = soundio_create();
    if (!soundio)
        panic("out of memory");

    if ((err = soundio_connect(soundio)))
        panic("error connecting: %s", soundio_strerror(err));

    soundio_flush_events(soundio);

    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0)
        panic("no output device found");

    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    if (!device)
        panic("out of memory");

    fprintf(stderr, "Output device: %s\n", device->name);

    struct SoundIoOutStream *outstream = soundio_outstream_create(device);
    outstream->format = SoundIoFormatFloat32NE;
    outstream->write_callback = write_callback;

    if ((err = soundio_outstream_open(outstream)))
        panic("unable to open device: %s", soundio_strerror(err));

    if (outstream->layout_error)
        fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

    if ((err = soundio_outstream_start(outstream)))
        panic("unable to start device: %s", soundio_strerror(err));

    for (;;)
        soundio_wait_events(soundio);

    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);
    return 0;
}
```

### Backend Priority

When you use `soundio_connect`, libsoundio tries these backends in order.
If unable to connect to that backend, due to the backend not being installed,
or the server not running, or the platform is wrong, the next backend is tried.

 0. JACK
 0. PulseAudio
 0. ALSA (Linux)
 0. CoreAudio (OSX)
 0. WASAPI (Windows)
 0. Dummy

If you don't like this order, you can use `soundio_connect_backend` to
explicitly choose a backend to connect to. You can use `soundio_backend_count`
and `soundio_get_backend` to get the list of available backends.

For complete API documentation, see `src/soundio.h`.

## Contributing

libsoundio is programmed in a tiny subset of C++11:

 * No STL.
 * No `new` or `delete`.
 * No `class`. All fields in structs are `public`.
 * No constructors or destructors.
 * No exceptions or run-time type information.
 * No references.
 * No linking against libstdc++.

Do not be fooled - this is a *C library*, not a C++ library. We just take
advantage of a select few C++11 compiler features such as templates, and then
link against libc.

### Building

Install the dependencies:

 * cmake
 * ALSA library (optional)
 * libjack2 (optional)
 * libpulseaudio (optional)

```
mkdir build
cd build
cmake ..
make
sudo make install
```

### Building for Windows

You can build libsoundio with [mxe](http://mxe.cc/). Follow the
[requirements](http://mxe.cc/#requirements) section to install the
packages necessary on your system. Then somewhere on your file system:

```
git clone https://github.com/mxe/mxe
cd mxe
make MXE_TARGETS='x86_64-w64-mingw32.static i686-w64-mingw32.static' gcc
```

Then in the libsoundio source directory (replace "/path/to/mxe" with the
appropriate path):

```
mkdir build-win32
cd build-win32
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/mxe/usr/i686-w64-mingw32.static/share/cmake/mxe-conf.cmake
make
```

```
mkdir build-win64
cd build-win64
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/mxe/usr/x86_64-w64-mingw32.static/share/cmake/mxe-conf.cmake
make
```

### Testing

For each backend, do the following:

 0. Run the unit tests: `./unit_tests`. To see test coverage, install lcov, run
   `make coverage`, and then view `coverage/index.html` in a browser.
 0. Run the example `./sio_list_devices` and make sure it does not crash, and
    the output looks good. If valgrind is available, use it.
 0. Run `./sio_list_devices --watch` and make sure it detects when you plug and
    unplug a USB microphone.
 0. Run `./sio_sine` and make sure you hear a sine wave. For backends with raw
    devices, run `./sio_sine --device id` (where 'id' is a device id you got
    from `sio_list_devices` and make sure you hear a sine wave.
 0. Run `./underflow` and read the testing instructions that it prints.
 0. Run `./sio_microphone` and ensure that it is both recording and playing
    back correctly. If possible use the `--in-device` and `--out-device`
    parameters to test a USB microphone in raw mode.

## Roadmap

 0. implement WASAPI (Windows) backend, get examples working
    - move the bulk of the `outstream_open_wasapi` code to the thread and
      have them communicate back and forth. because the thread has to do
      weird thread-local com stuff, and all that com stuff really needs to be
      called from the same thread.
 0. Make sure PulseAudio can handle refresh devices crashing before
    block_until_have_devices
 0. Integrate into libgroove and test with Groove Basin
 0. clear buffer maybe could take an argument to say how many frames to not clear
 0. create a test for clear buffer; ensure pause/play semantics work
 0. Verify that JACK xrun callback context is the same as process callback.
    If not, might need to hav xrun callback set a flag and have process callback
    call the underflow callback.
 0. Create a test for pausing and resuming input and output streams.
    - Should pause/resume be callable from outside the callbacks?
    - Ensure double pausing / double resuming works fine.
 0. Create a test for the latency / synchronization API.
    - Input is an audio file and some events indexed at particular frame - when
      listening the events should line up exactly with a beat or visual
      indicator, even when the latency is large.
    - Play the audio file, have the user press an input right at the beat. Find
      out what the frame index it thinks the user pressed it at and make sure
      that is correct.
 0. Create a test for input stream overflow handling.
 0. Allow calling functions from outside the callbacks as long as they first
    call lock and then unlock when done.
 0. use a documentation generator and host the docs somewhere
 0. add len arguments to APIs that have char *
    - replace strdup with `soundio_str_dupe`
 0. Support PulseAudio proplist properties for main context and streams
 0. Expose JACK options in `jack_client_open`
 0. mlock memory which is accessed in the real time path
 0. make rtprio warning a callback and have existing behavior be the default callback
 0. write detailed docs on buffer underflows explaining when they occur, what state
    changes are related to them, and how to recover from them.
 0. Consider testing on FreeBSD
 0. In ALSA do we need to wake up the poll when destroying the in or out stream?
 0. Detect PulseAudio server going offline and emit `on_backend_disconnect`.
 0. Add [sndio](http://www.sndio.org/) backend to support OpenBSD.
 0. Custom allocator support
 0. Support for stream icon.
    - PulseAudio: XDG icon name
    - WASAPI: path to .exe, .dll, or .ico
    - CoreAudio: CFURLRef image file
 0. clean up API and improve documentation
    - make sure every function which can return an error documents which errors
      it can return

## Planned Uses for libsoundio

 * [Genesis](https://github.com/andrewrk/genesis)
 * [libgroove](https://github.com/andrewrk/libgroove) ([Groove Basin](https://github.com/andrewrk/groovebasin))
