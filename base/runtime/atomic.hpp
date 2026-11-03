#pragma once

#include "util/typedefs.hpp"

namespace atl
{
    template< typename type >
    class atomic
    {
        static_assert( __atomic_always_lock_free( sizeof( type ), nullptr ) );

    public:
        constexpr atomic( ) = default;
        explicit constexpr atomic( type value ) :
            m_value{ value }
        {
        }

        atomic( const atomic & )            = delete;
        atomic &operator=( const atomic & ) = delete;

        ATLAS_FORCE_INLINE type load( ) const
        {
            return __atomic_load_n( &this->m_value, __ATOMIC_SEQ_CST );
        }

        ATLAS_FORCE_INLINE void store( type value )
        {
            __atomic_store_n( &this->m_value, value, __ATOMIC_SEQ_CST );
        }

        ATLAS_FORCE_INLINE type exchange( type value )
        {
            return __atomic_exchange_n( &this->m_value, value, __ATOMIC_SEQ_CST );
        }

        ATLAS_FORCE_INLINE type fetch_add( type value )
        {
            return __atomic_fetch_add( &this->m_value, value, __ATOMIC_SEQ_CST );
        }

        ATLAS_FORCE_INLINE bool compare_exchange( type &expected, type desired )
        {
            return __atomic_compare_exchange_n( &this->m_value, &expected, desired, false,
                                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
        }

        ATLAS_FORCE_INLINE void *wait_address( )
        {
            return &this->m_value;
        }

        ATLAS_FORCE_INLINE operator type( ) const
        {
            return this->load( );
        }

        ATLAS_FORCE_INLINE atomic &operator=( type value )
        {
            this->store( value );
            return *this;
        }

    private:
        type m_value{ };
    };
} // namespace atl
