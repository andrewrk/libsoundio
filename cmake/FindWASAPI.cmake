# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# WASAPI_FOUND
# WASAPI_INCLUDE_DIR
# WASAPI_LIBRARIES

if (WIN32)
    find_path(WASAPI_INCLUDE_DIR NAMES audioclient.h)

    set(WASAPI_LIBRARIES "")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WASAPI DEFAULT_MSG WASAPI_INCLUDE_DIR)

mark_as_advanced(WASAPI_INCLUDE_DIR WASAPI_LIBRARIES)
