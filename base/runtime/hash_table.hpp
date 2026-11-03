#pragma once

#include <new>

#include "hash.hpp"
#include "memory.hpp"
#include "util/typedefs.hpp"

namespace detail
{
    template< typename type >
    struct table_remove_reference
    {
        using value = type;
    };

    template< typename type >
    struct table_remove_reference< type & >
    {
        using value = type;
    };

    template< typename type >
    struct table_remove_reference< type && >
    {
        using value = type;
    };

    template< typename type >
    using table_remove_reference_t = typename table_remove_reference< type >::value;

    template< typename type >
    constexpr table_remove_reference_t< type > &&table_move( type &&value )
    {
        return static_cast< table_remove_reference_t< type > && >( value );
    }

    template< typename type >
    constexpr type &&table_forward( table_remove_reference_t< type > &value )
    {
        return static_cast< type && >( value );
    }

    template< typename type >
    constexpr type &&table_forward( table_remove_reference_t< type > &&value )
    {
        return static_cast< type && >( value );
    }

    // @ splitmix / murmur3 finalizer
    constexpr u64 hash_mix64( u64 value )
    {
        value ^= value >> 33u;
        value *= 0xff51afd7ed558ccdull;
        value ^= value >> 33u;
        value *= 0xc4ceb9fe1a85ec53ull;
        value ^= value >> 33u;
        return value;
    }

    inline constexpr u64 fnv64_basis{ 0xcbf29ce484222325ull };
    inline constexpr u64 fnv64_prime{ 0x100000001b3ull };

    template< typename type >
    constexpr u64 fnv64_bytes( const type *data, usize count )
    {
        u64 hash{ fnv64_basis };

        for ( usize index{ }; index < count; ++index )
        {
            hash ^= static_cast< u64 >( static_cast< u8 >( data[ index ] ) );
            hash *= fnv64_prime;
        }

        return hash;
    }

    constexpr usize table_align_up( usize value, usize alignment )
    {
        return ( value + alignment - 1 ) & ~( alignment - 1 );
    }

    inline constexpr u32 table_info_empty{ 0u };
    inline constexpr u32 table_info_distance_bits{ 24u };
    inline constexpr u32 table_info_distance_mask{ ( 1u << table_info_distance_bits ) - 1u };
    inline constexpr u32 table_info_fingerprint_shift{ table_info_distance_bits };

    ATLAS_FORCE_INLINE constexpr u32 table_make_info( u32 fingerprint, u32 distance )
    {
        return ( fingerprint << table_info_fingerprint_shift ) | ( distance & table_info_distance_mask );
    }

    ATLAS_FORCE_INLINE constexpr u32 table_info_distance( u32 info )
    {
        return info & table_info_distance_mask;
    }

    ATLAS_FORCE_INLINE constexpr u32 table_info_fingerprint( u32 info )
    {
        return info >> table_info_fingerprint_shift;
    }

    ATLAS_FORCE_INLINE constexpr bool table_info_is_empty( u32 info )
    {
        return ( info & table_info_distance_mask ) == 0u;
    }
} // namespace detail

namespace atl
{
    template< typename key_type >
    struct hasher;

    template< typename key_type >
    struct equal_to
    {
        constexpr bool operator( )( const key_type &left, const key_type &right ) const
        {
            return left == right;
        }
    };

    template<>
    struct hasher< bool >
    {
        constexpr u64 operator( )( bool value ) const
        {
            return detail::hash_mix64( value ? 1ull : 0ull );
        }
    };

    template<>
    struct hasher< char >
    {
        constexpr u64 operator( )( char value ) const
        {
            return detail::hash_mix64( static_cast< u64 >( static_cast< u8 >( value ) ) );
        }
    };

    template<>
    struct hasher< wchar_t >
    {
        constexpr u64 operator( )( wchar_t value ) const
        {
            return detail::hash_mix64( static_cast< u64 >( value ) );
        }
    };

    template<>
    struct hasher< u8 >
    {
        constexpr u64 operator( )( u8 value ) const
        {
            return detail::hash_mix64( static_cast< u64 >( value ) );
        }
    };

    template<>
    struct hasher< u16 >
    {
        constexpr u64 operator( )( u16 value ) const
        {
            return detail::hash_mix64( static_cast< u64 >( value ) );
        }
    };

    template<>
    struct hasher< u32 >
    {
        constexpr u64 operator( )( u32 value ) const
        {
            return detail::hash_mix64( static_cast< u64 >( value ) );
        }
    };

    template<>
    struct hasher< u64 >
    {
        constexpr u64 operator( )( u64 value ) const
        {
            return detail::hash_mix64( value );
        }
    };

    template<>
    struct hasher< i8 >
    {
        constexpr u64 operator( )( i8 value ) const
        {
            return detail::hash_mix64( static_cast< u64 >( static_cast< u8 >( value ) ) );
        }
    };

    template<>
    struct hasher< i16 >
    {
        constexpr u64 operator( )( i16 value ) const
        {
            return detail::hash_mix64( static_cast< u64 >( static_cast< u16 >( value ) ) );
        }
    };

    template<>
    struct hasher< i32 >
    {
        constexpr u64 operator( )( i32 value ) const
        {
            return detail::hash_mix64( static_cast< u64 >( static_cast< u32 >( value ) ) );
        }
    };

    template<>
    struct hasher< i64 >
    {
        constexpr u64 operator( )( i64 value ) const
        {
            return detail::hash_mix64( static_cast< u64 >( value ) );
        }
    };

    template< typename value_type >
    struct hasher< value_type * >
    {
        u64 operator( )( value_type *value ) const
        {
            return detail::hash_mix64( static_cast< u64 >( reinterpret_cast< uptr >( value ) ) );
        }
    };
} // namespace atl

namespace detail
{
    template< typename policy_type >
    class flat_hash_table
    {
    public:
        using key_type   = typename policy_type::key_type;
        using slot_type  = typename policy_type::slot_type;
        using hasher_type = atl::hasher< key_type >;
        using equal_type  = atl::equal_to< key_type >;
        using size_type   = usize;

        flat_hash_table( )                                    = default;
        flat_hash_table( const flat_hash_table & )            = delete;
        flat_hash_table &operator=( const flat_hash_table & ) = delete;

        flat_hash_table( flat_hash_table &&other )
        {
            *this = detail::table_move( other );
        }

        flat_hash_table &operator=( flat_hash_table &&other )
        {
            if ( this == &other )
            {
                return *this;
            }

            this->reset( );

            this->m_info     = other.m_info;
            this->m_slots    = other.m_slots;
            this->m_storage  = other.m_storage;
            this->m_size     = other.m_size;
            this->m_capacity = other.m_capacity;
            this->m_growth   = other.m_growth;

            other.m_info     = nullptr;
            other.m_slots    = nullptr;
            other.m_storage  = nullptr;
            other.m_size     = 0;
            other.m_capacity = 0;
            other.m_growth   = 0;

            return *this;
        }

        ~flat_hash_table( )
        {
            this->reset( );
        }

        size_type size( ) const
        {
            return this->m_size;
        }

        size_type capacity( ) const
        {
            return this->m_capacity;
        }

        bool empty( ) const
        {
            return this->m_size == 0;
        }

        u32 *info_data( ) const
        {
            return this->m_info;
        }

        slot_type *slot_data( ) const
        {
            return this->m_slots;
        }

        bool reserve( size_type count )
        {
            if ( count <= this->m_growth )
            {
                return true;
            }

            // target capacity ensures m_growth >= count; m_growth = cap - cap/5
            size_type target{ 8 };

            while ( target - ( target / 5 ) < count )
            {
                size_type next{ target * 2 };

                if ( next <= target )
                {
                    return false;
                }

                target = next;
            }

            return this->rehash( target );
        }

        void clear( )
        {
            if ( !this->m_capacity )
            {
                return;
            }

            for ( size_type index{ }; index < this->m_capacity; ++index )
            {
                if ( !detail::table_info_is_empty( this->m_info[ index ] ) )
                {
                    this->m_slots[ index ].~slot_type( );
                    this->m_info[ index ] = detail::table_info_empty;
                }
            }

            this->m_size = 0;
        }

        void reset( )
        {
            this->clear( );

            atl::free( this->m_storage );

            this->m_info     = nullptr;
            this->m_slots    = nullptr;
            this->m_storage  = nullptr;
            this->m_capacity = 0;
            this->m_growth   = 0;
        }

        slot_type *find_slot( const key_type &key )
        {
            if ( !this->m_size )
            {
                return nullptr;
            }

            size_type mask{ this->m_capacity - 1 };
            u64       hash{ this->m_hasher( key ) };
            size_type index{ static_cast< size_type >( hash ) & mask };
            u32       distance{ 1u };
            u32       fingerprint{ static_cast< u32 >( hash >> 56u ) };

            while ( true )
            {
                u32 info{ this->m_info[ index ] };
                u32 slot_distance{ detail::table_info_distance( info ) };

                if ( slot_distance < distance )
                {
                    return nullptr;
                }

                if ( slot_distance == distance &&
                     detail::table_info_fingerprint( info ) == fingerprint &&
                     this->m_equal( policy_type::key_from( this->m_slots[ index ] ), key ) )
                {
                    return &this->m_slots[ index ];
                }

                index = ( index + 1 ) & mask;
                ++distance;
            }
        }

        const slot_type *find_slot( const key_type &key ) const
        {
            return const_cast< flat_hash_table * >( this )->find_slot( key );
        }

        template< typename... args_t >
        slot_type *insert( const key_type &key, bool *inserted_out, args_t &&...arguments )
        {
            if ( inserted_out )
            {
                *inserted_out = false;
            }

            if ( this->m_size + 1 > this->m_growth )
            {
                size_type target{ this->m_capacity ? this->m_capacity * 2 : 8 };

                if ( target <= this->m_capacity && this->m_capacity )
                {
                    return nullptr;
                }

                if ( !this->rehash( target ) )
                {
                    return nullptr;
                }
            }

            size_type mask{ this->m_capacity - 1 };
            u64       hash{ this->m_hasher( key ) };
            size_type index{ static_cast< size_type >( hash ) & mask };
            u32       distance{ 1u };
            u32       fingerprint{ static_cast< u32 >( hash >> 56u ) };

            while ( true )
            {
                u32 info{ this->m_info[ index ] };
                u32 slot_distance{ detail::table_info_distance( info ) };

                if ( slot_distance < distance )
                {
                    break;
                }

                if ( slot_distance == distance &&
                     detail::table_info_fingerprint( info ) == fingerprint &&
                     this->m_equal( policy_type::key_from( this->m_slots[ index ] ), key ) )
                {
                    return &this->m_slots[ index ];
                }

                index = ( index + 1 ) & mask;
                ++distance;
            }

            alignas( slot_type ) unsigned char buffer[ sizeof( slot_type ) ]{ };
            slot_type                         *carry{ reinterpret_cast< slot_type * >( &buffer[ 0 ] ) };

            policy_type::construct( carry, key, detail::table_forward< args_t >( arguments )... );

            slot_type *result{ };

            while ( true )
            {
                u32 info{ this->m_info[ index ] };

                if ( detail::table_info_is_empty( info ) )
                {
                    new ( &this->m_slots[ index ] ) slot_type{ detail::table_move( *carry ) };
                    carry->~slot_type( );
                    this->m_info[ index ] = detail::table_make_info( fingerprint, distance );
                    ++this->m_size;

                    if ( inserted_out )
                    {
                        *inserted_out = true;
                    }

                    return result ? result : &this->m_slots[ index ];
                }

                u32 slot_distance{ detail::table_info_distance( info ) };

                if ( slot_distance < distance )
                {
                    u32 slot_fingerprint{ detail::table_info_fingerprint( info ) };

                    slot_type displaced{ detail::table_move( this->m_slots[ index ] ) };
                    this->m_slots[ index ].~slot_type( );
                    new ( &this->m_slots[ index ] ) slot_type{ detail::table_move( *carry ) };
                    carry->~slot_type( );
                    new ( carry ) slot_type{ detail::table_move( displaced ) };

                    this->m_info[ index ] = detail::table_make_info( fingerprint, distance );

                    if ( !result )
                    {
                        result = &this->m_slots[ index ];
                    }

                    fingerprint = slot_fingerprint;
                    distance    = slot_distance;
                }

                index = ( index + 1 ) & mask;
                ++distance;
            }
        }

        bool erase( const key_type &key )
        {
            slot_type *found{ this->find_slot( key ) };

            if ( !found )
            {
                return false;
            }

            size_type index{ static_cast< size_type >( found - this->m_slots ) };
            this->erase_at( index );
            return true;
        }

        size_type next_occupied( size_type index ) const
        {
            while ( index < this->m_capacity &&
                    detail::table_info_is_empty( this->m_info[ index ] ) )
            {
                ++index;
            }

            return index;
        }

    private:
        void erase_at( size_type index )
        {
            size_type mask{ this->m_capacity - 1 };

            this->m_slots[ index ].~slot_type( );
            this->m_info[ index ] = detail::table_info_empty;
            --this->m_size;

            size_type next{ ( index + 1 ) & mask };

            while ( true )
            {
                u32 next_info{ this->m_info[ next ] };
                u32 next_distance{ detail::table_info_distance( next_info ) };

                if ( next_distance <= 1u )
                {
                    return;
                }

                new ( &this->m_slots[ index ] ) slot_type{ detail::table_move( this->m_slots[ next ] ) };
                this->m_slots[ next ].~slot_type( );

                this->m_info[ index ] = detail::table_make_info(
                    detail::table_info_fingerprint( next_info ), next_distance - 1u );
                this->m_info[ next ] = detail::table_info_empty;

                index = next;
                next  = ( next + 1 ) & mask;
            }
        }

        bool rehash( size_type new_capacity )
        {
            size_type rounded{ 8 };

            while ( rounded < new_capacity )
            {
                size_type next{ rounded * 2 };

                if ( next <= rounded )
                {
                    return false;
                }

                rounded = next;
            }

            size_type info_bytes{ rounded * sizeof( u32 ) };
            size_type slot_offset{ detail::table_align_up( info_bytes, alignof( slot_type ) ) };
            size_type total_bytes{ slot_offset + rounded * sizeof( slot_type ) };

            void *raw{ atl::malloc( total_bytes ) };

            if ( !raw )
            {
                return false;
            }

            u32       *new_info{ static_cast< u32 * >( raw ) };
            slot_type *new_slots{ reinterpret_cast< slot_type * >( static_cast< u8 * >( raw ) + slot_offset ) };

            for ( size_type index{ }; index < rounded; ++index )
            {
                new_info[ index ] = detail::table_info_empty;
            }

            u32       *old_info{ this->m_info };
            slot_type *old_slots{ this->m_slots };
            void      *old_storage{ this->m_storage };
            size_type  old_capacity{ this->m_capacity };

            this->m_info     = new_info;
            this->m_slots    = new_slots;
            this->m_storage  = raw;
            this->m_capacity = rounded;
            this->m_growth = rounded - ( rounded / 5 ); // ~80%
            this->m_size   = 0;

            if ( old_info )
            {
                for ( size_type index{ }; index < old_capacity; ++index )
                {
                    if ( detail::table_info_is_empty( old_info[ index ] ) )
                    {
                        continue;
                    }

                    this->reinsert_slot( detail::table_move( old_slots[ index ] ) );
                    old_slots[ index ].~slot_type( );
                }

                atl::free( old_storage );
            }

            return true;
        }

        void reinsert_slot( slot_type &&slot )
        {
            // @ cheap variant of insert used during rehash when we already know the key
            size_type mask{ this->m_capacity - 1 };
            u64       hash{ this->m_hasher( policy_type::key_from( slot ) ) };
            size_type index{ static_cast< size_type >( hash ) & mask };
            u32       distance{ 1u };
            u32       fingerprint{ static_cast< u32 >( hash >> 56u ) };

            alignas( slot_type ) unsigned char buffer[ sizeof( slot_type ) ]{ };
            slot_type                         *carry{ reinterpret_cast< slot_type * >( &buffer[ 0 ] ) };

            new ( carry ) slot_type{ detail::table_move( slot ) };

            while ( true )
            {
                u32 info{ this->m_info[ index ] };

                if ( detail::table_info_is_empty( info ) )
                {
                    new ( &this->m_slots[ index ] ) slot_type{ detail::table_move( *carry ) };
                    carry->~slot_type( );
                    this->m_info[ index ] = detail::table_make_info( fingerprint, distance );
                    ++this->m_size;
                    return;
                }

                u32 slot_distance{ detail::table_info_distance( info ) };

                if ( slot_distance < distance )
                {
                    u32 slot_fingerprint{ detail::table_info_fingerprint( info ) };

                    slot_type displaced{ detail::table_move( this->m_slots[ index ] ) };
                    this->m_slots[ index ].~slot_type( );
                    new ( &this->m_slots[ index ] ) slot_type{ detail::table_move( *carry ) };
                    carry->~slot_type( );
                    new ( carry ) slot_type{ detail::table_move( displaced ) };

                    this->m_info[ index ] = detail::table_make_info( fingerprint, distance );

                    fingerprint = slot_fingerprint;
                    distance    = slot_distance;
                }

                index = ( index + 1 ) & mask;
                ++distance;
            }
        }

        u32         *m_info{ };
        slot_type   *m_slots{ };
        void        *m_storage{ };
        size_type    m_size{ };
        size_type    m_capacity{ };
        size_type    m_growth{ };
        hasher_type  m_hasher{ };
        equal_type   m_equal{ };
    };
} // namespace detail
