#include "integrity.hpp"

#include "api/api.hpp"
#include "memory.hpp"
#include "runtime/thread.hpp"
#include "util/crypto.hpp"
#include "util/json.hpp"
#include "util/pe_module.hpp"

extern api g_api;

namespace atlas_integrity
{
    u64 g_key_copy_a{ };
    u64 g_key_copy_b{ };

    u64 g_anchor_perf_a{ };
    u64 g_anchor_perf_b{ };
    u64 g_anchor_freq_a{ };
    u64 g_anchor_freq_b{ };
    u64 g_anchor_expiry_a{ };
    u64 g_anchor_expiry_b{ };
    u64 g_anchor_wall_a{ };
    u64 g_anchor_wall_b{ };

    u64 g_key_mask_a{ };
    u64 g_key_mask_b{ };
    u64 g_anchor_perf_mask_a{ };
    u64 g_anchor_perf_mask_b{ };
    u64 g_anchor_freq_mask_a{ };
    u64 g_anchor_freq_mask_b{ };
    u64 g_anchor_expiry_mask_a{ };
    u64 g_anchor_expiry_mask_b{ };
    u64 g_anchor_wall_mask_a{ };
    u64 g_anchor_wall_mask_b{ };

    u64 g_heartbeat_region_a{ };
    u64 g_heartbeat_region_b{ };
    u64 g_heartbeat_key_tag_a{ };
    u64 g_heartbeat_key_tag_b{ };

    ATLAS_NO_DESTROY atl::atomic< u64 > g_last_heartbeat_nonce{ };

    u64 g_dll_fingerprint_lo{ };
    u64 g_dll_fingerprint_hi{ };

    ATLAS_NO_DESTROY atl::atomic< u64 > g_response_counter{ };

    constexpr u64 heartbeat_region_mask_a_tag{ 0x6B2E97D14AC803F5ull };
    constexpr u64 heartbeat_region_mask_b_tag{ 0xC418A703D96F2B5Eull };
    constexpr u64 heartbeat_key_tag_mask_a_tag{ 0x9F03E51D2A47B68Cull };
    constexpr u64 heartbeat_key_tag_mask_b_tag{ 0x2D74C9B816E5A031ull };

    u64 g_heartbeat_region_mask_a{ };
    u64 g_heartbeat_region_mask_b{ };
    u64 g_heartbeat_key_tag_mask_a{ };
    u64 g_heartbeat_key_tag_mask_b{ };

    ATLAS_NO_DESTROY atl::atomic< u64 > g_poison{ };
    ATLAS_NO_DESTROY atl::atomic< u64 > g_last_burn_reason{ };

    ATLAS_NO_DESTROY atl::atomic< u64 > g_last_fail_nonce{ };
    ATLAS_NO_DESTROY atl::atomic< u64 > g_last_fail_burn_flags{ };
    ATLAS_NO_DESTROY atl::atomic< u64 > g_consecutive_hmac_fails{ };

    void capture( const u8 *session_id, const u8 *artifact_sha256 )
    {
        if ( !session_id || !artifact_sha256 )
        {
            return;
        }

        u64 key{ 0x9E3779B97F4A7C15ull };
        for ( usize index{ }; index < 16; ++index )
        {
            key = mix64( key ^ session_id[ index ] ^
                         ( static_cast< u64 >( index + 1 ) * 0x165667B19E3779F9ull ) );
        }
        for ( usize index{ }; index < 32; ++index )
        {
            key = mix64( key ^ artifact_sha256[ index ] ^
                         ( static_cast< u64 >( index + 17 ) * 0xC2B2AE3D27D4EB4Full ) );
        }
        key = mix64( key );

        g_key_mask_a           = mix64( key ^ key_mask_a_tag );
        g_key_mask_b           = mix64( key ^ key_mask_b_tag );
        g_anchor_perf_mask_a   = mix64( key ^ anchor_perf_mask_a_tag );
        g_anchor_perf_mask_b   = mix64( key ^ anchor_perf_mask_b_tag );
        g_anchor_freq_mask_a   = mix64( key ^ anchor_freq_mask_a_tag );
        g_anchor_freq_mask_b   = mix64( key ^ anchor_freq_mask_b_tag );
        g_anchor_expiry_mask_a = mix64( key ^ anchor_expiry_mask_a_tag );
        g_anchor_expiry_mask_b = mix64( key ^ anchor_expiry_mask_b_tag );
        g_anchor_wall_mask_a   = mix64( key ^ anchor_wall_mask_a_tag );
        g_anchor_wall_mask_b   = mix64( key ^ anchor_wall_mask_b_tag );

        g_heartbeat_region_mask_a  = mix64( key ^ heartbeat_region_mask_a_tag );
        g_heartbeat_region_mask_b  = mix64( key ^ heartbeat_region_mask_b_tag );
        g_heartbeat_key_tag_mask_a = mix64( key ^ heartbeat_key_tag_mask_a_tag );
        g_heartbeat_key_tag_mask_b = mix64( key ^ heartbeat_key_tag_mask_b_tag );

        g_key_copy_a = key ^ g_key_mask_a;
        g_key_copy_b = key ^ g_key_mask_b;

        u64 token{ mix64( key ^ health_token_tag ) };
        if ( token == 0 )
        {
            token = 1;
        }
        g_poison.store( token );

        u8 fingerprint_key[ 32 ]{ };
        {
            const u64 slot_values[ 4 ]{
                mix64( key ^ heartbeat_fingerprint_key_tag_0 ),
                mix64( key ^ heartbeat_fingerprint_key_tag_1 ),
                mix64( key ^ heartbeat_fingerprint_key_tag_2 ),
                mix64( key ^ heartbeat_fingerprint_key_tag_3 ),
            };
            for ( usize slot{ }; slot < 4; ++slot )
            {
                for ( usize byte{ }; byte < 8; ++byte )
                {
                    fingerprint_key[ slot * 8 + byte ] =
                        static_cast< u8 >( slot_values[ slot ] >> ( byte * 8u ) );
                }
            }
        }

        u8 fingerprint_full[ 32 ]{ };
        crypto::hmac_sha256( fingerprint_key, sizeof( fingerprint_key ),
                             artifact_sha256, 32, fingerprint_full );

        u64 fingerprint_lo{ };
        u64 fingerprint_hi{ };
        for ( usize index{ }; index < 8; ++index )
        {
            fingerprint_lo |= static_cast< u64 >( fingerprint_full[ index ] ) << ( index * 8u );
            fingerprint_hi |= static_cast< u64 >( fingerprint_full[ index + 8 ] ) << ( index * 8u );
        }
        g_dll_fingerprint_lo = fingerprint_lo;
        g_dll_fingerprint_hi = fingerprint_hi;
        g_response_counter.store( 0 );

        atl::memset( fingerprint_key, 0, sizeof( fingerprint_key ) );
        atl::memset( fingerprint_full, 0, sizeof( fingerprint_full ) );
        atl::memset( &fingerprint_lo, 0, sizeof( fingerprint_lo ) );
        atl::memset( &fingerprint_hi, 0, sizeof( fingerprint_hi ) );

        atl::memset( &key, 0, sizeof( key ) );
        atl::memset( &token, 0, sizeof( token ) );
    }

    void capture_time_anchor( u64 anchor_perf, u64 perf_freq,
                              u64 expiry_perf_ticks, u64 anchor_wall_seconds )
    {
        g_anchor_perf_a   = anchor_perf          ^ g_anchor_perf_mask_a;
        g_anchor_perf_b   = anchor_perf          ^ g_anchor_perf_mask_b;
        g_anchor_freq_a   = perf_freq            ^ g_anchor_freq_mask_a;
        g_anchor_freq_b   = perf_freq            ^ g_anchor_freq_mask_b;
        g_anchor_expiry_a = expiry_perf_ticks    ^ g_anchor_expiry_mask_a;
        g_anchor_expiry_b = expiry_perf_ticks    ^ g_anchor_expiry_mask_b;
        g_anchor_wall_a   = anchor_wall_seconds  ^ g_anchor_wall_mask_a;
        g_anchor_wall_b   = anchor_wall_seconds  ^ g_anchor_wall_mask_b;

        atl::memset( &anchor_perf, 0, sizeof( anchor_perf ) );
        atl::memset( &perf_freq, 0, sizeof( perf_freq ) );
        atl::memset( &expiry_perf_ticks, 0, sizeof( expiry_perf_ticks ) );
        atl::memset( &anchor_wall_seconds, 0, sizeof( anchor_wall_seconds ) );
    }

    void refresh_time_anchor( u64 kernel_perf, u64 kernel_wall_seconds )
    {
        const u64 old_perf  { g_anchor_perf_a   ^ g_anchor_perf_mask_a };
        const u64 old_expiry{ g_anchor_expiry_a ^ g_anchor_expiry_mask_a };
        const u64 deadline  { old_perf + old_expiry };
        if ( kernel_perf >= deadline )
        {
            poison_with( burn_reason_dll_anchor_expired );
            return;
        }
        const u64 new_expiry{ deadline - kernel_perf };

        g_anchor_perf_a   = kernel_perf          ^ g_anchor_perf_mask_a;
        g_anchor_perf_b   = kernel_perf          ^ g_anchor_perf_mask_b;
        g_anchor_expiry_a = new_expiry           ^ g_anchor_expiry_mask_a;
        g_anchor_expiry_b = new_expiry           ^ g_anchor_expiry_mask_b;
        g_anchor_wall_a   = kernel_wall_seconds  ^ g_anchor_wall_mask_a;
        g_anchor_wall_b   = kernel_wall_seconds  ^ g_anchor_wall_mask_b;

        atl::memset( &kernel_perf, 0, sizeof( kernel_perf ) );
        atl::memset( &kernel_wall_seconds, 0, sizeof( kernel_wall_seconds ) );
    }

    void capture_heartbeat_context( const void *heartbeat_region, u64 heartbeat_key_tag )
    {
        const u64 region_value{ reinterpret_cast< u64 >( heartbeat_region ) };
        g_heartbeat_region_a  = region_value       ^ g_heartbeat_region_mask_a;
        g_heartbeat_region_b  = region_value       ^ g_heartbeat_region_mask_b;
        g_heartbeat_key_tag_a = heartbeat_key_tag  ^ g_heartbeat_key_tag_mask_a;
        g_heartbeat_key_tag_b = heartbeat_key_tag  ^ g_heartbeat_key_tag_mask_b;

        u64 wipe_region{ region_value };
        u64 wipe_tag{ heartbeat_key_tag };
        atl::memset( &wipe_region, 0, sizeof( wipe_region ) );
        atl::memset( &wipe_tag, 0, sizeof( wipe_tag ) );
    }

    namespace
    {
        ATLAS_FORCE_INLINE void store_u64_le( u8 *destination, u64 value )
        {
            for ( usize index{ }; index < 8; ++index )
            {
                destination[ index ] = static_cast< u8 >( value >> ( index * 8u ) );
            }
        }
    }

    namespace
    {
        ATLAS_FORCE_INLINE bool wequal_i( const wchar_t *a, const wchar_t *b )
        {
            for ( ;; )
            {
                wchar_t ca{ *a }, cb{ *b };
                if ( ca >= L'A' && ca <= L'Z' ) ca = static_cast< wchar_t >( ca + ( L'a' - L'A' ) );
                if ( cb >= L'A' && cb <= L'Z' ) cb = static_cast< wchar_t >( cb + ( L'a' - L'A' ) );
                if ( ca != cb ) return false;
                if ( ca == 0 ) return true;
                ++a; ++b;
            }
        }

        ATLAS_FORCE_INLINE usize cstr_len( const char *s )
        {
            usize n{ };
            while ( s[ n ] ) ++n;
            return n;
        }

        bool memory_contains( const u8 *base, usize size, const char *needle )
        {
            const usize needle_len{ cstr_len( needle ) };
            if ( needle_len == 0 || size < needle_len ) return false;
            const u8 *end{ base + size - needle_len + 1 };
            for ( const u8 *p{ base }; p < end; ++p )
            {
                usize i{ };
                while ( i < needle_len && p[ i ] == static_cast< u8 >( needle[ i ] ) ) ++i;
                if ( i == needle_len ) return true;
            }
            return false;
        }

        constexpr const char *g_hider_signatures[] =
        {
            "TitanDebuggingCallBack",
            "TitanHideTE.dll",
            "TitanHide.sys",
            "HookLibraryx64.dll",
            "HookLibraryx86.dll",
            "HookedNtQueryInformationProcess",
            "HookedNtSetInformationProcess",
            "HookedNtCreateThreadEx",
            "HookedNativeCallInternal",
            "Failed to load ScyllaHide",
            "SCYLLA_HIDE",
        };

        constexpr const wchar_t *g_hider_module_names[] =
        {
            L"TitanHide.dll",
            L"TitanHideTE.dll",
            L"HookLibraryx64.dll",
            L"HookLibraryx86.dll",
        };

        bool scan_debugger_hiders( )
        {
            auto *process_environment_block{ li::detail::peb( ) };
            if ( !process_environment_block || !process_environment_block->Ldr )
            {
                return false;
            }
            LIST_ENTRY *head{ &process_environment_block->Ldr->InMemoryOrderModuleList };
            for ( LIST_ENTRY *entry{ head->Flink }; entry != head; entry = entry->Flink )
            {
                auto *module{ reinterpret_cast< LDR_DATA_TABLE_ENTRY * >(
                    reinterpret_cast< u8 * >( entry ) - sizeof( LIST_ENTRY ) ) };
                if ( !module->DllBase || !module->BaseDllName.Buffer ) continue;

                for ( const wchar_t *bad_name : g_hider_module_names )
                {
                    if ( wequal_i( module->BaseDllName.Buffer, bad_name ) )
                    {
                        return true;
                    }
                }

                util::pe_module pe{ module->DllBase };
                auto *nt{ pe.get_nt_header( ) };
                if ( !nt ) continue;
                const u8 *base{ reinterpret_cast< const u8 * >( module->DllBase ) };
                usize image_size{ nt->OptionalHeader.SizeOfImage };
                if ( image_size == 0 || image_size > ( 128ull * 1024ull * 1024ull ) ) continue;

                for ( const char *sig : g_hider_signatures )
                {
                    if ( memory_contains( base, image_size, sig ) )
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    }

    void check_heartbeat( )
    {
        if ( scan_debugger_hiders( ) )
        {
            poison_with( burn_reason_dll_hider_present );
            return;
        }

        const u64 region_a_plain{ g_heartbeat_region_a ^ g_heartbeat_region_mask_a };
        const u64 region_b_plain{ g_heartbeat_region_b ^ g_heartbeat_region_mask_b };
        const u64 tag_a_plain   { g_heartbeat_key_tag_a ^ g_heartbeat_key_tag_mask_a };
        const u64 tag_b_plain   { g_heartbeat_key_tag_b ^ g_heartbeat_key_tag_mask_b };
        if ( region_a_plain != region_b_plain || tag_a_plain != tag_b_plain )
        {
            poison_with( burn_reason_dll_tampered_mask );
            return;
        }
        const u64 region_value{ region_a_plain };
        const u64 key_tag{ tag_a_plain };
        if ( !region_value )
        {
            poison_with( burn_reason_dll_no_region );
            return;
        }

        heartbeat_frame   frame{ };
        const volatile u8 *source{ reinterpret_cast< const volatile u8 * >( region_value ) };
        u8                *destination{ reinterpret_cast< u8 * >( &frame ) };
        for ( usize index{ }; index < sizeof( frame ); ++index )
        {
            destination[ index ] = source[ index ];
        }

        g_last_fail_nonce.store( frame.nonce );
        g_last_fail_burn_flags.store( frame.burn_flags );

        if ( !frame.nonce )
        {
            return;
        }

        const u64 session_key{ g_key_copy_a ^ g_key_mask_a };
        u8        hmac_key[ 32 ]{ };
        store_u64_le( hmac_key + 0,  mix64( session_key ^ key_tag ^ heartbeat_hmac_key_tag_0 ) );
        store_u64_le( hmac_key + 8,  mix64( session_key ^ key_tag ^ heartbeat_hmac_key_tag_1 ) );
        store_u64_le( hmac_key + 16, mix64( session_key ^ key_tag ^ heartbeat_hmac_key_tag_2 ) );
        store_u64_le( hmac_key + 24, mix64( session_key ^ key_tag ^ heartbeat_hmac_key_tag_3 ) );

        u8 mac_input[ 32 ]{ };
        store_u64_le( mac_input + 0,  frame.kernel_perf );
        store_u64_le( mac_input + 8,  frame.kernel_wall_seconds );
        store_u64_le( mac_input + 16, frame.nonce );
        store_u64_le( mac_input + 24, frame.burn_flags );

        u8 expected_mac[ 32 ]{ };
        crypto::hmac_sha256( hmac_key, sizeof( hmac_key ), mac_input, sizeof( mac_input ), expected_mac );

        u8 diff{ };
        for ( usize index{ }; index < 32; ++index )
        {
            diff |= expected_mac[ index ] ^ frame.mac[ index ];
        }

        atl::memset( hmac_key, 0, sizeof( hmac_key ) );
        atl::memset( mac_input, 0, sizeof( mac_input ) );
        atl::memset( expected_mac, 0, sizeof( expected_mac ) );

        if ( diff )
        {
            g_consecutive_hmac_fails.fetch_add( 1 );
            poison_with( burn_reason_dll_frame_hmac );
            return;
        }

        g_consecutive_hmac_fails.store( 0 );

        if ( frame.burn_flags & heartbeat_burn_bit_set )
        {
            const u64 driver_reason{ frame.burn_flags & heartbeat_burn_reason_mask };
            poison_with( driver_reason ? driver_reason : heartbeat_burn_reason_handshake_hmac );
            return;
        }

        const u64 anchor_freq{ g_anchor_freq_a ^ g_anchor_freq_mask_a };
        if ( anchor_freq == 0 )
        {
            poison_with( burn_reason_dll_anchor_bad );
            return;
        }
        const u64 local_perf{ read_perf_counter( ) };
        if ( local_perf < frame.kernel_perf )
        {
            poison_with( burn_reason_dll_clock_skew );
            return;
        }
        const u64 lag_ticks{ local_perf - frame.kernel_perf };
        if ( lag_ticks / anchor_freq > heartbeat_max_lag_seconds )
        {
            poison_with( burn_reason_dll_lag_exceeded );
            return;
        }

        const u64 previous_nonce{ g_last_heartbeat_nonce.load( ) };
        if ( frame.nonce <= previous_nonce )
        {
            return;
        }

        g_last_heartbeat_nonce.store( frame.nonce );
        refresh_time_anchor( frame.kernel_perf, frame.kernel_wall_seconds );

        u8 response_key[ 32 ]{ };
        store_u64_le( response_key + 0,  mix64( session_key ^ key_tag ^ heartbeat_response_key_tag_0 ) );
        store_u64_le( response_key + 8,  mix64( session_key ^ key_tag ^ heartbeat_response_key_tag_1 ) );
        store_u64_le( response_key + 16, mix64( session_key ^ key_tag ^ heartbeat_response_key_tag_2 ) );
        store_u64_le( response_key + 24, mix64( session_key ^ key_tag ^ heartbeat_response_key_tag_3 ) );

        heartbeat_response_frame response{ };
        response.dll_perf_timestamp = local_perf;
        response.echo_nonce         = frame.nonce;
        response.response_counter   = g_response_counter.fetch_add( 1 ) + 1;
        response.dll_fingerprint_lo = g_dll_fingerprint_lo;
        response.dll_fingerprint_hi = g_dll_fingerprint_hi;
        response.reserved           = 0;

        u8 response_mac_input[ 48 ]{ };
        store_u64_le( response_mac_input + 0,  response.dll_perf_timestamp );
        store_u64_le( response_mac_input + 8,  response.echo_nonce );
        store_u64_le( response_mac_input + 16, response.response_counter );
        store_u64_le( response_mac_input + 24, response.dll_fingerprint_lo );
        store_u64_le( response_mac_input + 32, response.dll_fingerprint_hi );
        store_u64_le( response_mac_input + 40, response.reserved );

        crypto::hmac_sha256( response_key, sizeof( response_key ),
                             response_mac_input, sizeof( response_mac_input ), response.mac );

        volatile u8 *response_dest{
            reinterpret_cast< volatile u8 * >( region_value ) +
            heartbeat_section_response_offset };
        const u8 *response_src{ reinterpret_cast< const u8 * >( &response ) };
        for ( usize index{ }; index < sizeof( response ); ++index )
        {
            response_dest[ index ] = response_src[ index ];
        }

        atl::memset( response_key, 0, sizeof( response_key ) );
        atl::memset( response_mac_input, 0, sizeof( response_mac_input ) );
        atl::memset( &response, 0, sizeof( response ) );
        atl::memset( &frame, 0, sizeof( frame ) );
    }

    namespace
    {
        constexpr u64 ipc_watcher_period_ms{ 1000 };
        constexpr u64 ipc_watcher_startup_grace_ms{ 15000 };
        constexpr u32 anti_crack_response_limit{ 512 };

        void hex_encode_lower( const u8 *bytes, usize count, char *out )
        {
            static constexpr char table[ 17 ]{ "0123456789abcdef" };
            for ( usize index{ }; index < count; ++index )
            {
                out[ index * 2u + 0u ] = table[ ( bytes[ index ] >> 4u ) & 0xFu ];
                out[ index * 2u + 1u ] = table[ bytes[ index ] & 0xFu ];
            }
            out[ count * 2u ] = '\0';
        }

        const char *reason_label( u64 reason )
        {
            switch ( reason )
            {
                case heartbeat_burn_reason_handshake_hmac: return "driver burn, handshake_hmac";
                case heartbeat_burn_reason_fingerprint: return "driver burn, fingerprint";
                case heartbeat_burn_reason_debugger_attached: return "driver burn, debugger_attached";
                case heartbeat_burn_reason_handshake_timeout: return "driver burn, handshake_timeout";
                case heartbeat_burn_reason_kernel_tool: return "driver burn, kernel_tool";
                case burn_reason_dll_no_region: return "dll poisoned itself, ipc_region_null";
                case burn_reason_dll_tampered_mask: return "dll poisoned itself, mask_tampered";
                case burn_reason_dll_hider_present: return "dll poisoned itself, debugger_hider";
                case burn_reason_dll_anchor_bad: return "dll poisoned itself, anchor_corrupt";
                case burn_reason_dll_clock_skew: return "dll poisoned itself, clock_skew";
                case burn_reason_dll_lag_exceeded: return "dll poisoned itself, ipc_lag";
                case burn_reason_dll_frame_hmac: return "dll poisoned itself, frame_hmac";
                case burn_reason_dll_handshake_missing: return "dll poisoned itself, handshake_missing";
                case burn_reason_dll_key_mismatch: return "dll poisoned itself, key_mismatch";
                case burn_reason_dll_anchor_tampered: return "dll poisoned itself, anchor_tampered";
                case burn_reason_dll_anchor_expired: return "dll poisoned itself, anchor_expired";
                case burn_reason_dll_wall_drift: return "dll poisoned itself, wall_drift";
                case burn_reason_dll_perf_rewound: return "dll poisoned itself, perf_rewound";
                default: return "unknown";
            }
        }

        ATLAS_FORCE_INLINE void anti_crack_report( u64 reason )
        {
#if defined( ATLAS_DISABLE_CLOUD )
            ATLAS_UNUSED_PARAM( reason );
#else
            if ( !g_api.has_config_token( ) )
            {
                return;
            }

            char kernel_session_hex[ 33 ]{ };
            hex_encode_lower( g_api.session_id( ), 16, kernel_session_hex );

            json::writer writer{ };
            writer.begin_object( );
            writer.member( "kernel_session_id", string_view{ kernel_session_hex } );
            writer.member( "reason", string_view{ reason_label( reason ) } );
            writer.member( "reason_code", reason );
            writer.member( "nonce", g_last_fail_nonce.load( ) );
            writer.member( "burn_flags", g_last_fail_burn_flags.load( ) );
            writer.member( "consecutive_hmac_fails", g_consecutive_hmac_fails.load( ) );
            writer.end_object( );
            if ( !writer.ok( ) )
            {
                return;
            }

            atl::vector< char > response{ };
            u32                 status{ };
            g_api.request( ATLAS_XORWSTR( L"POST" ).get( ),
                           ATLAS_XORWSTR( L"/v1/sessions/anti_crack" ).get( ),
                           writer.text( ), response, anti_crack_response_limit, status );
            atl::memset( response.data( ), 0, response.size( ) );
#endif
        }

        [[noreturn]] void revoke_and_fastfail( u64 reason )
        {
            anti_crack_report( reason );
            g_api.clear_credentials( );
            I( ExitProcess )( 1 );
            __fastfail( 42069 );
            for ( ;; )
            {
            }
        }

        void ATLAS_STDCALL ipc_watcher_entry( void * )
        {
            const u64 started{ I( GetTickCount64 )( ) };
            for ( ;; )
            {
                check_heartbeat( );

                const bool handshake_complete{ g_last_heartbeat_nonce.load( ) != 0 };

                if ( !handshake_complete )
                {
                    if ( !healthy( ) ||
                         I( GetTickCount64 )( ) - started > ipc_watcher_startup_grace_ms )
                    {
                        return;
                    }
                } else if ( !healthy( ) )
                {
                    revoke_and_fastfail( last_burn_reason( ) );
                }

                I( Sleep )( ipc_watcher_period_ms );
            }
        }
    } // namespace

    bool ipc_watcher_startup( )
    {
#if defined( ATLAS_DISABLE_CLOUD )
        return true;
#else
        return atl::create_hidden_thread( &ipc_watcher_entry, nullptr );
#endif
    }

    bool wait_for_initial_handshake( u64 timeout_ms )
    {
#if defined( ATLAS_DISABLE_CLOUD )
        ATLAS_UNUSED_PARAM( timeout_ms );
        return true;
#else
        const u64 started{ I( GetTickCount64 )( ) };
        while ( g_last_heartbeat_nonce.load( ) == 0 )
        {
            if ( !g_poison.load( ) )
            {
                anti_crack_report( last_burn_reason( ) );
                return false;
            }
            if ( I( GetTickCount64 )( ) - started > timeout_ms )
            {
                anti_crack_report( burn_reason_dll_handshake_missing );
                return false;
            }
            I( Sleep )( 50 );
        }
        return true;
#endif
    }
}
