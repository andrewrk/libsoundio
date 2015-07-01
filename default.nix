with import <nixpkgs> {}; {
  libsoundioEnv = stdenv.mkDerivation {
    name = "libsoundio";
    buildInputs = [
      alsaLib
      cmake
      gcc5
      libjack2
      libpulseaudio
    ];
  };
}
