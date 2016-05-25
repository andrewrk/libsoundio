# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# WASAPI_FOUND
# AUDIOCLIENT_H

if (WIN32)
  include(CheckIncludeFile)
  check_include_file(audioclient.h AUDIOCLIENT_H)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WASAPI DEFAULT_MSG AUDIOCLIENT_H)
mark_as_advanced(AUDIOCLIENT_H)
