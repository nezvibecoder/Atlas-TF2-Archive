#pragma once

#include "util/typedefs.hpp"

inline constexpr usize array_npos{ static_cast< usize >( -1 ) };

namespace atl
{
    template< typename type, usize size_value >
    class array
    {
    public:
        static_assert( size_value > 0 );

        using value_type = type;

        static constexpr usize static_size{ size_value };

        constexpr type *data( )
        {
            return this->m_data;
        }

        constexpr const type *data( ) const
        {
            return this->m_data;
        }

        constexpr usize size( ) const
        {
            return size_value;
        }

        constexpr usize size_bytes( ) const
        {
            return sizeof( this->m_data );
        }

        constexpr bool empty( ) const
        {
            return false;
        }

        constexpr type *begin( )
        {
            return this->m_data;
        }

        constexpr const type *begin( ) const
        {
            return this->m_data;
        }

        constexpr type *end( )
        {
            return this->m_data + size_value;
        }

        constexpr const type *end( ) const
        {
            return this->m_data + size_value;
        }

        constexpr type &operator[]( usize index )
        {
            return this->m_data[ index ];
        }

        constexpr const type &operator[]( usize index ) const
        {
            return this->m_data[ index ];
        }

        constexpr type &front( )
        {
            return this->m_data[ 0 ];
        }

        constexpr const type &front( ) const
        {
            return this->m_data[ 0 ];
        }

        constexpr type &back( )
        {
            return this->m_data[ size_value - 1 ];
        }

        constexpr const type &back( ) const
        {
            return this->m_data[ size_value - 1 ];
        }

        constexpr bool valid( usize index ) const
        {
            return index < size_value;
        }

        constexpr void fill( const type &value )
        {
            for ( usize index{ }; index < size_value; ++index )
            {
                this->m_data[ index ] = value;
            }
        }

        constexpr void reset( )
        {
            this->fill( type{ } );
        }

        constexpr bool copy_from( const type *source, usize count )
        {
            if ( count > size_value )
            {
                return false;
            }

            if ( count && !source )
            {
                return false;
            }

            for ( usize index{ }; index < count; ++index )
            {
                this->m_data[ index ] = source[ index ];
            }

            return true;
        }

        constexpr usize index_of( const type &value ) const
        {
            for ( usize index{ }; index < size_value; ++index )
            {
                if ( this->m_data[ index ] == value )
                {
                    return index;
                }
            }

            return array_npos;
        }

        constexpr type *find( const type &value )
        {
            usize index{ this->index_of( value ) };
            return index != array_npos ? &this->m_data[ index ] : nullptr;
        }

        constexpr void swap( array &other )
        {
            for ( usize index{ }; index < size_value; ++index )
            {
                type temporary{ this->m_data[ index ] };
                this->m_data[ index ] = other.m_data[ index ];
                other.m_data[ index ] = temporary;
            }
        }

        constexpr usize capacity( ) const
        {
            return size_value;
        }

        constexpr const type &at( usize index ) const
        {
            return this->m_data[ index ];
        }

        constexpr usize count( const type &value ) const
        {
            usize result{ };

            for ( usize index{ }; index < size_value; ++index )
            {
                if ( this->m_data[ index ] == value )
                {
                    ++result;
                }
            }

            return result;
        }

        constexpr const type *find( const type &value ) const
        {
            usize index{ this->index_of( value ) };
            return index != array_npos ? &this->m_data[ index ] : nullptr;
        }

        constexpr bool contains( const type &value ) const
        {
            return this->index_of( value ) != array_npos;
        }

    private:
        type m_data[ size_value ]{ };
    };
} // namespace atl
