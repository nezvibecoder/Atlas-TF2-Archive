#include "crypto.hpp"

#include <intrin.h>

#include "runtime/memory.hpp"
#include "runtime/obfuscation.hpp"

constexpr u32   SEAL_MAGIC{ 0x46435441u }; // ATCF
constexpr u32   SEAL_VERSION{ 1 };         // occupies the two reserved bytes behind it as well
constexpr usize SEAL_HEADER{ 20 };         // magic, version, nonce
constexpr usize SEAL_NONCE_OFFSET{ 8 };
constexpr usize SEAL_TAG{ 8 };

constexpr u32 CHACHA_CONSTANTS[ 4 ]{ 0x61707865u, 0x3320646Eu, 0x79622D32u, 0x6B206574u };

ATLAS_FORCE_INLINE u32 load_u32( const u8 *source )
{
    return static_cast< u32 >( source[ 0 ] ) | ( static_cast< u32 >( source[ 1 ] ) << 8u ) |
           ( static_cast< u32 >( source[ 2 ] ) << 16u ) | ( static_cast< u32 >( source[ 3 ] ) << 24u );
}

ATLAS_FORCE_INLINE u64 load_u64( const u8 *source )
{
    return static_cast< u64 >( load_u32( source ) ) | ( static_cast< u64 >( load_u32( source + 4 ) ) << 32u );
}

ATLAS_FORCE_INLINE void store_u32( u8 *destination, u32 value )
{
    destination[ 0 ] = static_cast< u8 >( value );
    destination[ 1 ] = static_cast< u8 >( value >> 8u );
    destination[ 2 ] = static_cast< u8 >( value >> 16u );
    destination[ 3 ] = static_cast< u8 >( value >> 24u );
}

ATLAS_FORCE_INLINE void store_u64( u8 *destination, u64 value )
{
    store_u32( destination, static_cast< u32 >( value ) );
    store_u32( destination + 4, static_cast< u32 >( value >> 32u ) );
}

ATLAS_FORCE_INLINE void quarter_round( u32 &a, u32 &b, u32 &c, u32 &d )
{
    a += b;
    d = _rotl( d ^ a, 16 );
    c += d;
    b = _rotl( b ^ c, 12 );
    a += b;
    d = _rotl( d ^ a, 8 );
    c += d;
    b = _rotl( b ^ c, 7 );
}

static void chacha20_block( const u32 state[ 16 ], u8 out[ 64 ] )
{
    u32 working[ 16 ]{ };

    for ( usize index{ }; index < 16; ++index )
    {
        working[ index ] = state[ index ];
    }

    for ( usize round{ }; round < 10; ++round )
    {
        quarter_round( working[ 0 ], working[ 4 ], working[ 8 ], working[ 12 ] );
        quarter_round( working[ 1 ], working[ 5 ], working[ 9 ], working[ 13 ] );
        quarter_round( working[ 2 ], working[ 6 ], working[ 10 ], working[ 14 ] );
        quarter_round( working[ 3 ], working[ 7 ], working[ 11 ], working[ 15 ] );
        quarter_round( working[ 0 ], working[ 5 ], working[ 10 ], working[ 15 ] );
        quarter_round( working[ 1 ], working[ 6 ], working[ 11 ], working[ 12 ] );
        quarter_round( working[ 2 ], working[ 7 ], working[ 8 ], working[ 13 ] );
        quarter_round( working[ 3 ], working[ 4 ], working[ 9 ], working[ 14 ] );
    }

    for ( usize index{ }; index < 16; ++index )
    {
        store_u32( out + index * 4, working[ index ] + state[ index ] );
    }
}

static void chacha20_xor( const u8 key[ 32 ], const u8 nonce[ 12 ], u32 counter, u8 *buffer, usize size )
{
    u32 state[ 16 ]{ };

    for ( usize index{ }; index < 4; ++index )
    {
        state[ index ] = CHACHA_CONSTANTS[ index ];
    }

    for ( usize index{ }; index < 8; ++index )
    {
        state[ 4 + index ] = load_u32( key + index * 4 );
    }

    state[ 12 ] = counter;

    for ( usize index{ }; index < 3; ++index )
    {
        state[ 13 + index ] = load_u32( nonce + index * 4 );
    }

    u8 block[ 64 ]{ };

    for ( usize offset{ }; offset < size; offset += 64 )
    {
        chacha20_block( state, block );

        const usize left{ size - offset };
        const usize chunk{ left < 64 ? left : 64 };

        for ( usize index{ }; index < chunk; ++index )
        {
            buffer[ offset + index ] ^= block[ index ];
        }

        ++state[ 12 ];
    }
}

ATLAS_FORCE_INLINE void sip_round( u64 &v0, u64 &v1, u64 &v2, u64 &v3 )
{
    v0 += v1;
    v1 = _rotl64( v1, 13 );
    v1 ^= v0;
    v0 = _rotl64( v0, 32 );
    v2 += v3;
    v3 = _rotl64( v3, 16 );
    v3 ^= v2;
    v0 += v3;
    v3 = _rotl64( v3, 21 );
    v3 ^= v0;
    v2 += v1;
    v1 = _rotl64( v1, 17 );
    v1 ^= v2;
    v2 = _rotl64( v2, 32 );
}

static u64 siphash( const u8 key[ 16 ], const u8 *data, usize size )
{
    const u64 k0{ load_u64( key ) };
    const u64 k1{ load_u64( key + 8 ) };

    u64 v0{ k0 ^ 0x736F6D6570736575ull };
    u64 v1{ k1 ^ 0x646F72616E646F6Dull };
    u64 v2{ k0 ^ 0x6C7967656E657261ull };
    u64 v3{ k1 ^ 0x7465646279746573ull };

    const usize whole{ size - ( size & 7u ) };

    for ( usize offset{ }; offset < whole; offset += 8 )
    {
        const u64 word{ load_u64( data + offset ) };

        v3 ^= word;
        sip_round( v0, v1, v2, v3 );
        sip_round( v0, v1, v2, v3 );
        v0 ^= word;
    }

    u64 tail{ static_cast< u64 >( size & 0xFFu ) << 56u };
    for ( usize index{ size & 7u }; index; )
    {
        --index;
        tail |= static_cast< u64 >( data[ whole + index ] ) << ( index * 8u );
    }

    v3 ^= tail;
    sip_round( v0, v1, v2, v3 );
    sip_round( v0, v1, v2, v3 );
    v0 ^= tail;

    v2 ^= 0xFFu;
    sip_round( v0, v1, v2, v3 );
    sip_round( v0, v1, v2, v3 );
    sip_round( v0, v1, v2, v3 );
    sip_round( v0, v1, v2, v3 );

    return v0 ^ v1 ^ v2 ^ v3;
}

static void load_keys( u8 cipher[ 32 ], u8 mac[ 16 ] )
{
    const u32 cipher_words[ 8 ]{
        ATLAS_XOR32( 0x08F12747u ), ATLAS_XOR32( 0xBB0BA59Du ), ATLAS_XOR32( 0xE6B5DBABu ), ATLAS_XOR32( 0xC65AD4BCu ),
        ATLAS_XOR32( 0xB446F98Fu ), ATLAS_XOR32( 0xE5376560u ), ATLAS_XOR32( 0x704D5A49u ), ATLAS_XOR32( 0x07E927C8u )
    };

    const u32 mac_words[ 4 ]{
        ATLAS_XOR32( 0xE1C51042u ), ATLAS_XOR32( 0xC728D0B4u ), ATLAS_XOR32( 0x8F966BEEu ), ATLAS_XOR32( 0x92CF6234u )
    };

    for ( usize index{ }; index < 8; ++index )
    {
        store_u32( cipher + index * 4, cipher_words[ index ] );
    }

    for ( usize index{ }; index < 4; ++index )
    {
        store_u32( mac + index * 4, mac_words[ index ] );
    }
}

static void fill_nonce( u8 nonce[ 12 ] )
{
    u64 mixed{ __rdtsc( ) };

    mixed ^= reinterpret_cast< usize >( li::detail::peb( ) ); // moves with aslr
    mixed ^= *reinterpret_cast< volatile u32 * >( static_cast< usize >( 0x7FFE0330u ) );
    mixed ^= reinterpret_cast< u64 >( nonce );

    store_u64( nonce, atlas_obfuscation::mix64( mixed ) );
    store_u32( nonce + 8, static_cast< u32 >( __rdtsc( ) ) );
}

namespace crypto
{
    bool seal( string_view plain, atl::vector< u8 > &out )
    {
        const usize total{ SEAL_HEADER + plain.size( ) + SEAL_TAG };

        out.reset( );

        if ( !out.resize( total ) )
        {
            return false;
        }

        u8 *bytes{ out.data( ) };

        store_u32( bytes, SEAL_MAGIC );
        store_u32( bytes + 4, SEAL_VERSION );
        fill_nonce( bytes + SEAL_NONCE_OFFSET );
        atl::memcpy( bytes + SEAL_HEADER, plain.data( ), plain.size( ) );

        u8 cipher[ 32 ]{ };
        u8 mac[ 16 ]{ };
        load_keys( cipher, mac );

        chacha20_xor( cipher, bytes + SEAL_NONCE_OFFSET, 0, bytes + SEAL_HEADER, plain.size( ) );
        store_u64( bytes + SEAL_HEADER + plain.size( ), siphash( mac, bytes, SEAL_HEADER + plain.size( ) ) );
        return true;
    }

    bool open( const u8 *data, usize size, atl::vector< char > &out )
    {
        if ( !data || size < SEAL_HEADER + SEAL_TAG || load_u32( data ) != SEAL_MAGIC ||
             load_u32( data + 4 ) != SEAL_VERSION )
        {
            return false;
        }

        u8 cipher[ 32 ]{ };
        u8 mac[ 16 ]{ };
        load_keys( cipher, mac );

        const usize sealed{ size - SEAL_TAG };

        if ( siphash( mac, data, sealed ) != load_u64( data + sealed ) )
        {
            return false;
        }

        atl::vector< char > text{ };

        if ( !text.resize( sealed - SEAL_HEADER ) )
        {
            return false;
        }

        atl::memcpy( text.data( ), data + SEAL_HEADER, text.size( ) );
        chacha20_xor( cipher, data + SEAL_NONCE_OFFSET, 0, reinterpret_cast< u8 * >( text.data( ) ), text.size( ) );

        out = static_cast< atl::vector< char > && >( text );
        return true;
    }

    namespace
    {
        constexpr u32 SHA256_K[ 64 ]{
            0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u, 0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
            0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u, 0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
            0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu, 0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
            0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u, 0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
            0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u, 0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
            0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u, 0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
            0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u, 0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
            0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u, 0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u };

        inline u32 rotr32( u32 value, u32 bits ) { return ( value >> bits ) | ( value << ( 32u - bits ) ); }

        void sha256_compress( u32 state[ 8 ], const u8 block[ 64 ] )
        {
            u32 w[ 64 ];
            for ( usize i{ }; i < 16; ++i )
            {
                w[ i ] = ( static_cast< u32 >( block[ i * 4 + 0 ] ) << 24 ) |
                         ( static_cast< u32 >( block[ i * 4 + 1 ] ) << 16 ) |
                         ( static_cast< u32 >( block[ i * 4 + 2 ] ) << 8  ) |
                         ( static_cast< u32 >( block[ i * 4 + 3 ] )       );
            }
            for ( usize i{ 16 }; i < 64; ++i )
            {
                const u32 s0{ rotr32( w[ i - 15 ], 7 ) ^ rotr32( w[ i - 15 ], 18 ) ^ ( w[ i - 15 ] >> 3  ) };
                const u32 s1{ rotr32( w[ i -  2 ], 17 ) ^ rotr32( w[ i -  2 ], 19 ) ^ ( w[ i -  2 ] >> 10 ) };
                w[ i ] = w[ i - 16 ] + s0 + w[ i - 7 ] + s1;
            }
            u32 a{ state[ 0 ] }, b{ state[ 1 ] }, c{ state[ 2 ] }, d{ state[ 3 ] };
            u32 e{ state[ 4 ] }, f{ state[ 5 ] }, g{ state[ 6 ] }, h{ state[ 7 ] };
            for ( usize i{ }; i < 64; ++i )
            {
                const u32 S1{ rotr32( e, 6 ) ^ rotr32( e, 11 ) ^ rotr32( e, 25 ) };
                const u32 ch{ ( e & f ) ^ ( ( ~e ) & g ) };
                const u32 t1{ h + S1 + ch + SHA256_K[ i ] + w[ i ] };
                const u32 S0{ rotr32( a, 2 ) ^ rotr32( a, 13 ) ^ rotr32( a, 22 ) };
                const u32 mj{ ( a & b ) ^ ( a & c ) ^ ( b & c ) };
                const u32 t2{ S0 + mj };
                h = g; g = f; f = e; e = d + t1;
                d = c; c = b; b = a; a = t1 + t2;
            }
            state[ 0 ] += a; state[ 1 ] += b; state[ 2 ] += c; state[ 3 ] += d;
            state[ 4 ] += e; state[ 5 ] += f; state[ 6 ] += g; state[ 7 ] += h;
        }
    }

    void sha256( const u8 *data, usize size, u8 out[ 32 ] )
    {
        u32 state[ 8 ]{ 0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
                        0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u };
        const u64 bit_len{ static_cast< u64 >( size ) * 8ull };
        u8 block[ 64 ]{ };
        usize remaining{ size };
        const u8 *cursor{ data };
        while ( remaining >= 64 )
        {
            sha256_compress( state, cursor );
            cursor += 64;
            remaining -= 64;
        }
        for ( usize i{ }; i < remaining; ++i ) block[ i ] = cursor[ i ];
        block[ remaining ] = 0x80;
        if ( remaining >= 56 )
        {
            sha256_compress( state, block );
            for ( usize i{ }; i < 64; ++i ) block[ i ] = 0;
        }
        for ( usize i{ }; i < 8; ++i )
        {
            block[ 56 + i ] = static_cast< u8 >( bit_len >> ( ( 7 - i ) * 8u ) );
        }
        sha256_compress( state, block );
        for ( usize i{ }; i < 8; ++i )
        {
            out[ i * 4 + 0 ] = static_cast< u8 >( state[ i ] >> 24 );
            out[ i * 4 + 1 ] = static_cast< u8 >( state[ i ] >> 16 );
            out[ i * 4 + 2 ] = static_cast< u8 >( state[ i ] >> 8  );
            out[ i * 4 + 3 ] = static_cast< u8 >( state[ i ]       );
        }
    }

    void hmac_sha256( const u8 *key, usize key_size,
                      const u8 *data, usize data_size, u8 out[ 32 ] )
    {
        u8 k_block[ 64 ]{ };
        if ( key_size > 64 )
        {
            sha256( key, key_size, k_block );
        }
        else
        {
            for ( usize i{ }; i < key_size; ++i ) k_block[ i ] = key[ i ];
        }

        u8 ipad[ 64 ]{ };
        u8 opad[ 64 ]{ };
        for ( usize i{ }; i < 64; ++i )
        {
            ipad[ i ] = k_block[ i ] ^ 0x36;
            opad[ i ] = k_block[ i ] ^ 0x5C;
        }

        u8 inner_input[ 64 + 1024 ];
        atl::vector< u8 > inner_buf{ };
        const u8 *inner_src{ };
        usize     inner_src_size{ };
        if ( data_size + 64 <= sizeof( inner_input ) )
        {
            for ( usize i{ }; i < 64; ++i ) inner_input[ i ] = ipad[ i ];
            for ( usize i{ }; i < data_size; ++i ) inner_input[ 64 + i ] = data[ i ];
            inner_src      = inner_input;
            inner_src_size = 64 + data_size;
        }
        else
        {
            inner_buf.reserve( 64 + data_size );
            for ( usize i{ }; i < 64; ++i ) inner_buf.push_back( ipad[ i ] );
            for ( usize i{ }; i < data_size; ++i ) inner_buf.push_back( data[ i ] );
            inner_src      = inner_buf.data( );
            inner_src_size = inner_buf.size( );
        }
        u8 inner_hash[ 32 ];
        sha256( inner_src, inner_src_size, inner_hash );

        u8 outer_input[ 64 + 32 ];
        for ( usize i{ }; i < 64; ++i ) outer_input[ i ] = opad[ i ];
        for ( usize i{ }; i < 32; ++i ) outer_input[ 64 + i ] = inner_hash[ i ];
        sha256( outer_input, sizeof( outer_input ), out );
    }
} // namespace crypto
