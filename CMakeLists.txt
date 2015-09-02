cmake_minimum_required(VERSION 2.8.5)

if(CMAKE_VERSION VERSION_LESS 3.0.0)
    set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "library install dir (lib)")
    set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "header base install dir (include)")
    set(CMAKE_INSTALL_BINDIR "bin" CACHE PATH "user executables (bin)")
else()
    cmake_policy(SET CMP0042 NEW)
    cmake_policy(SET CMP0046 NEW)
    include(GNUInstallDirs)
endif()

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

option(ENABLE_JACK "Enable JACK backend" ON)
option(ENABLE_PULSEAUDIO "Enable PulseAudio backend" ON)
option(ENABLE_ALSA "Enable ALSA backend" ON)
option(ENABLE_COREAUDIO "Enable CoreAudio backend" ON)
option(ENABLE_WASAPI "Enable WASAPI backend" ON)

find_package(Threads)
if(Threads_FOUND)
    set(STATUS_THREADS "OK")
else(Threads_FOUND)
    set(STATUS_THREADS "not found")
endif(Threads_FOUND)

if(ENABLE_JACK)
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
else()
    set(STATUS_JACK "disabled")
    set(SOUNDIO_HAVE_JACK false)
    set(JACK_LIBRARY "")
endif()

if(ENABLE_PULSEAUDIO)
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
else()
    set(STATUS_PULSEAUDIO "disabled")
    set(SOUNDIO_HAVE_PULSEAUDIO false)
    set(PULSEAUDIO_LIBRARY "")
endif()

if(ENABLE_ALSA)
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
else()
    set(STATUS_ALSA "disabled")
    set(SOUNDIO_HAVE_ALSA false)
    set(ALSA_LIBRARIES "")
endif()

if(ENABLE_COREAUDIO)
    find_package(CoreAudio)
    if(COREAUDIO_FOUND)
        set(STATUS_COREAUDIO "OK")
        set(SOUNDIO_HAVE_COREAUDIO true)
        include_directories(${COREAUDIO_INCLUDE_DIR})

        find_path(COREFOUNDATION_INCLUDE_DIR NAMES CoreFoundation.h)
        find_library(COREFOUNDATION_LIBRARY NAMES CoreFoundation)
        include_directories(${COREFOUNDATION_INCLUDE_DIR})

        find_path(AUDIOUNIT_INCLUDE_DIR NAMES AudioUnit.h)
        find_library(AUDIOUNIT_LIBRARY NAMES AudioUnit)
        include_directories(${AUDIOUNIT_INCLUDE_DIR})
    else()
        set(STATUS_COREAUDIO "not found")
        set(SOUNDIO_HAVE_COREAUDIO false)
        set(COREAUDIO_LIBRARY "")
        set(COREFOUNDATION_LIBRARY "")
        set(AUDIOUNIT_LIBRARY "")
    endif()
else()
    set(STATUS_COREAUDIO "disabled")
    set(SOUNDIO_HAVE_COREAUDIO false)
    set(COREAUDIO_LIBRARY "")
    set(COREFOUNDATION_LIBRARY "")
    set(AUDIOUNIT_LIBRARY "")
endif()

if(ENABLE_WASAPI)
    find_package(WASAPI)
    if(WASAPI_FOUND)
        set(STATUS_WASAPI "OK")
        set(SOUNDIO_HAVE_WASAPI true)
    else()
        set(STATUS_WASAPI "not found")
        set(SOUNDIO_HAVE_WASAPI false)
    endif()
else()
    set(STATUS_WASAPI "disabled")
    set(SOUNDIO_HAVE_WASAPI false)
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
    ${CONFIGURE_OUT_FILE}
)

if(SOUNDIO_HAVE_JACK)
    set(LIBSOUNDIO_SOURCES ${LIBSOUNDIO_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/jack.cpp"
    )
endif()
if(SOUNDIO_HAVE_PULSEAUDIO)
    set(LIBSOUNDIO_SOURCES ${LIBSOUNDIO_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/pulseaudio.cpp"
    )
endif()
if(SOUNDIO_HAVE_ALSA)
    set(LIBSOUNDIO_SOURCES ${LIBSOUNDIO_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/alsa.cpp"
    )
endif()
if(SOUNDIO_HAVE_COREAUDIO)
    set(LIBSOUNDIO_SOURCES ${LIBSOUNDIO_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/coreaudio.cpp"
    )
endif()
if(SOUNDIO_HAVE_WASAPI)
    set(LIBSOUNDIO_SOURCES ${LIBSOUNDIO_SOURCES}
        "${CMAKE_SOURCE_DIR}/src/wasapi.cpp"
    )
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    "${CMAKE_SOURCE_DIR}/test"
    "${CMAKE_SOURCE_DIR}/src"
)

set(LIBSOUNDIO_LIBS
    ${JACK_LIBRARY}
    ${PULSEAUDIO_LIBRARY}
    ${ALSA_LIBRARIES}
    ${COREAUDIO_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
    ${AUDIOUNIT_LIBRARY}
    m
    ${CMAKE_THREAD_LIBS_INIT}
)



# GTFO, -lstdc++ !!
set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Werror -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Werror -pedantic")


set(LIB_CFLAGS "-std=c++11 -fno-exceptions -fno-rtti -fvisibility=hidden -Wall -Werror=strict-prototypes -Werror=old-style-definition -Werror=missing-prototypes -Wno-c99-extensions")
set(EXAMPLE_CFLAGS "-std=c99 -Wall")
set(TEST_CFLAGS "${LIB_CFLAGS} -fprofile-arcs -ftest-coverage")
set(TEST_LDFLAGS "-fprofile-arcs -ftest-coverage")

include(TestBigEndian)
test_big_endian(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    set(SOUNDIO_OS_BIG_ENDIAN true)
    set(SOUNDIO_OS_LITTLE_ENDIAN false)
else()
    set(SOUNDIO_OS_BIG_ENDIAN false)
    set(SOUNDIO_OS_LITTLE_ENDIAN true)
endif()
configure_file(
    "${CMAKE_SOURCE_DIR}/src/config.h.in"
    ${CONFIGURE_OUT_FILE}
)
set(DOXYGEN_CONF_FILE "${CMAKE_BINARY_DIR}/doxygen.conf")
configure_file(
    "${CMAKE_SOURCE_DIR}/doc/doxygen.conf.in"
    ${DOXYGEN_CONF_FILE}
)

add_library(libsoundio_shared SHARED ${LIBSOUNDIO_SOURCES})
set_target_properties(libsoundio_shared PROPERTIES
    OUTPUT_NAME soundio
    SOVERSION ${LIBSOUNDIO_VERSION_MAJOR}
    VERSION ${LIBSOUNDIO_VERSION}
    COMPILE_FLAGS ${LIB_CFLAGS}
    LINKER_LANGUAGE C
)
target_link_libraries(libsoundio_shared LINK_PUBLIC ${LIBSOUNDIO_LIBS})
install(TARGETS libsoundio_shared DESTINATION ${CMAKE_INSTALL_LIBDIR})


add_library(libsoundio_static STATIC ${LIBSOUNDIO_SOURCES})
set_target_properties(libsoundio_static PROPERTIES
    OUTPUT_NAME soundio
    COMPILE_FLAGS ${LIB_CFLAGS}
    LINKER_LANGUAGE C
)
install(TARGETS libsoundio_static DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES
    ${LIBSOUNDIO_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/soundio")

# Example Programs

add_executable(sio_sine example/sio_sine.c)
set_target_properties(sio_sine PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
target_link_libraries(sio_sine libsoundio_shared)
install(TARGETS sio_sine DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(sio_list_devices example/sio_list_devices.c)
set_target_properties(sio_list_devices PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
target_link_libraries(sio_list_devices libsoundio_shared)
install(TARGETS sio_list_devices DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(sio_microphone example/sio_microphone.c)
set_target_properties(sio_microphone PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
target_link_libraries(sio_microphone libsoundio_shared)
install(TARGETS sio_microphone DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(sio_record example/sio_record.c)
set_target_properties(sio_record PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
target_link_libraries(sio_record libsoundio_shared)
install(TARGETS sio_record DESTINATION ${CMAKE_INSTALL_BINDIR})


add_executable(unit_tests "${CMAKE_SOURCE_DIR}/test/unit_tests.cpp" ${LIBSOUNDIO_SOURCES})
target_link_libraries(unit_tests LINK_PUBLIC ${LIBSOUNDIO_LIBS})
set_target_properties(unit_tests PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${TEST_CFLAGS}
    LINK_FLAGS ${TEST_LDFLAGS}
)

add_executable(latency "${CMAKE_SOURCE_DIR}/test/latency.cpp" ${LIBSOUNDIO_SOURCES})
target_link_libraries(latency LINK_PUBLIC ${LIBSOUNDIO_LIBS})
set_target_properties(latency PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${LIB_CFLAGS}
)

add_executable(underflow test/underflow.c)
set_target_properties(underflow PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
target_link_libraries(underflow libsoundio_shared)

add_executable(backend_disconnect_recover test/backend_disconnect_recover.c)
set_target_properties(backend_disconnect_recover PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
target_link_libraries(backend_disconnect_recover libsoundio_shared)

add_executable(overflow test/overflow.c)
set_target_properties(overflow PROPERTIES
    LINKER_LANGUAGE C
    COMPILE_FLAGS ${EXAMPLE_CFLAGS})
target_link_libraries(overflow libsoundio_shared)



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

add_custom_target(doc
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND doxygen ${DOXYGEN_CONF_FILE}
)



message("\n"
    "Installation Summary\n"
    "--------------------\n"
    "* Install Directory            : ${CMAKE_INSTALL_PREFIX}\n"
    "* Build Type                   : ${CMAKE_BUILD_TYPE}\n"
)

message(
    "System Dependencies\n"
    "-------------------\n"
    "* threads                      : ${STATUS_THREADS}\n"
    "* JACK       (optional)        : ${STATUS_JACK}\n"
    "* PulseAudio (optional)        : ${STATUS_PULSEAUDIO}\n"
    "* ALSA       (optional)        : ${STATUS_ALSA}\n"
    "* CoreAudio  (optional)        : ${STATUS_COREAUDIO}\n"
    "* WASAPI     (optional)        : ${STATUS_WASAPI}\n"
)
