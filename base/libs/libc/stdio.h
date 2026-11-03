#pragma once

#include <stddef.h>

#define EOF    ( -1 )
#define BUFSIZ 512

typedef struct lua_crt_file FILE;
#define stdin ( static_cast< FILE * >( nullptr ) )

static inline FILE *fopen( const char *, const char * )
{
    return nullptr;
}

static inline FILE *freopen( const char *, const char *, FILE * )
{
    return nullptr;
}

static inline int fclose( FILE * )
{
    return 0;
}

static inline int getc( FILE * )
{
    return EOF;
}

static inline size_t fread( void *, size_t, size_t, FILE * )
{
    return 0;
}

static inline int feof( FILE * )
{
    return 1;
}

static inline int ferror( FILE * )
{
    return 1;
}

extern "C" int lua_crt_sprintf( char *buffer, const char *format, ... );
#define sprintf lua_crt_sprintf
