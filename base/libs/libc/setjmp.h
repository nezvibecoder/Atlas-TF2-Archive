#pragma once

typedef unsigned long long jmp_buf[ 32 ];

extern "C" int               lua_crt_setjmp( jmp_buf buffer ) __attribute__( ( returns_twice ) );
extern "C" [[noreturn]] void lua_crt_longjmp( jmp_buf buffer, int value );

#define setjmp( buffer )         lua_crt_setjmp( buffer )
#define longjmp( buffer, value ) lua_crt_longjmp( buffer, value )
