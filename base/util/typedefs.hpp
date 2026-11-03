#pragma once

using usize     = decltype( sizeof( 0 ) );
using u8        = unsigned char;
using u16       = unsigned short;
using u32       = unsigned int;
using u64       = unsigned long long;
using i8        = signed char;
using i16       = signed short;
using i32       = signed int;
using i64       = signed long long;
using f32       = float;
using f64       = double;
using pdiff     = decltype( static_cast< int * >( nullptr ) - static_cast< int * >( nullptr ) );
using nullptr_t = decltype( nullptr );
using va_list   = char *;

using DWORD = unsigned long;

#if defined( __x86_64__ )
using uptr = u64;
#else
using uptr = u32;
#endif // __x86_64__

#define ATLAS_UNUSED_PARAM( param )  ( ( void )param )
#define ATLAS_FORCE_INLINE           __forceinline
#define ATLAS_NOINLINE               __declspec( noinline )
#define ATLAS_NO_DESTROY             [[clang::no_destroy]]
#define ATLAS_NORETURN               __declspec( noreturn )
#define ATLAS_STDCALL                __stdcall
#define ATLAS_FASTCALL               __fastcall
#define ATLAS_THISCALL               __thiscall
#define ATLAS_CDECL                  __cdecl
#define ATLAS_VECTORCALL             __vectorcall
#define ATLAS_NAKED                  __declspec( naked )
#define ATLAS_EXPORT                 __declspec( dllexport )
#define ATLAS_KEEP_ME_NO_MATTER_WHAT __declspec( noinline ) __attribute( ( used ) )

#ifdef ULONG_MAX
#undef ULONG_MAX
#endif

#ifdef FLT_MAX
#undef FLT_MAX
#endif

#if defined( va_start ) || defined( va_arg ) || defined( va_copy ) || defined( va_end )
#undef va_start
#undef va_arg
#undef va_copy
#undef va_end
#endif

#define ULONG_MAX   ( 2147483647L * 2UL + 1UL )
#define FLT_MAX     3.40282347e+38F
#define FLT_EPSILON 1.192092896e-07F

// the compiler homes rdx/r8/r9 into the shadow space only when it sees its own va_start; a hand-rolled
// &last + 1 points at a local slot next to the parameter and the register arguments are never saved
#define va_start( ap, last ) __builtin_va_start( ap, last )
#define va_arg( ap, type )   __builtin_va_arg( ap, type )
#define va_copy( dest, src ) __builtin_va_copy( dest, src )
#define va_end( ap )         __builtin_va_end( ap )

struct point_t
{
    i32 x{ };
    i32 y{ };
};

struct rect_t
{
    i32 left{ };
    i32 top{ };
    i32 right{ };
    i32 bottom{ };
};

struct color_t
{
    u8 r{ };
    u8 g{ };
    u8 b{ };
    u8 a{ 255 };

    constexpr u32 d3d( ) const
    {
        return ( static_cast< u32 >( this->a ) << 24 ) | ( static_cast< u32 >( this->r ) << 16 ) |
               ( static_cast< u32 >( this->g ) << 8 ) | static_cast< u32 >( this->b );
    }

    constexpr u32 gdi( ) const
    {
        return static_cast< u32 >( this->r ) | ( static_cast< u32 >( this->g ) << 8 ) |
               ( static_cast< u32 >( this->b ) << 16 );
    }

    static constexpr u8 blend_channel( u8 from, u8 to, f32 at )
    {
        return static_cast< u8 >( static_cast< f32 >( from ) + ( static_cast< f32 >( to ) - static_cast< f32 >( from ) ) * at );
    }

    constexpr color_t blend( const color_t &to, f32 at ) const
    {
        return color_t{ blend_channel( this->r, to.r, at ), blend_channel( this->g, to.g, at ), blend_channel( this->b, to.b, at ), blend_channel( this->a, to.a, at ) };
    }
};

enum class bind_mode_t : u8
{
    hold,
    toggle,
    always,
};

struct bind_t
{
    u32         m_key{ };
    bind_mode_t m_mode{ bind_mode_t::hold };
    bool        m_toggled{ };
    u64         m_batch{ };
};

static_assert( sizeof( u8 ) == 1 );
static_assert( sizeof( u16 ) == 2 );
static_assert( sizeof( u32 ) == 4 );
static_assert( sizeof( u64 ) == 8 );
static_assert( sizeof( i8 ) == 1 );
static_assert( sizeof( i16 ) == 2 );
static_assert( sizeof( i32 ) == 4 );
static_assert( sizeof( i64 ) == 8 );
