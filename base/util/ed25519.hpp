#pragma once

#include "typedefs.hpp"

namespace ed25519
{
    inline constexpr u32 public_key_size{ 32 };
    inline constexpr u32 signature_size{ 64 };
    inline constexpr u32 maximum_message_size{ 128 };

    bool verify( const u8 *message, u32 message_size,
                 const u8 signature[ signature_size ],
                 const u8 public_key[ public_key_size ] ) noexcept;
} // namespace ed25519
