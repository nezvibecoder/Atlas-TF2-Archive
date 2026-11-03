#include "profiler.hpp"

#if defined( ATLAS_DEV )

#include "thread.hpp"
#include "util/format.hpp"

namespace
{
    constexpr usize MAX_PROFILE_COUNT{ 256 };
    constexpr usize HISTOGRAM_LIMIT_COUNT{ 14 };
    constexpr usize HISTOGRAM_BUCKET_COUNT{ HISTOGRAM_LIMIT_COUNT + 1 };

    constexpr u64 HISTOGRAM_US[ HISTOGRAM_LIMIT_COUNT ]{
        5, 10, 25, 50, 100, 250, 500, 1000, 2000, 5000, 10000, 25000, 50000, 100000,
    };

    struct alignas( 64 ) profile_stat
    {
        u8  m_lock{ };
        u64 m_calls{ };
        u64 m_total_ticks{ };
        u64 m_max_ticks{ };
        u64 m_over_250us{ };
        u64 m_over_1ms{ };
        u64 m_work{ };
        u64 m_histogram[ HISTOGRAM_BUCKET_COUNT ]{ };
    };

    struct profile_row
    {
        usize m_id{ };
        u64   m_calls{ };
        u64   m_total_ticks{ };
        u64   m_max_ticks{ };
        u64   m_over_250us{ };
        u64   m_over_1ms{ };
        u64   m_work{ };
        u64   m_histogram[ HISTOGRAM_BUCKET_COUNT ]{ };
    };

    atl::profiler_config g_config{ };
    profile_stat         g_stats[ MAX_PROFILE_COUNT ]{ };
    u64                  g_histogram_ticks[ HISTOGRAM_LIMIT_COUNT ]{ };
    u64                  g_frequency{ };
    u64                  g_250us_ticks{ };
    u64                  g_1ms_ticks{ };
    u8                   g_started{ };
    u8                   g_running{ };
    u64                  g_window_serial{ };
    void                *g_thread{ };
    void                *g_console_output{ };
    bool                 g_console_owned{ };

    ATLAS_FORCE_INLINE void lock_stat( profile_stat &stat )
    {
        for ( ;; )
        {
            u8 expected{ };
            if ( __atomic_compare_exchange_n( &stat.m_lock, &expected, u8{ 1 }, true,
                                              __ATOMIC_ACQUIRE, __ATOMIC_RELAXED ) )
            {
                return;
            }

            _mm_pause( );
        }
    }

    ATLAS_FORCE_INLINE void unlock_stat( profile_stat &stat )
    {
        __atomic_store_n( &stat.m_lock, u8{ 0 }, __ATOMIC_RELEASE );
    }

    ATLAS_FORCE_INLINE u64 ticks_for_us( u64 microseconds )
    {
        return ( g_frequency * microseconds + 999999ull ) / 1000000ull;
    }

    ATLAS_FORCE_INLINE char class_letter( atl::profile_class value )
    {
        switch ( value )
        {
            case atl::profile_class::passive: return 'P';
            case atl::profile_class::active: return 'A';
            case atl::profile_class::mixed: return 'M';
        }

        return '?';
    }

    template< typename... args_t >
    void console_line( string_view format, const args_t &...args )
    {
        if ( !g_console_output )
        {
            return;
        }

        char  buffer[ 768 ]{ };
        usize length{ str::format_to( buffer, sizeof( buffer ) - 2, format, args... ) };
        if ( length + 2 >= sizeof( buffer ) )
        {
            length = sizeof( buffer ) - 3;
        }

        buffer[ length++ ] = '\r';
        buffer[ length++ ] = '\n';
        buffer[ length ]   = 0;

        DWORD written{ };
        I( WriteConsoleA )( g_console_output, buffer, static_cast< DWORD >( length ), &written, nullptr );
    }

    u64 percentile_us( const profile_row &row, u32 percentile )
    {
        u64 samples{ };
        for ( usize bucket{ }; bucket < HISTOGRAM_BUCKET_COUNT; ++bucket )
        {
            samples += row.m_histogram[ bucket ];
        }

        if ( !samples )
        {
            return 0;
        }

        const u64 wanted{ ( samples * percentile + 99ull ) / 100ull };
        u64       seen{ };

        for ( usize bucket{ }; bucket < HISTOGRAM_BUCKET_COUNT; ++bucket )
        {
            seen += row.m_histogram[ bucket ];
            if ( seen < wanted )
            {
                continue;
            }

            if ( bucket < HISTOGRAM_LIMIT_COUNT )
            {
                return HISTOGRAM_US[ bucket ];
            }

            return HISTOGRAM_US[ HISTOGRAM_LIMIT_COUNT - 1 ];
        }

        return HISTOGRAM_US[ HISTOGRAM_LIMIT_COUNT - 1 ];
    }

    bool noisy_enough( const profile_row &row )
    {
        if ( !row.m_calls )
        {
            return false;
        }

        const u64 total_us{ row.m_total_ticks * 1000000ull / g_frequency };
        const u64 max_us{ row.m_max_ticks * 1000000ull / g_frequency };
        return total_us >= g_config.m_noise_total_us || max_us >= 250ull;
    }

    void flush_window( u64 window_ticks )
    {
        if ( !window_ticks || !g_frequency || !g_config.m_profiles || !g_config.m_profile_count )
        {
            return;
        }

        profile_row rows[ MAX_PROFILE_COUNT ]{ };
        usize       row_count{ };

        for ( usize id{ }; id < g_config.m_profile_count; ++id )
        {
            profile_stat &stat{ g_stats[ id ] };
            profile_row   row{ };
            lock_stat( stat );
            row.m_id          = id;
            row.m_calls       = stat.m_calls;
            row.m_total_ticks = stat.m_total_ticks;
            row.m_max_ticks   = stat.m_max_ticks;
            row.m_over_250us  = stat.m_over_250us;
            row.m_over_1ms    = stat.m_over_1ms;
            row.m_work        = stat.m_work;

            stat.m_calls       = 0;
            stat.m_total_ticks = 0;
            stat.m_max_ticks   = 0;
            stat.m_over_250us  = 0;
            stat.m_over_1ms    = 0;
            stat.m_work        = 0;

            for ( usize bucket{ }; bucket < HISTOGRAM_BUCKET_COUNT; ++bucket )
            {
                row.m_histogram[ bucket ]  = stat.m_histogram[ bucket ];
                stat.m_histogram[ bucket ] = 0;
            }
            unlock_stat( stat );

            if ( noisy_enough( row ) )
            {
                rows[ row_count++ ] = row;
            }
        }

        for ( usize index{ 1 }; index < row_count; ++index )
        {
            profile_row value{ rows[ index ] };
            usize       insert{ index };
            while ( insert && rows[ insert - 1 ].m_total_ticks < value.m_total_ticks )
            {
                rows[ insert ] = rows[ insert - 1 ];
                --insert;
            }
            rows[ insert ] = value;
        }

        const f64 window_seconds{ static_cast< f64 >( window_ticks ) / static_cast< f64 >( g_frequency ) };
        console_line( "" );
        const u64 serial{ ++g_window_serial };
        console_line( "[{} profiler] window #{} {:.2f}s | P=always-called/passive, A=feature-gated, M=mixed/shared",
                      g_config.m_name, serial, window_seconds );
        console_line( "inclusive scopes overlap: nested/parallel load percentages can sum above 100%; parent time includes profiler bookkeeping from nested scopes" );
        console_line( "class name | calls/s | avg us | p95~us | p99~us | max us | total ms | load% | >250us | >1ms | work rate | us/work" );

        const usize printed{ row_count < g_config.m_max_printed_rows ? row_count : g_config.m_max_printed_rows };
        for ( usize index{ }; index < printed; ++index )
        {
            const profile_row           &row{ rows[ index ] };
            const atl::profile_metadata &meta{ g_config.m_profiles[ row.m_id ] };
            const f64 calls_per_second{ static_cast< f64 >( row.m_calls ) / window_seconds };
            const f64 average_us{ static_cast< f64 >( row.m_total_ticks ) * 1000000.0 /
                                  ( static_cast< f64 >( g_frequency ) * static_cast< f64 >( row.m_calls ) ) };
            const f64 max_us{ static_cast< f64 >( row.m_max_ticks ) * 1000000.0 / static_cast< f64 >( g_frequency ) };
            const f64 total_ms{ static_cast< f64 >( row.m_total_ticks ) * 1000.0 / static_cast< f64 >( g_frequency ) };
            const f64 load{ static_cast< f64 >( row.m_total_ticks ) * 100.0 / static_cast< f64 >( window_ticks ) };

            if ( meta.m_work )
            {
                const f64 work_per_second{ static_cast< f64 >( row.m_work ) / window_seconds };
                if ( row.m_work )
                {
                    const f64 us_per_work{ static_cast< f64 >( row.m_total_ticks ) * 1000000.0 /
                                           ( static_cast< f64 >( g_frequency ) * static_cast< f64 >( row.m_work ) ) };
                    console_line( "{} {} | {:.1f} | {:.2f} | {} | {} | {:.2f} | {:.2f} | {:.2f} | {} | {} | {:.1f} {}/s | {:.2f}",
                                  class_letter( meta.m_class ), meta.m_name, calls_per_second, average_us,
                                  percentile_us( row, 95 ), percentile_us( row, 99 ), max_us, total_ms, load,
                                  row.m_over_250us, row.m_over_1ms, work_per_second, meta.m_work, us_per_work );
                } else
                {
                    console_line( "{} {} | {:.1f} | {:.2f} | {} | {} | {:.2f} | {:.2f} | {:.2f} | {} | {} | 0.0 {}/s | -",
                                  class_letter( meta.m_class ), meta.m_name, calls_per_second, average_us,
                                  percentile_us( row, 95 ), percentile_us( row, 99 ), max_us, total_ms, load,
                                  row.m_over_250us, row.m_over_1ms, meta.m_work );
                }
            } else
            {
                console_line( "{} {} | {:.1f} | {:.2f} | {} | {} | {:.2f} | {:.2f} | {:.2f} | {} | {} | - | -",
                              class_letter( meta.m_class ), meta.m_name, calls_per_second, average_us,
                              percentile_us( row, 95 ), percentile_us( row, 99 ), max_us, total_ms, load,
                              row.m_over_250us, row.m_over_1ms );
            }
        }

        if ( !printed )
        {
            console_line( "(no samples above noise floor: {} us total or 250 us max)", g_config.m_noise_total_us );
        }
    }

    void ATLAS_STDCALL profiler_thread( void * )
    {
        i64 window_started{ atl::profile_detail::timestamp( ) };

        while ( __atomic_load_n( &g_running, __ATOMIC_RELAXED ) )
        {
            I( Sleep )( g_config.m_flush_poll_ms );

            const i64 now{ atl::profile_detail::timestamp( ) };
            if ( now <= window_started )
            {
                window_started = now;
                continue;
            }

            const u64 elapsed{ static_cast< u64 >( now - window_started ) };
            if ( elapsed >= ticks_for_us( g_config.m_flush_window_us ) )
            {
                flush_window( elapsed );
                window_started = now;
            }
        }

        const i64 ended{ atl::profile_detail::timestamp( ) };
        if ( ended > window_started )
        {
            flush_window( static_cast< u64 >( ended - window_started ) );
        }
    }
} // namespace

namespace atl
{
    namespace profile_detail
    {
        u8 active{ };
        qpc_fn qpc{ };

        void record( profile_id id, u64 ticks, u64 work )
        {
            const usize index{ static_cast< usize >( id ) };
            if ( index >= g_config.m_profile_count || !ticks )
            {
                return;
            }

            profile_stat &stat{ g_stats[ index ] };
            lock_stat( stat );
            if ( !__atomic_load_n( &active, __ATOMIC_ACQUIRE ) )
            {
                unlock_stat( stat );
                return;
            }

            ++stat.m_calls;
            stat.m_total_ticks += ticks;
            stat.m_work += work;
            if ( ticks > stat.m_max_ticks )
            {
                stat.m_max_ticks = ticks;
            }

            if ( ticks > g_250us_ticks )
            {
                ++stat.m_over_250us;
            }
            if ( ticks > g_1ms_ticks )
            {
                ++stat.m_over_1ms;
            }

            usize bucket{ };
            while ( bucket < HISTOGRAM_LIMIT_COUNT && ticks > g_histogram_ticks[ bucket ] )
            {
                ++bucket;
            }
            ++stat.m_histogram[ bucket ];
            unlock_stat( stat );
        }
    } // namespace profile_detail

    bool profiler_startup( const profiler_config &config )
    {
        if ( !config.m_profiles || !config.m_profile_count || config.m_profile_count > MAX_PROFILE_COUNT ||
             !config.m_flush_poll_ms || !config.m_flush_window_us || !config.m_max_printed_rows )
        {
            return false;
        }

        for ( usize index{ }; index < config.m_profile_count; ++index )
        {
            if ( !config.m_profiles[ index ].m_name )
            {
                return false;
            }
        }

        u8 expected{ };
        if ( !__atomic_compare_exchange_n( &g_started, &expected, u8{ 1 }, false,
                                           __ATOMIC_ACQ_REL, __ATOMIC_RELAXED ) )
        {
            return true;
        }

        g_config = config;
        if ( !g_config.m_name )
        {
            g_config.m_name = "runtime";
        }
        if ( !g_config.m_console_title )
        {
            g_config.m_console_title = "Atlas Runtime Profiler";
        }

        profile_detail::qpc = I( QueryPerformanceCounter ).get( );
        if ( !profile_detail::qpc )
        {
            __atomic_store_n( &g_started, u8{ 0 }, __ATOMIC_RELEASE );
            return false;
        }

        LARGE_INTEGER frequency{ };
        if ( !I( QueryPerformanceFrequency )( &frequency ) || frequency.QuadPart <= 0 )
        {
            __atomic_store_n( &g_started, u8{ 0 }, __ATOMIC_RELEASE );
            return false;
        }

        g_frequency = static_cast< u64 >( frequency.QuadPart );
        g_250us_ticks = ticks_for_us( 250 );
        g_1ms_ticks   = ticks_for_us( 1000 );
        for ( usize bucket{ }; bucket < HISTOGRAM_LIMIT_COUNT; ++bucket )
        {
            g_histogram_ticks[ bucket ] = ticks_for_us( HISTOGRAM_US[ bucket ] );
        }

        g_console_owned  = I( AllocConsole )( ) != 0;
        g_console_output = I( GetStdHandle )( -11 ); // STD_OUTPUT_HANDLE
        if ( !g_console_output ||
             g_console_output == reinterpret_cast< void * >( ~static_cast< usize >( 0 ) ) )
        {
            g_console_output = nullptr;
            if ( g_console_owned )
            {
                I( FreeConsole )( );
                g_console_owned = false;
            }
            __atomic_store_n( &g_started, u8{ 0 }, __ATOMIC_RELEASE );
            return false;
        }

        I( SetConsoleTitleA )( g_config.m_console_title );
        console_line( "{} runtime profiler started. Sampling windows are approximately {:.2f} seconds.",
                      g_config.m_name, static_cast< f64 >( g_config.m_flush_window_us ) / 1000000.0 );
        console_line( "timing context WARNING: Debug /Od can inflate non-solver timings" );

        __atomic_store_n( &g_running, u8{ 1 }, __ATOMIC_RELEASE );
        if ( !atl::create_hidden_thread( &profiler_thread, nullptr, &g_thread ) )
        {
            __atomic_store_n( &g_running, u8{ 0 }, __ATOMIC_RELEASE );
            g_thread = nullptr;
            if ( g_console_owned )
            {
                I( FreeConsole )( );
                g_console_owned = false;
            }
            g_console_output = nullptr;
            __atomic_store_n( &g_started, u8{ 0 }, __ATOMIC_RELEASE );
            return false;
        }

        __atomic_store_n( &profile_detail::active, u8{ 1 }, __ATOMIC_RELEASE );
        return true;
    }

    void profiler_shutdown( )
    {
        if ( !__atomic_exchange_n( &g_started, u8{ 0 }, __ATOMIC_ACQ_REL ) )
        {
            return;
        }

        __atomic_store_n( &profile_detail::active, u8{ 0 }, __ATOMIC_RELEASE );
        __atomic_store_n( &g_running, u8{ 0 }, __ATOMIC_RELEASE );

        if ( g_thread )
        {
            I( NtWaitForSingleObject )( g_thread, 0, nullptr );
            I( CloseHandle )( g_thread );
            g_thread = nullptr;
        }

        console_line( "{} runtime profiler stopped.", g_config.m_name );
        if ( g_console_owned )
        {
            I( FreeConsole )( );
            g_console_owned = false;
        }
        g_console_output = nullptr;
    }

} // namespace atl

#endif // ATLAS_DEV
