#include "ed25519.hpp"


namespace
{
    using gf = i64[ 16 ];

    inline constexpr gf gf0{ };
    inline constexpr gf gf1{ 1 };
    inline constexpr gf curve_d{
        0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
        0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203
    };
    inline constexpr gf curve_d2{
        0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
        0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406
    };
    inline constexpr gf base_x{
        0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
        0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169
    };
    inline constexpr gf base_y{
        0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
        0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666
    };
    inline constexpr gf sqrt_m1{
        0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
        0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83
    };
    inline constexpr u8 field_p[ 32 ]{
        0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
    };
    inline constexpr u8 group_l[ 32 ]{
        0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
        0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
    };
    inline constexpr u8 identity_encoding[ 32 ]{ 1 };

    u64 rotate_right( u64 x, u32 c ) noexcept
    {
        return ( x >> c ) | ( x << ( 64 - c ) );
    }

    u64 load64( const u8 *x ) noexcept
    {
        u64 value{ };
        for ( u32 i{ }; i < 8; ++i )
        {
            value = ( value << 8 ) | x[ i ];
        }
        return value;
    }

    void store64( u8 *x, u64 value ) noexcept
    {
        for ( i32 i{ 7 }; i >= 0; --i )
        {
            x[ i ] = static_cast< u8 >( value );
            value >>= 8;
        }
    }

    bool bytes_equal( const u8 *left, const u8 *right, u32 size ) noexcept
    {
        u8 difference{ };
        for ( u32 i{ }; i < size; ++i )
        {
            difference |= left[ i ] ^ right[ i ];
        }
        return difference == 0;
    }

    bool bytes_less_than( const u8 *left, const u8 *right ) noexcept
    {
        for ( i32 i{ 31 }; i >= 0; --i )
        {
            if ( left[ i ] < right[ i ] )
            {
                return true;
            }
            if ( left[ i ] > right[ i ] )
            {
                return false;
            }
        }
        return false;
    }

    bool canonical_field_encoding( const u8 encoded[ 32 ] ) noexcept
    {
        u8 value[ 32 ]{ };
        for ( u32 i{ }; i < 32; ++i )
        {
            value[ i ] = encoded[ i ];
        }
        value[ 31 ] &= 0x7f;
        return bytes_less_than( value, field_p );
    }

    bool canonical_scalar_encoding( const u8 scalar[ 32 ] ) noexcept
    {
        return bytes_less_than( scalar, group_l );
    }

    void set25519( gf r, const gf a ) noexcept
    {
        for ( u32 i{ }; i < 16; ++i )
        {
            r[ i ] = a[ i ];
        }
    }

    void carry25519( gf o ) noexcept
    {
        for ( u32 i{ }; i < 16; ++i )
        {
            o[ i ] += ( 1ll << 16 );
            i64 carry{ o[ i ] >> 16 };
            o[ ( i + 1 ) * ( i < 15 ) ] += carry - 1 + 37 * ( carry - 1 ) * ( i == 15 );
            o[ i ] -= carry << 16;
        }
    }

    void select25519( gf p, gf q, i32 bit ) noexcept
    {
        i64 c{ ~( static_cast< i64 >( bit ) - 1 ) };
        for ( u32 i{ }; i < 16; ++i )
        {
            i64 t{ c & ( p[ i ] ^ q[ i ] ) };
            p[ i ] ^= t;
            q[ i ] ^= t;
        }
    }

    void pack25519( u8 out[ 32 ], const gf n ) noexcept
    {
        gf m{ };
        gf t{ };
        for ( u32 i{ }; i < 16; ++i )
        {
            t[ i ] = n[ i ];
        }
        carry25519( t );
        carry25519( t );
        carry25519( t );
        for ( u32 j{ }; j < 2; ++j )
        {
            m[ 0 ] = t[ 0 ] - 0xffed;
            for ( u32 i{ 1 }; i < 15; ++i )
            {
                m[ i ] = t[ i ] - 0xffff - ( ( m[ i - 1 ] >> 16 ) & 1 );
                m[ i - 1 ] &= 0xffff;
            }
            m[ 15 ] = t[ 15 ] - 0x7fff - ( ( m[ 14 ] >> 16 ) & 1 );
            i64 bit{ ( m[ 15 ] >> 16 ) & 1 };
            m[ 14 ] &= 0xffff;
            select25519( t, m, static_cast< i32 >( 1 - bit ) );
        }
        for ( u32 i{ }; i < 16; ++i )
        {
            out[ 2 * i ] = static_cast< u8 >( t[ i ] & 0xff );
            out[ 2 * i + 1 ] = static_cast< u8 >( t[ i ] >> 8 );
        }
    }

    bool neq25519( const gf a, const gf b ) noexcept
    {
        u8 c[ 32 ]{ };
        u8 d[ 32 ]{ };
        pack25519( c, a );
        pack25519( d, b );
        return !bytes_equal( c, d, 32 );
    }

    u8 parity25519( const gf a ) noexcept
    {
        u8 d[ 32 ]{ };
        pack25519( d, a );
        return d[ 0 ] & 1;
    }

    void unpack25519( gf o, const u8 n[ 32 ] ) noexcept
    {
        for ( u32 i{ }; i < 16; ++i )
        {
            o[ i ] = n[ 2 * i ] + ( static_cast< i64 >( n[ 2 * i + 1 ] ) << 8 );
        }
        o[ 15 ] &= 0x7fff;
    }

    void add_field( gf out, const gf a, const gf b ) noexcept
    {
        for ( u32 i{ }; i < 16; ++i )
        {
            out[ i ] = a[ i ] + b[ i ];
        }
    }

    void sub_field( gf out, const gf a, const gf b ) noexcept
    {
        for ( u32 i{ }; i < 16; ++i )
        {
            out[ i ] = a[ i ] - b[ i ];
        }
    }

    void mul_field( gf out, const gf a, const gf b ) noexcept
    {
        i64 t[ 31 ]{ };
        for ( u32 i{ }; i < 16; ++i )
        {
            for ( u32 j{ }; j < 16; ++j )
            {
                t[ i + j ] += a[ i ] * b[ j ];
            }
        }
        for ( u32 i{ }; i < 15; ++i )
        {
            t[ i ] += 38 * t[ i + 16 ];
        }
        for ( u32 i{ }; i < 16; ++i )
        {
            out[ i ] = t[ i ];
        }
        carry25519( out );
        carry25519( out );
    }

    void square_field( gf out, const gf a ) noexcept
    {
        mul_field( out, a, a );
    }

    void invert25519( gf out, const gf input ) noexcept
    {
        gf c{ };
        for ( u32 i{ }; i < 16; ++i )
        {
            c[ i ] = input[ i ];
        }
        for ( i32 a{ 253 }; a >= 0; --a )
        {
            square_field( c, c );
            if ( a != 2 && a != 4 )
            {
                mul_field( c, c, input );
            }
        }
        for ( u32 a{ }; a < 16; ++a )
        {
            out[ a ] = c[ a ];
        }
    }

    void pow2523( gf out, const gf input ) noexcept
    {
        gf c{ };
        for ( u32 i{ }; i < 16; ++i )
        {
            c[ i ] = input[ i ];
        }
        for ( i32 a{ 250 }; a >= 0; --a )
        {
            square_field( c, c );
            if ( a != 1 )
            {
                mul_field( c, c, input );
            }
        }
        for ( u32 a{ }; a < 16; ++a )
        {
            out[ a ] = c[ a ];
        }
    }

    inline u64 choose( u64 x, u64 y, u64 z ) noexcept
    {
        return ( x & y ) ^ ( ~x & z );
    }

    inline u64 majority( u64 x, u64 y, u64 z ) noexcept
    {
        return ( x & y ) ^ ( x & z ) ^ ( y & z );
    }

    inline u64 sigma_big0( u64 x ) noexcept
    {
        return rotate_right( x, 28 ) ^ rotate_right( x, 34 ) ^ rotate_right( x, 39 );
    }

    inline u64 sigma_big1( u64 x ) noexcept
    {
        return rotate_right( x, 14 ) ^ rotate_right( x, 18 ) ^ rotate_right( x, 41 );
    }

    inline u64 sigma_small0( u64 x ) noexcept
    {
        return rotate_right( x, 1 ) ^ rotate_right( x, 8 ) ^ ( x >> 7 );
    }

    inline u64 sigma_small1( u64 x ) noexcept
    {
        return rotate_right( x, 19 ) ^ rotate_right( x, 61 ) ^ ( x >> 6 );
    }

    inline constexpr u64 sha512_k[ 80 ]{
        0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull,
        0x3956c25bf348b538ull, 0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull,
        0xd807aa98a3030242ull, 0x12835b0145706fbeull, 0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull,
        0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull, 0xc19bf174cf692694ull,
        0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
        0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull,
        0x983e5152ee66dfabull, 0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull,
        0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull, 0x06ca6351e003826full, 0x142929670a0e6e70ull,
        0x27b70a8546d22ffCull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull, 0x53380d139d95b3dfull,
        0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
        0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull,
        0xd192e819d6ef5218ull, 0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull,
        0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull, 0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull,
        0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull, 0x682e6ff3d6b2b8a3ull,
        0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
        0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull,
        0xca273eceea26619cull, 0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull,
        0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull, 0x113f9804bef90daeull, 0x1b710b35131c471bull,
        0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull, 0x431d67c49c100d4cull,
        0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull
    };

    inline constexpr u8 sha512_iv[ 64 ]{
        0x6a, 0x09, 0xe6, 0x67, 0xf3, 0xbc, 0xc9, 0x08,
        0xbb, 0x67, 0xae, 0x85, 0x84, 0xca, 0xa7, 0x3b,
        0x3c, 0x6e, 0xf3, 0x72, 0xfe, 0x94, 0xf8, 0x2b,
        0xa5, 0x4f, 0xf5, 0x3a, 0x5f, 0x1d, 0x36, 0xf1,
        0x51, 0x0e, 0x52, 0x7f, 0xad, 0xe6, 0x82, 0xd1,
        0x9b, 0x05, 0x68, 0x8c, 0x2b, 0x3e, 0x6c, 0x1f,
        0x1f, 0x83, 0xd9, 0xab, 0xfb, 0x41, 0xbd, 0x6b,
        0x5b, 0xe0, 0xcd, 0x19, 0x13, 0x7e, 0x21, 0x79
    };

    u64 hash_blocks( u8 state[ 64 ], const u8 *message, u64 size ) noexcept
    {
        u64 z[ 8 ]{ };
        u64 a[ 8 ]{ };
        u64 b[ 8 ]{ };
        u64 w[ 16 ]{ };

        for ( u32 i{ }; i < 8; ++i )
        {
            z[ i ] = a[ i ] = load64( state + 8 * i );
        }

        while ( size >= 128 )
        {
            for ( u32 i{ }; i < 16; ++i )
            {
                w[ i ] = load64( message + 8 * i );
            }

            for ( u32 i{ }; i < 80; ++i )
            {
                for ( u32 j{ }; j < 8; ++j )
                {
                    b[ j ] = a[ j ];
                }
                u64 t{
                    a[ 7 ] + sigma_big1( a[ 4 ] ) + choose( a[ 4 ], a[ 5 ], a[ 6 ] ) +
                    sha512_k[ i ] + w[ i % 16 ]
                };
                b[ 7 ] = t + sigma_big0( a[ 0 ] ) + majority( a[ 0 ], a[ 1 ], a[ 2 ] );
                b[ 3 ] += t;
                for ( u32 j{ }; j < 8; ++j )
                {
                    a[ ( j + 1 ) % 8 ] = b[ j ];
                }
                if ( i % 16 == 15 )
                {
                    for ( u32 j{ }; j < 16; ++j )
                    {
                        w[ j ] += w[ ( j + 9 ) % 16 ] +
                            sigma_small0( w[ ( j + 1 ) % 16 ] ) +
                            sigma_small1( w[ ( j + 14 ) % 16 ] );
                    }
                }
            }

            for ( u32 i{ }; i < 8; ++i )
            {
                a[ i ] += z[ i ];
                z[ i ] = a[ i ];
            }

            message += 128;
            size -= 128;
        }

        for ( u32 i{ }; i < 8; ++i )
        {
            store64( state + 8 * i, z[ i ] );
        }

        return size;
    }

    void hash_sha512( u8 out[ 64 ], const u8 *message, u64 size ) noexcept
    {
        u8 h[ 64 ]{ };
        u8 x[ 256 ]{ };
        u64 original_size{ size };
        const u8 *cursor{ message };

        for ( u32 i{ }; i < 64; ++i )
        {
            h[ i ] = sha512_iv[ i ];
        }

        hash_blocks( h, cursor, size );
        cursor += size;
        size &= 127;
        cursor -= size;

        for ( u64 i{ }; i < size; ++i )
        {
            x[ i ] = cursor[ i ];
        }
        x[ size ] = 128;

        u64 padded_size{ static_cast< u64 >( 256 - 128 * ( size < 112 ) ) };
        x[ padded_size - 9 ] = static_cast< u8 >( original_size >> 61 );
        store64( x + padded_size - 8, original_size << 3 );
        hash_blocks( h, x, padded_size );

        for ( u32 i{ }; i < 64; ++i )
        {
            out[ i ] = h[ i ];
        }
    }

    inline constexpr u64 scalar_l[ 32 ]{
        0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
        0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10
    };

    void mod_l( u8 out[ 32 ], i64 x[ 64 ] ) noexcept
    {
        for ( i32 i{ 63 }; i >= 32; --i )
        {
            i64 carry{ };
            i32 j{ };
            for ( j = i - 32; j < i - 12; ++j )
            {
                x[ j ] += carry - 16 * x[ i ] * static_cast< i64 >( scalar_l[ j - ( i - 32 ) ] );
                carry = ( x[ j ] + 128 ) >> 8;
                x[ j ] -= carry << 8;
            }
            x[ j ] += carry;
            x[ i ] = 0;
        }
        i64 carry{ };
        for ( u32 j{ }; j < 32; ++j )
        {
            x[ j ] += carry - ( x[ 31 ] >> 4 ) * static_cast< i64 >( scalar_l[ j ] );
            carry = x[ j ] >> 8;
            x[ j ] &= 255;
        }
        for ( u32 j{ }; j < 32; ++j )
        {
            x[ j ] -= carry * static_cast< i64 >( scalar_l[ j ] );
        }
        for ( u32 i{ }; i < 32; ++i )
        {
            x[ i + 1 ] += x[ i ] >> 8;
            out[ i ] = static_cast< u8 >( x[ i ] & 255 );
        }
    }

    void reduce( u8 scalar[ 64 ] ) noexcept
    {
        i64 x[ 64 ]{ };
        for ( u32 i{ }; i < 64; ++i )
        {
            x[ i ] = scalar[ i ];
            scalar[ i ] = 0;
        }
        mod_l( scalar, x );
    }

    void add_point( gf p[ 4 ], gf q[ 4 ] ) noexcept
    {
        gf a{ };
        gf b{ };
        gf c{ };
        gf d{ };
        gf t{ };
        gf e{ };
        gf f{ };
        gf g{ };
        gf h{ };

        sub_field( a, p[ 1 ], p[ 0 ] );
        sub_field( t, q[ 1 ], q[ 0 ] );
        mul_field( a, a, t );
        add_field( b, p[ 0 ], p[ 1 ] );
        add_field( t, q[ 0 ], q[ 1 ] );
        mul_field( b, b, t );
        mul_field( c, p[ 3 ], q[ 3 ] );
        mul_field( c, c, curve_d2 );
        mul_field( d, p[ 2 ], q[ 2 ] );
        add_field( d, d, d );
        sub_field( e, b, a );
        sub_field( f, d, c );
        add_field( g, d, c );
        add_field( h, b, a );

        mul_field( p[ 0 ], e, f );
        mul_field( p[ 1 ], h, g );
        mul_field( p[ 2 ], g, f );
        mul_field( p[ 3 ], e, h );
    }

    void conditional_swap( gf p[ 4 ], gf q[ 4 ], u8 bit ) noexcept
    {
        for ( u32 i{ }; i < 4; ++i )
        {
            select25519( p[ i ], q[ i ], bit );
        }
    }

    void pack_point( u8 out[ 32 ], gf p[ 4 ] ) noexcept
    {
        gf tx{ };
        gf ty{ };
        gf zi{ };
        invert25519( zi, p[ 2 ] );
        mul_field( tx, p[ 0 ], zi );
        mul_field( ty, p[ 1 ], zi );
        pack25519( out, ty );
        out[ 31 ] ^= parity25519( tx ) << 7;
    }

    void scalar_multiply( gf p[ 4 ], gf q[ 4 ], const u8 scalar[ 32 ] ) noexcept
    {
        set25519( p[ 0 ], gf0 );
        set25519( p[ 1 ], gf1 );
        set25519( p[ 2 ], gf1 );
        set25519( p[ 3 ], gf0 );
        for ( i32 i{ 255 }; i >= 0; --i )
        {
            u8 bit{ static_cast< u8 >( ( scalar[ i / 8 ] >> ( i & 7 ) ) & 1 ) };
            conditional_swap( p, q, bit );
            add_point( q, p );
            add_point( p, p );
            conditional_swap( p, q, bit );
        }
    }

    void scalar_base( gf p[ 4 ], const u8 scalar[ 32 ] ) noexcept
    {
        gf q[ 4 ]{ };
        set25519( q[ 0 ], base_x );
        set25519( q[ 1 ], base_y );
        set25519( q[ 2 ], gf1 );
        mul_field( q[ 3 ], base_x, base_y );
        scalar_multiply( p, q, scalar );
    }

    bool unpack_negative( gf r[ 4 ], const u8 p[ 32 ] ) noexcept
    {
        gf t{ };
        gf chk{ };
        gf num{ };
        gf den{ };
        gf den2{ };
        gf den4{ };
        gf den6{ };
        set25519( r[ 2 ], gf1 );
        unpack25519( r[ 1 ], p );
        square_field( num, r[ 1 ] );
        mul_field( den, num, curve_d );
        sub_field( num, num, r[ 2 ] );
        add_field( den, r[ 2 ], den );

        square_field( den2, den );
        square_field( den4, den2 );
        mul_field( den6, den4, den2 );
        mul_field( t, den6, num );
        mul_field( t, t, den );

        pow2523( t, t );
        mul_field( t, t, num );
        mul_field( t, t, den );
        mul_field( t, t, den );
        mul_field( r[ 0 ], t, den );

        square_field( chk, r[ 0 ] );
        mul_field( chk, chk, den );
        if ( neq25519( chk, num ) )
        {
            mul_field( r[ 0 ], r[ 0 ], sqrt_m1 );
        }

        square_field( chk, r[ 0 ] );
        mul_field( chk, chk, den );
        if ( neq25519( chk, num ) )
        {
            return false;
        }

        if ( parity25519( r[ 0 ] ) == ( p[ 31 ] >> 7 ) )
        {
            sub_field( r[ 0 ], gf0, r[ 0 ] );
        }

        mul_field( r[ 3 ], r[ 0 ], r[ 1 ] );
        return true;
    }

    bool small_order_point( gf point[ 4 ] ) noexcept
    {
        gf work[ 4 ]{ };
        u8 encoded[ 32 ]{ };
        for ( u32 i{ }; i < 4; ++i )
        {
            set25519( work[ i ], point[ i ] );
        }
        add_point( work, work );
        add_point( work, work );
        add_point( work, work );
        pack_point( encoded, work );
        return bytes_equal( encoded, identity_encoding, 32 );
    }

    bool valid_point_encoding( const u8 encoded[ 32 ], gf point[ 4 ] ) noexcept
    {
        return canonical_field_encoding( encoded ) &&
            unpack_negative( point, encoded ) &&
            !small_order_point( point );
    }
} // namespace

namespace ed25519
{
    bool verify( const u8 *message, u32 message_size,
                 const u8 signature[ signature_size ],
                 const u8 public_key[ public_key_size ] ) noexcept
    {
        if ( !message || !signature || !public_key || message_size > maximum_message_size )
        {
            return false;
        }

        if ( !canonical_scalar_encoding( signature + 32 ) )
        {
            return false;
        }

        gf public_point[ 4 ]{ };
        gf r_point[ 4 ]{ };
        if ( !valid_point_encoding( public_key, public_point ) ||
             !valid_point_encoding( signature, r_point ) )
        {
            return false;
        }

        u8 hash_input[ signature_size + maximum_message_size ]{ };
        u32 hash_size{ signature_size + message_size };
        for ( u32 i{ }; i < 32; ++i )
        {
            hash_input[ i ] = signature[ i ];
            hash_input[ i + 32 ] = public_key[ i ];
        }
        for ( u32 i{ }; i < message_size; ++i )
        {
            hash_input[ i + 64 ] = message[ i ];
        }

        u8 h[ 64 ]{ };
        hash_sha512( h, hash_input, hash_size );
        reduce( h );

        gf p[ 4 ]{ };
        gf q[ 4 ]{ };
        scalar_multiply( p, public_point, h );
        scalar_base( q, signature + 32 );
        add_point( p, q );

        u8 check[ 32 ]{ };
        pack_point( check, p );
        return bytes_equal( signature, check, 32 );
    }
} // namespace ed25519
