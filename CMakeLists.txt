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
    "${CMAKE_SOURCE_DIR}/src/soundio.h"
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


set(LIB_CFLAGS "-std=c++11 -fno-exceptions -fno-rtti -Werror -Wall -Werror=strict-prototypes -Werror=old-style-definition -Werror=missing-prototypes -fdiagnostics-color=auto")
set(EXAMPLE_CFLAGS "-std=c99 -pedantic -Werror -Wall")
set(EXAMPLE_INCLUDES "${CMAKE_SOURCE_DIR}/src")
set(TEST_CFLAGS "${LIB_CFLAGS} -fprofile-arcs -ftest-coverage")
set(TEST_LDFLAGS "-fprofile-arcs -ftest-coverage")
set(TEST_INCLUDES "${CMAKE_SOURCE_DIR}/test")

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
    ${ALSA_INCLUDE_DIRS}
)
target_link_libraries(libsoundio_shared LINK_PUBLIC
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

add_executable(sine example/sine.c)
set_target_properties(sine PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
include_directories(${EXAMPLE_INCLUDES})
target_link_libraries(sine libsoundio_shared)

add_executable(list_devices example/list_devices.c)
set_target_properties(list_devices PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
include_directories(${EXAMPLE_INCLUDES})
target_link_libraries(list_devices libsoundio_shared)

add_executable(microphone example/microphone.c)
set_target_properties(microphone PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
include_directories(${EXAMPLE_INCLUDES})
target_link_libraries(microphone libsoundio_shared)



enable_testing()
add_executable(unit_tests ${TEST_SOURCES})
target_link_libraries(unit_tests LINK_PUBLIC
    ${CMAKE_THREAD_LIBS_INIT}
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
    "* pulseaudio                   : ${STATUS_PULSEAUDIO}\n"
    "* ALSA                         : ${STATUS_ALSA}\n"
)
