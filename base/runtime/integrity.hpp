#pragma once

#include "atomic.hpp"
#include "libs/lazy_importer.hpp"
#include "util/time.hpp"
#include "util/typedefs.hpp"

namespace atlas_integrity
{
    inline constexpr u64 heartbeat_max_lag_seconds{ 60 };

    inline constexpr u64 heartbeat_hmac_key_tag_0{ 0x5D9F4E27B61C8A03ull };
    inline constexpr u64 heartbeat_hmac_key_tag_1{ 0x3E1A72CFB08D5496ull };
    inline constexpr u64 heartbeat_hmac_key_tag_2{ 0xA7E5B390C14F826Dull };
    inline constexpr u64 heartbeat_hmac_key_tag_3{ 0x1F6C83D5A907E24Bull };

    inline constexpr u64 heartbeat_response_key_tag_0{ 0x9A4E72C1B503F86Dull };
    inline constexpr u64 heartbeat_response_key_tag_1{ 0x27B9103ED8CA4F67ull };
    inline constexpr u64 heartbeat_response_key_tag_2{ 0xC6E58B2714F30A9Dull };
    inline constexpr u64 heartbeat_response_key_tag_3{ 0x08FD4A6E72C593B1ull };

    inline constexpr u64 heartbeat_fingerprint_key_tag_0{ 0x4F7B2E938A05C61Dull };
    inline constexpr u64 heartbeat_fingerprint_key_tag_1{ 0xBE1C840A72F93D56ull };
    inline constexpr u64 heartbeat_fingerprint_key_tag_2{ 0x1D63F057BCE48A29ull };
    inline constexpr u64 heartbeat_fingerprint_key_tag_3{ 0x80A2375FC914EBD6ull };

    inline constexpr u32 heartbeat_section_response_offset{ 0x40 };
    inline constexpr u32 heartbeat_section_total_size{ 0xC0 };

    inline constexpr u64 key_mask_a_tag          { 0x7A2E4F18D3C19B07ull };
    inline constexpr u64 key_mask_b_tag          { 0x4B8D62A091E57F2Dull };
    inline constexpr u64 anchor_perf_mask_a_tag  { 0x183F7E529C4D6A81ull };
    inline constexpr u64 anchor_perf_mask_b_tag  { 0x9D62B4A731E058F7ull };
    inline constexpr u64 anchor_freq_mask_a_tag  { 0x2E59C816F74A3D52ull };
    inline constexpr u64 anchor_freq_mask_b_tag  { 0x6AC13E89D52B47F1ull };
    inline constexpr u64 anchor_expiry_mask_a_tag{ 0x4B7E92D581C3F60Aull };
    inline constexpr u64 anchor_expiry_mask_b_tag{ 0xC27A5F8E34D1960Bull };
    inline constexpr u64 anchor_wall_mask_a_tag  { 0x75D82E9C4A1F6B30ull };
    inline constexpr u64 anchor_wall_mask_b_tag  { 0x3EB67A2958D1C40Full };

    inline constexpr u64 health_token_tag{ 0x1F5A09B7E6D2C843ull };

    inline constexpr u64 wall_drift_tolerance_seconds{ 2 };

    constexpr u64 mix64( u64 value )
    {
        value ^= value >> 30u;
        value *= 0xBF58476D1CE4E5B9ull;
        value ^= value >> 27u;
        value *= 0x94D049BB133111EBull;
        value ^= value >> 31u;
        return value;
    }

    static_assert( mix64( 0x0123456789ABCDEFull ) == 0xB2C058E4EBB5112Cull );

    void capture( const u8 *session_id, const u8 *artifact_sha256 );

    void capture_time_anchor( u64 anchor_perf, u64 perf_freq,
                              u64 expiry_perf_ticks, u64 anchor_wall_seconds );

    void refresh_time_anchor( u64 kernel_perf, u64 kernel_wall_seconds );

    void capture_heartbeat_context( const void *heartbeat_region, u64 heartbeat_key_tag );

    struct heartbeat_frame
    {
        u64 kernel_perf;
        u64 kernel_wall_seconds;
        u64 nonce;
        u64 burn_flags;
        u8  mac[ 32 ];
    };
    static_assert( sizeof( heartbeat_frame ) == 64 );

    inline constexpr u64 heartbeat_burn_bit_set                 { 0x1ull };
    inline constexpr u64 heartbeat_burn_reason_mask             { 0xEull };
    inline constexpr u64 heartbeat_burn_reason_handshake_hmac   { 0x2ull };
    inline constexpr u64 heartbeat_burn_reason_fingerprint      { 0x4ull };
    inline constexpr u64 heartbeat_burn_reason_debugger_attached{ 0x6ull };
    inline constexpr u64 heartbeat_burn_reason_handshake_timeout{ 0x8ull };
    inline constexpr u64 heartbeat_burn_reason_kernel_tool      { 0xAull };

    struct heartbeat_response_frame
    {
        u64 dll_perf_timestamp;
        u64 echo_nonce;
        u64 response_counter;
        u64 dll_fingerprint_lo;
        u64 dll_fingerprint_hi;
        u64 reserved;
        u8  mac[ 32 ];
        u8  reserved_padding[ 16 ];
    };
    static_assert( sizeof( heartbeat_response_frame ) == 96 );

    void check_heartbeat( );

    bool ipc_watcher_startup( );

    bool wait_for_initial_handshake( u64 timeout_ms );

    extern atl::atomic< u64 > g_poison;
    extern atl::atomic< u64 > g_last_burn_reason;

    ATLAS_FORCE_INLINE u64 last_burn_reason( )
    {
        return g_last_burn_reason.load( );
    }

    inline constexpr u64 burn_reason_dll_no_region{ 0x0101ull };
    inline constexpr u64 burn_reason_dll_tampered_mask{ 0x0102ull };
    inline constexpr u64 burn_reason_dll_hider_present{ 0x0104ull };
    inline constexpr u64 burn_reason_dll_anchor_bad{ 0x0108ull };
    inline constexpr u64 burn_reason_dll_clock_skew{ 0x0110ull };
    inline constexpr u64 burn_reason_dll_lag_exceeded{ 0x0120ull };
    inline constexpr u64 burn_reason_dll_frame_hmac{ 0x0140ull };
    inline constexpr u64 burn_reason_dll_handshake_missing{ 0x0180ull };
    inline constexpr u64 burn_reason_dll_key_mismatch{ 0x0201ull };
    inline constexpr u64 burn_reason_dll_anchor_tampered{ 0x0202ull };
    inline constexpr u64 burn_reason_dll_anchor_expired{ 0x0204ull };
    inline constexpr u64 burn_reason_dll_wall_drift{ 0x0208ull };
    inline constexpr u64 burn_reason_dll_perf_rewound{ 0x0210ull };

    ATLAS_FORCE_INLINE bool healthy( )
    {
#if defined( ATLAS_DISABLE_CLOUD )
        return true;
#else
        return g_poison.load( ) != 0;
#endif
    }

    ATLAS_FORCE_INLINE void poison( )
    {
        g_poison.store( 0 );
    }

    ATLAS_FORCE_INLINE void poison_with( u64 reason )
    {
        g_last_burn_reason.store( reason );
        poison( );
    }

    extern u64 g_key_copy_a;
    extern u64 g_key_copy_b;

    extern u64 g_anchor_perf_a;
    extern u64 g_anchor_perf_b;
    extern u64 g_anchor_freq_a;
    extern u64 g_anchor_freq_b;
    extern u64 g_anchor_expiry_a;
    extern u64 g_anchor_expiry_b;
    extern u64 g_anchor_wall_a;
    extern u64 g_anchor_wall_b;

    extern u64 g_key_mask_a;
    extern u64 g_key_mask_b;
    extern u64 g_anchor_perf_mask_a;
    extern u64 g_anchor_perf_mask_b;
    extern u64 g_anchor_freq_mask_a;
    extern u64 g_anchor_freq_mask_b;
    extern u64 g_anchor_expiry_mask_a;
    extern u64 g_anchor_expiry_mask_b;
    extern u64 g_anchor_wall_mask_a;
    extern u64 g_anchor_wall_mask_b;

    ATLAS_FORCE_INLINE u64 read_perf_counter( )
    {
        LARGE_INTEGER counter{ };
        if ( I( QueryPerformanceCounter )( &counter ) == 0 || counter.QuadPart < 0 )
        {
            return 0;
        }
        return static_cast< u64 >( counter.QuadPart );
    }

    ATLAS_FORCE_INLINE u64 read_wall_seconds( )
    {
        return util::current_unix_time( );
    }

    ATLAS_FORCE_INLINE void check_key( u64 salt_in )
    {
        volatile u64 salt{ salt_in };
        const u64 left { g_key_copy_a ^ g_key_mask_a ^ salt };
        const u64 right{ g_key_copy_b ^ g_key_mask_b ^ salt };
        if ( left != right )
        {
            poison_with( burn_reason_dll_key_mismatch );
        }
    }

    ATLAS_FORCE_INLINE void check_time_anchor( u64 salt_in )
    {
        volatile u64 salt{ salt_in };
        for ( u32 attempt{ }; ; ++attempt )
        {
            if ( ( g_anchor_perf_a   ^ g_anchor_perf_mask_a   ^ salt ) ==
                     ( g_anchor_perf_b   ^ g_anchor_perf_mask_b   ^ salt ) &&
                 ( g_anchor_freq_a   ^ g_anchor_freq_mask_a   ^ salt ) ==
                     ( g_anchor_freq_b   ^ g_anchor_freq_mask_b   ^ salt ) &&
                 ( g_anchor_expiry_a ^ g_anchor_expiry_mask_a ^ salt ) ==
                     ( g_anchor_expiry_b ^ g_anchor_expiry_mask_b ^ salt ) &&
                 ( g_anchor_wall_a   ^ g_anchor_wall_mask_a   ^ salt ) ==
                     ( g_anchor_wall_b   ^ g_anchor_wall_mask_b   ^ salt ) )
            {
                break;
            }
            if ( attempt >= 3 )
            {
                poison_with( burn_reason_dll_anchor_tampered );
                return;
            }
            _mm_pause( );
        }

        const u64 anchor_perf  { g_anchor_perf_a   ^ g_anchor_perf_mask_a };
        const u64 anchor_freq  { g_anchor_freq_a   ^ g_anchor_freq_mask_a };
        const u64 anchor_expiry{ g_anchor_expiry_a ^ g_anchor_expiry_mask_a };
        const u64 anchor_wall  { g_anchor_wall_a   ^ g_anchor_wall_mask_a };

        if ( anchor_freq == 0 )
        {
            poison_with( burn_reason_dll_anchor_bad );
            return;
        }

        const u64 perf_now{ read_perf_counter( ) };
        const u64 wall_now{ read_wall_seconds( ) };

        if ( perf_now < anchor_perf )
        {
            poison_with( burn_reason_dll_perf_rewound );
            return;
        }

        const u64 perf_elapsed{ perf_now - anchor_perf };
        if ( perf_elapsed > anchor_expiry )
        {
            poison_with( burn_reason_dll_anchor_expired );
            return;
        }

        const u64 perf_elapsed_seconds{ perf_elapsed / anchor_freq };
        const u64 predicted_wall{ anchor_wall + perf_elapsed_seconds };
        const u64 drift{ wall_now > predicted_wall ? wall_now - predicted_wall
                                                   : predicted_wall - wall_now };
        if ( drift > wall_drift_tolerance_seconds )
        {
            poison_with( burn_reason_dll_wall_drift );
        }
    }
}

#define ATLAS_INTEGRITY_CHECK( site_salt )                                     \
    do                                                                         \
    {                                                                          \
        volatile u64 atlas_salt{ static_cast< u64 >( site_salt ) };            \
        ::atlas_integrity::check_key( atlas_salt );                            \
        ::atlas_integrity::check_time_anchor( atlas_salt );                    \
    } while ( false )
