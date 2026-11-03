#pragma clang section rodata = ".lstr$m"

#define IEEE_8087
#define NO_ERRNO
#define NO_HEX_FP
#define NO_INFNAN_CHECK

#define PRIVATE_MEM ( 64 * 1024 )
#define MALLOC      lua_crt_dtoa_overflow

#define dtoa     lua_crt_dtoa
#define dtoa_r   lua_crt_dtoa_r
#define freedtoa lua_crt_freedtoa

#include "libs/dtoa.c"

extern "C" void *lua_crt_dtoa_overflow( size_t )
{
    __builtin_trap( );
}
