#pragma once

#include "libs/lazy_importer.hpp"
#include "util/typedefs.hpp"

namespace atl
{
    using profile_id = u16;

    enum class profile_class : u8
    {
        passive,
        active,
        mixed,
    };

    struct profile_metadata
    {
        const char   *m_name{ };
        profile_class m_class{ profile_class::mixed };
        const char   *m_work{ };
    };

#if defined( ATLAS_DEV )
    struct profiler_config
    {
        const char             *m_name{ "runtime" };
        const char             *m_console_title{ "Atlas Runtime Profiler" };
        const profile_metadata *m_profiles{ };
        usize                   m_profile_count{ };
        u32                     m_flush_poll_ms{ 200 };
        u64                     m_flush_window_us{ 2000000 };
        u64                     m_noise_total_us{ 100 };
        usize                   m_max_printed_rows{ 24 };
    };
#else
    struct profiler_config
    {
        const char             *m_name{ nullptr };
        const char             *m_console_title{ nullptr };
        const profile_metadata *m_profiles{ };
        usize                   m_profile_count{ };
        u32                     m_flush_poll_ms{ 200 };
        u64                     m_flush_window_us{ 2000000 };
        u64                     m_noise_total_us{ 100 };
        usize                   m_max_printed_rows{ 24 };
    };
#endif // ATLAS_DEV

#if defined( ATLAS_DEV )
    namespace profile_detail
    {
        using qpc_fn = decltype( &::QueryPerformanceCounter );

        extern u8     active;
        extern qpc_fn qpc;

        void record( profile_id id, u64 ticks, u64 work );

        ATLAS_FORCE_INLINE i64 timestamp( )
        {
            LARGE_INTEGER value{ };
            return qpc && qpc( &value ) ? value.QuadPart : 0;
        }
    } // namespace profile_detail

    class profile_scope
    {
    public:
        ATLAS_FORCE_INLINE explicit profile_scope( profile_id id ) :
            m_id{ id }
        {
            if ( __atomic_load_n( &profile_detail::active, __ATOMIC_ACQUIRE ) )
            {
                this->m_started = profile_detail::timestamp( );
            }
        }

        template< typename id_t >
        ATLAS_FORCE_INLINE explicit profile_scope( id_t id ) :
            profile_scope{ static_cast< profile_id >( id ) }
        {
        }

        ATLAS_FORCE_INLINE ~profile_scope( )
        {
            if ( this->m_started <= 0 )
            {
                return;
            }

            const i64 ended{ profile_detail::timestamp( ) };
            if ( ended > this->m_started )
            {
                profile_detail::record( this->m_id, static_cast< u64 >( ended - this->m_started ), this->m_work );
            }
        }

        ATLAS_FORCE_INLINE void add_work( u64 work )
        {
            this->m_work += work;
        }

        profile_scope( const profile_scope & )            = delete;
        profile_scope &operator=( const profile_scope & ) = delete;

    private:
        profile_id m_id{ };
        i64        m_started{ };
        u64        m_work{ };
    };

    bool profiler_startup( const profiler_config &config );
    void profiler_shutdown( );
#else
    class profile_scope
    {
    public:
        ATLAS_FORCE_INLINE explicit profile_scope( profile_id )
        {
        }

        template< typename id_t >
        ATLAS_FORCE_INLINE explicit profile_scope( id_t )
        {
        }

        ATLAS_FORCE_INLINE void add_work( u64 )
        {
        }
    };

    inline bool profiler_startup( const profiler_config & )
    {
        return true;
    }

    inline void profiler_shutdown( )
    {
    }
#endif
} // namespace atl

#define ATLAS_PROFILE_JOIN_INNER( lhs, rhs ) lhs##rhs
#define ATLAS_PROFILE_JOIN( lhs, rhs )       ATLAS_PROFILE_JOIN_INNER( lhs, rhs )

#if defined( ATLAS_DEV )
#define ATLAS_PROFILE( id )                                                      \
    ::atl::profile_scope ATLAS_PROFILE_JOIN( atlas_profile_scope_, __COUNTER__ ) \
    {                                                                            \
        id                                                                       \
    }
#define ATLAS_PROFILE_NAMED( name, id ) \
    ::atl::profile_scope name           \
    {                                   \
        id                              \
    }
#define ATLAS_PROFILE_ADD_WORK( name, units ) name.add_work( static_cast< u64 >( units ) )
#else
#define ATLAS_PROFILE( id )                   ( ( void )0 )
#define ATLAS_PROFILE_NAMED( name, id )       ( ( void )0 )
#define ATLAS_PROFILE_ADD_WORK( name, units ) ( ( void )0 )
#endif
