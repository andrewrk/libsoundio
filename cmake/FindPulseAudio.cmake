# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# PULSEAUDIO_FOUND
# PULSEAUDIO_INCLUDE_DIR
# PULSEAUDIO_LIBRARY

find_path(PULSEAUDIO_INCLUDE_DIR NAMES pulse/pulseaudio.h)

find_library(PULSEAUDIO_LIBRARY NAMES pulse)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PULSEAUDIO DEFAULT_MSG PULSEAUDIO_LIBRARY PULSEAUDIO_INCLUDE_DIR)

mark_as_advanced(PULSEAUDIO_INCLUDE_DIR PULSEAUDIO_LIBRARY)
