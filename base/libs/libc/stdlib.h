#pragma once

#include <stddef.h>

extern "C" double lua_crt_strtod( const char *text, char **end );
#define strtod lua_crt_strtod

static inline int abs( int value )
{
    return value < 0 ? -value : value;
}

[[noreturn]] static inline void abort( )
{
    __builtin_trap( );
}

static inline void *realloc( void *, size_t )
{
    return nullptr;
}

static inline void free( void * )
{
}
