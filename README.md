# libsoundio

C library which provides cross-platform audio input and output. The API is
suitable for real-time software such as digital audio workstations as well
as consumer software such as music players.

This library is an abstraction; however it prioritizes performance and power
over API convenience. Features that only exist in some sound backends are
exposed.

This library is a work-in-progress.

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

libsoundio is programmed in a tiny subset of C++:

 * No STL.
 * No `new` or `delete`.
 * No `class`. All fields in structs are `public`.
 * No exceptions or run-time type information.
 * No references.
 * No linking against libstdc++.

## Roadmap

 0. Dummy
 0. PulseAudio
 0. JACK
 0. ALSA (Linux)
 0. CoreAudio (OSX)
 0. ASIO (Windows)
 0. DirectSound (Windows)
 0. OSS (BSD)

## Planned Uses for libsoundio

 * [Genesis](https://github.com/andrewrk/genesis)
 * [libgroove](https://github.com/andrewrk/libgroove) ([Groove Basin](https://github.com/andrewrk/groovebasin))
