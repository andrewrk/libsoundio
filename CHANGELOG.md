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
