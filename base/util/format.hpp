#pragma once

#include "string.hpp"

namespace str
{
    namespace detail
    {
        struct format_spec
        {
            bool valid{ true };
            bool hex{ };
            bool upper{ };
            bool has_precision{ };
            u8   precision{ };
        };

        struct format_writer
        {
            char *data{ };
            usize limit{ };
            usize size{ };
            bool  truncated{ };

            void push( char value )
            {
                if ( this->size >= this->limit )
                {
                    this->truncated = true;
                    return;
                }

                this->data[ this->size ] = value;
                ++this->size;
            }

            void push( string_view value )
            {
                for ( usize index{ }; index < value.size( ); ++index )
                {
                    this->push( value[ index ] );
                }
            }
        };

        inline format_spec parse_format_spec( string_view body )
        {
            format_spec spec{ };

            if ( body.empty( ) )
            {
                return spec;
            }

            if ( body[ 0 ] != ':' || body.size( ) < 2 )
            {
                spec.valid = false;
                return spec;
            }

            string_view options{ body.substr( 1 ) };

            if ( options.size( ) == 1 && ( options[ 0 ] == 'x' || options[ 0 ] == 'X' ) )
            {
                spec.hex   = true;
                spec.upper = options[ 0 ] == 'X';
                return spec;
            }

            if ( options.size( ) == 3 && options[ 0 ] == '.' && is_digit( options[ 1 ] ) && options[ 2 ] == 'f' )
            {
                u8 precision{ static_cast< u8 >( options[ 1 ] - '0' ) };

                if ( precision <= 6 )
                {
                    spec.has_precision = true;
                    spec.precision     = precision;
                    return spec;
                }
            }

            spec.valid = false;
            return spec;
        }

        inline void write_unsigned( format_writer &writer, u64 value, const format_spec &spec )
        {
            const char *digits{ spec.upper ? "0123456789ABCDEF" : "0123456789abcdef" };
            u64         base{ spec.hex ? 16ull : 10ull };

            char  buffer[ 20 ]{ };
            usize index{ sizeof( buffer ) };

            do
            {
                --index;
                buffer[ index ] = digits[ value % base ];
                value /= base;
            } while ( value );

            writer.push( string_view{ buffer + index, sizeof( buffer ) - index } );
        }

        inline void write_signed( format_writer &writer, i64 value, const format_spec &spec )
        {
            u64 magnitude{ static_cast< u64 >( value ) };

            if ( value < 0 )
            {
                writer.push( '-' );
                magnitude = 0 - magnitude;
            }

            write_unsigned( writer, magnitude, spec );
        }

        inline void write_double( format_writer &writer, double value, const format_spec &spec )
        {
            if ( value != value )
            {
                writer.push( string_view{ "nan" } );
                return;
            }

            if ( value < 0.0 )
            {
                writer.push( '-' );
                value = -value;
            }

            // strictly above the largest finite double is infinity
            if ( value > 1.7976931348623157e308 )
            {
                writer.push( string_view{ "inf" } );
                return;
            }

            u8 precision{ spec.has_precision ? spec.precision : u8{ 2 } };

            u64 scale{ 1 };

            for ( u8 index{ }; index < precision; ++index )
            {
                scale *= 10;
            }

            constexpr double integer_limit{ 18446744073709551616.0 }; // 2^64
            constexpr u64    integer_max{ 0xffffffffffffffffull };

            u64 integer_part{ integer_max };
            u64 fraction_part{ };

            if ( value < integer_limit )
            {
                integer_part = static_cast< u64 >( value );

                double fraction{ value - static_cast< double >( integer_part ) };

                if ( !( fraction >= 0.0 && fraction < 1.0 ) )
                {
                    fraction = 0.0;
                }

                // the bias absorbs decimal literals stored just under the half step
                fraction_part = static_cast< u64 >( fraction * static_cast< double >( scale ) + 0.5 + 1e-9 );

                if ( fraction_part >= scale )
                {
                    fraction_part -= scale;

                    if ( integer_part < integer_max )
                    {
                        ++integer_part;
                    }
                }
            }

            format_spec decimal_spec{ };
            write_unsigned( writer, integer_part, decimal_spec );

            if ( !precision )
            {
                return;
            }

            writer.push( '.' );

            char buffer[ 6 ]{ };

            for ( usize index{ precision }; index; )
            {
                --index;
                buffer[ index ] = static_cast< char >( '0' + fraction_part % 10 );
                fraction_part /= 10;
            }

            writer.push( string_view{ buffer, precision } );
        }

        inline void format_value( format_writer &writer, bool value, const format_spec & )
        {
            writer.push( value ? string_view{ "true" } : string_view{ "false" } );
        }

        inline void format_value( format_writer &writer, char value, const format_spec & )
        {
            writer.push( value );
        }

        inline void format_value( format_writer &writer, i8 value, const format_spec &spec )
        {
            write_signed( writer, value, spec );
        }

        inline void format_value( format_writer &writer, i16 value, const format_spec &spec )
        {
            write_signed( writer, value, spec );
        }

        inline void format_value( format_writer &writer, i32 value, const format_spec &spec )
        {
            write_signed( writer, value, spec );
        }

        inline void format_value( format_writer &writer, i64 value, const format_spec &spec )
        {
            write_signed( writer, value, spec );
        }

        inline void format_value( format_writer &writer, long value, const format_spec &spec )
        {
            write_signed( writer, value, spec );
        }

        inline void format_value( format_writer &writer, u8 value, const format_spec &spec )
        {
            write_unsigned( writer, value, spec );
        }

        inline void format_value( format_writer &writer, u16 value, const format_spec &spec )
        {
            write_unsigned( writer, value, spec );
        }

        inline void format_value( format_writer &writer, u32 value, const format_spec &spec )
        {
            write_unsigned( writer, value, spec );
        }

        inline void format_value( format_writer &writer, u64 value, const format_spec &spec )
        {
            write_unsigned( writer, value, spec );
        }

        inline void format_value( format_writer &writer, unsigned long value, const format_spec &spec )
        {
            write_unsigned( writer, value, spec );
        }

        inline void format_value( format_writer &writer, float value, const format_spec &spec )
        {
            write_double( writer, value, spec );
        }

        inline void format_value( format_writer &writer, double value, const format_spec &spec )
        {
            write_double( writer, value, spec );
        }

        inline void format_value( format_writer &writer, const char *value, const format_spec & )
        {
            if ( !value )
            {
                return;
            }

            writer.push( string_view{ value } );
        }

        inline void format_value( format_writer &writer, string_view value, const format_spec & )
        {
            writer.push( value );
        }

        struct format_argument
        {
            const void *value{ };
            void ( *write )( format_writer &, const void *, const format_spec & ){ };
        };

        template< typename value_type >
        void write_erased( format_writer &writer, const void *value, const format_spec &spec )
        {
            format_value( writer, *static_cast< const value_type * >( value ), spec );
        }

        template< typename value_type >
        format_argument make_format_argument( const value_type &value )
        {
            return format_argument{ &value, &write_erased< value_type > };
        }

        inline usize format_run( char *out, usize capacity, string_view fmt, const format_argument *arguments,
                                 usize argument_count, bool &truncated )
        {
            truncated = false;

            if ( !out || !capacity )
            {
                return 0;
            }

            format_writer writer{ out, capacity - 1 };
            usize         argument_index{ };

            for ( usize index{ }; index < fmt.size( ); ++index )
            {
                char value{ fmt[ index ] };

                if ( value == '{' )
                {
                    if ( index + 1 < fmt.size( ) && fmt[ index + 1 ] == '{' )
                    {
                        writer.push( '{' );
                        ++index;
                        continue;
                    }

                    usize close{ fmt.find( '}', index + 1 ) };

                    if ( close == npos )
                    {
                        break;
                    }

                    format_spec spec{ parse_format_spec( fmt.substr( index + 1, close - index - 1 ) ) };

                    if ( spec.valid && argument_index < argument_count )
                    {
                        const format_argument &argument{ arguments[ argument_index ] };
                        argument.write( writer, argument.value, spec );
                    }

                    ++argument_index;
                    index = close;
                    continue;
                }

                if ( value == '}' && index + 1 < fmt.size( ) && fmt[ index + 1 ] == '}' )
                {
                    ++index;
                }

                writer.push( value );
            }

            out[ writer.size ] = 0;
            truncated          = writer.truncated;
            return writer.size;
        }
    } // namespace detail

    template< typename... args_t >
    usize format_to( char *out, usize capacity, string_view fmt, const args_t &...args )
    {
        detail::format_argument arguments[ sizeof...( args_t ) + 1 ]{ detail::make_format_argument( args )... };
        bool                    truncated{ };

        return detail::format_run( out, capacity, fmt, arguments, sizeof...( args_t ), truncated );
    }

    template< usize capacity_value, typename... args_t >
    bool format( basic_fixed_string< char, capacity_value > &out, string_view fmt, const args_t &...args )
    {
        detail::format_argument arguments[ sizeof...( args_t ) + 1 ]{ detail::make_format_argument( args )... };
        bool                    truncated{ };
        usize                   written{ detail::format_run( out.data( ), capacity_value + 1, fmt, arguments,
                                                             sizeof...( args_t ), truncated ) };

        // restore the internal size after writing through data( ); assign copies index to index,
        // so the overlapping source is harmless
        out.assign( string_view{ out.data( ), written } );
        return !truncated;
    }
} // namespace str
