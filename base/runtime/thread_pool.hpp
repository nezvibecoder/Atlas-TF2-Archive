#pragma once

#include "atomic.hpp"
#include "util/typedefs.hpp"

namespace atl
{
    using job_fn = void ( * )( void *context, i32 index, i32 worker );

    class thread_pool
    {
    public:
        static constexpr i32 MAX_WORKERS{ 4 };

        enum class failure_kind : u8
        {
            none,
            wait,
            join,
            close,
        };

        struct failure_t
        {
            failure_kind m_kind{ };
            i32          m_worker{ };
            i32          m_status{ }; // NTSTATUS
        };

        static i32 default_workers( );

        [[nodiscard]] bool startup( i32 workers );
        [[nodiscard]] bool shutdown( );
        [[nodiscard]] bool run( job_fn fn, void *context, i32 count );

        i32 workers( ) const
        {
            return this->m_worker_count.load( );
        }

        failure_t last_failure( ) const
        {
            const u64 failure{ this->m_last_failure.load( ) };
            return { static_cast< failure_kind >( failure >> 40 ),
                     static_cast< i32 >( ( failure >> 32 ) & 0xFFu ),
                     static_cast< i32 >( static_cast< u32 >( failure ) ) };
        }

        void set_enabled( bool enabled )
        {
            this->m_enabled.store( enabled );
        }

        bool enabled( ) const
        {
            return this->m_enabled.load( );
        }

        // how many jobs it is worth handing to run( ) at once; the caller is one of them
        i32 concurrency( ) const
        {
            return this->m_enabled.load( ) && !this->m_stop.load( ) ? this->m_worker_count.load( ) + 1 : 1;
        }

    private:
        struct run_t
        {
            job_fn        m_fn{ };
            void         *m_context{ };
            i32           m_count{ };
            atomic< i64 > m_next{ }; // workers also reserve one index beyond the end
            atomic< i32 > m_remaining{ };
        };

        struct worker_t
        {
            thread_pool *m_pool{ };
            i32          m_index{ };
            void        *m_handle{ };
            atomic< u32 > m_signal{ };
        };

        static void ATLAS_STDCALL worker_entry( void *parameter );

        void worker_loop( worker_t &worker );
        void serve( run_t &run, i32 worker );
        void wake_workers( i32 count );
        bool shutdown_locked( );
        void wait_for_zero( atomic< i32 > &value );
        void park( void *address, void *expected, usize size, i32 worker );
        void record_failure( failure_kind kind, i32 worker, i32 status );

        run_t             m_run{ };
        atomic< run_t * > m_current{ };
        atomic< i32 >     m_active{ };     // workers between reading m_current and being done with it
        atomic< bool >    m_busy{ };
        atomic< bool >    m_stop{ };
        worker_t          m_workers[ MAX_WORKERS ]{ };
        atomic< i32 >     m_worker_count{ };
        atomic< bool >    m_enabled{ true };
        atomic< bool >    m_wait_failed{ };
        atomic< u64 >     m_last_failure{ };
    };
} // namespace atl

extern atl::thread_pool g_thread_pool;
