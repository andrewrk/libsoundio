cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0042 NEW)
cmake_policy(SET CMP0046 NEW)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

project(libsoundio C CXX)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(LIBSOUNDIO_VERSION_MAJOR 0)
set(LIBSOUNDIO_VERSION_MINOR 0)
set(LIBSOUNDIO_VERSION_PATCH 0)
set(LIBSOUNDIO_VERSION "${LIBSOUNDIO_VERSION_MAJOR}.${LIBSOUNDIO_VERSION_MINOR}.${LIBSOUNDIO_VERSION_PATCH}")
message("Configuring libsoundio version ${LIBSOUNDIO_VERSION}")

find_package(Threads)
if(Threads_FOUND)
    set(STATUS_THREADS "OK")
else(Threads_FOUND)
    set(STATUS_THREADS "not found")
endif(Threads_FOUND)

find_package(JACK)
if(JACK_FOUND)
    set(STATUS_JACK "OK")
    set(SOUNDIO_HAVE_JACK true)
    include_directories(${JACK_INCLUDE_DIR})
else()
    set(STATUS_JACK "not found")
    set(SOUNDIO_HAVE_JACK false)
    set(JACK_LIBRARY "")
endif()

find_package(PulseAudio)
if(PULSEAUDIO_FOUND)
    set(STATUS_PULSEAUDIO "OK")
    set(SOUNDIO_HAVE_PULSEAUDIO true)
    include_directories(${PULSEAUDIO_INCLUDE_DIR})
else()
    set(STATUS_PULSEAUDIO "not found")
    set(SOUNDIO_HAVE_PULSEAUDIO false)
    set(PULSEAUDIO_LIBRARY "")
endif()

find_package(ALSA)
if(ALSA_FOUND)
    set(STATUS_ALSA "OK")
    set(SOUNDIO_HAVE_ALSA true)
    include_directories(${ALSA_INCLUDE_DIRS})
else()
    set(STATUS_ALSA "not found")
    set(SOUNDIO_HAVE_ALSA false)
    set(ALSA_LIBRARIES "")
endif()

set(LIBSOUNDIO_SOURCES
    "${CMAKE_SOURCE_DIR}/src/soundio.cpp"
    "${CMAKE_SOURCE_DIR}/src/util.cpp"
    "${CMAKE_SOURCE_DIR}/src/os.cpp"
    "${CMAKE_SOURCE_DIR}/src/dummy.cpp"
    "${CMAKE_SOURCE_DIR}/src/channel_layout.cpp"
    "${CMAKE_SOURCE_DIR}/src/ring_buffer.cpp"
)

set(CONFIGURE_OUT_FILE "${CMAKE_BINARY_DIR}/config.h")
set(LIBSOUNDIO_HEADERS
    "${CMAKE_SOURCE_DIR}/soundio/soundio.h"
    "${CMAKE_SOURCE_DIR}/soundio/os.h"
    ${CONFIGURE_OUT_FILE}
)

set(TEST_SOURCES
    "${CMAKE_SOURCE_DIR}/test/unit_tests.cpp"
    "${CMAKE_SOURCE_DIR}/src/util.cpp"
    "${CMAKE_SOURCE_DIR}/src/os.cpp"
    "${CMAKE_SOURCE_DIR}/src/soundio.cpp"
    "${CMAKE_SOURCE_DIR}/src/dummy.cpp"
    "${CMAKE_SOURCE_DIR}/src/channel_layout.cpp"
    "${CMAKE_SOURCE_DIR}/src/ring_buffer.cpp"
)

if(SOUNDIO_HAVE_JACK)
    set(LIBSOUNDIO_SOURCES ${LIBSOUNDIO_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/jack.cpp"
    )
    set(TEST_SOURCES ${TEST_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/jack.cpp"
    )
endif()
if(SOUNDIO_HAVE_PULSEAUDIO)
    set(LIBSOUNDIO_SOURCES ${LIBSOUNDIO_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/pulseaudio.cpp"
    )
    set(TEST_SOURCES ${TEST_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/pulseaudio.cpp"
    )
endif()
if(SOUNDIO_HAVE_ALSA)
    set(LIBSOUNDIO_SOURCES ${LIBSOUNDIO_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/alsa.cpp"
    )
    set(TEST_SOURCES ${TEST_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/alsa.cpp"
    )
endif()


# GTFO, -lstdc++ !!
set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-variable -Wno-unused-but-set-variable")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-unused-variable -Wno-unused-but-set-variable")


set(LIB_CFLAGS "-std=c++11 -fno-exceptions -fno-rtti -Werror -Wall -Werror=strict-prototypes -Werror=old-style-definition -Werror=missing-prototypes")
set(EXAMPLE_CFLAGS "-std=c99 -pedantic -Werror -Wall")
set(EXAMPLE_INCLUDES "${CMAKE_SOURCE_DIR}/src")
set(TEST_CFLAGS "${LIB_CFLAGS} -fprofile-arcs -ftest-coverage")
set(TEST_LDFLAGS "-fprofile-arcs -ftest-coverage")
set(TEST_INCLUDES "${CMAKE_SOURCE_DIR}/test")

include(TestBigEndian)
test_big_endian(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    set(SOUNDIO_OS_BIG_ENDIAN true)
    set(SOUNDIO_OS_LITTLE_ENDIAN false)
else()
    set(SOUNDIO_OS_BIG_ENDIAN false)
    set(SOUNDIO_OS_LITTLE_ENDIAN true)
endif()
configure_file (
    "${CMAKE_SOURCE_DIR}/src/config.h.in"
    ${CONFIGURE_OUT_FILE}
)

add_library(libsoundio_shared SHARED ${LIBSOUNDIO_SOURCES})
set_target_properties(libsoundio_shared PROPERTIES
    OUTPUT_NAME soundio
    SOVERSION ${LIBSOUNDIO_VERSION_MAJOR}
    VERSION ${LIBSOUNDIO_VERSION}
    COMPILE_FLAGS ${LIB_CFLAGS}
    LINKER_LANGUAGE C
)
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)
target_link_libraries(libsoundio_shared LINK_PUBLIC
    ${JACK_LIBRARY}
    ${PULSEAUDIO_LIBRARY}
    ${ALSA_LIBRARIES}
    m
    ${CMAKE_THREAD_LIBS_INIT}
)
install(TARGETS libsoundio_shared DESTINATION lib)


add_library(libsoundio_static STATIC ${LIBSOUNDIO_SOURCES})
set_target_properties(libsoundio_static PROPERTIES
    OUTPUT_NAME soundio
    COMPILE_FLAGS ${LIB_CFLAGS}
    LINKER_LANGUAGE C
)
install(TARGETS libsoundio_static DESTINATION lib)

install(FILES
    ${LIBSOUNDIO_HEADERS}
    ${CONFIGURE_OUT_FILE}
    DESTINATION "include/soundio")

# Example Programs

add_executable(sio_sine example/sio_sine.c)
set_target_properties(sio_sine PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
include_directories(${EXAMPLE_INCLUDES})
target_link_libraries(sio_sine libsoundio_shared)
install(TARGETS sio_sine DESTINATION bin)

add_executable(sio_list_devices example/sio_list_devices.c)
set_target_properties(sio_list_devices PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
include_directories(${EXAMPLE_INCLUDES})
target_link_libraries(sio_list_devices libsoundio_shared)
install(TARGETS sio_list_devices DESTINATION bin)

add_executable(sio_microphone example/sio_microphone.c)
set_target_properties(sio_microphone PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
include_directories(${EXAMPLE_INCLUDES})
target_link_libraries(sio_microphone libsoundio_shared)
install(TARGETS sio_microphone DESTINATION bin)



enable_testing()
add_executable(unit_tests ${TEST_SOURCES})
target_link_libraries(unit_tests LINK_PUBLIC
    ${CMAKE_THREAD_LIBS_INIT}
    ${JACK_LIBRARY}
    ${PULSEAUDIO_LIBRARY}
    ${ALSA_LIBRARIES}
    m
)
set_target_properties(unit_tests PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${TEST_CFLAGS}
    LINK_FLAGS ${TEST_LDFLAGS}
)
include_directories(${TEST_INCLUDES})
add_test(UnitTests unit_tests)

add_executable(underflow test/underflow.c)
set_target_properties(underflow PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
include_directories(${EXAMPLE_INCLUDES})
target_link_libraries(underflow libsoundio_shared)
add_test(Underflow underflow)



add_custom_target(coverage
    DEPENDS unit_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND lcov --directory . --zerocounters --rc lcov_branch_coverage=1
    COMMAND ./unit_tests
    COMMAND lcov --directory . --capture --output-file coverage.info --rc lcov_branch_coverage=1
    COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info.cleaned --rc lcov_branch_coverage=1
    COMMAND genhtml -o coverage coverage.info.cleaned --rc lcov_branch_coverage=1
    COMMAND rm coverage.info coverage.info.cleaned
)



message("\n"
    "Installation Summary\n"
    "--------------------\n"
    "* Install Directory            : ${CMAKE_INSTALL_PREFIX}\n"
)

message(
    "System Dependencies\n"
    "-------------------\n"
    "* threads                      : ${STATUS_THREADS}\n"
    "* JACK (optional)              : ${STATUS_JACK}\n"
    "* PulseAudio (optional)        : ${STATUS_PULSEAUDIO}\n"
    "* ALSA (optional)              : ${STATUS_ALSA}\n"
)
