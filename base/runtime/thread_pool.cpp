#include "thread_pool.hpp"

#include <intrin.h>

#include "assert.hpp"
#include "libs/lazy_importer.hpp"
#include "math/math.hpp"
#include "thread.hpp"
#include "util/address.hpp"

extern "C"
{
    i32 ATLAS_STDCALL  RtlWaitOnAddress( volatile void *address, void *compare, usize size, i64 *timeout );
    void ATLAS_STDCALL RtlWakeAddressAll( void *address );
}

ATLAS_NO_DESTROY atl::thread_pool g_thread_pool{ };

namespace
{
    constexpr i32 SPIN_BEFORE_PARK{ 256 };

    class operation_guard
    {
    public:
        explicit operation_guard( atl::atomic< bool > &busy ) :
            m_busy{ busy }, m_acquired{ !busy.exchange( true ) }
        {
        }

        ~operation_guard( )
        {
            if ( this->m_acquired )
            {
                this->m_busy.store( false );
            }
        }

        operation_guard( const operation_guard & )            = delete;
        operation_guard &operator=( const operation_guard & ) = delete;

        explicit operator bool( ) const
        {
            return this->m_acquired;
        }

    private:
        atl::atomic< bool > &m_busy;
        bool                 m_acquired;
    };
} // namespace

namespace atl
{
    i32 thread_pool::default_workers( )
    {
        SYSTEM_INFO info{ };
        I( GetSystemInfo )( &info );

        return math::clamp( static_cast< i32 >( info.dwNumberOfProcessors ) / 2 - 1, 1, MAX_WORKERS );
    }

    bool thread_pool::startup( i32 workers )
    {
        if ( workers < 1 || workers > MAX_WORKERS )
        {
            return false;
        }

        operation_guard operation{ this->m_busy };
        if ( !operation || this->m_worker_count.load( ) != 0 )
        {
            return false;
        }

        if ( !I( RtlWaitOnAddress ) || !I( RtlWakeAddressAll ) ||
             !I( NtWaitForSingleObject ) || !I( CloseHandle ) || !I( GetLastError ) || !I( Sleep ) )
        {
            return false;
        }

        this->m_last_failure.store( 0 );
        this->m_wait_failed.store( false );
        this->m_stop.store( false );

        for ( i32 index{ }; index < workers; ++index )
        {
            worker_t &worker{ this->m_workers[ index ] };

            worker.m_pool  = this;
            worker.m_index = index + 1; // 0 is the caller
            worker.m_signal.store( 0 );

            if ( !create_hidden_thread( &worker_entry, &worker, &worker.m_handle ) )
            {
                worker.m_pool  = nullptr;
                worker.m_index = 0;
                this->shutdown_locked( );
                return false;
            }

            this->m_worker_count.store( index + 1 );
        }

        return true;
    }

    bool thread_pool::shutdown( )
    {
        operation_guard operation{ this->m_busy };
        return operation && this->shutdown_locked( );
    }

    bool thread_pool::shutdown_locked( )
    {
        this->m_stop.store( true );
        this->wake_workers( this->m_worker_count.load( ) );

        for ( i32 index{ this->m_worker_count.load( ) - 1 }; index >= 0; --index )
        {
            worker_t &worker{ this->m_workers[ index ] };

            const i32 status{ I( NtWaitForSingleObject )( worker.m_handle, 0, nullptr ) };
            if ( status != 0 )
            {
                this->record_failure( failure_kind::join, worker.m_index, status );
                return false;
            }

            if ( !I( CloseHandle )( worker.m_handle ) )
            {
                this->record_failure( failure_kind::close, worker.m_index, static_cast< i32 >( I( GetLastError )( ) ) );
                return false;
            }

            worker.m_handle = nullptr;
            worker.m_pool   = nullptr;
            worker.m_index  = 0;
            worker.m_signal.store( 0 );
            this->m_worker_count.store( index );
        }

        return true;
    }

    bool thread_pool::run( job_fn fn, void *context, i32 count )
    {
        if ( count < 0 || ( count > 0 && !fn ) )
        {
            return false;
        }

        operation_guard operation{ this->m_busy };
        if ( !operation || ( this->m_stop.load( ) && this->m_worker_count.load( ) != 0 ) )
        {
            return false;
        }

        if ( count == 0 )
        {
            return true;
        }

        const i32 workers{ this->m_worker_count.load( ) };
        if ( count == 1 || !this->m_enabled.load( ) || workers == 0 )
        {
            for ( i32 index{ }; index < count; ++index )
            {
                fn( context, index, 0 );
            }

            return true;
        }

        this->wait_for_zero( this->m_active );

        Assert( this->m_current.load( ) == nullptr );

        this->m_run.m_fn      = fn;
        this->m_run.m_context = context;
        this->m_run.m_count   = count;
        this->m_run.m_next.store( 0 );
        this->m_run.m_remaining.store( count );

        this->m_current.store( &this->m_run );
        this->wake_workers( count - 1 < workers ? count - 1 : workers );

        this->serve( this->m_run, 0 );
        this->wait_for_zero( this->m_run.m_remaining );

        this->m_current.store( nullptr );
        return true;
    }

    void thread_pool::serve( run_t &run, i32 worker )
    {
        for ( ;; )
        {
            const i64 index{ run.m_next.fetch_add( 1 ) };

            if ( index >= run.m_count )
            {
                return;
            }

            run.m_fn( run.m_context, static_cast< i32 >( index ), worker );
            if ( run.m_remaining.fetch_add( -1 ) == 1 )
            {
                I( RtlWakeAddressAll )( run.m_remaining.wait_address( ) );
            }
        }
    }

    void thread_pool::wake_workers( i32 count )
    {
        for ( i32 index{ }; index < count; ++index )
        {
            auto &signal{ this->m_workers[ index ].m_signal };
            signal.fetch_add( 1 );
            I( RtlWakeAddressAll )( signal.wait_address( ) );
        }
    }

    void thread_pool::worker_loop( worker_t &worker )
    {
        u32 seen{ };

        for ( ;; )
        {
            i32 spins{ };

            while ( worker.m_signal.load( ) == seen && !this->m_stop.load( ) )
            {
                if ( spins < SPIN_BEFORE_PARK )
                {
                    ++spins;
                    _mm_pause( );
                } else
                {
                    this->park( worker.m_signal.wait_address( ), &seen, sizeof( seen ), worker.m_index );
                }
            }

            if ( this->m_stop.load( ) )
            {
                return;
            }

            seen = worker.m_signal.load( );

            this->m_active.fetch_add( 1 );

            if ( run_t * current{ this->m_current.load( ) } )
            {
                this->serve( *current, worker.m_index );
            }

            if ( this->m_active.fetch_add( -1 ) == 1 )
            {
                I( RtlWakeAddressAll )( this->m_active.wait_address( ) );
            }
        }
    }

    void thread_pool::wait_for_zero( atomic< i32 > &value )
    {
        i32 spins{ };
        for ( i32 observed{ value.load( ) }; observed != 0; observed = value.load( ) )
        {
            if ( spins < SPIN_BEFORE_PARK )
            {
                ++spins;
                _mm_pause( );
            } else
            {
                this->park( value.wait_address( ), &observed, sizeof( observed ), 0 );
            }
        }
    }

    void thread_pool::park( void *address, void *expected, usize size, i32 worker )
    {
        if ( this->m_wait_failed.load( ) )
        {
            I( Sleep )( 1 );
            return;
        }

        i64       timeout{ -1000000 };
        const i32 status{ I( RtlWaitOnAddress )( address, expected, size, &timeout ) };
        if ( status != 0 && status != WAIT_TIMEOUT )
        {
            this->record_failure( failure_kind::wait, worker, status );
            this->m_wait_failed.store( true );
            I( Sleep )( 1 );
        }
    }

    void thread_pool::record_failure( failure_kind kind, i32 worker, i32 status )
    {
        this->m_last_failure.store( ( static_cast< u64 >( kind ) << 40 ) |
                                    ( static_cast< u64 >( worker ) << 32 ) |
                                    static_cast< u32 >( status ) );
    }

    void ATLAS_STDCALL thread_pool::worker_entry( void *parameter )
    {
        auto *worker{ static_cast< worker_t * >( parameter ) };

        worker->m_pool->worker_loop( *worker );
    }
} // namespace atl
