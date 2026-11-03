#pragma once

#include "libs/lazy_importer.hpp"
#include "typedefs.hpp"

namespace util
{
    inline u64 current_unix_time( )
    {
        constexpr u64 ticks_per_second{ 10000000 };
        constexpr u64 windows_epoch_seconds{ 11644473600 };
        i64           system_time{ };

        if ( I( NtQuerySystemTime )( &system_time ) < 0 || system_time <= 0 )
        {
            return 0;
        }

        const u64 seconds{ static_cast< u64 >( system_time ) / ticks_per_second };
        return seconds > windows_epoch_seconds ? seconds - windows_epoch_seconds : 0;
    }

#if defined( ATLAS_PROVIDE_OWN_WINDEFS )
    struct system_time_t
    {
        u16 m_year{ };
        u16 m_month{ };
        u16 m_day_of_week{ };
        u16 m_day{ };
        u16 m_hour{ };
        u16 m_minute{ };
        u16 m_second{ };
        u16 m_milliseconds{ };
    };
#else
    using system_time_t = SYSTEMTIME;
#endif

    inline system_time_t local_time( )
    {
        system_time_t local{ };

        if ( I( GetLocalTime ) )
        {
            I( GetLocalTime )( &local );
        }

        return local;
    }
} // namespace util
