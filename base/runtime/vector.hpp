#pragma once

#include <new>

#include "memory.hpp"
#include "util/typedefs.hpp"

namespace detail
{
    template< typename type >
    struct remove_reference
    {
        using value = type;
    };

    template< typename type >
    struct remove_reference< type & >
    {
        using value = type;
    };

    template< typename type >
    struct remove_reference< type && >
    {
        using value = type;
    };

    template< typename type >
    using remove_reference_t = typename remove_reference< type >::value;

    template< typename type >
    constexpr remove_reference_t< type > &&move( type &&value )
    {
        return static_cast< remove_reference_t< type > && >( value );
    }

    inline constexpr usize max_usize{ static_cast< usize >( -1 ) };

    inline bool checked_multiply( usize left, usize right, usize *result )
    {
        if ( !result )
        {
            return false;
        }

        if ( left && right > max_usize / left )
        {
            return false;
        }

        *result = left * right;
        return true;
    }

    inline void *alloc_vector_data( usize size )
    {
        if ( !size )
        {
            return nullptr;
        }

        return atl::malloc( size );
    }

    inline void free_vector_data( void *data )
    {
        atl::free( data );
    }
} // namespace detail

namespace atl
{
    template< typename type >
    class vector
    {
    public:
        using value_type = type;

        vector( )                           = default;
        vector( const vector & )            = delete;
        vector &operator=( const vector & ) = delete;

        constexpr vector( vector &&other )
        {
            *this = detail::move( other );
        }

        constexpr vector &operator=( vector &&other )
        {
            if ( this == &other )
            {
                return *this;
            }

            this->reset( );

            this->m_data     = other.m_data;
            this->m_size     = other.m_size;
            this->m_capacity = other.m_capacity;

            other.m_data     = nullptr;
            other.m_size     = 0;
            other.m_capacity = 0;

            return *this;
        }

        ~vector( )
        {
            this->reset( );
        }

        inline constexpr type *data( )
        {
            return this->m_data;
        }

        inline constexpr const type *data( ) const
        {
            return this->m_data;
        }

        inline constexpr usize size( ) const
        {
            return this->m_size;
        }

        inline constexpr usize capacity( ) const
        {
            return this->m_capacity;
        }

        inline constexpr usize size_bytes( ) const
        {
            return this->m_size * sizeof( type );
        }

        inline constexpr usize capacity_bytes( ) const
        {
            return this->m_capacity * sizeof( type );
        }

        inline constexpr bool empty( ) const
        {
            return this->m_size == 0;
        }

        inline constexpr void swap( vector &other )
        {
            if ( this == &other )
            {
                return;
            }

            type *data{ this->m_data };
            usize size{ this->m_size };
            usize capacity{ this->m_capacity };

            this->m_data     = other.m_data;
            this->m_size     = other.m_size;
            this->m_capacity = other.m_capacity;

            other.m_data     = data;
            other.m_size     = size;
            other.m_capacity = capacity;
        }

        inline constexpr bool valid( usize index ) const
        {
            return index < this->m_size;
        }

        inline constexpr type *begin( )
        {
            return this->m_data;
        }

        inline constexpr const type *begin( ) const
        {
            return this->m_data;
        }

        inline constexpr type *end( )
        {
            return this->m_data ? this->m_data + this->m_size : nullptr;
        }

        inline constexpr const type *end( ) const
        {
            return this->m_data ? this->m_data + this->m_size : nullptr;
        }

        inline constexpr type &operator[]( usize index )
        {
            return this->m_data[ index ];
        }

        inline constexpr const type &operator[]( usize index ) const
        {
            return this->m_data[ index ];
        }

        inline constexpr type *at( usize index )
        {
            return this->valid( index ) ? &this->m_data[ index ] : nullptr;
        }

        inline constexpr const type *at( usize index ) const
        {
            return this->valid( index ) ? &this->m_data[ index ] : nullptr;
        }

        inline constexpr type &front( )
        {
            return this->m_data[ 0 ];
        }

        inline constexpr const type &front( ) const
        {
            return this->m_data[ 0 ];
        }

        inline constexpr type &back( )
        {
            return this->m_data[ this->m_size - 1 ];
        }

        inline constexpr const type &back( ) const
        {
            return this->m_data[ this->m_size - 1 ];
        }

        inline constexpr bool reserve( usize capacity )
        {
            if ( capacity <= this->m_capacity )
            {
                return true;
            }

            return this->reallocate( capacity );
        }

        inline constexpr bool resize( usize size )
        {
            if ( size < this->m_size )
            {
                this->destroy_range( size, this->m_size );
                this->m_size = size;
                return true;
            }

            if ( size == this->m_size )
            {
                return true;
            }

            if ( !this->reserve( size ) )
            {
                return false;
            }

            while ( this->m_size < size )
            {
                new ( &this->m_data[ this->m_size ] ) type{ };
                ++this->m_size;
            }

            return true;
        }

        inline constexpr bool resize( usize size, const type &value )
        {
            if ( size < this->m_size )
            {
                this->destroy_range( size, this->m_size );
                this->m_size = size;
                return true;
            }

            if ( size == this->m_size )
            {
                return true;
            }

            if ( !this->reserve( size ) )
            {
                return false;
            }

            while ( this->m_size < size )
            {
                new ( &this->m_data[ this->m_size ] ) type{ value };
                ++this->m_size;
            }

            return true;
        }

        inline constexpr bool assign( const type *data, usize count )
        {
            if ( count && !data )
            {
                return false;
            }

            if ( !this->reserve( count ) )
            {
                return false;
            }

            this->clear( );

            for ( usize index{ }; index < count; ++index )
            {
                new ( &this->m_data[ index ] ) type{ data[ index ] };
            }

            this->m_size = count;
            return true;
        }

        inline constexpr bool assign( const vector &other )
        {
            if ( this == &other )
            {
                return true;
            }

            return this->assign( other.m_data, other.m_size );
        }

        template< typename... args >
        inline constexpr type *emplace_back( args &&...arguments )
        {
            if ( this->m_size == detail::max_usize )
            {
                return nullptr;
            }

            usize size{ this->m_size + 1 };

            if ( !this->grow_to_fit( size ) )
            {
                return nullptr;
            }

            type *slot{ &this->m_data[ this->m_size ] };
            new ( slot ) type{ static_cast< args && >( arguments )... };
            ++this->m_size;
            return slot;
        }

        inline constexpr bool push_back( const type &value )
        {
            return this->emplace_back( value ) != nullptr;
        }

        inline constexpr bool push_back( type &&value )
        {
            return this->emplace_back( detail::move( value ) ) != nullptr;
        }

        inline constexpr bool pop_back( )
        {
            if ( !this->m_size )
            {
                return false;
            }

            --this->m_size;
            this->m_data[ this->m_size ].~type( );
            return true;
        }

        inline constexpr bool erase( usize index )
        {
            if ( !this->valid( index ) )
            {
                return false;
            }

            this->m_data[ index ].~type( );

            for ( usize current{ index }; current + 1 < this->m_size; ++current )
            {
                new ( &this->m_data[ current ] ) type{ detail::move( this->m_data[ current + 1 ] ) };
                this->m_data[ current + 1 ].~type( );
            }

            --this->m_size;
            return true;
        }

        inline constexpr bool swap_erase( usize index )
        {
            if ( !this->valid( index ) )
            {
                return false;
            }

            usize last{ this->m_size - 1 };
            this->m_data[ index ].~type( );

            if ( index != last )
            {
                new ( &this->m_data[ index ] ) type{ detail::move( this->m_data[ last ] ) };
                this->m_data[ last ].~type( );
            }

            --this->m_size;
            return true;
        }

        inline constexpr void clear( )
        {
            this->destroy_range( 0, this->m_size );
            this->m_size = 0;
        }

        inline constexpr void reset( )
        {
            this->clear( );
            detail::free_vector_data( this->m_data );

            this->m_data     = nullptr;
            this->m_capacity = 0;
        }

    private:
        inline constexpr void destroy_range( usize first, usize last )
        {
            for ( usize index{ first }; index < last; ++index )
            {
                this->m_data[ index ].~type( );
            }
        }

        inline constexpr bool grow_to_fit( usize size )
        {
            if ( size <= this->m_capacity )
            {
                return true;
            }

            usize capacity{ this->m_capacity ? this->m_capacity * 2 : 4 };

            if ( capacity < size || capacity < this->m_capacity )
            {
                capacity = size;
            }

            return this->reserve( capacity );
        }

        inline constexpr bool reallocate( usize capacity )
        {
            usize bytes{ };

            if ( !detail::checked_multiply( capacity, sizeof( type ), &bytes ) )
            {
                return false;
            }

            void *raw_data{ detail::alloc_vector_data( bytes ) };

            if ( !raw_data )
            {
                return false;
            }

            type *data{ static_cast< type * >( raw_data ) };

            for ( usize index{ }; index < this->m_size; ++index )
            {
                new ( &data[ index ] ) type{ detail::move( this->m_data[ index ] ) };
            }

            this->destroy_range( 0, this->m_size );
            detail::free_vector_data( this->m_data );

            this->m_data     = data;
            this->m_capacity = capacity;
            return true;
        }

        type *m_data{ };
        usize m_size{ };
        usize m_capacity{ };
    };
} // namespace atl
