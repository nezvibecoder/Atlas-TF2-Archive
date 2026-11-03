#pragma once

#include "libs/lazy_importer.hpp"
#include "util/format.hpp"

namespace exceptions
{
    bool initialize_own_module( );
    long on_caught_exception( PEXCEPTION_POINTERS info );
}
