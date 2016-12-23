# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# JACK_FOUND
# JACK_INCLUDE_DIR
# JACK_LIBRARY

find_path(JACK_INCLUDE_DIR NAMES jack/jack.h)

find_library(JACK_LIBRARY NAMES jack)

include(CheckLibraryExists)
check_library_exists(jack "jack_set_port_rename_callback" "${JACK_LIBRARY}" HAVE_jack_set_port_rename_callback)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JACK DEFAULT_MSG JACK_LIBRARY JACK_INCLUDE_DIR HAVE_jack_set_port_rename_callback)

mark_as_advanced(JACK_INCLUDE_DIR JACK_LIBRARY)
