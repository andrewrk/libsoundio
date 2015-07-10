# libsoundio

C library which provides cross-platform audio input and output. The API is
suitable for real-time software such as digital audio workstations as well
as consumer software such as music players.

This library is an abstraction; however it prioritizes performance and power
over API convenience. Features that only exist in some sound backends are
exposed.

**This library is a work-in-progress.**

## Alternatives

 * [PortAudio](http://www.portaudio.com/)
   - It does not support [PulseAudio](http://www.freedesktop.org/wiki/Software/PulseAudio/).
   - It logs messages to stdio and you can't turn that off.
   - It is not written by me.
 * [rtaudio](https://www.music.mcgill.ca/~gary/rtaudio/)
   - It is not a C library.
   - It uses [exceptions](http://stackoverflow.com/questions/1736146/why-is-exception-handling-bad).
   - It is not written by me.
 * [SDL](https://www.libsdl.org/)
   - It comes with a bunch of other baggage - display, windowing, input
     handling, and lots more.
   - It is not designed with real-time low latency audio in mind.
   - Listing audio devices is [broken](https://github.com/andrewrk/node-groove/issues/13).
   - It does not support recording devices.
   - It is not written by me.

## How It Works

libsoundio tries these backends in order. If unable to connect to that backend,
due to the backend not being installed, or the server not running, or the
platform is wrong, the next backend is tried.

 0. JACK
 0. PulseAudio
 0. ALSA (Linux)
 0. CoreAudio (OSX)
 0. ASIO (Windows)
 0. DirectSound (Windows)
 0. OSS (BSD)
 0. Dummy

## Contributing

libsoundio is programmed in a tiny subset of C++11:

 * No STL.
 * No `new` or `delete`.
 * No `class`. All fields in structs are `public`.
 * No exceptions or run-time type information.
 * No references.
 * No linking against libstdc++.

Don't get tricked - this is a *C library*, not a C++ library. We just take
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
make gcc
```

Then in the libsoundio source directory (replace "/path/to/mxe" with the
appropriate path):

```
mkdir build-win
cd build-win
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/mxe/usr/i686-w64-mingw32.static/share/cmake/mxe-conf.cmake
make
```

#### Running the Tests

```
make test
```

For more detailed output:

```
make
./unit_tests
```

To see test coverage, install lcov, run `make coverage` and then
view `coverage/index.html` in a browser.

## Roadmap

 0. implement ALSA (Linux) backend, get examples working
 0. pipe record to playback example working with dummy linux, osx, windows
 0. pipe record to playback example working with pulseaudio linux
 0. implement CoreAudio (OSX) backend, get examples working
 0. implement DirectSound (Windows) backend, get examples working
 0. implement JACK backend, get examples working
 0. Avoid calling `panic` in PulseAudio.
 0. implement ASIO (Windows) backend, get examples working
 0. clean up API and improve documentation
    - make sure every function which can return an error documents which errors
      it can return
    - consider doing the public/private struct thing and make `backend_data` a
      union instead of a `void *`
 0. use a documentation generator and host the docs somewhere
 0. -fvisibility=hidden and then explicitly export stuff
 0. Integrate into libgroove and test with Groove Basin
 0. Consider testing on FreeBSD
 0. look at microphone example and determine if fewer memcpys can be done
    with the audio data
    - pulseaudio has peek() drop() which sucks, but what if libsoundio lets you
      specify how much to peek() and if you don't peek all of it, save the
      unused to a buffer for you.
 0. add len arguments to APIs that have char *

## Planned Uses for libsoundio

 * [Genesis](https://github.com/andrewrk/genesis)
 * [libgroove](https://github.com/andrewrk/libgroove) ([Groove Basin](https://github.com/andrewrk/groovebasin))
