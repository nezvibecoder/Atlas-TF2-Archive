#pragma once

#include "util/typedefs.hpp"

struct api_hook_binding
{
    u32 id{ };
    u32 reserved{ };
    u64 encoded_original{ };
};

static_assert( sizeof( api_hook_binding ) == 16 );
static_assert( __builtin_offsetof( api_hook_binding, id ) == 0 );
static_assert( __builtin_offsetof( api_hook_binding, encoded_original ) == 8 );

namespace atlashook
{
    constexpr u64 mix64( u64 value )
    {
        value ^= value >> 30u;
        value *= 0xBF58476D1CE4E5B9ull;
        value ^= value >> 27u;
        value *= 0x94D049BB133111EBull;
        value ^= value >> 31u;
        return value;
    }

    static_assert( mix64( 0x0123456789ABCDEFull ) == 0xB2C058E4EBB5112Cull );

    inline u64 binding_key( const void *binding, u32 id,
                            const u8 *session_id, const u8 *artifact_sha256 )
    {
        if ( !binding || !id || !session_id || !artifact_sha256 )
        {
            return 0;
        }

        u64 mixed{ reinterpret_cast< u64 >( binding ) ^
                   ( static_cast< u64 >( id ) << 32u ) ^ id ^
                   0xD6E8FEB86659FD93ull };

        for ( usize index{ }; index < 16; ++index )
        {
            mixed = mix64( mixed ^ session_id[ index ] ^
                           ( static_cast< u64 >( index + 1 ) * 0x9E3779B185EBCA87ull ) );
        }

        for ( usize index{ }; index < 32; ++index )
        {
            mixed = mix64( mixed ^ artifact_sha256[ index ] ^
                           ( static_cast< u64 >( index + 17 ) * 0xC2B2AE3D27D4EB4Full ) );
        }

        mixed = mix64( mixed );
        return mixed ? mixed : 0xA24BAED4963EE407ull;
    }

    inline void *decode_original( const api_hook_binding *binding,
                                  const u8 *session_id, const u8 *artifact_sha256 )
    {
        if ( !binding )
        {
            return nullptr;
        }

        const u64 key{ binding_key( binding, binding->id, session_id, artifact_sha256 ) };
        return reinterpret_cast< void * >( binding->encoded_original ^ key );
    }
} // namespace atlashook
