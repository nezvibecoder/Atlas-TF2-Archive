#pragma once

#include "hook/binding.hpp"
#include "hook/inline_hook.hpp"
#include "hook/utility.hpp"
#include "runtime/hash.hpp"
#include "runtime/atomic.hpp"
#include "runtime/mutex.hpp"
#include "runtime/vector.hpp"
#include "util/address.hpp"
#include "util/string.hpp"

inline constexpr u32 api_bootstrap_magic{ 0x42505441u }; // ATPB
inline constexpr u16 api_bootstrap_version{ 5 };
inline constexpr u32 api_bundle_magic{ 0x444C5441u }; // ATLD
inline constexpr u16 api_bundle_schema{ 5 };
inline constexpr u32 api_bundle_header_size{ 200 };
inline constexpr u32 api_module_hash_size{ 32 };
inline constexpr u32 api_ntoskrnl_hash_size{ 32 };
inline constexpr u32 api_max_bundle_size{ 512 * 1024 };
inline constexpr u32 api_max_runtime_flags{ 64 };
inline constexpr u32 api_max_offsets{ 2048 };
inline constexpr u32 api_max_signatures{ 512 };
inline constexpr u32 api_max_hooks{ 128 };
inline constexpr u16 api_invalid_index{ 1024 };
inline constexpr u32 api_max_user_size{ 33 };
inline constexpr u32 api_hook_type_inline_x64{ 1 };
inline constexpr u32 api_hook_type_vfunc_x64{ 2 };
inline constexpr u32 api_hook_type_inline_x86{ 3 };
inline constexpr u32 api_hook_type_vfunc_x86{ 4 };

#if defined( __x86_64__ )
inline constexpr u32 api_hook_type_inline{ api_hook_type_inline_x64 };
inline constexpr u32 api_hook_type_vfunc{ api_hook_type_vfunc_x64 };
#else
inline constexpr u32 api_hook_type_inline{ api_hook_type_inline_x86 };
inline constexpr u32 api_hook_type_vfunc{ api_hook_type_vfunc_x86 };
#endif

enum class api_module : u32
{
    invalid_module = 0,

    client         = 1,
    engine         = 2,
    studiorender   = 3,
    vgui2          = 4,
    vguimatsurface = 5,
    tier0          = 6,
    materialsystem = 7,
    vstdlib        = 8,
    vphysics       = 9,
    shaderapidx9   = 10,
    steamclient64  = 11,
    gameoverlay    = 12,
    // Insert new modules here.

    module_max,
};

enum class api_signature_resolve : u8
{
    address  = 0,
    relative = 1,
};

#pragma pack( push, 4 )
struct api_bootstrap
{
    u32                     m_magic{ };
    u16                     m_version{ };
    u16                     m_size{ };
    const void             *m_bundle{ };
    u32                     m_bundle_size{ };
    u32                     m_hook_binding_count{ };
    u8                      m_artifact_sha256[ 32 ]{ };
    u8                      m_session_id[ 16 ]{ };
    const api_hook_binding *m_hook_bindings{ };
    u64                     m_anchor_perf_counter{ };
    u64                     m_perf_frequency{ };
    u64                     m_expiry_perf_ticks{ };
    u64                     m_anchor_wall_seconds{ };
    const void             *m_heartbeat_region{ };
    u64                     m_heartbeat_key_tag{ };
    volatile u32            m_status{ };
    volatile u32            m_result{ };
};
#pragma pack( pop )

struct api_bundle_header
{
    u32 m_magic{ };
    u16 m_schema_version{ };
    u16 m_header_size{ };
    u32 m_total_size{ };
    u32 m_product{ };
    u32 m_channel{ };
    u32 m_reserved{ };
    u64 m_generation{ };
    u64 m_issued_at_unix{ };
    u64 m_expires_at_unix{ };
    u8  m_artifact_sha256[ 32 ]{ };
    u8  m_session_id[ 16 ]{ };
    u32 m_metadata_offset{ };
    u32 m_runtime_flags_offset{ };
    u32 m_runtime_flags_count{ };
    u32 m_offsets_offset{ };
    u32 m_offsets_count{ };
    u32 m_signatures_offset{ };
    u32 m_signatures_count{ };
    u32 m_data_offset{ };
    u32 m_data_size{ };
    u32 m_hooks_offset{ };
    u32 m_hooks_count{ };
    u32 m_module_hashes_offset{ };
    u32 m_module_hashes_count{ };
    u32 m_reserved_tail{ };
    u32 m_kernel_offsets_offset{ };
    u32 m_kernel_offsets_size{ };
    u8  m_ntoskrnl_hash[ api_ntoskrnl_hash_size ]{ };
    u64 m_reserved_schema5{ };
};

struct api_offset_record
{
    u32 m_id{ };
    u32 m_reserved{ };
    u64 m_value{ };
};

struct api_build_metadata
{
    u32 m_size{ };
    u32 m_game_build{ };
    u32 m_module_revision{ };
    u32 m_minimum_loader_version{ };
    u32 m_minimum_driver_version{ };
    u32 m_minimum_driver_protocol{ };
    u64 m_published_at_unix{ };
    u32 m_local_hooks{ };
    u32 m_reserved[ 7 ]{ };
};

#pragma pack( push, 4 )
struct api_signature_record
{
    u32                   m_id{ };
    api_module            m_module{ };
    u64                   m_encoded_rva{ };
    api_signature_resolve m_resolve{ };
    u8                    m_dereference_count{ };
    u16                   m_pad{ };
    i32                   m_addend{ };
    u32                   m_reserved{ };
};
#pragma pack( pop )
static_assert( sizeof( api_signature_record ) == 28 );
static_assert( __builtin_offsetof( api_signature_record, m_encoded_rva ) == 8 );
static_assert( __builtin_offsetof( api_signature_record, m_resolve ) == 16 );
static_assert( __builtin_offsetof( api_signature_record, m_dereference_count ) == 17 );
static_assert( __builtin_offsetof( api_signature_record, m_addend ) == 20 );

struct api_hook_record
{
    u32 m_id{ };
    u32 m_signature_id{ };
    u32 m_type{ };
    u32 m_target{ };
};

static_assert( sizeof( api_bootstrap ) == ( sizeof( void * ) == 8 ? 136 : 124 ) );
static_assert( __builtin_offsetof( api_bootstrap, m_hook_bindings ) == ( sizeof( void * ) == 8 ? 72 : 68 ) );
static_assert( __builtin_offsetof( api_bootstrap, m_anchor_perf_counter ) == ( sizeof( void * ) == 8 ? 80 : 72 ) );
static_assert( __builtin_offsetof( api_bootstrap, m_heartbeat_region ) == ( sizeof( void * ) == 8 ? 112 : 104 ) );
static_assert( __builtin_offsetof( api_bootstrap, m_heartbeat_key_tag ) == ( sizeof( void * ) == 8 ? 120 : 108 ) );
static_assert( __builtin_offsetof( api_bootstrap, m_status ) == ( sizeof( void * ) == 8 ? 128 : 116 ) );
static_assert( sizeof( api_bundle_header ) == api_bundle_header_size );
static_assert( sizeof( api_build_metadata ) == 64 );
static_assert( sizeof( api_offset_record ) == 16 );
static_assert( sizeof( api_signature_record ) == 28 );
static_assert( sizeof( api_hook_record ) == 16 );

class api_hook
{
public:
    api_hook( ) = default;

    api_hook( const api_hook & )            = delete;
    api_hook &operator=( const api_hook & ) = delete;
    api_hook( api_hook && )                 = default;
    api_hook &operator=( api_hook && )      = default;

    explicit api_hook( void *original ) : m_original( original )
    {
    }

    api_hook( void *target, void *destination ) : m_local( atlashook::inline_hook::create(
                                                      target, destination, atlashook::inline_hook::start_disabled ) )
    {
    }

    bool enable( )
    {
        return this->m_local ? this->m_local.enable( ) : static_cast< bool >( this->m_original );
    }

    void reset( )
    {
        this->m_local.reset( );
        this->m_original.reset( );
    }

    template< typename ret = void, typename... args >
    ATLAS_FORCE_INLINE ret stdcall( args... arguments ) const
    {
        using function_t = ret( ATLAS_STDCALL * )( args... );
        function_t function{ reinterpret_cast< function_t >( this->original( ) ) };
        return atlashook::detail::call_or_default< ret >( function, arguments... );
    }

    template< typename ret = void, typename... args >
    ATLAS_FORCE_INLINE ret thiscall( args... arguments ) const
    {
        using function_t = ret( ATLAS_THISCALL * )( args... );
        function_t function{ reinterpret_cast< function_t >( this->original( ) ) };
        return atlashook::detail::call_or_default< ret >( function, arguments... );
    }

    template< typename ret = void, typename... args >
    ATLAS_FORCE_INLINE ret ccall( args... arguments ) const
    {
        using function_t = ret( ATLAS_CDECL * )( args... );
        function_t function{ reinterpret_cast< function_t >( this->original( ) ) };
        return atlashook::detail::call_or_default< ret >( function, arguments... );
    }

    template< typename ret = void, typename... args >
    ATLAS_FORCE_INLINE ret fastcall( args... arguments ) const
    {
        using function_t = ret( ATLAS_FASTCALL * )( args... );
        function_t function{ reinterpret_cast< function_t >( this->original( ) ) };
        return atlashook::detail::call_or_default< ret >( function, arguments... );
    }

    template< typename ret = void, typename... args >
    ATLAS_FORCE_INLINE ret vectorcall( args... arguments ) const
    {
        using function_t = ret( ATLAS_VECTORCALL * )( args... );
        function_t function{ reinterpret_cast< function_t >( this->original( ) ) };
        return atlashook::detail::call_or_default< ret >( function, arguments... );
    }

    explicit ATLAS_FORCE_INLINE operator bool( ) const
    {
        return static_cast< bool >( this->m_local ) || static_cast< bool >( this->m_original );
    }

    ATLAS_FORCE_INLINE void *original( ) const
    {
        return this->m_local ? this->m_local.original< void * >( ) : this->m_original.get( );
    }

private:
    atlashook::inline_hook                       m_local{ };
    atlashook::detail::encoded_pointer< void * > m_original{ };
};

class api
{
public:
    bool startup( void *bootstrap );
    bool release_runtime_package( );

    bool cache_module( api_module module );
    void cache_modules( );

    util::address get_signature_pkg( hash_t pkg ) const;
    u16           get_index_pkg( hash_t pkg ) const;
    api_hook      get_hook_pkg( hash_t pkg, void *destination, void *interface_out = nullptr ) const;

    const char *user( ) const
    {
        return this->m_user;
    }

    bool has_config_token( ) const
    {
        return this->m_has_config_token;
    }

    const api_bootstrap *bootstrap( ) const
    {
        return this->m_bootstrap;
    }

    const u8 *session_id( ) const
    {
        return this->m_session_id;
    }

    void clear_credentials( );
    bool replace_config_token( string_view token );

    bool request( const wchar_t       *verb,
                  const wchar_t       *path,
                  string_view          body,
                  atl::vector< char > &output,
                  usize                limit,
                  u32                 &status ) const;

private:
    bool valid_bundle( const api_bootstrap *bootstrap ) const;

    const api_bootstrap        *m_bootstrap{ };
    const api_bundle_header    *m_header{ };
    const api_build_metadata   *m_metadata{ };
    const api_offset_record    *m_offsets{ };
    const api_signature_record *m_signatures{ };
    const api_hook_record      *m_hooks{ };
    const u8                   *m_data{ };
    u8                          m_session_id[ 16 ]{ };
    u8                          m_artifact_sha256[ 32 ]{ };
    char                        m_user[ api_max_user_size ]{ };
    char                        m_config_token[ 257 ]{ };
    atl::atomic< bool >          m_has_config_token{ };
    mutable atl::mutex          m_token_lock{ };
    void                       *m_http_library{ };
};

#if defined( ATLAS_DISABLE_CLOUD )
struct development_hook
{
    development_hook( ) = default;
    constexpr development_hook( u32 id, u16 idx ) :
        m_is_index_hook( true ), m_module( api_module::client ), m_signature( nullptr ), m_dereference_amount( 0 ), m_is_relative( false ), m_disp( 0 ), m_ip( 0 ), m_index( idx ), m_id( id )
    {
    }
    constexpr development_hook( u32 id, api_module module, const char *sig, i32 deref_count = 0, bool is_relative = false, i32 disp = 0, i32 ip = 0 ) :
        m_is_index_hook( false ), m_module( module ), m_signature( sig ), m_dereference_amount( deref_count ), m_is_relative( is_relative ), m_disp( disp ), m_ip( ip ), m_index( 1024 ), m_id( id )
    {
    } // i'm mentally ill

    bool        m_is_index_hook{ false };
    api_module  m_module{ api_module::client };
    const char *m_signature{ nullptr };
    i32         m_dereference_amount{ 0 };
    bool        m_is_relative{ false };
    i32         m_disp{ 0 };
    i32         m_ip{ 0 };
    u16         m_index{ 0 };

    u32 m_id{ };
};

struct development_index
{
    u16 m_index{ api_invalid_index };
    u32 m_id{ };
};

struct development_signature
{
    api_module  m_module{ };
    const char *m_signature{ };
    i32         m_dereference_amount{ 0 };
    bool        m_is_relative{ false };
    i32         m_disp{ 0 };
    i32         m_ip{ 0 };
    u32         m_id{ 0x0 };
};

extern const development_index     *g_dev_indices;
extern const usize                  g_dev_index_count;
extern const development_hook      *g_dev_hooks;
extern const usize                  g_dev_hook_count;
extern const development_signature *g_dev_signatures;
extern const usize                  g_dev_signature_count;
#endif
