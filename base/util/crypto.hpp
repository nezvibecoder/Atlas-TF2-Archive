#pragma once

#include "runtime/vector.hpp"

#include "string.hpp"
#include "typedefs.hpp"

namespace crypto
{
    bool seal( string_view plain, atl::vector< u8 > &out );
    bool open( const u8 *data, usize size, atl::vector< char > &out );

    void sha256( const u8 *data, usize size, u8 out[ 32 ] );
    void hmac_sha256( const u8 *key, usize key_size,
                      const u8 *data, usize data_size, u8 out[ 32 ] );
} // namespace crypto
