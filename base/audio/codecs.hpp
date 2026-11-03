#pragma once

#define MINIMP3_FLOAT_OUTPUT
#include "libs/minimp3.h"

#define STB_VORBIS_NO_CRT
#define STB_VORBIS_NO_PULLDATA_API
#define STB_VORBIS_HEADER_ONLY
#include "libs/stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY
