#pragma once

#include "util/typedefs.hpp"

// @ Should we use VMProtect on this later?
namespace atlas_obfuscation
{
    template< usize... indices >
    struct index_sequence
    {
    };

    template< usize count, usize... indices >
    struct make_index_sequence_impl : make_index_sequence_impl< count - 1u, count - 1u, indices... >
    {
    };

    template< usize... indices >
    struct make_index_sequence_impl< 0u, indices... >
    {
        using type = index_sequence< indices... >;
    };

    template< usize count >
    using make_index_sequence = typename make_index_sequence_impl< count >::type;

    inline constexpr u32 xor32_seed{ 0x8F4B3A71u };
    inline constexpr u64 xorstr_seed{ 0x52F1D09C6A4B378Bull };
    inline constexpr u64 xorwstr_seed{ 0xD49B70A16E35C24Full };

    constexpr u32 rotate_left32( u32 value, u32 bits )
    {
        u32 shift{ bits & 31u };
        return ( value << shift ) | ( value >> ( ( 32u - shift ) & 31u ) );
    }

    constexpr u32 rotate_right32( u32 value, u32 bits )
    {
        u32 shift{ bits & 31u };
        return ( value >> shift ) | ( value << ( ( 32u - shift ) & 31u ) );
    }

    constexpr u32 mix32( u32 value )
    {
        value ^= value >> 16u;
        value *= 0x7FEB352Du;
        value ^= value >> 15u;
        value *= 0x846CA68Bu;
        value ^= value >> 16u;
        return value;
    }

    consteval u32 build_seed32( u32 line, u32 counter, usize size, u32 salt )
    {
        u32 key{ salt ^ 0x811C9DC5u };
        key ^= mix32( line + 0x9E3779B9u );
        key ^= rotate_left32( mix32( counter + 0x85EBCA6Bu ), 11u );
        key ^= rotate_right32( mix32( static_cast< u32 >( size ) + 0xC2B2AE35u ), 7u );

        for ( usize index{ }; index < sizeof( __TIME__ ); ++index )
        {
            key = ( key ^ static_cast< u8 >( __TIME__[ index ] ) ) * 16777619u;
        }

        key = mix32( key ^ rotate_left32( salt, ( counter & 15u ) + 3u ) );
        return key ? key : mix32( salt ^ 0xA5A55A5Au );
    }

    consteval u32 constant_key32( u32 line, u32 counter, usize size, u32 salt )
    {
        return build_seed32( line, counter, size, salt ^ 0xB5297A4Du );
    }

    consteval u32 shift32( u32 line, u32 counter, usize size )
    {
        return ( line + counter + static_cast< u32 >( size * 3u ) ) % 19u + 5u;
    }

    consteval u32 xor_mask32( u32 line, u32 counter, usize size, u32 salt )
    {
        return build_seed32( line ^ 0x27D4EB2Du, counter + 0x165667B1u, size + sizeof( u32 ),
                             salt ^ 0x68E31DA4u );
    }

    consteval u32 addend32( u32 line, u32 counter, usize size, u32 salt )
    {
        return build_seed32( line + 0x9E3779B9u, counter ^ 0xC2B2AE35u, size + sizeof( usize ),
                             salt ^ 0x1B873593u );
    }

    consteval u32 encode_u32( u32 value, u32 line, u32 counter, usize size, u32 salt )
    {
        u32 key{ constant_key32( line, counter, size, salt ) };
        u32 shift{ shift32( line, counter, size ) };
        u32 encoded{ value ^ key };
        encoded ^= xor_mask32( line, counter, size, salt );
        encoded = rotate_left32( encoded, shift );
        encoded += addend32( line, counter, size, salt );
        return encoded;
    }

    ATLAS_FORCE_INLINE u32 decode_u32( u32 value, u32 key, u32 shift, u32 mask, u32 addend )
    {
        volatile u32 decoded{ value };
        decoded -= addend;
        decoded = rotate_right32( decoded, shift );
        decoded ^= mask;
        decoded ^= key;
        return decoded;
    }

    // using this to keep vfunc indexes obfuscated
    template< u32 salt_value >
    class u32_argument
    {
    private:
        static consteval u32 line( u32 value )
        {
            return mix32( value ^ salt_value ^ 0xB5297A4Du );
        }

        static consteval u32 counter( u32 value )
        {
            return rotate_left32( mix32( value + salt_value + 0x68E31DA4u ), value & 15u );
        }

    public:
        consteval u32_argument( u32 value ) : m_value{ encode_u32( value, line( value ), counter( value ), sizeof( value ), salt_value ) },
                                              m_key{ constant_key32( line( value ), counter( value ), sizeof( value ), salt_value ) },
                                              m_shift{ shift32( line( value ), counter( value ), sizeof( value ) ) },
                                              m_mask{ xor_mask32( line( value ), counter( value ), sizeof( value ), salt_value ) },
                                              m_addend{ addend32( line( value ), counter( value ), sizeof( value ), salt_value ) }
        {
        }

        ATLAS_FORCE_INLINE u32 get( ) const
        {
            return decode_u32( this->m_value, this->m_key, this->m_shift, this->m_mask, this->m_addend );
        }

        ATLAS_FORCE_INLINE operator u32( ) const
        {
            return this->get( );
        }

    private:
        u32 m_value{ };
        u32 m_key{ };
        u32 m_shift{ };
        u32 m_mask{ };
        u32 m_addend{ };
    };

    using relative_u32 = u32_argument< 0xA24BAE47u >;
    using vfunc_u32    = u32_argument< 0x6D2B79F5u >;

    constexpr u64 mix64( u64 value )
    {
        value ^= value >> 33u;
        value *= 0xC2B2AE3D27D4EB4Full;
        value ^= value >> 29u;
        value *= 0x165667B19E3779F9ull;
        value ^= value >> 32u;
        return value;
    }

    template< usize file_size >
    consteval u64 build_seed64( u32 line, u32 counter, usize size, usize unit_size, u64 salt,
                                const char ( &file )[ file_size ] )
    {
        u64 key{ salt ^ 14695981039346656037ull };
        key ^= mix64( static_cast< u64 >( line ) + 0x9E3779B97F4A7C15ull );
        key ^= mix64( static_cast< u64 >( counter ) + 0xC2B2AE3D27D4EB4Full );
        key ^= mix64( static_cast< u64 >( size ) << 17u );
        key ^= mix64( static_cast< u64 >( unit_size ) << 41u );

        for ( usize index{ }; index < sizeof( __TIME__ ); ++index )
        {
            key = ( key ^ static_cast< u8 >( __TIME__[ index ] ) ) * 1099511628211ull;
        }

        for ( usize index{ }; index < file_size - 1u; ++index )
        {
            key = ( key ^ static_cast< u8 >( file[ index ] ) ) * 1099511628211ull;
        }

        key = mix64( key ^ ( salt + static_cast< u64 >( counter ) * 0x165667B19E3779F9ull ) );
        return key ? key : mix64( salt ^ 0xD6E8FEB86659FD93ull );
    }

    template< usize file_size >
    consteval u64 constant_key64( u32 line, u32 counter, usize size, usize unit_size, u64 salt,
                                  const char ( &file )[ file_size ] )
    {
        return build_seed64( line, counter, size, unit_size, salt, file );
    }

    template< typename char_type, usize size_value, u64 key_value >
    class string_storage
    {
    public:
        static constexpr usize chars_per_block{ sizeof( u64 ) / sizeof( char_type ) };

        template< typename... values_t >
        constexpr string_storage( values_t... values ) : m_data{ static_cast< char_type >( values )... }
        {
        }

        ~string_storage( )
        {
            volatile char_type *data{ this->m_data };
            for ( usize index{ }; index < size_value; ++index )
            {
                data[ index ] = char_type{ };
            }
        }

        ATLAS_FORCE_INLINE char_type *get( )
        {
            if ( !this->m_decoded )
            {
                u64 mask{ };
                for ( usize index{ }; index < size_value; ++index )
                {
                    if ( index % chars_per_block == 0 )
                    {
                        mask = this->decode_mask( index / chars_per_block );
                    }

                    this->m_data[ index ] = static_cast< char_type >(
                        this->m_data[ index ] ^ static_cast< char_type >(
                                                    mask >> ( ( index % chars_per_block ) * sizeof( char_type ) * 8u ) ) );
                }

                this->m_decoded = true;
            }

            return this->m_data;
        }

        static constexpr char_type encode_mask( usize index )
        {
            u64 mixed{ mix64( key_value ^ static_cast< u64 >( size_value * sizeof( char_type ) ) ^
                              ( static_cast< u64 >( index / chars_per_block ) + 1ull ) * 0x9E3779B97F4A7C15ull ) };
            return static_cast< char_type >( mixed >> ( ( index % chars_per_block ) * sizeof( char_type ) * 8u ) );
        }

        ATLAS_FORCE_INLINE static u64 decode_mask( usize block )
        {
            volatile u64 key{ key_value };

            return mix64( key ^ static_cast< u64 >( size_value * sizeof( char_type ) ) ^
                          ( static_cast< u64 >( block ) + 1ull ) * 0x9E3779B97F4A7C15ull );
        }

    private:
        char_type m_data[ size_value ]{ };
        bool      m_decoded{ };
    };

    template< u64 key_value, typename char_type, usize size_value, usize... indices >
    consteval auto make_string_storage( const char_type ( &value )[ size_value ], index_sequence< indices... > )
    {
        return string_storage< char_type, size_value, key_value >{
            static_cast< char_type >( value[ indices ] ^
                                      string_storage< char_type, size_value, key_value >::encode_mask( indices ) )...
        };
    }
} // namespace atlas_obfuscation

#if !defined( ATLAS_XOR32 )
#define ATLAS_XOR32_IMPL( value, counter )                                                                     \
    ( ::atlas_obfuscation::decode_u32(                                                                         \
        ::atlas_obfuscation::encode_u32( static_cast< u32 >( value ), __LINE__, static_cast< u32 >( counter ), \
                                         sizeof( value ), ::atlas_obfuscation::xor32_seed ),                   \
        ::atlas_obfuscation::constant_key32( __LINE__, static_cast< u32 >( counter ), sizeof( value ),         \
                                             ::atlas_obfuscation::xor32_seed ),                                \
        ::atlas_obfuscation::shift32( __LINE__, static_cast< u32 >( counter ), sizeof( value ) ),              \
        ::atlas_obfuscation::xor_mask32( __LINE__, static_cast< u32 >( counter ), sizeof( value ),             \
                                         ::atlas_obfuscation::xor32_seed ),                                    \
        ::atlas_obfuscation::addend32( __LINE__, static_cast< u32 >( counter ), sizeof( value ),               \
                                       ::atlas_obfuscation::xor32_seed ) ) )
#define ATLAS_XOR32( value ) \
    ATLAS_XOR32_IMPL( value, __COUNTER__ )
#endif

#define ATLAS_XORSTR_IMPL( value, counter )                                                 \
    ::atlas_obfuscation::make_string_storage< ::atlas_obfuscation::constant_key64(          \
        __LINE__, static_cast< u32 >( counter ), sizeof( value ), sizeof( ( value )[ 0 ] ), \
        ::atlas_obfuscation::xorstr_seed, __FILE__ ) >(                                     \
        value, ::atlas_obfuscation::make_index_sequence< sizeof( value ) / sizeof( ( value )[ 0 ] ) >{ } )

#define ATLAS_XORSTR( value ) \
    ATLAS_XORSTR_IMPL( value, __COUNTER__ )

#define ATLAS_XORSTR_STATIC( value )                             \
    ( []( ) -> const char *                                      \
    {                                                            \
        ATLAS_NO_DESTROY static auto s{ ATLAS_XORSTR( value ) }; \
        return s.get( );                                         \
    }( ) )

#define ATLAS_XORWSTR_IMPL( value, counter )                                                \
    ::atlas_obfuscation::make_string_storage< ::atlas_obfuscation::constant_key64(          \
        __LINE__, static_cast< u32 >( counter ), sizeof( value ), sizeof( ( value )[ 0 ] ), \
        ::atlas_obfuscation::xorwstr_seed, __FILE__ ) >(                                    \
        value, ::atlas_obfuscation::make_index_sequence< sizeof( value ) / sizeof( ( value )[ 0 ] ) >{ } )

#define ATLAS_XORWSTR( value ) \
    ATLAS_XORWSTR_IMPL( value, __COUNTER__ )

#define ATLAS_XORWSTR_STATIC( value )                             \
    ( []( ) -> const wchar_t *                                    \
    {                                                             \
        ATLAS_NO_DESTROY static auto s{ ATLAS_XORWSTR( value ) }; \
        return s.get( );                                          \
    }( ) )
