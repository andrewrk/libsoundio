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
