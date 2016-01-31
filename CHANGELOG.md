### Version 1.1.0 (2016-01-31)

 * JACK: delete broken pause implementation. Previously, calling
   `soundio_outstream_pause` or `soundio_instream_pause` during the
   `write_callback` or `read_callback` would cause a deadlock. Now, attempting
   to pause always results in `SoundIoErrorBackendIncompatible`.
 * PulseAudio: improve latency handling code. It now passes the latency test
   along with all the other backends.
 * PulseAudio: fix incorrect outstream `software_latency`.
 * libsoundio source code is now pure C, no C++ mixed in.
 * ALSA: better device detection.
   - No longer suppress sysdefault.
   - If default and sysdefault are missing, use the first device as the default
     device.
 * Workaround for Raspberry Pi driver that incorrectly reports itself as Output
   when it is actually Input.
 * ALSA: let alsa lib choose period settings. Fixes behavior with many ALSA
   devices.
 * ALSA: fix potential cleanup deadlock.
 * ALSA: fix crash for devices with null description, thanks to Charles Lehner.
 * CoreAudio: drop support for MacOS 10.9. There was a bug for this system that
   was never resolved, so it didn't work in the first place.
 * Record example handles device not found and probe errors gracefully.
 * Fix typo in microphone example, thanks to James Dyson.
 * Improve documentation.
 * New functions available: `soundio_version_string`, `soundio_version_major`,
   `soundio_version_minor`, `soundio_version_patch`.
 * libsoundio source code now builds with MSVC, thanks to Raphaël Londeix.

### Version 1.0.3 (2015-10-20)

 * Architecture independent header files.
 * Add --latency and --sample-rate to sine example.
 * ALSA: fix deadlock under some circumstances.
 * dummy: fix deadlock when pause called from `write_callback`.
 * Fix double clean-up corruption when opening stream fails.
 * Add --device and --raw to underflow test.
 * ALSA: use period size to calculate buffer size, fixes opening output stream
   sometimes resulting in an error.

### Version 1.0.2 (2015-09-24)

 * build: fix GNUInstallDirs not working.
 * docs: fix incorrect docs for `soundio_instream_pause`.
 * PulseAudio: fix `soundio_outstream_pause` triggering assertion when called
   from within `write_callback`.
 * fix mirrored memory not working on Linux (fixes corrupted data in ring
   buffer).
 * os: fix crash when creating non high priority thread fails.
 * docs: fix typos and cleanup.
 * fix and add test for `soundio_device_nearest_sample_rate`.

### Version 1.0.1 (2015-09-11)

 * libsoundio no longer depends on or links against libm.
 * ALSA: treat ALSA as unavailable when /dev/snd does not exist.
 * ALSA: remove duplicate assert.
 * ALSA: remove stray print statement.
 * ALSA: pausing returns error code when state is invalid instead of reaching
   assertion failure in pcm.c.
 * JACK: fix infinite loop when refreshing devices.
 * PulseAudio: better clear buffer implementation.
 * dummy backend: fix sometimes calling `write_callback` with
  `frame_count_max` equal to 0.
 * os: fix some variables accidentally not declared static.
 * macos: fix not cleaning up condition variables.
 * macos: avoid allocation when getting time.
 * docs: note that `read_callback` and `write_callback` must be real time safe.
 * docs: record example demonstrates proper real time safety by not calling
   fwrite in `read_callback`.
 * docs: add note to record example about shutting down.
 * docs: make microphone example latency a command line argument.
 * build: fix build on linux with clang.
 * build: static libs, examples, and tests are optional.

### Version 1.0.0 (2015-09-03)

 * Initial public release.
