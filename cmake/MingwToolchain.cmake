# Required by cmake if `uname -s` is inadaquate
set(CMAKE_SYSTEM_NAME               Windows)
set(CMAKE_SYSTEM_VERSION            1)

# Mingw architecture default is 64-bit
# To override: 
#   $ export MINGW_ARCH=32 
if(DEFINED ENV{MINGW_ARCH}) 
        set(MINGW_ARCH              "$ENV{MINGW_ARCH}")
else()
        set(MINGW_ARCH              "64")
endif()

if(${MINGW_ARCH} STREQUAL "32")
        set(CMAKE_SYSTEM_PROCESSOR  "i686")
elseif(${MINGW_ARCH} STREQUAL "64")
        set(CMAKE_SYSTEM_PROCESSOR  "x86_64")
else()
        message(FATAL_ERROR         "Unknown system architecture specified") 
endif()

# Path to mingw
set(MINGW_PREFIX                    "/opt/mingw${MINGW_ARCH}")
# Linux mingw requires explicitly defined tools to prevent clash with native system tools
set(MINGW_TOOL_PREFIX               ${MINGW_PREFIX}/bin/${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32-)

# Windows msys mingw ships with a mostly suitable preconfigured environment
if(DEFINED ENV{MSYSCON})
        set(CMAKE_GENERATOR         "MSYS Makefiles" CACHE STRING "" FORCE)
        set(MINGW_PREFIX            "/mingw${MINGW_ARCH}")
        set(MINGW_TOOL_PREFIX       "${MINGW_PREFIX}/bin/") 

        # Msys compiler does not support @CMakeFiles/Include syntax
        set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES   OFF)
        set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)
endif()

# The target environment
set(CMAKE_FIND_ROOT_PATH            ${MINGW_PREFIX})
set(CMAKE_INSTALL_PREFIX            ${MINGW_PREFIX})

# which compilers to use for C and C++
set(CMAKE_C_COMPILER                ${MINGW_TOOL_PREFIX}gcc)
set(CMAKE_CXX_COMPILER              ${MINGW_TOOL_PREFIX}g++)
set(CMAKE_RC_COMPILER               ${MINGW_TOOL_PREFIX}windres)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Fix header finds
set(WASAPI_INCLUDE_DIR              ${MINGW_PREFIX}/include)

# Set release to be defaut
if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE        "Release" CACHE STRING "Defaulting to Release build type for mingw")
endif()

