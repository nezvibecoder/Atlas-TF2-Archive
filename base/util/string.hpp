#pragma once

#include "typedefs.hpp"

#include "libs/lazy_importer.hpp"

namespace str
{
    inline constexpr usize npos{ static_cast< usize >( -1 ) };

    inline constexpr bool is_space( char value )
    {
        return value == ' ' || value == '\t';
    }

    inline constexpr bool is_digit( char value )
    {
        return value >= '0' && value <= '9';
    }

    inline constexpr bool is_alpha( char value )
    {
        return ( value >= 'A' && value <= 'Z' ) || ( value >= 'a' && value <= 'z' );
    }

    inline constexpr bool is_upper( char value )
    {
        return value >= 'A' && value <= 'Z';
    }

    inline constexpr bool is_lower( char value )
    {
        return value >= 'a' && value <= 'z';
    }

    inline constexpr char to_lower( char value )
    {
        if ( value >= 'A' && value <= 'Z' )
        {
            return static_cast< char >( value + 32 );
        }

        return value;
    }

    inline constexpr wchar_t to_lower( wchar_t value )
    {
        if ( value >= L'A' && value <= L'Z' )
        {
            return static_cast< wchar_t >( value + 32 );
        }

        return value;
    }

    inline constexpr bool bytes_equal( const u8 *left, const u8 *right, usize size )
    {
        if ( !size )
        {
            return true;
        }

        if ( !left || !right )
        {
            return false;
        }

        for ( usize index{ }; index < size; ++index )
        {
            if ( left[ index ] != right[ index ] )
            {
                return false;
            }
        }

        return true;
    }

    inline constexpr bool bytes_empty( const u8 *value, usize size )
    {
        if ( !value )
        {
            return true;
        }

        for ( usize index{ }; index < size; ++index )
        {
            if ( value[ index ] )
            {
                return false;
            }
        }

        return true;
    }

    inline constexpr usize strlen( char *head )
    {
        char *index{ head };

        while ( *index != '\0' )
        {
            ++index;
        }

        return static_cast< usize >( index - head );
    }

    template< typename char_type >
    constexpr usize length( const char_type *value )
    {
        if ( !value )
        {
            return 0;
        }

        usize size{ };

        while ( value[ size ] )
        {
            ++size;
        }

        return size;
    }

    template< typename char_type >
    constexpr usize bounded_length( const char_type *value, usize capacity )
    {
        if ( !value || !capacity )
        {
            return 0;
        }

        usize size{ };

        while ( size < capacity && value[ size ] )
        {
            ++size;
        }

        return size;
    }

    template< typename char_type >
    constexpr i32 compare( const char_type *left, usize left_size, const char_type *right, usize right_size )
    {
        if ( !left_size && !right_size )
        {
            return 0;
        }

        if ( !left )
        {
            return -1;
        }

        if ( !right )
        {
            return 1;
        }

        usize count{ left_size < right_size ? left_size : right_size };

        for ( usize index{ }; index < count; ++index )
        {
            if ( left[ index ] < right[ index ] )
            {
                return -1;
            }

            if ( left[ index ] > right[ index ] )
            {
                return 1;
            }
        }

        if ( left_size < right_size )
        {
            return -1;
        }

        if ( left_size > right_size )
        {
            return 1;
        }

        return 0;
    }

    template< typename char_type >
    constexpr i32 compare( const char_type *left, const char_type *right )
    {
        return compare( left, length( left ), right, length( right ) );
    }

    template< typename char_type >
    constexpr i32 compare_ignore_case( const char_type *left, usize left_size, const char_type *right,
                                       usize right_size )
    {
        if ( !left_size && !right_size )
        {
            return 0;
        }

        if ( !left )
        {
            return -1;
        }

        if ( !right )
        {
            return 1;
        }

        usize count{ left_size < right_size ? left_size : right_size };

        for ( usize index{ }; index < count; ++index )
        {
            char_type left_value{ to_lower( left[ index ] ) };
            char_type right_value{ to_lower( right[ index ] ) };

            if ( left_value < right_value )
            {
                return -1;
            }

            if ( left_value > right_value )
            {
                return 1;
            }
        }

        if ( left_size < right_size )
        {
            return -1;
        }

        if ( left_size > right_size )
        {
            return 1;
        }

        return 0;
    }

    template< typename char_type >
    constexpr i32 compare_ignore_case( const char_type *left, const char_type *right )
    {
        return compare_ignore_case( left, length( left ), right, length( right ) );
    }

    template< typename char_type >
    constexpr bool equals( const char_type *left, usize left_size, const char_type *right, usize right_size )
    {
        return compare( left, left_size, right, right_size ) == 0;
    }

    template< typename char_type >
    constexpr bool equals( const char_type *left, const char_type *right )
    {
        return compare( left, right ) == 0;
    }

    template< typename char_type >
    constexpr bool equals_ignore_case( const char_type *left, usize left_size, const char_type *right,
                                       usize right_size )
    {
        return compare_ignore_case( left, left_size, right, right_size ) == 0;
    }

    template< typename char_type >
    constexpr bool equals_ignore_case( const char_type *left, const char_type *right )
    {
        return compare_ignore_case( left, right ) == 0;
    }

    template< typename char_type >
    constexpr bool starts_with( const char_type *value, usize value_size, const char_type *prefix,
                                usize prefix_size )
    {
        if ( !prefix_size )
        {
            return true;
        }

        if ( prefix_size > value_size )
        {
            return false;
        }

        if ( !value || !prefix )
        {
            return false;
        }

        return equals( value, prefix_size, prefix, prefix_size );
    }

    template< typename char_type >
    constexpr bool ends_with( const char_type *value, usize value_size, const char_type *suffix,
                              usize suffix_size )
    {
        if ( !suffix_size )
        {
            return true;
        }

        if ( suffix_size > value_size )
        {
            return false;
        }

        if ( !value || !suffix )
        {
            return false;
        }

        return equals( value + value_size - suffix_size, suffix_size, suffix, suffix_size );
    }

    template< typename char_type >
    constexpr usize find( const char_type *value, usize value_size, char_type needle, usize offset = 0 )
    {
        if ( !value || offset >= value_size )
        {
            return npos;
        }

        for ( usize index{ offset }; index < value_size; ++index )
        {
            if ( value[ index ] == needle )
            {
                return index;
            }
        }

        return npos;
    }

    template< typename char_type >
    constexpr usize find( const char_type *value, usize value_size, const char_type *needle, usize needle_size,
                          usize offset = 0 )
    {
        if ( !needle_size )
        {
            return offset <= value_size ? offset : npos;
        }

        if ( !value || !needle || offset >= value_size || needle_size > value_size - offset )
        {
            return npos;
        }

        usize last_index{ value_size - needle_size };

        for ( usize index{ offset }; index <= last_index; ++index )
        {
            if ( equals( value + index, needle_size, needle, needle_size ) )
            {
                return index;
            }
        }

        return npos;
    }

    template< typename char_type >
    constexpr bool contains( const char_type *value, usize value_size, const char_type *needle,
                             usize needle_size )
    {
        return find( value, value_size, needle, needle_size ) != npos;
    }

    template< typename char_type >
    constexpr usize rfind( const char_type *value, usize value_size, char_type needle, usize offset = npos )
    {
        if ( !value || !value_size )
        {
            return npos;
        }

        usize index{ offset >= value_size ? value_size - 1 : offset };

        while ( true )
        {
            if ( value[ index ] == needle )
            {
                return index;
            }

            if ( !index )
            {
                return npos;
            }

            --index;
        }
    }

    template< typename char_type >
    constexpr usize rfind( const char_type *value, usize value_size, const char_type *needle, usize needle_size,
                           usize offset = npos )
    {
        if ( !needle_size )
        {
            return offset < value_size ? offset : value_size;
        }

        if ( !value || !needle || needle_size > value_size )
        {
            return npos;
        }

        usize last_valid{ value_size - needle_size };
        usize index{ offset < last_valid ? offset : last_valid };

        while ( true )
        {
            if ( equals( value + index, needle_size, needle, needle_size ) )
            {
                return index;
            }

            if ( !index )
            {
                return npos;
            }

            --index;
        }
    }

    template< typename char_type >
    constexpr bool byte_in_set( char_type value, const char_type *set, usize set_size )
    {
        for ( usize index{ }; index < set_size; ++index )
        {
            if ( set[ index ] == value )
            {
                return true;
            }
        }

        return false;
    }

    template< typename char_type >
    constexpr usize find_first_of( const char_type *value, usize value_size, const char_type *set,
                                   usize set_size, usize offset = 0 )
    {
        if ( !value || !set || !set_size || offset >= value_size )
        {
            return npos;
        }

        for ( usize index{ offset }; index < value_size; ++index )
        {
            if ( byte_in_set( value[ index ], set, set_size ) )
            {
                return index;
            }
        }

        return npos;
    }

    template< typename char_type >
    constexpr usize find_last_of( const char_type *value, usize value_size, const char_type *set,
                                  usize set_size, usize offset = npos )
    {
        if ( !value || !value_size || !set || !set_size )
        {
            return npos;
        }

        usize index{ offset >= value_size ? value_size - 1 : offset };

        while ( true )
        {
            if ( byte_in_set( value[ index ], set, set_size ) )
            {
                return index;
            }

            if ( !index )
            {
                return npos;
            }

            --index;
        }
    }

    template< typename char_type >
    constexpr usize find_first_not_of( const char_type *value, usize value_size, const char_type *set,
                                       usize set_size, usize offset = 0 )
    {
        if ( !value || offset >= value_size )
        {
            return npos;
        }

        for ( usize index{ offset }; index < value_size; ++index )
        {
            if ( !byte_in_set( value[ index ], set, set_size ) )
            {
                return index;
            }
        }

        return npos;
    }

    template< typename char_type >
    constexpr usize find_last_not_of( const char_type *value, usize value_size, const char_type *set,
                                      usize set_size, usize offset = npos )
    {
        if ( !value || !value_size )
        {
            return npos;
        }

        usize index{ offset >= value_size ? value_size - 1 : offset };

        while ( true )
        {
            if ( !byte_in_set( value[ index ], set, set_size ) )
            {
                return index;
            }

            if ( !index )
            {
                return npos;
            }

            --index;
        }
    }

    template< typename char_type >
    bool copy( char_type *destination, usize capacity, const char_type *source, usize source_size )
    {
        if ( !destination || !capacity )
        {
            return false;
        }

        if ( !source )
        {
            destination[ 0 ] = 0;
            return source_size == 0;
        }

        usize write_size{ source_size < capacity ? source_size : capacity - 1 };

        for ( usize index{ }; index < write_size; ++index )
        {
            destination[ index ] = source[ index ];
        }

        destination[ write_size ] = 0;
        return source_size < capacity;
    }

    template< typename char_type >
    bool copy( char_type *destination, usize capacity, const char_type *source )
    {
        return copy( destination, capacity, source, length( source ) );
    }

    template< typename char_type >
    bool append( char_type *destination, usize capacity, const char_type *source, usize source_size )
    {
        if ( !destination || !capacity )
        {
            return false;
        }

        usize current_size{ bounded_length( destination, capacity ) };

        if ( current_size >= capacity )
        {
            destination[ capacity - 1 ] = 0;
            return false;
        }

        return copy( destination + current_size, capacity - current_size, source, source_size );
    }

    template< typename char_type >
    bool append( char_type *destination, usize capacity, const char_type *source )
    {
        return append( destination, capacity, source, length( source ) );
    }
} // namespace str

template< typename char_type >
class basic_string_view
{
public:
    constexpr basic_string_view( ) = default;

    constexpr basic_string_view( const char_type *value ) :
        m_data{ value },
        m_size{ str::length( value ) }
    {
    }

    constexpr basic_string_view( const char_type *value, usize size ) :
        m_data{ value },
        m_size{ size }
    {
    }

    constexpr const char_type *data( ) const
    {
        return this->m_data;
    }

    constexpr usize size( ) const
    {
        return this->m_size;
    }

    constexpr bool empty( ) const
    {
        return this->m_size == 0;
    }

    constexpr const char_type *begin( ) const
    {
        return this->m_data;
    }

    constexpr const char_type *end( ) const
    {
        return this->m_data ? this->m_data + this->m_size : nullptr;
    }

    constexpr const char_type &operator[]( usize index ) const
    {
        return this->m_data[ index ];
    }

    constexpr const char_type *at( usize index ) const
    {
        return index < this->m_size ? &this->m_data[ index ] : nullptr;
    }

    constexpr const char_type &front( ) const
    {
        return this->m_data[ 0 ];
    }

    constexpr const char_type &back( ) const
    {
        return this->m_data[ this->m_size - 1 ];
    }

    constexpr basic_string_view substr( usize offset, usize count = str::npos ) const
    {
        if ( offset >= this->m_size )
        {
            return basic_string_view{ };
        }

        usize available{ this->m_size - offset };
        usize length{ count < available ? count : available };
        return basic_string_view{ this->m_data + offset, length };
    }

    constexpr void remove_prefix( usize count )
    {
        if ( count >= this->m_size )
        {
            this->m_data = this->m_data ? this->m_data + this->m_size : nullptr;
            this->m_size = 0;
            return;
        }

        this->m_data += count;
        this->m_size -= count;
    }

    constexpr void remove_suffix( usize count )
    {
        if ( count >= this->m_size )
        {
            this->m_size = 0;
            return;
        }

        this->m_size -= count;
    }

    constexpr bool operator==( basic_string_view other ) const
    {
        return this->equals( other );
    }

    constexpr bool operator!=( basic_string_view other ) const
    {
        return !( *this == other );
    }

    constexpr bool operator<( basic_string_view other ) const
    {
        return this->compare( other ) < 0;
    }

    constexpr bool operator<=( basic_string_view other ) const
    {
        return this->compare( other ) <= 0;
    }

    constexpr bool operator>( basic_string_view other ) const
    {
        return this->compare( other ) > 0;
    }

    constexpr bool operator>=( basic_string_view other ) const
    {
        return this->compare( other ) >= 0;
    }

    constexpr i32 compare( basic_string_view other ) const
    {
        return str::compare( this->m_data, this->m_size, other.m_data, other.m_size );
    }

    constexpr i32 compare_ignore_case( basic_string_view other ) const
    {
        return str::compare_ignore_case( this->m_data, this->m_size, other.m_data, other.m_size );
    }

    constexpr bool equals( basic_string_view other ) const
    {
        return this->compare( other ) == 0;
    }

    constexpr bool equals_ignore_case( basic_string_view other ) const
    {
        return this->compare_ignore_case( other ) == 0;
    }

    constexpr bool starts_with( basic_string_view prefix ) const
    {
        return str::starts_with( this->m_data, this->m_size, prefix.m_data, prefix.m_size );
    }

    constexpr bool ends_with( basic_string_view suffix ) const
    {
        return str::ends_with( this->m_data, this->m_size, suffix.m_data, suffix.m_size );
    }

    constexpr usize find( char_type needle, usize offset = 0 ) const
    {
        return str::find( this->m_data, this->m_size, needle, offset );
    }

    constexpr usize find( basic_string_view needle, usize offset = 0 ) const
    {
        return str::find( this->m_data, this->m_size, needle.m_data, needle.m_size, offset );
    }

    constexpr usize rfind( char_type needle, usize offset = str::npos ) const
    {
        return str::rfind( this->m_data, this->m_size, needle, offset );
    }

    constexpr usize rfind( basic_string_view needle, usize offset = str::npos ) const
    {
        return str::rfind( this->m_data, this->m_size, needle.m_data, needle.m_size, offset );
    }

    constexpr usize find_first_of( basic_string_view set, usize offset = 0 ) const
    {
        return str::find_first_of( this->m_data, this->m_size, set.m_data, set.m_size, offset );
    }

    constexpr usize find_first_of( char_type needle, usize offset = 0 ) const
    {
        return str::find( this->m_data, this->m_size, needle, offset );
    }

    constexpr usize find_last_of( basic_string_view set, usize offset = str::npos ) const
    {
        return str::find_last_of( this->m_data, this->m_size, set.m_data, set.m_size, offset );
    }

    constexpr usize find_last_of( char_type needle, usize offset = str::npos ) const
    {
        return str::rfind( this->m_data, this->m_size, needle, offset );
    }

    constexpr usize find_first_not_of( basic_string_view set, usize offset = 0 ) const
    {
        return str::find_first_not_of( this->m_data, this->m_size, set.m_data, set.m_size, offset );
    }

    constexpr usize find_last_not_of( basic_string_view set, usize offset = str::npos ) const
    {
        return str::find_last_not_of( this->m_data, this->m_size, set.m_data, set.m_size, offset );
    }

    constexpr bool contains( basic_string_view needle ) const
    {
        return this->find( needle ) != str::npos;
    }

    constexpr bool contains( char_type needle ) const
    {
        return this->find( needle ) != str::npos;
    }

private:
    const char_type *m_data{ };
    usize            m_size{ };
};

using string_view  = basic_string_view< char >;
using wstring_view = basic_string_view< wchar_t >;

template< typename char_type, usize capacity_value >
class basic_fixed_string
{
public:
    constexpr basic_fixed_string( ) = default;

    bool assign( basic_string_view< char_type > value )
    {
        if ( value.size( ) && !value.data( ) )
        {
            return false;
        }

        if ( value.size( ) > capacity_value )
        {
            return false;
        }

        for ( usize index{ }; index < value.size( ); ++index )
        {
            this->m_data[ index ] = value[ index ];
        }

        this->m_size                 = value.size( );
        this->m_data[ this->m_size ] = 0;
        return true;
    }

    bool append( basic_string_view< char_type > value )
    {
        if ( value.size( ) && !value.data( ) )
        {
            return false;
        }

        if ( value.size( ) > capacity_value - this->m_size )
        {
            return false;
        }

        for ( usize index{ }; index < value.size( ); ++index )
        {
            this->m_data[ this->m_size + index ] = value[ index ];
        }

        this->m_size += value.size( );
        this->m_data[ this->m_size ] = 0;
        return true;
    }

    bool push_back( char_type value )
    {
        if ( this->m_size >= capacity_value )
        {
            return false;
        }

        this->m_data[ this->m_size ] = value;
        ++this->m_size;
        this->m_data[ this->m_size ] = 0;
        return true;
    }

    bool pop_back( )
    {
        if ( !this->m_size )
        {
            return false;
        }

        --this->m_size;
        this->m_data[ this->m_size ] = 0;
        return true;
    }

    bool resize( usize size )
    {
        return this->resize( size, 0 );
    }

    bool resize( usize size, char_type value )
    {
        if ( size > capacity_value )
        {
            return false;
        }

        while ( this->m_size < size )
        {
            this->m_data[ this->m_size ] = value;
            ++this->m_size;
        }

        this->m_size                 = size;
        this->m_data[ this->m_size ] = 0;
        return true;
    }

    bool truncate( usize size )
    {
        if ( size >= this->m_size )
        {
            return true;
        }

        this->m_size                 = size;
        this->m_data[ this->m_size ] = 0;
        return true;
    }

    bool erase( usize index )
    {
        return this->erase( index, 1 );
    }

    bool erase( usize index, usize count )
    {
        if ( index >= this->m_size )
        {
            return false;
        }

        if ( !count )
        {
            return true;
        }

        if ( count > this->m_size - index )
        {
            count = this->m_size - index;
        }

        for ( usize current{ index }; current + count < this->m_size; ++current )
        {
            this->m_data[ current ] = this->m_data[ current + count ];
        }

        this->m_size -= count;
        this->m_data[ this->m_size ] = 0;
        return true;
    }

    void clear( )
    {
        this->m_size      = 0;
        this->m_data[ 0 ] = 0;
    }

    constexpr char_type *data( )
    {
        return this->m_data;
    }

    constexpr const char_type *data( ) const
    {
        return this->m_data;
    }

    constexpr const char_type *c_str( ) const
    {
        return this->m_data;
    }

    constexpr usize size( ) const
    {
        return this->m_size;
    }

    constexpr usize capacity( ) const
    {
        return capacity_value;
    }

    constexpr bool empty( ) const
    {
        return this->m_size == 0;
    }

    constexpr char_type &operator[]( usize index )
    {
        return this->m_data[ index ];
    }

    constexpr const char_type &operator[]( usize index ) const
    {
        return this->m_data[ index ];
    }

    constexpr basic_string_view< char_type > view( ) const
    {
        return basic_string_view< char_type >{ this->m_data, this->m_size };
    }

    constexpr operator basic_string_view< char_type >( ) const
    {
        return this->view( );
    }

private:
    char_type m_data[ capacity_value + 1 ]{ };
    usize     m_size{ };
};

template< usize capacity_value >
using fixed_string = basic_fixed_string< char, capacity_value >; // @ avoids heap alloc

template< usize capacity_value >
using wfixed_string = basic_fixed_string< wchar_t, capacity_value >;

namespace atl
{
    template< typename key_type >
    struct hasher;

    template< usize capacity_value >
    using string = ::basic_fixed_string< char, capacity_value >;

    template< usize capacity_value >
    using wstring = ::basic_fixed_string< wchar_t, capacity_value >;

    using string_view  = ::string_view;
    using wstring_view = ::wstring_view;

    inline void format( // va_start pins the frame; this cannot be inlined
        char       *buf,
        usize       size,
        const char *fmt, ... )
    {
        va_list args{ };
        va_start( args, fmt );
        I( vsnprintf )( buf, size, fmt, args ); // NOTE: Hoku; I might be the smartest girl alive.
        va_end( args );

        buf[ size - 1 ] = '\0';
    }

    inline void format_append(
        char       *buf,
        usize       size,
        const char *fmt, ... )
    {
        if ( !buf || !size || !fmt )
        {
            return;
        }

        const usize length{ str::strlen( buf ) };
        if ( length >= size - 1 )
        {
            buf[ size - 1 ] = '\0';
            return;
        }

        va_list args{ };
        va_start( args, fmt );
        I( vsnprintf )( buf + length, size - length, fmt, args );
        va_end( args );

        buf[ size - 1 ] = '\0';
    }

    inline i32 to_ansi(
        const wchar_t *uni,
        char          *ansi,
        usize          size )
    {
        i32 result{ I( WideCharToMultiByte )( CP_UTF8, 0, uni, -1, ansi, size, nullptr, nullptr ) };
        ansi[ size - 1 ] = '\0';
        return result;
    }
} // namespace atl

template< typename char_type >
struct atl::hasher< basic_string_view< char_type > >
{
    constexpr u64 operator( )( basic_string_view< char_type > value ) const
    {
        constexpr u64 fnv_basis{ 0xcbf29ce484222325ull };
        constexpr u64 fnv_prime{ 0x100000001b3ull };

        u64 hash{ fnv_basis };

        for ( usize index{ }; index < value.size( ); ++index )
        {
            using unsigned_char = unsigned char;

            if constexpr ( sizeof( char_type ) == 1 )
            {
                hash ^= static_cast< u64 >( static_cast< unsigned_char >( value[ index ] ) );
                hash *= fnv_prime;
            } else
            {
                auto raw{ static_cast< unsigned long long >( value[ index ] ) };

                for ( usize byte{ }; byte < sizeof( char_type ); ++byte )
                {
                    hash ^= static_cast< u64 >( ( raw >> ( byte * 8u ) ) & 0xffull );
                    hash *= fnv_prime;
                }
            }
        }

        return hash;
    }
};
