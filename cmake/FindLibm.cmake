# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# LIBM_FOUND
# LIBM_LIBRARY

include(CheckCSourceCompiles)

set(find_math_program
    "#include <math.h>
    int main() {
      sin(0.0);
      return 0;
    }")

set(CMAKE_REQUIRED_FLAGS "")
set(CMAKE_REQUIRED_LIBRARIES "")
check_c_source_compiles("${find_math_program}" libm_linked_automatically)
if(libm_linked_automatically)
    set(LIBM_FOUND TRUE)
    set(LIBM_LIBRARY "")
    return()
endif()

set(CMAKE_REQUIRED_LIBRARIES "m")
check_c_source_compiles("${find_math_program}" libm_linked_explicitly)
if(libm_linked_explicitly)
    set(LIBM_FOUND TRUE)
    set(LIBM_LIBRARY "m")
    return()
endif()

set(LIBM_FOUND FALSE)
set(LIBM_LIBRARY "")
message(SEND_ERROR "-- Could NOT figure out how to link libm")
