#pragma once

#include "runtime/vector.hpp"

#include "string.hpp"
#include "typedefs.hpp"

namespace json
{
    inline constexpr usize MAX_DEPTH{ 32 };
    inline constexpr u32   INVALID_NODE{ static_cast< u32 >( -1 ) };

    enum class type_t : u8
    {
        null,
        boolean,
        number,
        string,
        array,
        object,
    };

    enum class error_t : u8
    {
        none,
        no_input,
        unexpected_token,
        bad_literal,
        bad_number,
        bad_string,
        bad_escape,
        unterminated,
        trailing_data,
        depth_exceeded,
        allocation_failed,
    };

    namespace detail
    {
        struct parser;

        template< typename left, typename right >
        inline constexpr bool is_same{ false };

        template< typename type >
        inline constexpr bool is_same< type, type >{ true };

        // @ long and unsigned long are distinct from i32 and u32 under the MSVC ABI, so a DWORD
        // sized member needs its own entry here
        template< typename type >
        inline constexpr bool is_integer{
            is_same< type, i8 > || is_same< type, i16 > || is_same< type, i32 > || is_same< type, i64 > ||
            is_same< type, u8 > || is_same< type, u16 > || is_same< type, u32 > || is_same< type, u64 > ||
            is_same< type, long > || is_same< type, unsigned long >
        };

        template< typename type >
        inline constexpr bool is_real{ is_same< type, f32 > || is_same< type, f64 > };

        template< typename type >
        inline constexpr bool is_number{ is_same< type, bool > || is_integer< type > || is_real< type > };

        template< typename type >
        constexpr type clamp_integer( i64 value )
        {
            constexpr bool signed_target{ static_cast< type >( -1 ) < static_cast< type >( 0 ) };
            constexpr i64  bits{ static_cast< i64 >( sizeof( type ) ) * 8 };

            if constexpr ( signed_target )
            {
                if constexpr ( bits < 64 )
                {
                    constexpr i64 high{ ( static_cast< i64 >( 1 ) << ( bits - 1 ) ) - 1 };
                    constexpr i64 low{ -high - 1 };

                    value = value > high ? high : value;
                    value = value < low ? low : value;
                }
            } else
            {
                value = value < 0 ? 0 : value;

                if constexpr ( bits < 64 )
                {
                    constexpr i64 high{ ( static_cast< i64 >( 1 ) << bits ) - 1 };
                    value = value > high ? high : value;
                }
            }

            return static_cast< type >( value );
        }

        struct span_t
        {
            u32 m_offset{ };
            u32 m_count{ };
        };

        union payload_t
        {
            bool   m_boolean;
            i64    m_integer;
            f64    m_real;
            span_t m_span;
        };

        struct node_t
        {
            type_t    m_type{ type_t::null };
            bool      m_integral{ };
            u32       m_next{ INVALID_NODE };
            span_t    m_key{ };
            payload_t m_payload{ };
        };
    } // namespace detail

    class document;
    class value
    {
    public:
        value( ) = default;

        type_t type( ) const;
        bool   valid( ) const;
        usize  size( ) const;

        value operator[]( string_view name ) const;
        value operator[]( usize index ) const;

        template< typename value_type >
        value_type as( value_type fallback = { } ) const
        {
            if constexpr ( detail::is_same< value_type, bool > )
            {
                bool result{ };
                return this->boolean( result ) ? result : fallback;
            } else if constexpr ( detail::is_integer< value_type > )
            {
                i64 result{ };
                return this->number( result ) ? detail::clamp_integer< value_type >( result ) : fallback;
            } else if constexpr ( detail::is_real< value_type > )
            {
                f64 result{ };

                if ( !this->number( result ) )
                {
                    return fallback;
                }

                if constexpr ( detail::is_same< value_type, f32 > )
                {
                    result = result > FLT_MAX ? FLT_MAX : result;
                    result = result < -FLT_MAX ? -FLT_MAX : result;
                }

                return static_cast< value_type >( result );
            } else if constexpr ( detail::is_same< value_type, string_view > )
            {
                string_view result{ };
                return this->text( result ) ? result : fallback;
            } else
            {
                static_assert( detail::is_number< value_type >, "json::value reads bools, integers, floats and string_view" );
                return value_type( );
            }
        }

    private:
        friend class document;

        value( const document *owner, u32 index );

        bool boolean( bool &out ) const;
        bool number( i64 &out ) const;
        bool number( f64 &out ) const;
        bool text( string_view &out ) const;

        const detail::node_t *node( ) const;

        const document *m_document{ };
        u32             m_index{ INVALID_NODE };
    };

    class document
    {
    public:
        ~document( );
        bool    parse( string_view text, bool sensitive = false );
        value   root( ) const;
        error_t error( ) const;
        usize   error_offset( ) const;

    private:
        friend class value;
        friend struct detail::parser;

        void reset( );

        const detail::node_t *node( u32 index ) const;
        string_view           pooled( detail::span_t span ) const;

        atl::vector< detail::node_t > m_nodes{ };
        atl::vector< char >           m_pool{ };
        error_t                       m_error{ error_t::none };
        usize                         m_error_offset{ };
        bool                          m_sensitive{ };
    };

    class writer
    {
    public:
        void begin_object( );
        void end_object( );
        void begin_array( );
        void end_array( );

        void key( string_view name );

        template< typename value_type >
            requires detail::is_number< value_type >
        void write( value_type value )
        {
            if constexpr ( detail::is_same< value_type, bool > )
            {
                this->write_boolean( value );
            } else if constexpr ( detail::is_real< value_type > )
            {
                this->write_real( static_cast< f64 >( value ), detail::is_same< value_type, f32 > );
            } else if constexpr ( static_cast< value_type >( -1 ) < static_cast< value_type >( 0 ) )
            {
                this->write_integer( static_cast< i64 >( value ) );
            } else
            {
                this->write_unsigned( static_cast< u64 >( value ) );
            }
        }

        void write( string_view text );

        template< typename value_type >
        void member( string_view name, const value_type &value )
        {
            this->key( name );
            this->write( value );
        }

        bool        ok( ) const;
        string_view text( ) const;

    private:
        void open( char bracket );
        void close( char bracket );
        void prefix( );
        void indent( usize depth );
        void quoted( string_view text );

        void write_boolean( bool value );
        void write_integer( i64 value );
        void write_unsigned( u64 value );
        void write_real( f64 value, bool single );

        void emit_integer( i64 value );
        void emit_unsigned( u64 value );
        void push( char value );
        void push( string_view text );

        atl::vector< char > m_buffer{ };
        usize               m_depth{ };
        u64                 m_populated{ };
        bool                m_after_key{ };
        bool                m_failed{ };
    };
} // namespace json
