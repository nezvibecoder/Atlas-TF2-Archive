#pragma clang section rodata = ".lstr$m"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>

#include "util/typedefs.hpp"

extern "C" char *lua_crt_dtoa( double value, int mode, int digits, int *decimal_point, int *sign, char **end );
extern "C" void  lua_crt_freedtoa( char *digits );

#if defined( __x86_64__ )
extern "C" __attribute__( ( naked ) ) int lua_crt_setjmp( unsigned long long * )
{
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov [rcx + 0x00], rbx\n"
        "mov [rcx + 0x08], rbp\n"
        "mov [rcx + 0x10], rdi\n"
        "mov [rcx + 0x18], rsi\n"
        "mov [rcx + 0x20], r12\n"
        "mov [rcx + 0x28], r13\n"
        "mov [rcx + 0x30], r14\n"
        "mov [rcx + 0x38], r15\n"
        "lea rdx, [rsp + 8]\n"
        "mov [rcx + 0x40], rdx\n"
        "mov rdx, [rsp]\n"
        "mov [rcx + 0x48], rdx\n"
        "movdqu [rcx + 0x50], xmm6\n"
        "movdqu [rcx + 0x60], xmm7\n"
        "movdqu [rcx + 0x70], xmm8\n"
        "movdqu [rcx + 0x80], xmm9\n"
        "movdqu [rcx + 0x90], xmm10\n"
        "movdqu [rcx + 0xA0], xmm11\n"
        "movdqu [rcx + 0xB0], xmm12\n"
        "movdqu [rcx + 0xC0], xmm13\n"
        "movdqu [rcx + 0xD0], xmm14\n"
        "movdqu [rcx + 0xE0], xmm15\n"
        "stmxcsr [rcx + 0xF0]\n"
        "fnstcw [rcx + 0xF4]\n"
        "xor eax, eax\n"
        "ret\n"
        ".att_syntax prefix\n" );
}

extern "C" __attribute__( ( naked ) ) void lua_crt_longjmp( unsigned long long *, int )
{
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov eax, edx\n"
        "test eax, eax\n"
        "jnz 1f\n"
        "inc eax\n"
        "1:\n"
        "mov rbx, [rcx + 0x00]\n"
        "mov rbp, [rcx + 0x08]\n"
        "mov rdi, [rcx + 0x10]\n"
        "mov rsi, [rcx + 0x18]\n"
        "mov r12, [rcx + 0x20]\n"
        "mov r13, [rcx + 0x28]\n"
        "mov r14, [rcx + 0x30]\n"
        "mov r15, [rcx + 0x38]\n"
        "movdqu xmm6, [rcx + 0x50]\n"
        "movdqu xmm7, [rcx + 0x60]\n"
        "movdqu xmm8, [rcx + 0x70]\n"
        "movdqu xmm9, [rcx + 0x80]\n"
        "movdqu xmm10, [rcx + 0x90]\n"
        "movdqu xmm11, [rcx + 0xA0]\n"
        "movdqu xmm12, [rcx + 0xB0]\n"
        "movdqu xmm13, [rcx + 0xC0]\n"
        "movdqu xmm14, [rcx + 0xD0]\n"
        "movdqu xmm15, [rcx + 0xE0]\n"
        "ldmxcsr [rcx + 0xF0]\n"
        "fldcw [rcx + 0xF4]\n"
        "mov rsp, [rcx + 0x40]\n"
        "jmp qword ptr [rcx + 0x48]\n"
        ".att_syntax prefix\n" );
}
#else
extern "C" __attribute__( ( naked ) ) int lua_crt_setjmp( unsigned long long * )
{
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov ecx, [esp + 4]\n"
        "mov [ecx + 0x00], ebx\n"
        "mov [ecx + 0x04], esi\n"
        "mov [ecx + 0x08], edi\n"
        "mov [ecx + 0x0C], ebp\n"
        "lea edx, [esp + 4]\n"
        "mov [ecx + 0x10], edx\n"
        "mov edx, [esp]\n"
        "mov [ecx + 0x14], edx\n"
        "xor eax, eax\n"
        "ret\n"
        ".att_syntax prefix\n" );
}

extern "C" __attribute__( ( naked ) ) void lua_crt_longjmp( unsigned long long *, int )
{
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov ecx, [esp + 4]\n"
        "mov eax, [esp + 8]\n"
        "test eax, eax\n"
        "jnz 1f\n"
        "inc eax\n"
        "1:\n"
        "mov ebx, [ecx + 0x00]\n"
        "mov esi, [ecx + 0x04]\n"
        "mov edi, [ecx + 0x08]\n"
        "mov ebp, [ecx + 0x0C]\n"
        "mov esp, [ecx + 0x10]\n"
        "jmp dword ptr [ecx + 0x14]\n"
        ".att_syntax prefix\n" );
}
#endif

struct format_spec_t
{
    bool m_left{ };
    bool m_plus{ };
    bool m_space{ };
    bool m_alternate{ };
    bool m_zero{ };
    bool m_long_long{ };
    int  m_width{ };
    int  m_precision{ -1 };
    char m_conversion{ };
};

struct format_writer_t
{
    char *m_out{ };
    int   m_written{ };

    void put( char value )
    {
        this->m_out[ this->m_written++ ] = value;
    }

    void repeat( char value, int count )
    {
        for ( ; count > 0; --count )
        {
            this->put( value );
        }
    }

    void text( const char *value, int length )
    {
        for ( int index{ }; index < length; ++index )
        {
            this->put( value[ index ] );
        }
    }

    void field( const format_spec_t &spec, const char *prefix, int prefix_length, const char *body,
                int body_length, bool zero_pad )
    {
        const int padding{ spec.m_width - prefix_length - body_length };

        if ( spec.m_left )
        {
            this->text( prefix, prefix_length );
            this->text( body, body_length );
            this->repeat( ' ', padding );
            return;
        }

        if ( zero_pad )
        {
            this->text( prefix, prefix_length );
            this->repeat( '0', padding );
            this->text( body, body_length );
            return;
        }

        this->repeat( ' ', padding );
        this->text( prefix, prefix_length );
        this->text( body, body_length );
    }
};

ATLAS_FORCE_INLINE const char *parse_spec( const char *cursor, format_spec_t &spec )
{
    for ( ;; ++cursor )
    {
        switch ( *cursor )
        {
            case '-': spec.m_left = true; continue;
            case '+': spec.m_plus = true; continue;
            case ' ': spec.m_space = true; continue;
            case '#': spec.m_alternate = true; continue;
            case '0': spec.m_zero = true; continue;
            default: break;
        }

        break;
    }

    while ( *cursor >= '0' && *cursor <= '9' )
    {
        spec.m_width = spec.m_width * 10 + ( *cursor++ - '0' );
    }

    if ( *cursor == '.' )
    {
        spec.m_precision = 0;
        ++cursor;

        while ( *cursor >= '0' && *cursor <= '9' )
        {
            spec.m_precision = spec.m_precision * 10 + ( *cursor++ - '0' );
        }
    }

    if ( cursor[ 0 ] == 'l' && cursor[ 1 ] == 'l' )
    {
        spec.m_long_long = true;
        cursor += 2;
    }

    spec.m_conversion = *cursor;
    return cursor;
}

ATLAS_FORCE_INLINE void format_integer( format_writer_t &writer, const format_spec_t &spec, unsigned long long magnitude,
                                        bool negative )
{
    const char        conversion{ spec.m_conversion };
    const bool        is_signed{ conversion == 'd' || conversion == 'i' };
    const u32         radix{ conversion == 'o' ? 8u : ( conversion == 'x' || conversion == 'X' ) ? 16u
                                                                                                 : 10u };
    const char *const digit_set{ conversion == 'X' ? "0123456789ABCDEF" : "0123456789abcdef" };

    char reversed[ 128 ]{ };
    int  count{ };

    for ( unsigned long long rest{ magnitude }; rest; rest /= radix )
    {
        reversed[ count++ ] = digit_set[ rest % radix ];
    }

    const int minimum{ spec.m_precision < 0 ? 1 : spec.m_precision };

    while ( count < minimum )
    {
        reversed[ count++ ] = '0';
    }

    if ( conversion == 'o' && spec.m_alternate && ( !count || reversed[ count - 1 ] != '0' ) )
    {
        reversed[ count++ ] = '0';
    }

    char body[ 128 ]{ };
    for ( int index{ }; index < count; ++index )
    {
        body[ index ] = reversed[ count - 1 - index ];
    }

    char prefix[ 2 ]{ };
    int  prefix_length{ };

    if ( is_signed && negative )
    {
        prefix[ prefix_length++ ] = '-';
    } else if ( is_signed && spec.m_plus )
    {
        prefix[ prefix_length++ ] = '+';
    } else if ( is_signed && spec.m_space )
    {
        prefix[ prefix_length++ ] = ' ';
    } else if ( radix == 16 && spec.m_alternate && magnitude )
    {
        prefix[ prefix_length++ ] = '0';
        prefix[ prefix_length++ ] = conversion;
    }

    writer.field( spec, prefix, prefix_length, body, count, spec.m_zero && spec.m_precision < 0 );
}

ATLAS_FORCE_INLINE char digit_at( const char *digits, int count, int index )
{
    return index >= 0 && index < count ? digits[ index ] : '0';
}

ATLAS_FORCE_INLINE int compose_fixed( char *out, const char *digits, int count, int point, int fraction, bool dot )
{
    int length{ };

    if ( point <= 0 )
    {
        out[ length++ ] = '0';
    } else
    {
        for ( int index{ }; index < point; ++index )
        {
            out[ length++ ] = digit_at( digits, count, index );
        }
    }

    if ( fraction || dot )
    {
        out[ length++ ] = '.';
    }

    for ( int index{ }; index < fraction; ++index )
    {
        out[ length++ ] = digit_at( digits, count, point + index );
    }

    return length;
}

ATLAS_FORCE_INLINE int compose_exponent( char *out, const char *digits, int count, int exponent, int fraction, bool dot,
                                         bool upper )
{
    int length{ };

    out[ length++ ] = digit_at( digits, count, 0 );

    if ( fraction || dot )
    {
        out[ length++ ] = '.';
    }

    for ( int index{ }; index < fraction; ++index )
    {
        out[ length++ ] = digit_at( digits, count, index + 1 );
    }

    out[ length++ ] = upper ? 'E' : 'e';
    out[ length++ ] = exponent < 0 ? '-' : '+';

    const int magnitude{ exponent < 0 ? -exponent : exponent };

    if ( magnitude >= 100 )
    {
        out[ length++ ] = static_cast< char >( '0' + magnitude / 100 );
    }

    out[ length++ ] = static_cast< char >( '0' + magnitude / 10 % 10 );
    out[ length++ ] = static_cast< char >( '0' + magnitude % 10 );
    return length;
}

ATLAS_FORCE_INLINE int strip_fraction( char *body, int length )
{
    int mantissa_end{ length };

    for ( int index{ }; index < length; ++index )
    {
        if ( body[ index ] == 'e' || body[ index ] == 'E' )
        {
            mantissa_end = index;
            break;
        }
    }

    int has_point{ };
    for ( int index{ }; index < mantissa_end; ++index )
    {
        has_point |= body[ index ] == '.';
    }

    if ( !has_point )
    {
        return length;
    }

    int cut{ mantissa_end };

    while ( body[ cut - 1 ] == '0' )
    {
        --cut;
    }

    if ( body[ cut - 1 ] == '.' )
    {
        --cut;
    }

    const int removed{ mantissa_end - cut };

    for ( int index{ mantissa_end }; index < length; ++index )
    {
        body[ index - removed ] = body[ index ];
    }

    return length - removed;
}

ATLAS_FORCE_INLINE void format_float( format_writer_t &writer, const format_spec_t &spec, double value )
{
    const char conversion{ spec.m_conversion };
    const bool upper{ conversion == 'E' || conversion == 'F' || conversion == 'G' };
    const bool negative{ __builtin_signbit( value ) != 0 };

    char prefix[ 1 ]{ };
    int  prefix_length{ };

    if ( negative )
    {
        prefix[ prefix_length++ ] = '-';
    } else if ( spec.m_plus )
    {
        prefix[ prefix_length++ ] = '+';
    } else if ( spec.m_space )
    {
        prefix[ prefix_length++ ] = ' ';
    }

    if ( __builtin_isinf( value ) || __builtin_isnan( value ) )
    {
        const char *word{ __builtin_isnan( value ) ? ( upper ? "NAN" : "nan" ) : ( upper ? "INF" : "inf" ) };
        writer.field( spec, prefix, prefix_length, word, 3, false );
        return;
    }

    const double magnitude{ negative ? -value : value };
    const int    precision{ spec.m_precision < 0 ? 6 : spec.m_precision };

    char  body[ 512 ]{ };
    int   length{ };
    int   point{ };
    int   sign{ };
    char *end{ };

    if ( conversion == 'f' || conversion == 'F' )
    {
        char *digits{ lua_crt_dtoa( magnitude, 3, precision, &point, &sign, &end ) };
        length = compose_fixed( body, digits, static_cast< int >( end - digits ), point, precision, spec.m_alternate );
        lua_crt_freedtoa( digits );
    } else if ( conversion == 'e' || conversion == 'E' )
    {
        char *digits{ lua_crt_dtoa( magnitude, 2, precision + 1, &point, &sign, &end ) };
        length = compose_exponent( body, digits, static_cast< int >( end - digits ), point - 1, precision,
                                   spec.m_alternate, upper );
        lua_crt_freedtoa( digits );
    } else
    {
        const int significant{ precision ? precision : 1 };
        char     *digits{ lua_crt_dtoa( magnitude, 2, significant, &point, &sign, &end ) };
        const int count{ static_cast< int >( end - digits ) };
        const int exponent{ point - 1 };

        if ( exponent < significant && exponent >= -4 )
        {
            length = compose_fixed( body, digits, count, point, significant - 1 - exponent, spec.m_alternate );
        } else
        {
            length = compose_exponent( body, digits, count, exponent, significant - 1, spec.m_alternate, upper );
        }

        lua_crt_freedtoa( digits );

        if ( !spec.m_alternate )
        {
            length = strip_fraction( body, length );
        }
    }

    writer.field( spec, prefix, prefix_length, body, length, spec.m_zero );
}

extern "C" int lua_crt_sprintf( char *buffer, const char *format, ... )
{
    va_list arguments;
    va_start( arguments, format );

    format_writer_t writer{ buffer };

    for ( const char *cursor{ format }; *cursor; ++cursor )
    {
        if ( *cursor != '%' )
        {
            writer.put( *cursor );
            continue;
        }

        format_spec_t spec{ };
        cursor = parse_spec( cursor + 1, spec );

        switch ( spec.m_conversion )
        {
            case 'd':
            case 'i':
            {
                const long long          value{ spec.m_long_long ? va_arg( arguments, long long ) : va_arg( arguments, int ) };
                const unsigned long long magnitude{ value < 0 ? 0ull - static_cast< unsigned long long >( value )
                                                              : static_cast< unsigned long long >( value ) };
                format_integer( writer, spec, magnitude, value < 0 );
                break;
            }

            case 'u':
            case 'o':
            case 'x':
            case 'X':
            {
                const unsigned long long value{ spec.m_long_long ? va_arg( arguments, unsigned long long )
                                                                 : va_arg( arguments, unsigned int ) };
                format_integer( writer, spec, value, false );
                break;
            }

            case 'c':
            {
                const char value{ static_cast< char >( va_arg( arguments, int ) ) };
                writer.field( spec, nullptr, 0, &value, 1, false );
                break;
            }

            case 's':
            {
                const char *value{ va_arg( arguments, const char * ) };
                int         length{ };

                while ( value[ length ] && ( spec.m_precision < 0 || length < spec.m_precision ) )
                {
                    ++length;
                }

                writer.field( spec, nullptr, 0, value, length, false );
                break;
            }

            case 'p':
            {
                format_spec_t hex{ spec };
                hex.m_conversion = 'x';
                hex.m_alternate  = true;
                format_integer( writer, hex, reinterpret_cast< uptr >( va_arg( arguments, void * ) ), false );
                break;
            }

            case 'e':
            case 'E':
            case 'f':
            case 'F':
            case 'g':
            case 'G': format_float( writer, spec, va_arg( arguments, double ) ); break;

            case '%': writer.put( '%' ); break;
            default: break;
        }
    }

    va_end( arguments );

    writer.put( '\0' );
    return writer.m_written - 1;
}
