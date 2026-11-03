#include "memory.hpp"

#if defined( __x86_64__ )
extern "C" __attribute__( ( naked ) ) void __chkstk( )
{
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov r10, rsp\n"
        "mov r11, rsp\n"
        "sub r11, rax\n"
        "1:\n"
        "sub r10, 0x1000\n"
        "test byte ptr [r10], 0\n"
        "cmp r10, r11\n"
        "ja 1b\n"
        "ret\n"
        ".att_syntax prefix\n" );
}
#else
extern "C" __attribute__( ( naked ) ) void _chkstk( )
{
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "push ecx\n"
        "lea ecx, [esp + 4]\n"
        "sub ecx, eax\n"
        "mov eax, esp\n"
        "and eax, -0x1000\n"
        "1:\n"
        "cmp ecx, eax\n"
        "jae 2f\n"
        "sub eax, 0x1000\n"
        "test dword ptr [eax], eax\n"
        "jmp 1b\n"
        "2:\n"
        "mov eax, ecx\n"
        "pop ecx\n"
        "xchg esp, eax\n"
        "mov eax, dword ptr [eax]\n"
        "mov dword ptr [esp], eax\n"
        "ret\n"
        ".att_syntax prefix\n" );
}

ATLAS_FORCE_INLINE u64 divide_u64( u64 dividend, u64 divisor, u64 &remainder )
{
    if ( ( divisor >> 32 ) == 0 )
    {
        const u32 short_divisor{ static_cast< u32 >( divisor ) };
        const u32 high{ static_cast< u32 >( dividend >> 32 ) };
        u32       quotient_low{ static_cast< u32 >( dividend ) };
        u32       rest{ high % short_divisor };

        __asm__( "divl %[divisor]" : "+a"( quotient_low ), "+d"( rest ) : [divisor] "rm"( short_divisor ) );

        remainder = rest;
        return ( static_cast< u64 >( high / short_divisor ) << 32 ) | quotient_low;
    }

    u64 quotient{ };
    remainder = 0;

    for ( i32 bit{ 63 }; bit >= 0; --bit )
    {
        remainder = ( remainder << 1 ) | ( ( dividend >> bit ) & 1 );

        if ( remainder >= divisor )
        {
            remainder -= divisor;
            quotient |= 1ull << bit;
        }
    }

    return quotient;
}

ATLAS_FORCE_INLINE u64 magnitude( i64 value )
{
    return value < 0 ? 0 - static_cast< u64 >( value ) : static_cast< u64 >( value );
}

extern "C" u64 ATLAS_STDCALL aulldiv( u64 dividend, u64 divisor ) __asm__( "__aulldiv" );
extern "C" u64 ATLAS_STDCALL aullrem( u64 dividend, u64 divisor ) __asm__( "__aullrem" );
extern "C" i64 ATLAS_STDCALL alldiv( i64 dividend, i64 divisor ) __asm__( "__alldiv" );
extern "C" i64 ATLAS_STDCALL allrem( i64 dividend, i64 divisor ) __asm__( "__allrem" );

extern "C" u64 ATLAS_STDCALL aulldiv( u64 dividend, u64 divisor )
{
    u64 remainder{ };
    return divide_u64( dividend, divisor, remainder );
}

extern "C" u64 ATLAS_STDCALL aullrem( u64 dividend, u64 divisor )
{
    u64 remainder{ };
    divide_u64( dividend, divisor, remainder );
    return remainder;
}

extern "C" i64 ATLAS_STDCALL alldiv( i64 dividend, i64 divisor )
{
    u64       remainder{ };
    const u64 quotient{ divide_u64( magnitude( dividend ), magnitude( divisor ), remainder ) };
    return ( dividend < 0 ) != ( divisor < 0 ) ? static_cast< i64 >( 0 - quotient ) : static_cast< i64 >( quotient );
}

extern "C" i64 ATLAS_STDCALL allrem( i64 dividend, i64 divisor )
{
    u64 remainder{ };
    divide_u64( magnitude( dividend ), magnitude( divisor ), remainder );
    return dividend < 0 ? static_cast< i64 >( 0 - remainder ) : static_cast< i64 >( remainder );
}
#endif

using atexit_func = void( __cdecl * )( void );
extern "C" i32 ATLAS_CDECL atexit( atexit_func func )
{
    func( );
    return 0;
}

extern "C" int _fltused{ };

// Abstract classes put this in the vtable slot of every pure virtual; Reaching it means a
// pure method was called on a partially constructed object, so there is nothing to recover
// to; it only has to exist so those vtables link without the CRT
extern "C" void ATLAS_CDECL _purecall( )
{
    __debugbreak( );
}

extern "C" ATLAS_NOINLINE void *memset( void *destination, int value, usize size )
{
    return atl::memset( destination, value, size );
}

extern "C" ATLAS_NOINLINE void *memcpy( void *destination, const void *source, usize size )
{
    return atl::memcpy( destination, source, size );
}

extern "C" ATLAS_NOINLINE void *memmove( void *destination, const void *source, usize size )
{
    auto       *to{ static_cast< u8 * >( destination ) };
    const auto *from{ static_cast< const u8 * >( source ) };

    if ( !size || to <= from || to >= from + size )
    {
        return atl::memcpy( destination, source, size );
    }

    u8       *to_last{ to + size - 1 };
    const u8 *from_last{ from + size - 1 };
    __asm__ volatile( "std\n\trep movsb\n\tcld" : "+D"( to_last ), "+S"( from_last ), "+c"( size ) : : "memory", "cc" );

    return destination;
}

extern "C" ATLAS_NOINLINE int memcmp( const void *left, const void *right, usize size )
{
    const auto *a{ static_cast< const u8 * >( left ) };
    const auto *b{ static_cast< const u8 * >( right ) };

    for ( usize index{ }; index < size; ++index )
    {
        if ( a[ index ] != b[ index ] )
        {
            return a[ index ] < b[ index ] ? -1 : 1;
        }
    }

    return 0;
}

void *ATLAS_CDECL operator new( uptr size )
{
    return atl::malloc( size );
}

void *ATLAS_CDECL operator new[]( uptr size )
{
    return atl::malloc( size );
}

void ATLAS_CDECL operator delete( void *buffer )
{
    return atl::free( buffer );
}

void ATLAS_CDECL operator delete[]( void *buffer )
{
    return atl::free( buffer );
}

void ATLAS_CDECL operator delete( void *buffer, uptr size )
{
    return operator delete( buffer );
}
