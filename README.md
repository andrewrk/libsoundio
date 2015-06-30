# libsoundio

C library which provides cross-platform audio input and output. The API is
suitable for real-time software such as digital audio workstations as well
as consumer software such as music players.

## How It Works

On Linux, libsoundio tries in order: JACK, PulseAudio, ALSA, Dummy.
On OSX, libsoundio tries in order: JACK, PulseAudio, CoreAudio, Dummy.
On Windows, libsoundio tries in order: ASIO, DirectSound, Dummy.
On BSD, libsoundio tries in order: JACK, PulseAudio, OSS, Dummy.

## Roadmap

 * Dummy (all)
 * PulseAudio (Linux, OSX)
 * ALSA (Linux)
 * JACK (Linux, OSX)
 * CoreAudio (OSX)
 * DirectSound (Windows)
 * ASIO (Windows)
 * OSS (BSD)
