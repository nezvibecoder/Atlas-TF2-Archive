#pragma once

#include "libs/lazy_importer.hpp"

#if defined( ATLAS_DEV )
extern "C" void
assert_internal(
    const char *expression,
    const char *file,
    const char *function,
    u32         line );

#define Assert( expression )                                                  \
    {                                                                         \
        if ( ( expression ) != true )                                         \
        {                                                                     \
            assert_internal( #expression, __FILE__, __FUNCTION__, __LINE__ ); \
        }                                                                     \
    }
#else
#define Assert( expression )
#endif // _DEBUG
