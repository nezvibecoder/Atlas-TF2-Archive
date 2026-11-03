#include "assert.hpp"

#include "util/format.hpp"

#if defined( ATLAS_DEV )
static char assert_buf[ 8912 ]{ };

extern "C" void
assert_internal(
    const char *expression,
    const char *file,
    const char *function,
    u32         line )
{
    if ( !expression ||
         !file ||
         !function )
    {
        return;
    }

    atl::format(
        assert_buf,
        sizeof( assert_buf ),
        ATLAS_XORSTR( "Assertion failed on line %i in file %s\n\n"
                      "Expression: %s\n"
                      "Function: %s\n\n"
                      "Attach a debugger and press OK to debug." )
            .get( ),
        line,
        file,
        expression,
        function );
    I( MessageBoxA )( nullptr, assert_buf, ATLAS_XORSTR( "Assertion failed." ).get( ), MB_ICONERROR );
}
#endif // ATLAS_DEV
