#pragma once

namespace detail
{
    template< typename type >
    struct sort_remove_reference
    {
        using value = type;
    };

    template< typename type >
    struct sort_remove_reference< type & >
    {
        using value = type;
    };

    template< typename type >
    struct sort_remove_reference< type && >
    {
        using value = type;
    };

    template< typename type >
    using sort_remove_reference_t = typename sort_remove_reference< type >::value;

    template< typename type >
    constexpr sort_remove_reference_t< type > &&sort_move( type &&value )
    {
        return static_cast< sort_remove_reference_t< type > && >( value );
    }
} // namespace detail

namespace atl
{
    template< typename type, typename compare >
    void sort( type *first, type *last, compare cmp )
    {
        if ( !first || first == last )
        {
            return;
        }

        for ( type *current{ first + 1 }; current < last; ++current )
        {
            type value{ detail::sort_move( *current ) };
            type *position{ current };

            while ( position > first && cmp( value, *( position - 1 ) ) )
            {
                *position = detail::sort_move( *( position - 1 ) );
                --position;
            }

            *position = detail::sort_move( value );
        }
    }

    template< typename container, typename compare >
    void sort( container &c, compare cmp )
    {
        sort( c.data( ), c.data( ) + c.size( ), cmp );
    }
} // namespace atl
