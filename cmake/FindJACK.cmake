# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# JACK_FOUND
# JACK_INCLUDE_DIR
# JACK_LIBRARY

find_path(JACK_INCLUDE_DIR NAMES jack/jack.h)

find_library(JACK_LIBRARY NAMES jack)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JACK DEFAULT_MSG JACK_LIBRARY JACK_INCLUDE_DIR)

mark_as_advanced(JACK_INCLUDE_DIR JACK_LIBRARY)
