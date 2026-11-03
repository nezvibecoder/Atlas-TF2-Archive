#include "api.hpp"

#include "runtime/integrity.hpp"
#include "runtime/obfuscation.hpp"
#include "runtime/unordered_map.hpp"
#include "util/pe_module.hpp"
#include "util/string.hpp"

namespace
{
    u64 signature_key( u32 id, const u8 session_id[ 16 ], const u8 artifact_sha256[ 32 ] )
    {
        u64 mixed{ ( static_cast< u64 >( id ) << 32u ) ^ static_cast< u64 >( id ) ^ 0x8F3A95E12D7B46C5ull };
        for ( usize index{ }; index < 16; ++index )
        {
            mixed = atlas_integrity::mix64(
                mixed ^ static_cast< u64 >( session_id[ index ] ) ^
                ( static_cast< u64 >( index + 1 ) * 0x9E3779B185EBCA87ull ) );
        }
        for ( usize index{ }; index < 32; ++index )
        {
            mixed = atlas_integrity::mix64(
                mixed ^ static_cast< u64 >( artifact_sha256[ index ] ) ^
                ( static_cast< u64 >( index + 17 ) * 0xC2B2AE3D27D4EB4Full ) );
        }
        mixed = atlas_integrity::mix64( mixed );
        return mixed ? mixed : 0xC73D15AE2894B07Full;
    }
}

// TODO: Move me to importer lib.
extern "C"
{
    void *ATLAS_STDCALL WinHttpOpen( const wchar_t *, u32, const wchar_t *, const wchar_t *, u32 );
    i32 ATLAS_STDCALL   WinHttpSetTimeouts( void *, i32, i32, i32, i32 );
    i32 ATLAS_STDCALL   WinHttpSetOption( void *, u32, void *, u32 );
    void *ATLAS_STDCALL WinHttpConnect( void *, const wchar_t *, u16, u32 );
    void *ATLAS_STDCALL WinHttpOpenRequest( void *, const wchar_t *, const wchar_t *,
                                            const wchar_t *, const wchar_t *, const wchar_t **, u32 );
    i32 ATLAS_STDCALL   WinHttpSendRequest( void *, const wchar_t *, u32, void *, u32, u32, usize );
    i32 ATLAS_STDCALL   WinHttpReceiveResponse( void *, void * );
    i32 ATLAS_STDCALL   WinHttpQueryHeaders( void *, u32, const wchar_t *, void *, u32 *, u32 * );
    i32 ATLAS_STDCALL   WinHttpReadData( void *, void *, u32, u32 * );
    i32 ATLAS_STDCALL   WinHttpCloseHandle( void * );
}

static bool equal_bytes( const u8 *left, const u8 *right, u32 size )
{
    if ( !left || !right )
    {
        return false;
    }

    u8 difference{ };
    for ( u32 index{ }; index < size; ++index )
    {
        difference |= left[ index ] ^ right[ index ];
    }
    return difference == 0;
}

static bool valid_span( u32 total, u32 offset, u32 count, u32 element_size )
{
    const u64 size{ static_cast< u64 >( count ) * element_size };
    return offset <= total && size <= static_cast< u64 >( total - offset );
}

static util::pe_module package_module( api_module module )
{
    switch ( module )
    {
        case api_module::client: return util::find_module( ATLAS_XORWSTR( L"client.dll" ).get( ) );
        case api_module::engine: return util::find_module( ATLAS_XORWSTR( L"engine.dll" ).get( ) );
        case api_module::studiorender: return util::find_module( ATLAS_XORWSTR( L"studiorender.dll" ).get( ) );
        case api_module::vgui2: return util::find_module( ATLAS_XORWSTR( L"vgui2.dll" ).get( ) );
        case api_module::vguimatsurface: return util::find_module( ATLAS_XORWSTR( L"vguimatsurface.dll" ).get( ) );
        case api_module::tier0: return util::find_module( ATLAS_XORWSTR( L"tier0.dll" ).get( ) );
        case api_module::materialsystem: return util::find_module( ATLAS_XORWSTR( L"materialsystem.dll" ).get( ) );
        case api_module::vstdlib: return util::find_module( ATLAS_XORWSTR( L"vstdlib.dll" ).get( ) );
        case api_module::vphysics: return util::find_module( ATLAS_XORWSTR( L"vphysics.dll" ).get( ) );
        case api_module::shaderapidx9: return util::find_module( ATLAS_XORWSTR( L"shaderapidx9.dll" ).get( ) );
#if defined( __x86_64__ )
        case api_module::steamclient64: return util::find_module( ATLAS_XORWSTR( L"steamclient64.dll" ).get( ) );
        case api_module::gameoverlay: return util::find_module( ATLAS_XORWSTR( L"gameoverlayrenderer64.dll" ).get( ) );
#else
        case api_module::steamclient64: return util::find_module( ATLAS_XORWSTR( L"steamclient.dll" ).get( ) );
        case api_module::gameoverlay: return util::find_module( ATLAS_XORWSTR( L"gameoverlayrenderer.dll" ).get( ) );
#endif
        default: return util::pe_module{ nullptr };
    }
}

ATLAS_NO_DESTROY static atl::unordered_map< u32, util::pe_module > module_cache{ };

bool api::startup( void *bootstrap )
{
    this->m_bootstrap  = nullptr;
    this->m_header     = nullptr;
    this->m_metadata   = nullptr;
    this->m_offsets    = nullptr;
    this->m_signatures = nullptr;
    this->m_hooks      = nullptr;
    this->m_data       = nullptr;
    this->m_user[ 0 ]  = 0;
    this->clear_credentials( );

#if defined( ATLAS_DISABLE_CLOUD )
    ATLAS_UNUSED_PARAM( bootstrap );
    str::copy( this->m_user, sizeof( this->m_user ), ATLAS_XORSTR( "dev" ).get( ) );
    return true;
#else
    const auto *input{ static_cast< const api_bootstrap * >( bootstrap ) };
    if ( !this->valid_bundle( input ) )
    {
        return false;
    }

    const auto *bytes{ static_cast< const u8 * >( input->m_bundle ) };
    this->m_bootstrap  = input;
    this->m_header     = reinterpret_cast< const api_bundle_header * >( bytes );
    this->m_metadata   = reinterpret_cast< const api_build_metadata * >( bytes + this->m_header->m_metadata_offset );
    this->m_offsets    = reinterpret_cast< const api_offset_record * >( bytes + this->m_header->m_offsets_offset );
    this->m_signatures = reinterpret_cast< const api_signature_record * >( bytes + this->m_header->m_signatures_offset );
    this->m_hooks      = reinterpret_cast< const api_hook_record * >( bytes + this->m_header->m_hooks_offset );
    this->m_data       = bytes + this->m_header->m_data_offset;

    atl::memcpy( this->m_session_id, input->m_session_id, sizeof( this->m_session_id ) );
    atl::memcpy( this->m_artifact_sha256, input->m_artifact_sha256, sizeof( this->m_artifact_sha256 ) );

    for ( u32 index{ }; index < this->m_header->m_offsets_count; ++index )
    {
        const api_offset_record &record{ this->m_offsets[ index ] };
        if ( record.m_id != HASH_CT( "atlas.runtime.user" ) || record.m_value >= this->m_header->m_data_size )
        {
            continue;
        }

        const char *name{ reinterpret_cast< const char * >( this->m_data + record.m_value ) };
        str::copy( this->m_user, sizeof( this->m_user ), name,
                   str::bounded_length( name, this->m_header->m_data_size - record.m_value ) );
        break;
    }
    for ( u32 index{ }; index < this->m_header->m_offsets_count; ++index )
    {
        const api_offset_record &record{ this->m_offsets[ index ] };
        if ( record.m_id != HASH_CT( "atlas.runtime.config_token" ) || record.m_value >= this->m_header->m_data_size )
        {
            continue;
        }
        const char *token{ reinterpret_cast< const char * >( this->m_data + record.m_value ) };
        const usize length{ str::bounded_length( token, this->m_header->m_data_size - record.m_value ) };
        if ( length < sizeof( this->m_config_token ) && length < this->m_header->m_data_size - record.m_value )
        {
            str::copy( this->m_config_token, sizeof( this->m_config_token ), token, length );
        }
        break;
    }
    this->m_http_library     = I( LoadLibraryExA )( ATLAS_XORSTR( "winhttp.dll" ).get( ), nullptr, 0x800 );
    this->m_has_config_token = this->m_config_token[ 0 ] != 0;
    if ( !this->m_http_library || !this->m_has_config_token )
    {
        this->clear_credentials( );
        return false;
    }

    atlas_integrity::capture( input->m_session_id, input->m_artifact_sha256 );
    atlas_integrity::capture_time_anchor( input->m_anchor_perf_counter,
                                          input->m_perf_frequency,
                                          input->m_expiry_perf_ticks,
                                          input->m_anchor_wall_seconds );
    atlas_integrity::capture_heartbeat_context( input->m_heartbeat_region,
                                                input->m_heartbeat_key_tag );
    return true;
#endif
}

void api::clear_credentials( )
{
    atl::scoped_lock guard{ this->m_token_lock };
    this->m_has_config_token = false;
    // atl::memset uses volatile assembly, so dead-store elimination cannot remove this wipe.
    atl::memset( this->m_config_token, 0, sizeof( this->m_config_token ) );
}

bool api::replace_config_token( string_view token )
{
    if ( token.empty( ) || token.size( ) >= sizeof( this->m_config_token ) )
    {
        return false;
    }
    for ( const char c : token )
    {
        if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
                ( c >= '0' && c <= '9' ) || c == '-' || c == '_' || c == '.' ) )
        {
            return false;
        }
    }
    atl::scoped_lock guard{ this->m_token_lock };
    if ( !this->m_has_config_token )
    {
        return false;
    }
    atl::memset( this->m_config_token, 0, sizeof( this->m_config_token ) );
    str::copy( this->m_config_token, sizeof( this->m_config_token ), token.data( ), token.size( ) );
    return true;
}

ATLAS_FORCE_INLINE bool wipe_package_region( const void *address, usize size )
{
    void *region{ const_cast< void * >( address ) };
    usize region_size{ size };
    u32   old_protection{ };
    if ( I( NtProtectVirtualMemory )( util::get_current_process( ), &region, &region_size,
                                      PAGE_READWRITE, &old_protection ) < 0 )
    {
        return false;
    }
    atl::memset( const_cast< void * >( address ), 0, size );
    u32 ignored{ };
    return I( NtProtectVirtualMemory )( util::get_current_process( ), &region, &region_size,
                                        old_protection, &ignored ) >= 0;
}

bool api::release_runtime_package( )
{
#if !defined( ATLAS_DISABLE_CLOUD )
    // The driver maps these pages read-only. All package lookups and binding decodes are complete.
    const bool bundle_wiped{ wipe_package_region( this->m_bootstrap->m_bundle, this->m_bootstrap->m_bundle_size ) };
    const bool bindings_wiped{ !this->m_bootstrap->m_hook_binding_count ||
                               wipe_package_region( this->m_bootstrap->m_hook_bindings,
                                                    this->m_bootstrap->m_hook_binding_count * sizeof( api_hook_binding ) ) };

    const bool descriptor_wiped{
        wipe_package_region( this->m_bootstrap, sizeof( api_bootstrap ) ) };
    this->m_bootstrap  = nullptr;
    this->m_header     = nullptr;
    this->m_metadata   = nullptr;
    this->m_offsets    = nullptr;
    this->m_signatures = nullptr;
    this->m_hooks      = nullptr;
    this->m_data       = nullptr;
    return bundle_wiped && bindings_wiped && descriptor_wiped;
#else
    return true;
#endif
}

bool api::cache_module( api_module module )
{
    const u32 id{ static_cast< u32 >( module ) };
    if ( id <= static_cast< u32 >( api_module::invalid_module ) ||
         id >= static_cast< u32 >( api_module::module_max ) )
    {
        return false;
    }

    const util::pe_module *cached{ module_cache.get( id ) };
    if ( cached )
    {
        return !cached->is_null( ) && cached->is_valid( );
    }

    util::pe_module resolved{ package_module( module ) };
    if ( resolved.is_null( ) || !resolved.is_valid( ) )
    {
        return false;
    }

    return module_cache.insert( id, resolved ) != nullptr;
}

void api::cache_modules( )
{
    for ( u32 i{ 1 }; i < ( u32 )api_module::module_max; ++i )
    {
        while ( !this->cache_module( static_cast< api_module >( i ) ) )
        {
            _mm_pause( );
        }
    }
}

bool api::valid_bundle( const api_bootstrap *bootstrap ) const
{
    if ( !bootstrap || bootstrap->m_magic != api_bootstrap_magic ||
         bootstrap->m_version != api_bootstrap_version ||
         bootstrap->m_size != sizeof( api_bootstrap ) || !bootstrap->m_bundle ||
         bootstrap->m_bundle_size < api_bundle_header_size ||
         bootstrap->m_bundle_size > api_max_bundle_size )
    {
        return false;
    }

    const auto *header{ static_cast< const api_bundle_header * >( bootstrap->m_bundle ) };
    if ( header->m_magic != api_bundle_magic ||
         header->m_schema_version != api_bundle_schema ||
         header->m_header_size != api_bundle_header_size ||
         header->m_total_size != bootstrap->m_bundle_size ||
         ( header->m_product != 1 && header->m_product != 2 && header->m_product != 3 ) ||
         header->m_generation == 0 || header->m_expires_at_unix <= header->m_issued_at_unix ||
         header->m_reserved || header->m_reserved_tail ||
         !equal_bytes( header->m_artifact_sha256, bootstrap->m_artifact_sha256, 32 ) ||
         !equal_bytes( header->m_session_id, bootstrap->m_session_id, 16 ) ||
         header->m_offsets_count > api_max_offsets ||
         header->m_runtime_flags_count > api_max_runtime_flags ||
         header->m_signatures_count > api_max_signatures ||
         header->m_hooks_count > api_max_hooks ||
         !valid_span( header->m_total_size, header->m_metadata_offset,
                      1, sizeof( api_build_metadata ) ) ||
         !valid_span( header->m_total_size, header->m_runtime_flags_offset,
                      header->m_runtime_flags_count, 16 ) ||
         !valid_span( header->m_total_size, header->m_offsets_offset,
                      header->m_offsets_count, sizeof( api_offset_record ) ) ||
         !valid_span( header->m_total_size, header->m_signatures_offset,
                      header->m_signatures_count, sizeof( api_signature_record ) ) ||
         !valid_span( header->m_total_size, header->m_hooks_offset,
                      header->m_hooks_count, sizeof( api_hook_record ) ) ||
         !valid_span( header->m_total_size, header->m_data_offset,
                      header->m_data_size, 1 ) )
    {
        return false;
    }

    if ( header->m_channel != 1 && header->m_channel != 2 )
    {
        return false;
    }

    const auto *bytes{ static_cast< const u8 * >( bootstrap->m_bundle ) };
    const auto *metadata{ reinterpret_cast< const api_build_metadata * >(
        bytes + header->m_metadata_offset ) };
    if ( metadata->m_size != sizeof( api_build_metadata ) || !metadata->m_game_build ||
         !metadata->m_module_revision || !metadata->m_published_at_unix ||
         metadata->m_published_at_unix > header->m_issued_at_unix ||
         metadata->m_local_hooks > 1 ||
         ( metadata->m_local_hooks && bootstrap->m_hook_binding_count ) ||
         ( !metadata->m_local_hooks &&
           bootstrap->m_hook_binding_count != header->m_hooks_count ) ||
         ( bootstrap->m_hook_binding_count && !bootstrap->m_hook_bindings ) )
    {
        return false;
    }
    for ( u32 index{ }; index < 7; ++index )
    {
        if ( metadata->m_reserved[ index ] )
        {
            return false;
        }
    }

    const auto *offsets{ reinterpret_cast< const api_offset_record * >(
        bytes + header->m_offsets_offset ) };
    for ( u32 index{ }; index < header->m_offsets_count; ++index )
    {
        if ( !offsets[ index ].m_id || offsets[ index ].m_reserved )
        {
            return false;
        }
    }

    const auto *signatures{ reinterpret_cast< const api_signature_record * >(
        bytes + header->m_signatures_offset ) };
    for ( u32 index{ }; index < header->m_signatures_count; ++index )
    {
        const api_signature_record &signature{ signatures[ index ] };
        if ( !signature.m_id || signature.m_module < api_module::client ||
             signature.m_module >= api_module::module_max ||
             signature.m_resolve > api_signature_resolve::relative ||
             signature.m_dereference_count > 4 || signature.m_pad || signature.m_reserved )
        {
            return false;
        }
    }

    const auto *hooks{ reinterpret_cast< const api_hook_record * >(
        bytes + header->m_hooks_offset ) };
    for ( u32 index{ }; index < header->m_hooks_count; ++index )
    {
        const api_hook_record &record{ hooks[ index ] };
        if ( !record.m_id || record.m_target > api_invalid_index )
        {
            return false;
        }

        if ( record.m_type == api_hook_type_inline )
        {
            continue;
        }

        // vfunc resolves against a caller-supplied interface at runtime; the loader
        // cannot pre-decode it, so it is only valid alongside local_hooks metadata
        if ( record.m_type == api_hook_type_vfunc &&
             metadata->m_local_hooks && !record.m_signature_id && record.m_target )
        {
            continue;
        }

        return false;
    }
    return true;
}

util::address api::get_signature_pkg( hash_t pkg ) const
{
#if defined( ATLAS_DISABLE_CLOUD )
    for ( usize entry{ }; entry < g_dev_signature_count; ++entry )
    {
        const development_signature &signature_pkg{ g_dev_signatures[ entry ] };
        if ( signature_pkg.m_id == pkg )
        {
            const util::pe_module *module{ module_cache.get( static_cast< u32 >( signature_pkg.m_module ) ) };
            if ( !module || module->is_null( ) || !module->is_valid( ) )
            {
                return util::address{ nullptr };
            }

            auto sig_address{ module->find_pattern( signature_pkg.m_signature ) };
            if ( sig_address.is_null( ) )
            {
                char buf[ 512 ]{ };
                atl::format( buf, sizeof( buf ), "This pattern failed to be found:\n\n%s", signature_pkg.m_signature );
                I( MessageBoxA )( NULL, buf, "SIGNATURE FAILED", MB_ICONERROR );
                return sig_address;
            }

            if ( signature_pkg.m_is_relative )
            {
                sig_address.rel2abs( signature_pkg.m_disp, signature_pkg.m_ip );
            }

            if ( signature_pkg.m_dereference_amount > 0 )
            {
                for ( i32 i{ }; i < signature_pkg.m_dereference_amount; ++i )
                {
                    sig_address.deref( );
                }
            }

            return sig_address;
        }
    }
#else
    if ( !this->m_header || !this->m_bootstrap || !pkg )
    {
        return util::address{ nullptr };
    }

    for ( u32 index{ }; index < this->m_header->m_signatures_count; ++index )
    {
        const api_signature_record &signature{ this->m_signatures[ index ] };
        if ( signature.m_id != pkg )
        {
            continue;
        }

        const util::pe_module *module{ module_cache.get( static_cast< u32 >( signature.m_module ) ) };
        if ( !module || module->is_null( ) || !module->is_valid( ) )
        {
            return util::address{ nullptr };
        }
        const IMAGE_NT_HEADERS *nt{ module->get_nt_header( ) };
        if ( !nt )
        {
            return util::address{ nullptr };
        }

        const u64 key{ signature_key(
            signature.m_id, this->m_bootstrap->m_session_id, this->m_bootstrap->m_artifact_sha256 ) };
        const u64 rva{ signature.m_encoded_rva ^ key };
        if ( rva > nt->OptionalHeader.SizeOfImage )
        {
            return util::address{ nullptr };
        }

        uptr cursor{ static_cast< uptr >(
            static_cast< i64 >( reinterpret_cast< uptr >( module->get_dos_header( ) ) + rva ) +
            signature.m_addend ) };
        if ( signature.m_resolve == api_signature_resolve::relative )
        {
            if ( !module->contains( reinterpret_cast< void * >( cursor ), sizeof( i32 ) ) )
            {
                return util::address{ nullptr };
            }
            const i32 displacement{ *reinterpret_cast< const i32 * >( cursor ) };
            cursor = static_cast< uptr >(
                static_cast< i64 >( cursor + sizeof( i32 ) ) + displacement );
        }

        for ( u8 dereference{ }; dereference < signature.m_dereference_count; ++dereference )
        {
            if ( !cursor )
            {
                return util::address{ nullptr };
            }
            cursor = *reinterpret_cast< const uptr * >( cursor );
        }
        return util::address{ cursor };
    }
#endif // ATLAS_DEV
    return util::address{ nullptr };
}

u16 api::get_index_pkg( hash_t pkg ) const
{
#if defined( ATLAS_DISABLE_CLOUD )
    for ( usize entry{ }; entry < g_dev_index_count; ++entry )
    {
        const development_index &index{ g_dev_indices[ entry ] };
        if ( index.m_id == pkg )
        {
            return index.m_index;
        }
    }
#else
    if ( this->m_header && pkg )
    {
        for ( u32 index{ }; index < this->m_header->m_offsets_count; ++index )
        {
            if ( this->m_offsets[ index ].m_id == pkg &&
                 this->m_offsets[ index ].m_value < api_invalid_index )
            {
                return static_cast< u16 >( this->m_offsets[ index ].m_value );
            }
        }
    }
#endif
    return api_invalid_index;
}

api_hook api::get_hook_pkg( hash_t pkg, void *destination, void *interface_out ) const
{
#if defined( ATLAS_DISABLE_CLOUD )
    for ( usize entry{ }; entry < g_dev_hook_count; ++entry )
    {
        const development_hook &hook_pkg{ g_dev_hooks[ entry ] };
        if ( hook_pkg.m_id == pkg )
        {
            if ( hook_pkg.m_is_index_hook )
            {
                return api_hook{ util::get_virtual( interface_out, hook_pkg.m_index ), destination };
            } else
            {
                auto module{ module_cache.find( ( u32 )hook_pkg.m_module )->value };
                if ( module.is_null( ) || !module.is_valid( ) )
                {
                    return api_hook{ };
                }

                auto sig_address{ module.find_pattern( hook_pkg.m_signature ) };
                if ( sig_address.is_null( ) )
                {
                    char buf[ 512 ]{ };
                    atl::format( buf, sizeof( buf ), "This hook failed to be found:\n\n%s", hook_pkg.m_signature );
                    I( MessageBoxA )( NULL, buf, "HOOK FAILED", MB_ICONERROR );
                    return api_hook{ };
                }

                if ( hook_pkg.m_is_relative )
                {
                    sig_address.rel2abs( hook_pkg.m_disp, hook_pkg.m_ip );
                }

                if ( hook_pkg.m_dereference_amount > 0 )
                {
                    for ( i32 i{ }; i < hook_pkg.m_dereference_amount; ++i )
                    {
                        sig_address.deref( );
                    }
                }

                return api_hook{ sig_address.get_as< void * >( ), destination };
            }
        }
    }
#else
    if ( !this->m_bootstrap || !this->m_header || !this->m_metadata ||
         !pkg || !destination )
    {
        return api_hook{ };
    }

    const api_hook_record *hook{ };
    for ( u32 index{ }; index < this->m_header->m_hooks_count; ++index )
    {
        if ( this->m_hooks[ index ].m_id == pkg )
        {
            hook = &this->m_hooks[ index ];
            break;
        }
    }
    if ( !hook )
    {
        return api_hook{ };
    }

    if ( this->m_metadata->m_local_hooks )
    {
        for ( u32 index{ }; index < this->m_bootstrap->m_hook_binding_count; ++index )
        {
            const api_hook_binding &binding{ this->m_bootstrap->m_hook_bindings[ index ] };
            if ( binding.id == pkg && !binding.reserved )
            {
                void *target{ atlashook::decode_original(
                    &binding, this->m_bootstrap->m_session_id,
                    this->m_bootstrap->m_artifact_sha256 ) };
                return util::is_executable_address( target )
                           ? api_hook{ target, destination }
                           : api_hook{ };
            }
        }

        if ( hook->m_type == api_hook_type_vfunc )
        {
            void *target{ interface_out
                              ? util::get_virtual( interface_out,
                                                   static_cast< u16 >( hook->m_target - 1 ) )
                              : nullptr };
            return util::is_executable_address( target )
                       ? api_hook{ target, destination }
                       : api_hook{ };
        }

        if ( !hook->m_signature_id )
        {
            return api_hook{ };
        }

        util::address resolved{ this->get_signature_pkg( hook->m_signature_id ) };
        void         *target{ reinterpret_cast< void * >( resolved.get_raw( ) ) };
        if ( hook->m_target )
        {
            target = target ? util::get_virtual(
                                  target, static_cast< u16 >( hook->m_target - 1 ) )
                            : nullptr;
        }
        return util::is_executable_address( target )
                   ? api_hook{ target, destination }
                   : api_hook{ };
    }

    for ( u32 index{ }; index < this->m_bootstrap->m_hook_binding_count; ++index )
    {
        const api_hook_binding &binding{ this->m_bootstrap->m_hook_bindings[ index ] };
        if ( binding.id == pkg && !binding.reserved )
        {
            void *original{ atlashook::decode_original(
                &binding, this->m_bootstrap->m_session_id,
                this->m_bootstrap->m_artifact_sha256 ) };
            return util::is_executable_address( original ) ? api_hook{ original } : api_hook{ };
        }
    }
#endif
    return api_hook{ };
}

bool api::request( const wchar_t       *verb,
                   const wchar_t       *path,
                   string_view          body,
                   atl::vector< char > &output,
                   usize                limit,
                   u32                 &status ) const
{
    atl::memset( output.data( ), 0, output.size( ) );
    output.clear( );
    status = 0;
    if ( !this->m_http_library )
    {
        return false;
    }

    wchar_t headers[ 352 ]{ L"Content-Type: application/json\r\nAuthorization: Bearer " };
    usize   at{ sizeof( L"Content-Type: application/json\r\nAuthorization: Bearer " ) / sizeof( wchar_t ) - 1 };
    {
        atl::scoped_lock guard{ this->m_token_lock };
        if ( !this->m_has_config_token )
        {
            return false;
        }
        for ( const char *c{ this->m_config_token }; *c; ++c )
        {
            headers[ at++ ] = *c;
        }
    }
    headers[ at++ ] = L'\r';
    headers[ at ]   = L'\n';

    void *session{ I( WinHttpOpen )( L"Atlas", 4, nullptr, nullptr, 0 ) };
    if ( !session )
    {
        atl::memset( headers, 0, sizeof( headers ) );
        return false;
    }
    I( WinHttpSetTimeouts )( session, 5000, 5000, 5000, 5000 );
    void *connection{ I( WinHttpConnect )( session, L"api.atlascheat.dev", 443, 0 ) };
    void *request{ connection ? I( WinHttpOpenRequest )( connection, verb, path, nullptr, nullptr, nullptr, 0x800000 )
                              : nullptr };

    u32  redirects{ };
    u32  status_size{ sizeof( status ) };
    bool ok{ request && I( WinHttpSetOption )( request, 88, &redirects, sizeof( redirects ) ) &&
             I( WinHttpSendRequest )( request, headers, static_cast< u32 >( -1 ), const_cast< char * >( body.data( ) ),
                                      static_cast< u32 >( body.size( ) ), static_cast< u32 >( body.size( ) ), 0 ) };
    atl::memset( headers, 0, sizeof( headers ) );
    ok = ok && I( WinHttpReceiveResponse )( request, nullptr ) &&
         I( WinHttpQueryHeaders )( request, 19 | 0x20000000, nullptr, &status, &status_size, nullptr );
    char buffer[ 4096 ]{ };
    while ( ok && status >= 200 && status < 300 )
    {
        u32 read{ };
        if ( !I( WinHttpReadData )( request, buffer, sizeof( buffer ), &read ) )
        {
            ok = false;
            break;
        }
        if ( !read )
        {
            break;
        }
        const usize old_size{ output.size( ) };
        if ( read > limit - old_size || !output.resize( old_size + read ) )
        {
            ok = false;
            break;
        }
        for ( u32 i{ }; i < read; ++i )
        {
            output[ old_size + i ] = buffer[ i ];
        }
        atl::memset( buffer, 0, sizeof( buffer ) );
    }
    atl::memset( buffer, 0, sizeof( buffer ) );
    if ( !ok )
    {
        atl::memset( output.data( ), 0, output.size( ) );
        output.clear( );
    }
    if ( request )
    {
        I( WinHttpCloseHandle )( request );
    }
    if ( connection )
    {
        I( WinHttpCloseHandle )( connection );
    }
    I( WinHttpCloseHandle )( session );
    return ok;
}
