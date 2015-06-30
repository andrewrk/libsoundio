with import <nixpkgs> {}; {
  libsoundioEnv = stdenv.mkDerivation {
    name = "libsoundio";
    buildInputs = [
      alsaLib
      cmake
      gcc49
      libjack2
      libpulseaudio
    ];
  };
}
