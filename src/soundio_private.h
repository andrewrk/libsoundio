/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_SOUNDIO_PRIVATE_H
#define SOUNDIO_SOUNDIO_PRIVATE_H

// This exists for __declspec(dllexport) and __declspec(dllimport) to be
// defined correctly without the library user having to do anything.
#define SOUNDIO_BUILDING_LIBRARY
#include "soundio/soundio.h"

#endif
