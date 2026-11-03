#pragma once

#include <intrin.h>

#include "util/typedefs.hpp"

namespace atl
{
    class mutex
    {
    public:
        mutex( )                          = default;
        mutex( const mutex & )            = delete;
        mutex &operator=( const mutex & ) = delete;

        void lock( )
        {
            while ( !this->try_lock( ) )
            {
                _mm_pause( );
            }
        }

        bool try_lock( )
        {
            return _InterlockedCompareExchange( &this->m_state, locked, unlocked ) == unlocked;
        }

        void unlock( )
        {
            _InterlockedExchange( &this->m_state, unlocked );
        }

    private:
        static constexpr long unlocked{ 0 };
        static constexpr long locked{ 1 };

        volatile long m_state{ unlocked };
    };

    class scoped_lock
    {
    public:
        explicit scoped_lock( mutex &lock ) :
            m_lock{ &lock }
        {
            this->m_lock->lock( );
        }

        scoped_lock( const scoped_lock & )            = delete;
        scoped_lock &operator=( const scoped_lock & ) = delete;

        ~scoped_lock( )
        {
            this->m_lock->unlock( );
        }

    private:
        mutex *m_lock{ };
    };
} // namespace atl
