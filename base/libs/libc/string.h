#pragma once

#include <stddef.h>

extern "C" void *memcpy( void *destination, const void *source, size_t size );
extern "C" void *memset( void *destination, int value, size_t size );
extern "C" int   memcmp( const void *left, const void *right, size_t size );

static inline size_t strlen( const char *text )
{
    size_t length{ };

    while ( text[ length ] )
    {
        ++length;
    }

    return length;
}

static inline int strcmp( const char *left, const char *right )
{
    while ( *left && *left == *right )
    {
        ++left;
        ++right;
    }

    return static_cast< unsigned char >( *left ) - static_cast< unsigned char >( *right );
}

static inline int strncmp( const char *left, const char *right, size_t count )
{
    for ( ; count; --count, ++left, ++right )
    {
        if ( *left != *right || !*left )
        {
            return static_cast< unsigned char >( *left ) - static_cast< unsigned char >( *right );
        }
    }

    return 0;
}

static inline int strcoll( const char *left, const char *right )
{
    return strcmp( left, right );
}

static inline char *strchr( const char *text, int value )
{
    for ( ;; ++text )
    {
        if ( *text == static_cast< char >( value ) )
        {
            return const_cast< char * >( text );
        }

        if ( !*text )
        {
            return nullptr;
        }
    }
}

static inline void *memchr( const void *memory, int value, size_t size )
{
    const auto *cursor{ static_cast< const unsigned char * >( memory ) };

    for ( ; size; --size, ++cursor )
    {
        if ( *cursor == static_cast< unsigned char >( value ) )
        {
            return const_cast< unsigned char * >( cursor );
        }
    }

    return nullptr;
}

static inline size_t strspn( const char *text, const char *accept )
{
    size_t length{ };

    while ( text[ length ] && strchr( accept, text[ length ] ) )
    {
        ++length;
    }

    return length;
}

static inline char *strpbrk( const char *text, const char *accept )
{
    for ( ; *text; ++text )
    {
        if ( strchr( accept, *text ) )
        {
            return const_cast< char * >( text );
        }
    }

    return nullptr;
}

static inline char *strstr( const char *haystack, const char *needle )
{
    const size_t length{ strlen( needle ) };

    for ( ; *haystack; ++haystack )
    {
        if ( !strncmp( haystack, needle, length ) )
        {
            return const_cast< char * >( haystack );
        }
    }

    return length ? nullptr : const_cast< char * >( haystack );
}

static inline char *strcpy( char *destination, const char *source )
{
    char *out{ destination };

    while ( ( *destination++ = *source++ ) )
    {
    }

    return out;
}

static inline char *strerror( int )
{
    return const_cast< char * >( "unavailable" );
}
