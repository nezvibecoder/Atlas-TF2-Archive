#pragma once

#include "libs/lazy_importer.hpp"
#include "util/address.hpp"
#include "util/typedefs.hpp"

namespace atl
{
    ATLAS_FORCE_INLINE void *memset( void *src, int val, usize size )
    {
        void *out{ src };
        __asm__ volatile( "rep stosb" : "+D"( out ), "+c"( size ) : "a"( val ) : "memory" );

        return src;
    }

    ATLAS_FORCE_INLINE void *memcpy( void *dst, const void *src, usize size )
    {
        void *out{ dst };
        __asm__ volatile( "rep movsb" : "+D"( out ), "+S"( src ), "+c"( size ) : : "memory" );

        return dst;
    }

    ATLAS_FORCE_INLINE void *malloc( usize size )
    {
        if ( !size )
        {
            return nullptr;
        }

        void *base{ };
        usize region_size{ size };

        if ( I( NtAllocateVirtualMemory )( util::get_current_process( ), &base, 0, &region_size,
                                           MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) < 0 )
        {
            return nullptr;
        }

        return base;
    }

    ATLAS_FORCE_INLINE void *realloc( usize size )
    {
        return nullptr;
    }

    ATLAS_FORCE_INLINE void free( void *address )
    {
        if ( !address )
        {
            return;
        }

        void *base{ address };
        usize region_size{ };
        I( NtFreeVirtualMemory )( util::get_current_process( ), &base, &region_size, MEM_RELEASE );
    }
} // namespace atl

// These need to be dangling out here. Required by the compiler.
void *ATLAS_CDECL operator new( uptr size );
void *ATLAS_CDECL operator new[]( uptr size );
void ATLAS_CDECL  operator delete( void *buffer );
void ATLAS_CDECL  operator delete[]( void *buffer );
void ATLAS_CDECL  operator delete( void *buffer, uptr size );

#if defined( ATLAS_PROVIDE_OWN_WINDEFS )
// Just a rebuild from the Windows SDK.
#define ZeroMemory( src, size ) \
    atl::memset( src, 0, size );
#endif
