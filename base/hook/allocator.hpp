#pragma once

#include "utility.hpp"

namespace atlashook
{
    class allocation
    {
    public:
        allocation( )                               = default;
        allocation( const allocation & )            = delete;
        allocation &operator=( const allocation & ) = delete;
        allocation( allocation &&other );
        allocation &operator=( allocation &&other );
        ~allocation( ) = default;

        bool create( usize size );
        void reset( );

        ATLAS_FORCE_INLINE u8 *data( ) const
        {
            return this->m_data;
        }

        ATLAS_FORCE_INLINE usize size( ) const
        {
            return this->m_size;
        }

        explicit ATLAS_FORCE_INLINE operator bool( ) const
        {
            return this->m_data && this->m_size;
        }

    private:
        u8   *m_data{ };
        usize m_size{ };
    };
} // namespace atlashook
