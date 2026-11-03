#pragma once

#include <intrin.h>

#include "libs/lazy_importer.hpp"

#include "util/address.hpp"

#include "runtime/array.hpp"
#include "runtime/memory.hpp"
#include "runtime/vector.hpp"

namespace atlashook
{
    enum class error : u8
    {
        none              = 0,
        invalid_argument  = 1 << 0,
        allocation_failed = 1 << 1,
        object_limit      = 1 << 2,
        bad_index         = 1 << 3,
        already_hooked    = 1 << 4,
        unsupported       = 1 << 5,
        protection_failed = 1 << 6,
        modified          = 1 << 7,
    };

    inline constexpr error operator|( error left, error right )
    {
        return static_cast< error >( static_cast< u8 >( left ) | static_cast< u8 >( right ) );
    }

    inline constexpr error operator&( error left, error right )
    {
        return static_cast< error >( static_cast< u8 >( left ) & static_cast< u8 >( right ) );
    }

    inline error &operator|=( error &left, error right )
    {
        left = left | right;
        return left;
    }

    inline constexpr bool has_error( error value, error flag )
    {
        return ( static_cast< u8 >( value & flag ) != 0 );
    }

    namespace detail
    {
        ATLAS_FORCE_INLINE usize mix_entropy( usize value )
        {
            if constexpr ( sizeof( usize ) >= sizeof( u64 ) )
            {
                value ^= value >> 33u;
                value *= static_cast< usize >( 0xC2B2AE3D27D4EB4Full );
                value ^= value >> 29u;
                value *= static_cast< usize >( 0x165667B19E3779F9ull );
                value ^= value >> 32u;
                return value;
            } else
            {
                value ^= value >> 16u;
                value *= static_cast< usize >( 0x7FEB352Du );
                value ^= value >> 15u;
                value *= static_cast< usize >( 0x846CA68Bu );
                value ^= value >> 16u;
                return value;
            }
        }

        ATLAS_FORCE_INLINE usize runtime_entropy( const void *storage )
        {
            usize stack{ reinterpret_cast< usize >( &stack ) };
            usize peb{ reinterpret_cast< usize >( li::detail::peb( ) ) };
            usize cookie{ static_cast< usize >( *reinterpret_cast< volatile u32 * >( static_cast< usize >( 0x7FFE0330u ) ) ) };
            usize ticks{ static_cast< usize >( __rdtsc( ) ) };
            return mix_entropy( stack ^ peb ^ cookie ^ ticks ^ reinterpret_cast< usize >( storage ) );
        }

        ATLAS_FORCE_INLINE usize pointer_key( const void *storage, usize value )
        {
            usize key{ runtime_entropy( storage ) ^ mix_entropy( value + static_cast< usize >( 0x9E3779B97F4A7C15ull ) ) };
            return key ? key : static_cast< usize >( 0xD6E8FEB86659FD93ull );
        }

        template< typename pointer_type >
        class encoded_pointer
        {
        public:
            encoded_pointer( ) = default;

            ATLAS_FORCE_INLINE encoded_pointer( const encoded_pointer &other )
            {
                this->set( other.get( ) );
            }

            ATLAS_FORCE_INLINE encoded_pointer &operator=( const encoded_pointer &other )
            {
                if ( this != &other )
                {
                    this->set( other.get( ) );
                }

                return *this;
            }

            ATLAS_FORCE_INLINE encoded_pointer( encoded_pointer &&other )
            {
                this->set( other.get( ) );
                other.reset( );
            }

            ATLAS_FORCE_INLINE encoded_pointer &operator=( encoded_pointer &&other )
            {
                if ( this != &other )
                {
                    this->set( other.get( ) );
                    other.reset( );
                }

                return *this;
            }

            ATLAS_FORCE_INLINE encoded_pointer( pointer_type value )
            {
                this->set( value );
            }

            ATLAS_FORCE_INLINE encoded_pointer &operator=( pointer_type value )
            {
                this->set( value );
                return *this;
            }

            ATLAS_FORCE_INLINE void set( pointer_type value )
            {
                usize raw{ reinterpret_cast< usize >( value ) };
                this->m_key   = pointer_key( this, raw );
                this->m_value = raw ^ this->m_key;
            }

            ATLAS_FORCE_INLINE pointer_type get( ) const
            {
                return reinterpret_cast< pointer_type >( this->m_value ^ this->m_key );
            }

            ATLAS_FORCE_INLINE void reset( )
            {
                this->m_value = 0;
                this->m_key   = 0;
            }

            explicit ATLAS_FORCE_INLINE operator bool( ) const
            {
                return this->get( ) != pointer_type{ };
            }

        private:
            usize m_value{ };
            usize m_key{ };
        };

        template< typename left, typename right >
        inline constexpr bool is_same{ false };

        template< typename type >
        inline constexpr bool is_same< type, type >{ true };

        inline void copy_bytes( void *destination, const void *source, usize size )
        {
            atl::memcpy( destination, source, size );
        }

        inline void zero_bytes( void *destination, usize size )
        {
            atl::memset( destination, 0, size );
        }

        template< typename ret, typename function_t, typename... args >
        ATLAS_FORCE_INLINE ret call_or_default( function_t function, args... arguments )
        {
            if ( !function )
            {
                if constexpr ( is_same< ret, void > )
                {
                    return;
                } else
                {
                    return ret{ };
                }
            }
            return function( arguments... );
        }
    } // namespace detail
} // namespace atlashook
