#define LUA_USE_LONGJMP
#define LUA_USE_JUMPTABLE        1
#define l_signalT                int
#define lua_getlocaledecpoint( ) '.'

#pragma clang section rodata = ".lstr$m"

#define lua_writestring( text, size )       ( ( void )0 )
#define lua_writeline( )                    ( ( void )0 )
#define lua_writestringerror( format, arg ) ( ( void )0 )

#include "lprefix.h"

#define LUA_CORE
#define LUA_LIB
#define ltable_c
#define lvm_c
#include "luaconf.h"

// clang-format off
#include "lzio.c"
#include "lctype.c"
#include "lopcodes.c"
#include "lmem.c"
#include "lundump.c"
#include "ldump.c"
#include "lstate.c"
#include "lgc.c"
#include "llex.c"
#include "lcode.c"
#include "lparser.c"
#include "ldebug.c"
#include "lfunc.c"
#include "lobject.c"
#include "ltm.c"
#include "lstring.c"
#include "ltable.c"
#include "ldo.c"
#include "lvm.c"
#include "lapi.c"

#include "lauxlib.c"

#include "lbaselib.c"
#include "lcorolib.c"
#include "lmathlib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"
// clang-format on
