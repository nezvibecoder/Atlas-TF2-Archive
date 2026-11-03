#include "movement_recorder.hpp"

#include "config_storage.hpp"
#include "gui/gui.hpp"
#include "gui/notifications.hpp"
#include "render/render.hpp"
#include "runtime/thread.hpp"
#include "util/base64.hpp"
#include "util/format.hpp"

#if !defined( ATLAS_DISABLE_CLOUD )
#include "api/api.hpp"
#endif

#include "event_notifiers.hpp"
#include "ragebot.hpp"

ATLAS_NO_DESTROY movement_recorder g_recorder{ };

constexpr i32 MOVE_BUTTONS{ IN_JUMP | IN_DUCK | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT };

constexpr f32 AIR_SPEED_CAP{ 30.0f };
constexpr f32 DUCK_SPEED_CROP{ 0.33333333f };
constexpr i32 MOVE_DIRECTIONS{ 64 };
constexpr f32 MOVE_WEAKEST{ 0.125f };

constexpr i32   ROUTE_DRAW_SEGMENTS{ 512 };
constexpr usize ROUTE_AHEAD_LIMIT{ 128 };
constexpr u8    ROUTE_PREVIEW_ALPHA{ 82 };
constexpr u8    ROUTE_REMAINDER_ALPHA{ 41 };
constexpr u8    ROUTE_TRAVELED_ALPHA{ 190 };
constexpr u8    ROUTE_EVENT_ALPHA{ 230 };
constexpr u8    ROUTE_PREVIEW_EVENT_ALPHA{ 120 };
constexpr u8    ROUTE_PAST_EVENT_ALPHA{ 64 };
constexpr f32   ROUTE_CARET_SPREAD{ 6.0f };
constexpr f32   ROUTE_CARET_HEIGHT{ 12.0f };
constexpr f32   ROUTE_BURST_SIZE{ 5.0f };

constexpr i32 RING_SEGMENTS{ 32 };
constexpr i32 RING_TICKS{ 12 };
constexpr f64 RING_TURNS_PER_SECOND{ 0.6 };
constexpr f32 RING_TICK_LIT{ 7.0f };
constexpr f32 RING_TICK_DIM{ 3.5f };
constexpr u8  RING_EDGE_ALPHA{ 140 };
constexpr u8  RING_DIM_ALPHA{ 100 };
constexpr f32 END_RING_RADIUS{ 12.0f };

constexpr f32 MARKER_FADE_SHARE{ 0.25f };
constexpr u8  MARKER_IDLE_ALPHA{ 150 };
constexpr u8  MARKER_LEADER_ALPHA{ 90 };
constexpr f32 LABEL_LIFT{ VEC_HULL_MAX.z + 8.0f };
constexpr i32 LABEL_PADDING{ 7 };
constexpr i32 LABEL_PADDING_Y{ 3 };
constexpr i32 LABEL_STRIPE{ 2 };
constexpr i32 LABEL_NAME_ROW{ 15 };
constexpr i32 LABEL_META_ROW{ 11 };
constexpr i32 LABEL_HINT_ROW{ 17 };
constexpr i32 LABEL_CHIP_HEIGHT{ 11 };
constexpr i32 LABEL_CHIP_GAP{ 5 };

constexpr i32 KEY_WIDTH{ 20 };
constexpr i32 KEY_HEIGHT{ 16 };
constexpr i32 KEY_GAP{ 3 };
constexpr i32 KEY_CLUSTER_GAP{ 6 };
constexpr i32 KEY_CLUSTER_WIDTH{ KEY_WIDTH * 3 + KEY_GAP * 2 };
constexpr i32 KEY_TRACK_GAP{ 6 };
constexpr i32 KEY_TRACK_HEIGHT{ 2 };
constexpr i32 KEY_CONTENT_HEIGHT{ KEY_HEIGHT * 3 + KEY_GAP * 2 + KEY_TRACK_GAP + KEY_TRACK_HEIGHT };
constexpr i32 KEY_CARD_WIDTH{ INDICATOR_WIDTH };
constexpr i32 KEY_CARD_HEIGHT{ INDICATOR_HEIGHT - INDICATOR_WELL_HEIGHT + KEY_CONTENT_HEIGHT };
constexpr i32 KEY_TITLE_ROOM{ KEY_CARD_WIDTH - 7 * 2 - 6 };
constexpr u8  KEY_LIT_WASH{ 72 };
constexpr u32 KEY_RECORD_PULSE{ 1400 };
constexpr u8  KEY_RECORD_FLOOR{ 100 };

constexpr usize ROUTE_BYTES_LIMIT{ sizeof( recorder_header_t ) + RECORDER_FRAME_LIMIT * sizeof( recorder_frame_t ) };

ATLAS_FORCE_INLINE bool unpack_route( const atl::vector< u8 > &bytes, recorder_header_t &header, atl::vector< recorder_frame_t > &frames )
{
    header = recorder_header_t{ };

    if ( bytes.size( ) < RECORDER_HEADER_CLIPLESS )
    {
        return false;
    }

    atl::memcpy( &header, bytes.data( ), RECORDER_HEADER_CLIPLESS );

    if ( header.m_magic != RECORDER_MAGIC ||
         ( header.m_version != RECORDER_VERSION && header.m_version != RECORDER_VERSION_CLIPLESS &&
           header.m_version != RECORDER_VERSION_UNFLAGGED ) ||
         header.m_frames < 2 || header.m_frames > RECORDER_FRAME_LIMIT )
    {
        return false;
    }

    const bool  clipped{ header.m_version == RECORDER_VERSION };
    const bool  unflagged{ header.m_version == RECORDER_VERSION_UNFLAGGED };
    const usize header_size{ clipped ? sizeof( header ) : RECORDER_HEADER_CLIPLESS };
    const usize frame_size{ unflagged ? sizeof( recorder_unflagged_frame_t ) : sizeof( recorder_frame_t ) };

    if ( bytes.size( ) != header_size + header.m_frames * frame_size || !frames.resize( header.m_frames ) )
    {
        return false;
    }

    header.m_clip = RECORDER_CLIP_UNKNOWN;

    if ( clipped )
    {
        atl::memcpy( &header.m_clip, bytes.data( ) + RECORDER_HEADER_CLIPLESS, sizeof( header.m_clip ) );
    }

    const u8 *cursor{ bytes.data( ) + header_size };

    if ( !unflagged )
    {
        atl::memcpy( frames.data( ), cursor, header.m_frames * frame_size );
        return true;
    }

    for ( recorder_frame_t &frame : frames )
    {
        recorder_unflagged_frame_t stored{ };
        atl::memcpy( &stored, cursor, sizeof( stored ) );
        cursor += sizeof( stored );

        frame = recorder_frame_t{ stored.m_origin, stored.m_velocity, stored.m_angles, stored.m_forward, stored.m_side, stored.m_buttons };
    }

    return true;
}

ATLAS_FORCE_INLINE bool pack_route( const recorder_header_t &header, const atl::vector< recorder_frame_t > &frames, atl::vector< u8 > &bytes )
{
    const usize size{ frames.size( ) * sizeof( recorder_frame_t ) };

    if ( !bytes.resize( sizeof( header ) + size ) )
    {
        return false;
    }

    atl::memcpy( bytes.data( ), &header, sizeof( header ) );
    atl::memcpy( bytes.data( ) + sizeof( header ), frames.data( ), size );
    return true;
}

#if defined( ATLAS_DISABLE_CLOUD )
constexpr usize ROUTE_PATH_CAPACITY{ 1024 };
constexpr usize ROUTE_PATH_TAIL{ 128 };
constexpr usize ROUTE_EXTENSION_LENGTH{ 6 };

ATLAS_FORCE_INLINE usize route_folder( wchar_t ( &path )[ ROUTE_PATH_CAPACITY ], string_view map )
{
    u32 length{ I( GetModuleFileNameW )( nullptr, path, ROUTE_PATH_CAPACITY - ROUTE_PATH_TAIL ) };

    if ( !length || length >= ROUTE_PATH_CAPACITY - ROUTE_PATH_TAIL || map.empty( ) )
    {
        return 0;
    }

    while ( length && path[ length - 1 ] != L'\\' )
    {
        --length;
    }

    path[ length ] = L'\0';
    str::append( path, ROUTE_PATH_CAPACITY, ATLAS_XORWSTR( L"atlas-routes" ).get( ) );
    I( CreateDirectoryW )( path, nullptr );

    usize cursor{ str::length( path ) };
    path[ cursor++ ] = L'\\';

    for ( const char value : map )
    {
        path[ cursor++ ] = static_cast< wchar_t >( value );
    }

    path[ cursor ] = L'\0';
    I( CreateDirectoryW )( path, nullptr );

    path[ cursor++ ] = L'\\';
    path[ cursor ]   = L'\0';
    return cursor;
}

ATLAS_FORCE_INLINE bool route_path( wchar_t ( &path )[ ROUTE_PATH_CAPACITY ], string_view map, string_view name,
                                    const wchar_t *extension )
{
    usize cursor{ route_folder( path, map ) };

    if ( !cursor )
    {
        return false;
    }

    for ( const char value : name )
    {
        path[ cursor++ ] = static_cast< wchar_t >( value );
    }

    path[ cursor ] = L'\0';
    str::append( path, ROUTE_PATH_CAPACITY, extension );
    return true;
}

ATLAS_FORCE_INLINE bool store_ready( )
{
    return true;
}

ATLAS_FORCE_INLINE const char *store_failure( u32, const char *local )
{
    return local;
}

ATLAS_FORCE_INLINE bool read_blob( string_view map, string_view name, atl::vector< u8 > &bytes )
{
    wchar_t path[ ROUTE_PATH_CAPACITY ]{ };
    if ( !route_path( path, map, name, ATLAS_XORWSTR( L".route" ).get( ) ) )
    {
        return false;
    }

    void *file{ I( CreateFileW )( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr ) };
    if ( file == INVALID_HANDLE_VALUE )
    {
        return false;
    }

    LARGE_INTEGER size{ };
    DWORD         read{ };
    const bool    ok{ I( GetFileSizeEx )( file, &size ) && size.QuadPart <= static_cast< i64 >( ROUTE_BYTES_LIMIT ) &&
                      bytes.resize( static_cast< usize >( size.QuadPart ) ) &&
                      I( ReadFile )( file, bytes.data( ), static_cast< DWORD >( size.QuadPart ), &read, nullptr ) &&
                      read == size.QuadPart };

    I( CloseHandle )( file );
    return ok;
}

ATLAS_FORCE_INLINE bool store_list( string_view map, atl::vector< recorder_blob_t > &blobs, u32 &status )
{
    status = 0;

    wchar_t pattern[ ROUTE_PATH_CAPACITY ]{ };
    if ( !route_folder( pattern, map ) )
    {
        return false;
    }

    str::append( pattern, ROUTE_PATH_CAPACITY, ATLAS_XORWSTR( L"*.route" ).get( ) );

    WIN32_FIND_DATAW found{ };
    void            *search{ I( FindFirstFileW )( pattern, &found ) };

    if ( search == INVALID_HANDLE_VALUE )
    {
        return true;
    }

    do
    {
        const usize length{ str::length( found.cFileName ) };
        if ( length <= ROUTE_EXTENSION_LENGTH || length - ROUTE_EXTENSION_LENGTH > RECORDER_NAME_LENGTH ||
             !str::ends_with( found.cFileName, length, ATLAS_XORWSTR( L".route" ).get( ), ROUTE_EXTENSION_LENGTH ) )
        {
            continue;
        }

        char  name[ RECORDER_NAME_LENGTH + 1 ]{ };
        usize size{ };
        bool  ascii{ true };

        for ( ; ascii && size < length - ROUTE_EXTENSION_LENGTH; ++size )
        {
            ascii        = found.cFileName[ size ] < 0x80;
            name[ size ] = static_cast< char >( found.cFileName[ size ] );
        }

        const string_view stem{ name, size };
        recorder_blob_t   blob{ };

        if ( !ascii || !configs::valid_name( stem ) || !read_blob( map, stem, blob.m_bytes ) )
        {
            continue;
        }

        blob.m_name.assign( stem );
        blobs.push_back( static_cast< recorder_blob_t && >( blob ) );
    } while ( blobs.size( ) < RECORDER_ROUTE_CAPACITY && I( FindNextFileW )( search, &found ) );

    I( FindClose )( search );
    return true;
}

ATLAS_FORCE_INLINE bool store_put( string_view map, string_view name, const atl::vector< u8 > &bytes, u32 &status )
{
    status = 0;

    wchar_t path[ ROUTE_PATH_CAPACITY ]{ };
    wchar_t temporary[ ROUTE_PATH_CAPACITY ]{ };
    if ( !route_path( path, map, name, ATLAS_XORWSTR( L".route" ).get( ) ) ||
         !route_path( temporary, map, name, ATLAS_XORWSTR( L".tmp" ).get( ) ) )
    {
        return false;
    }

    void *file{ I( CreateFileW )( temporary, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr ) };
    if ( file == INVALID_HANDLE_VALUE )
    {
        return false;
    }

    DWORD written{ };
    bool  ok{ I( WriteFile )( file, bytes.data( ), static_cast< DWORD >( bytes.size( ) ), &written, nullptr ) &&
              written == bytes.size( ) && I( FlushFileBuffers )( file ) };

    I( CloseHandle )( file );

    ok = ok && I( MoveFileExW )( temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH );
    if ( !ok )
    {
        I( DeleteFileW )( temporary );
    }

    return ok;
}

ATLAS_FORCE_INLINE bool store_remove( string_view map, string_view name, u32 &status )
{
    status = 0;

    wchar_t path[ ROUTE_PATH_CAPACITY ]{ };
    return route_path( path, map, name, ATLAS_XORWSTR( L".route" ).get( ) ) && I( DeleteFileW )( path );
}
#else
extern api g_api;

constexpr usize ROUTE_URL_CAPACITY{ 128 };
constexpr usize ROUTE_REPLY_LIMIT{ 64 * 1024 };
constexpr usize ROUTE_TEXT_LIMIT{ ( ROUTE_BYTES_LIMIT + 2 ) / 3 * 4 + ROUTE_REPLY_LIMIT };

ATLAS_FORCE_INLINE bool store_ready( )
{
    return g_api.has_config_token( );
}

ATLAS_FORCE_INLINE const char *store_failure( u32 status, const char * )
{
    switch ( status )
    {
        case 0: return ATLAS_XORSTR_STATIC( "Could not reach Cloud." );
        case 401:
        case 403: return ATLAS_XORSTR_STATIC( "Cloud session rejected." );
        case 404: return ATLAS_XORSTR_STATIC( "Cloud recordings are not deployed yet." );
        case 409: return ATLAS_XORSTR_STATIC( "This map already holds 32 recordings." );
        case 413: return ATLAS_XORSTR_STATIC( "This recording is too long for Cloud." );
        case 429: return ATLAS_XORSTR_STATIC( "Cloud rate limit reached." );
        default: return ATLAS_XORSTR_STATIC( "Cloud request failed." );
    }
}

ATLAS_FORCE_INLINE void route_url( wchar_t ( &url )[ ROUTE_URL_CAPACITY ], string_view map, string_view name )
{
    url[ 0 ] = L'\0';
    str::append( url, ROUTE_URL_CAPACITY, ATLAS_XORWSTR( L"/v1/routes/" ).get( ) );

    usize cursor{ str::length( url ) };

    for ( const char value : map )
    {
        url[ cursor++ ] = static_cast< wchar_t >( value );
    }

    if ( !name.empty( ) )
    {
        url[ cursor++ ] = L'/';

        for ( const char value : name )
        {
            url[ cursor++ ] = static_cast< wchar_t >( value );
        }
    }

    url[ cursor ] = L'\0';
}

ATLAS_FORCE_INLINE bool store_list( string_view map, atl::vector< recorder_blob_t > &blobs, u32 &status )
{
    wchar_t url[ ROUTE_URL_CAPACITY ]{ };
    route_url( url, map, { } );

    atl::vector< char > listing{ };
    json::document      document{ };

    if ( !g_api.request( ATLAS_XORWSTR( L"GET" ).get( ), url, { }, listing, ROUTE_REPLY_LIMIT, status ) || status != 200 ||
         !document.parse( string_view{ listing.data( ), listing.size( ) } ) )
    {
        return false;
    }

    const json::value items{ document.root( )[ ATLAS_XORSTR( "routes" ).get( ) ] };
    if ( items.type( ) != json::type_t::array )
    {
        return false;
    }

    for ( usize index{ }; index < items.size( ) && blobs.size( ) < RECORDER_ROUTE_CAPACITY; ++index )
    {
        const string_view name{ items[ index ][ ATLAS_XORSTR( "name" ).get( ) ].as< string_view >( ) };
        if ( !configs::valid_name( name ) )
        {
            continue;
        }

        atl::vector< char > reply{ };
        json::document      route{ };
        recorder_blob_t     blob{ };
        u32                 route_status{ };

        route_url( url, map, name );

        if ( !g_api.request( ATLAS_XORWSTR( L"GET" ).get( ), url, { }, reply, ROUTE_TEXT_LIMIT, route_status ) ||
             route_status != 200 || !route.parse( string_view{ reply.data( ), reply.size( ) } ) ||
             !base64::decode( route.root( )[ ATLAS_XORSTR( "data" ).get( ) ].as< string_view >( ), blob.m_bytes ) )
        {
            continue;
        }

        blob.m_name.assign( name );
        blobs.push_back( static_cast< recorder_blob_t && >( blob ) );
    }

    return true;
}

ATLAS_FORCE_INLINE bool store_put( string_view map, string_view name, const atl::vector< u8 > &bytes, u32 &status )
{
    status = 0;

    atl::vector< char > text{ };
    if ( !base64::append( bytes.data( ), bytes.size( ), text ) )
    {
        return false;
    }

    json::writer writer{ };
    writer.begin_object( );
    writer.member( ATLAS_XORSTR( "data" ).get( ), string_view{ text.data( ), text.size( ) } );
    writer.end_object( );

    wchar_t url[ ROUTE_URL_CAPACITY ]{ };
    route_url( url, map, name );

    atl::vector< char > reply{ };
    return writer.ok( ) && g_api.request( ATLAS_XORWSTR( L"PUT" ).get( ), url, writer.text( ), reply, ROUTE_REPLY_LIMIT, status ) &&
           status >= 200 && status < 300;
}

ATLAS_FORCE_INLINE bool store_remove( string_view map, string_view name, u32 &status )
{
    wchar_t url[ ROUTE_URL_CAPACITY ]{ };
    route_url( url, map, name );

    atl::vector< char > reply{ };
    return g_api.request( ATLAS_XORWSTR( L"DELETE" ).get( ), url, { }, reply, ROUTE_REPLY_LIMIT, status ) && status >= 200 &&
           status < 300;
}
#endif

ATLAS_FORCE_INLINE u32 command_keys( f32 forward, f32 side, i32 buttons )
{
    u32 keys{ };

    if ( forward > 0.0f || ( buttons & IN_FORWARD ) )
    {
        keys |= RECORDER_KEY_W;
    }

    if ( forward < 0.0f || ( buttons & IN_BACK ) )
    {
        keys |= RECORDER_KEY_S;
    }

    if ( side < 0.0f || ( buttons & IN_MOVELEFT ) )
    {
        keys |= RECORDER_KEY_A;
    }

    if ( side > 0.0f || ( buttons & IN_MOVERIGHT ) )
    {
        keys |= RECORDER_KEY_D;
    }

    if ( buttons & IN_JUMP )
    {
        keys |= RECORDER_KEY_JUMP;
    }

    if ( buttons & IN_DUCK )
    {
        keys |= RECORDER_KEY_DUCK;
    }

    if ( buttons & IN_ATTACK )
    {
        keys |= RECORDER_KEY_M1;
    }

    if ( buttons & IN_ATTACK2 )
    {
        keys |= RECORDER_KEY_M2;
    }

    return keys;
}

ATLAS_FORCE_INLINE bool bind_pressed( bind_t &bind, bool &held )
{
    const bool active{ gui::bind_active( bind ) };
    const bool edge{ active && !held };

    held = active;
    return edge;
}

ATLAS_FORCE_INLINE void notify( notify_kind_t kind, const char *message )
{
    push_notification( kind, ICON_RULER, ATLAS_XORSTR_STATIC( "Recorder" ), message );
}

ATLAS_FORCE_INLINE bool weapon_matches( i32 clip )
{
    C_TFWeaponBase *weapon{ g_cl.m_weapon };
    if ( !weapon )
    {
        return false;
    }

    const f32 curtime{ TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) };

    return ( clip == RECORDER_CLIP_UNKNOWN || weapon->m_iClip1( ) == clip ) && weapon->m_iReloadMode( ) == TF_RELOAD_START &&
           weapon->m_flNextPrimaryAttack( ) <= curtime && g_cl.m_local->m_flNextAttack( ) <= curtime;
}

#if defined( ATLAS_DEV )
ATLAS_FORCE_INLINE void trace_weapon( const char *phase, i32 frame )
{
    C_TFWeaponBase *weapon{ g_cl.m_weapon };
    const f32       curtime{ TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) };

    DBGOUT( "recorder: %s %i weapon clip %i reload %i next %.3f prior %.3f attack %.3f", phase, frame, weapon->m_iClip1( ),
            weapon->m_iReloadMode( ), weapon->m_flNextPrimaryAttack( ) - curtime, weapon->m_flReloadPriorNextFire( ) - curtime,
            g_cl.m_local->m_flNextAttack( ) - curtime );
}
#endif

ATLAS_FORCE_INLINE vec3 command_velocity( const recorder_physics_t &physics, vec3 velocity, f32 forward_move, f32 side_move )
{
    vec3 wish{ physics.m_forward * forward_move + physics.m_right * side_move };

    f32 wish_speed{ math::smaller( wish.normalize( ), physics.m_ground.max_speed ) };

    if ( physics.m_air )
    {
        const f32 add{ math::smaller( wish_speed, physics.m_air_cap ) - velocity.dot( wish ) };

        if ( add > 0.0f )
        {
            velocity += wish * math::smaller( physics.m_accelerate * wish_speed * physics.m_ground.dt * physics.m_ground.surface_friction, add );
        }

        return velocity;
    }

    if ( physics.m_ducked )
    {
        wish_speed *= DUCK_SPEED_CROP;
    }

    strafe_prediction::ground_step( velocity, wish, wish_speed, physics.m_ground );
    strafe_prediction::ground_clamp_back_speed( velocity, physics.m_yaw, physics.m_ground.max_speed, physics.m_back, physics.m_back_min );
    return velocity;
}

ATLAS_FORCE_INLINE recorder_physics_t live_physics( f32 yaw, bool air )
{
    const bool lost_footing{ g_cl.m_local->InCond( TF_COND_LOST_FOOTING ) };

    recorder_physics_t physics{ };
    physics.m_ground     = strafe_prediction::ground_physics{ TICK_INTERVAL,
                                                              g_tf2.sv_friction->GetFloat( ),
                                                              g_tf2.sv_stopspeed->GetFloat( ),
                                                              g_tf2.sv_accelerate->GetFloat( ),
                                                              strafe_prediction::movement_max_speed( g_cl.m_local->m_flMaxspeed( ), lost_footing ),
                                                              g_cl.m_local->m_surfaceFriction( ),
                                                              lost_footing };
    physics.m_yaw        = yaw;
    physics.m_air        = air;
    physics.m_ducked     = g_cl.m_local->IsDucked( );
    physics.m_accelerate = air ? g_tf2.sv_airaccelerate->GetFloat( ) : physics.m_ground.acceleration;
    physics.m_air_cap    = AIR_SPEED_CAP * ( g_cl.m_local->InCond( TF_COND_PARACHUTE_ACTIVE ) ? g_tf2.tf_parachute_aircontrol->GetFloat( ) : 1.0f );
    physics.m_back       = g_tf2.tf_clamp_back_speed->GetFloat( );
    physics.m_back_min   = g_tf2.tf_clamp_back_speed_min->GetFloat( );

    math::angle_vectors( vec3{ 0.0f, yaw, 0.0f }, &physics.m_forward, &physics.m_right );
    return physics;
}

ATLAS_FORCE_INLINE f32 command_authority( const recorder_physics_t &physics )
{
    const f32 wish{ physics.m_ground.max_speed * ( physics.m_ducked && !physics.m_air ? DUCK_SPEED_CROP : 1.0f ) };
    const f32 added{ physics.m_accelerate * wish * physics.m_ground.dt * physics.m_ground.surface_friction };

    return physics.m_air ? math::smaller( added, physics.m_air_cap ) : added;
}

ATLAS_FORCE_INLINE vec3 closing_velocity( const recorder_physics_t &physics, const vec3 &gap )
{
    const vec3 flat{ gap.x, gap.y, 0.0f };
    const f32  distance{ flat.length( ) };

    if ( distance <= 0.0f || physics.m_air )
    {
        return vec3{ };
    }

    const f32 authority{ command_authority( physics ) / physics.m_ground.dt };
    const f32 closing{ math::smaller( distance / physics.m_ground.dt, math::sqrt( 2.0f * authority * distance ) ) };

    return flat * ( closing / distance );
}

ATLAS_FORCE_INLINE vec2 best_move( const recorder_physics_t &physics, const vec3 &velocity, const vec3 &goal, vec2 preferred )
{
    vec2 best{ preferred };
    f32  best_cost{ ( command_velocity( physics, velocity, preferred.x, preferred.y ) - goal ).length2d_sqr( ) };

    const auto consider{ [ & ]( f32 forward_move, f32 side_move )
    {
        const f32 cost{ ( command_velocity( physics, velocity, forward_move, side_move ) - goal ).length2d_sqr( ) };

        if ( cost < best_cost )
        {
            best_cost = cost;
            best      = vec2{ forward_move, side_move };
        }
    } };

    consider( 0.0f, 0.0f );

    vec3 toward{ goal - velocity };
    toward.normalize( );

    for ( f32 strength{ 1.0f }; strength >= MOVE_WEAKEST; strength *= 0.5f )
    {
        const f32 reach{ physics.m_ground.max_speed * strength };

        for ( i32 direction{ }; direction < MOVE_DIRECTIONS; ++direction )
        {
            const f32 angle{ 2.0f * PI * static_cast< f32 >( direction ) / static_cast< f32 >( MOVE_DIRECTIONS ) };
            consider( reach * math::cos( angle ), reach * math::sin( angle ) );
        }

        consider( reach * toward.dot( physics.m_forward ), reach * toward.dot( physics.m_right ) );
    }

    return best;
}

ATLAS_FORCE_INLINE u8 marker_alpha( f32 distance )
{
    const f32 reach{ static_cast< f32 >( g_cfg.m_recorder.m_marker_distance ) };
    return static_cast< u8 >( math::clamp( ( reach - distance ) / ( reach * MARKER_FADE_SHARE ), 0.0f, 1.0f ) * 255.0f );
}

ATLAS_FORCE_INLINE i32 label_text_y( i32 top, i32 height )
{
    return top + ( height - LABEL_FONT_INK_HEIGHT ) / 2 - LABEL_FONT_INK_TOP;
}

ATLAS_FORCE_INLINE void draw_key( rect_t bounds, const char *label, u8 mix, u8 enter )
{
    const color_t wash{ gui::mix_color( g_theme.m_selected_background, g_theme.m_accent, KEY_LIT_WASH ) };

    g_render.box( bounds, gui::with_alpha( gui::mix_color( CONTROL_BACKGROUND, wash, mix ), enter ), true );
    g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                  gui::with_alpha( gui::mix_color( CONTROL_BORDER, g_theme.m_accent, mix ), enter ) );
    g_render.text( MENU_FONT_TINY( ), point_t{ ( bounds.left + bounds.right ) / 2, indicator_text_y( bounds ) },
                   gui::with_alpha( gui::mix_color( MENU_TEXT_DIM, CHECKBOX_MARK_COLOR, mix ), enter ), label, false,
                   render::text_align::center );
}

void recorder_wait_text( recorder_wait_t wait, f32 distance, f32 height, char ( &out )[ 32 ] )
{
    switch ( wait )
    {
        case recorder_wait_t::walk:
            str::format_to( out, sizeof( out ), ATLAS_XORSTR( "{} u to start" ).get( ), static_cast< i32 >( distance ) );
            return;

        case recorder_wait_t::height:
            str::format_to( out, sizeof( out ), ATLAS_XORSTR( "start {} u {}" ).get( ), static_cast< i32 >( math::abs( height ) ),
                            height < 0.0f ? ATLAS_XORSTR_STATIC( "below" ) : ATLAS_XORSTR_STATIC( "above" ) );
            return;

        case recorder_wait_t::land: str::copy( out, sizeof( out ), ATLAS_XORSTR( "land to start" ).get( ) ); return;
        case recorder_wait_t::weapon: str::copy( out, sizeof( out ), ATLAS_XORSTR( "weapon not ready" ).get( ) ); return;
        default: str::copy( out, sizeof( out ), ATLAS_XORSTR( "starting" ).get( ) ); return;
    }
}

void movement_recorder::on_level_init( const char *map_name )
{
    this->m_map.assign( string_view{ } );

    for ( const char *cursor{ map_name }; *cursor && this->m_map.size( ) < RECORDER_MAP_LENGTH; ++cursor )
    {
        const char value{ str::to_lower( *cursor ) };

        this->m_map.push_back( str::is_alpha( value ) || str::is_digit( value ) || value == '-' ? value : '_' );
    }

    this->m_listing_due = true;
    this->publish_catalog( );
}

void movement_recorder::on_level_shutdown( )
{
    this->m_phase       = recorder_phase_t::idle;
    this->m_active      = nullptr;
    this->m_has_take    = false;
    this->m_keys        = 0;
    this->m_listing_due = false;
    this->m_take.m_frames.clear( );
    this->m_routes.clear( );
    this->m_map.assign( string_view{ } );
    this->m_selected.assign( string_view{ } );
    this->publish_catalog( );
}

void movement_recorder::on_pre_prediction( )
{
    if ( this->m_phase == recorder_phase_t::idle )
    {
        return;
    }

    this->m_start_origin   = g_cl.m_local->m_vecOrigin( );
    this->m_start_velocity = g_cl.m_local->m_vecVelocity( );

    if ( this->m_phase == recorder_phase_t::armed )
    {
        this->try_start_replay( );
    }

    if ( this->m_phase == recorder_phase_t::aligning )
    {
        this->align( );
    }

    if ( this->m_phase == recorder_phase_t::replaying )
    {
        this->steer( );
    }

    if ( this->replaying( ) )
    {
        const recorder_frame_t &frame{ this->m_active->m_frames[ this->m_phase == recorder_phase_t::aligning ? 0 : this->m_frame ] };
        g_cl.m_cmd->viewangles = vec3{ frame.m_angles.x, frame.m_angles.y, 0.0f };

        if ( this->m_phase == recorder_phase_t::replaying && ( frame.m_flags & RECORDER_FRAME_PSILENT ) )
        {
            g_cl.m_psilent_choked = true;
        }
    }
}

void movement_recorder::on_create_move_end( )
{
    const CUserCmd &cmd{ *g_cl.m_cmd };

    if ( this->m_phase == recorder_phase_t::waiting && command_keys( cmd.forwardmove, cmd.sidemove, cmd.buttons ) )
    {
        this->m_phase       = recorder_phase_t::recording;
        this->m_class       = g_cl.m_local->m_iClass( );
        this->m_take.m_clip = g_cl.m_weapon->m_iClip1( );

#if defined( ATLAS_DEV )
        this->m_traced_clip = this->m_take.m_clip;
#endif
    }

    if ( this->m_phase == recorder_phase_t::aligning )
    {
        this->apply_alignment( );
        return;
    }

    if ( this->m_phase == recorder_phase_t::replaying )
    {
        this->apply_frame( );
        return;
    }

    if ( this->m_phase != recorder_phase_t::recording )
    {
        return;
    }

    if ( g_cl.m_local->m_iClass( ) != this->m_class )
    {
        this->stop_recording( );
        return;
    }

    this->m_take.m_frames.push_back( recorder_frame_t{ this->m_start_origin, this->m_start_velocity,
                                                       vec2{ cmd.viewangles.x, cmd.viewangles.y }, cmd.forwardmove, cmd.sidemove,
                                                       cmd.buttons, g_cl.m_psilent_choked ? RECORDER_FRAME_PSILENT : 0u } );

    DBGOUT( "recorder: rec %i tick %i cmd %i ack %i %s %i | vel %.1f %.1f %.1f | move %.0f %.0f | btn %x | %s%s",
            static_cast< i32 >( this->m_take.m_frames.size( ) ) - 1, g_tf2.m_globals->tickcount, cmd.command_number,
            g_tf2.m_cl->command_ack, g_cl.m_send_packet ? "send" : "choke", g_tf2.m_cl->chokedcommands,
            this->m_start_velocity.x, this->m_start_velocity.y, this->m_start_velocity.z, cmd.forwardmove, cmd.sidemove,
            cmd.buttons, g_cl.m_local->IsOnGround( ) ? "ground" : "air", g_cl.m_local->IsDucked( ) ? " ducked" : "" );

#if defined( ATLAS_DEV )
    if ( ( cmd.buttons & IN_ATTACK ) || g_cl.m_weapon->m_iClip1( ) != this->m_traced_clip )
    {
        trace_weapon( "rec", static_cast< i32 >( this->m_take.m_frames.size( ) ) - 1 );
        this->m_traced_clip = g_cl.m_weapon->m_iClip1( );
    }
#endif

    this->m_keys = command_keys( cmd.forwardmove, cmd.sidemove, cmd.buttons );
    this->m_pressed |= this->m_keys;

    if ( this->m_take.m_frames.size( ) >= RECORDER_FRAME_LIMIT )
    {
        this->stop_recording( );
    }
}

void movement_recorder::paint( )
{
    this->poll_store( );
    this->service( );
    this->watch( );
    this->publish( );

    if ( !g_cfg.m_recorder.m_enabled || !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        return;
    }

    if ( g_cfg.m_recorder.m_markers )
    {
        this->draw_markers( );
    }

    if ( g_cfg.m_recorder.m_route )
    {
        this->draw_paths( );
    }
}

void movement_recorder::paint_keys( )
{
    recorder_status_t status{ };
    u32               taps{ };

    {
        atl::scoped_lock lock{ this->m_lock };
        status       = this->m_status;
        taps         = this->m_taps;
        this->m_taps = 0;
    }

    const bool live{ status.m_phase != recorder_phase_t::idle };
    const bool wanted{ g_cfg.m_recorder.m_enabled && g_cfg.m_recorder.m_key_overlay && ( live || g_gui.open( ) ) };

    if ( !wanted && !this->m_key_card.m_enter )
    {
        hide_indicator( this->m_key_card );

        for ( u8 &mix : this->m_key_mix )
        {
            mix = 0;
        }

        return;
    }

    f32 progress{ };

    if ( status.m_phase == recorder_phase_t::replaying && status.m_frames )
    {
        progress = static_cast< f32 >( status.m_frame ) / static_cast< f32 >( status.m_frames );
    } else if ( status.m_phase == recorder_phase_t::recording )
    {
        progress = 1.0f;
    }

    this->m_key_card.m_at = g_cfg.m_recorder.m_key_overlay_position;
    const u32 delta{ step_indicator( this->m_key_card, progress, g_cl.m_screen_width, g_cl.m_screen_height, KEY_CARD_WIDTH,
                                     KEY_CARD_HEIGHT, wanted ) };
    g_cfg.m_recorder.m_key_overlay_position = this->m_key_card.m_at;

    const u32 lit{ status.m_keys | taps };

    for ( usize index{ }; index < RECORDER_KEY_COUNT; ++index )
    {
        if ( lit & ( 1u << index ) )
        {
            this->m_key_mix[ index ] = 255;
            continue;
        }

        gui::step_mix( this->m_key_mix[ index ], false, delta, WIDGET_TOGGLE_DURATION );
    }

    char    title[ 64 ]{ };
    char    state[ 32 ]{ };
    color_t state_color{ CONTROL_VALUE };

    switch ( status.m_phase )
    {
        case recorder_phase_t::waiting:
            str::copy( title, sizeof( title ), ATLAS_XORSTR( "recording" ).get( ) );
            str::copy( state, sizeof( state ), ATLAS_XORSTR( "waiting for input" ).get( ) );
            break;

        case recorder_phase_t::recording:
            str::copy( title, sizeof( title ), ATLAS_XORSTR( "recording" ).get( ) );
            str::format_to( state, sizeof( state ), ATLAS_XORSTR( "{:.2f} s" ).get( ),
                            static_cast< f32 >( status.m_frames ) * status.m_interval );
            state_color = g_theme.m_accent;
            break;

        case recorder_phase_t::armed:
            str::format_to( title, sizeof( title ), ATLAS_XORSTR( "replay: {}" ).get( ), status.m_route.view( ) );
            recorder_wait_text( status.m_wait, status.m_start_distance, status.m_start_height, state );
            state_color = STATUS_WARNING;
            break;

        case recorder_phase_t::aligning:
            str::format_to( title, sizeof( title ), ATLAS_XORSTR( "replay: {}" ).get( ), status.m_route.view( ) );
            str::copy( state, sizeof( state ), ATLAS_XORSTR( "aligning" ).get( ) );
            state_color = STATUS_WARNING;
            break;

        case recorder_phase_t::replaying:
            str::format_to( title, sizeof( title ), ATLAS_XORSTR( "replay: {}" ).get( ), status.m_route.view( ) );
            str::format_to( state, sizeof( state ), ATLAS_XORSTR( "{:.2f} / {:.2f}" ).get( ),
                            static_cast< f32 >( status.m_frame ) * status.m_interval,
                            static_cast< f32 >( status.m_frames ) * status.m_interval );
            break;

        default:
            str::copy( title, sizeof( title ), ATLAS_XORSTR( "recorder" ).get( ) );
            str::copy( state, sizeof( state ), ATLAS_XORSTR( "idle" ).get( ) );
            break;
    }

    const i32 state_width{ gui::estimate_tiny_text_width( state ) };

    char fitted[ 64 ]{ };
    gui::fit_text( fitted, title, KEY_TITLE_ROOM - state_width );

    const indicator_frame_t frame{ draw_indicator_frame( this->m_key_card, fitted, CARD_TITLE_TEXT ) };
    const u8                enter{ frame.m_enter };
    const rect_t            content{ frame.m_content };

    g_render.text( MENU_FONT_TINY( ), frame.m_state, gui::with_alpha( state_color, enter ), state, false,
                   render::text_align::right );

    if ( status.m_phase == recorder_phase_t::recording )
    {
        const i32 dot{ frame.m_state.x - state_width - 8 };
        gui::fill_rounded_box( rect_t{ dot, frame.m_state.y + 2, dot + 5, frame.m_state.y + 7 }, 2,
                               gui::with_alpha( g_theme.m_accent, enter ) );
    }

    const i32 second_row{ content.top + KEY_HEIGHT + KEY_GAP };
    const i32 third_row{ second_row + KEY_HEIGHT + KEY_GAP };
    const i32 right{ content.left + KEY_CLUSTER_WIDTH + KEY_CLUSTER_GAP };
    const i32 half{ ( content.right - right - KEY_GAP ) / 2 };

    const rect_t caps[ RECORDER_KEY_COUNT ]{
        rect_t{ content.left + KEY_WIDTH + KEY_GAP, content.top, content.left + KEY_WIDTH * 2 + KEY_GAP, content.top + KEY_HEIGHT },
        rect_t{ content.left, second_row, content.left + KEY_WIDTH, second_row + KEY_HEIGHT },
        rect_t{ content.left + KEY_WIDTH + KEY_GAP, second_row, content.left + KEY_WIDTH * 2 + KEY_GAP, second_row + KEY_HEIGHT },
        rect_t{ content.left + ( KEY_WIDTH + KEY_GAP ) * 2, second_row, content.left + KEY_CLUSTER_WIDTH, second_row + KEY_HEIGHT },
        rect_t{ content.left, third_row, content.left + KEY_CLUSTER_WIDTH, third_row + KEY_HEIGHT },
        rect_t{ right, content.top, content.right, content.top + KEY_HEIGHT },
        rect_t{ right, second_row, right + half, second_row + KEY_HEIGHT },
        rect_t{ content.right - half, second_row, content.right, second_row + KEY_HEIGHT },
    };

    const char *const labels[ RECORDER_KEY_COUNT ]{
        ATLAS_XORSTR_STATIC( "W" ),
        ATLAS_XORSTR_STATIC( "A" ),
        ATLAS_XORSTR_STATIC( "S" ),
        ATLAS_XORSTR_STATIC( "D" ),
        ATLAS_XORSTR_STATIC( "jump" ),
        ATLAS_XORSTR_STATIC( "duck" ),
        ATLAS_XORSTR_STATIC( "m1" ),
        ATLAS_XORSTR_STATIC( "m2" ),
    };

    for ( usize index{ }; index < RECORDER_KEY_COUNT; ++index )
    {
        draw_key( caps[ index ], labels[ index ], this->m_key_mix[ index ], enter );
    }

    const rect_t speed{ right, third_row, content.right, third_row + KEY_HEIGHT };
    char         speed_text[ 16 ]{ };
    str::format_to( speed_text, sizeof( speed_text ), ATLAS_XORSTR( "{} u/s" ).get( ), static_cast< i32 >( status.m_speed ) );

    draw_indicator_well( speed, enter );
    g_render.text( MENU_FONT_TINY( ), point_t{ ( speed.left + speed.right ) / 2, indicator_text_y( speed ) },
                   gui::with_alpha( CONTROL_VALUE, enter ), speed_text, false, render::text_align::center );

    const rect_t track{ content.left, content.bottom - KEY_TRACK_HEIGHT, content.right, content.bottom };
    const i32    filled{ static_cast< i32 >( this->m_key_card.m_fill * static_cast< f32 >( track.right - track.left ) + 0.5f ) };
    const u8     glow{ status.m_phase == recorder_phase_t::recording
                           ? gui::mix_channel( KEY_RECORD_FLOOR, 255, gui::pulse( gui::clock( ), KEY_RECORD_PULSE ) )
                           : static_cast< u8 >( 255 ) };

    g_render.box( track, gui::with_alpha( CONTROL_BACKGROUND_HOVER, enter ), true );
    g_render.box( rect_t{ track.left, track.top, track.left + filled, track.bottom },
                  gui::with_alpha( gui::with_alpha( g_theme.m_accent, glow ), enter ), true );
}

bool movement_recorder::replaying( ) const
{
    return this->m_phase == recorder_phase_t::aligning || this->m_phase == recorder_phase_t::replaying;
}

void movement_recorder::request( recorder_request_t kind, string_view route, string_view name )
{
    pending_t pending{ kind };
    pending.m_route.assign( route );
    pending.m_name.assign( name );

    atl::scoped_lock lock{ this->m_lock };
    this->m_requests.push_back( pending );
}

recorder_status_t movement_recorder::status( ) const
{
    atl::scoped_lock lock{ this->m_lock };
    return this->m_status;
}

bool movement_recorder::catalog( u32 &serial, recorder_entry_t ( &entries )[ RECORDER_ENTRY_CAPACITY ], usize &count,
                                 usize &focus ) const
{
    atl::scoped_lock lock{ this->m_lock };

    if ( serial == this->m_serial )
    {
        return false;
    }

    serial = this->m_serial;
    count  = this->m_entry_count;
    focus  = this->m_focus;

    for ( usize index{ }; index < count; ++index )
    {
        entries[ index ] = this->m_entries[ index ];
    }

    return true;
}

void movement_recorder::service( )
{
    atl::vector< pending_t > requests{ };

    {
        atl::scoped_lock lock{ this->m_lock };
        requests.swap( this->m_requests );
    }

    for ( const pending_t &pending : requests )
    {
        switch ( pending.m_kind )
        {
            case recorder_request_t::record: this->toggle_recording( ); break;
            case recorder_request_t::replay: this->begin_replay( pending.m_route.view( ) ); break;
            case recorder_request_t::stop: this->stop( ); break;
            case recorder_request_t::save: this->save_route( pending.m_route.view( ), pending.m_name.view( ) ); break;
            case recorder_request_t::remove: this->remove_route( pending.m_route.view( ) ); break;
            case recorder_request_t::select: this->m_selected = pending.m_route; break;
            case recorder_request_t::refresh: this->m_listing_due = true; break;
        }
    }
}

void movement_recorder::watch( )
{
    if ( !g_cfg.m_recorder.m_enabled || !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local || !g_cl.m_local->IsAlive( ) )
    {
        this->stop( );
        return;
    }

    if ( bind_pressed( g_cfg.m_recorder.m_record_key, this->m_bind_held[ 0 ] ) )
    {
        this->toggle_recording( );
    }

    if ( !bind_pressed( g_cfg.m_recorder.m_replay_key, this->m_bind_held[ 1 ] ) )
    {
        return;
    }

    if ( this->m_phase == recorder_phase_t::armed || this->replaying( ) )
    {
        this->stop_replay( );
        return;
    }

    const route_t *standing{ this->standing_in( ) };
    this->begin_replay( standing ? standing->m_name.view( ) : this->m_selected.view( ) );
}

void movement_recorder::publish( )
{
    recorder_status_t status{ };
    status.m_phase    = this->m_phase;
    status.m_interval = TICK_INTERVAL;
    status.m_routes   = this->m_routes.size( );
    status.m_has_take = this->m_has_take;
    status.m_keys     = this->m_phase != recorder_phase_t::idle ? this->m_keys : 0;
    status.m_map      = this->m_map;

    if ( g_tf2.m_engine->IsInGame( ) && g_cl.m_local )
    {
        status.m_origin = g_cl.m_local->m_vecOrigin( );
        status.m_speed  = g_cl.m_local->m_vecVelocity( ).length2d( );
    }

    if ( this->m_phase == recorder_phase_t::recording )
    {
        status.m_frames = static_cast< i32 >( this->m_take.m_frames.size( ) );
    }

    if ( this->m_active )
    {
        const vec3 &start{ this->m_active->m_frames[ 0 ].m_origin };

        status.m_route          = this->m_active->m_name;
        status.m_frame          = this->m_frame;
        status.m_frames         = static_cast< i32 >( this->m_active->m_frames.size( ) );
        status.m_start_distance = ( start - status.m_origin ).length2d( );
        status.m_start_height   = start.z - status.m_origin.z;
        status.m_wait           = this->start_wait( *this->m_active );
    }

    atl::scoped_lock lock{ this->m_lock };
    this->m_status = status;
    this->m_taps |= this->m_pressed;
    this->m_pressed = 0;
}

void movement_recorder::publish_catalog( usize focus )
{
    atl::scoped_lock lock{ this->m_lock };

    usize count{ };

    const auto list{ [ & ]( const route_t &route, bool take )
    {
        recorder_entry_t &entry{ this->m_entries[ count++ ] };
        entry.m_name     = route.m_name;
        entry.m_start    = route.m_frames[ 0 ].m_origin;
        entry.m_length   = route.m_length;
        entry.m_class    = route.m_class;
        entry.m_commands = static_cast< i32 >( route.m_frames.size( ) );
        entry.m_jumps    = route.m_jumps;
        entry.m_take     = take;
    } };

    if ( this->m_has_take )
    {
        list( this->m_take, true );
    }

    for ( const route_t &route : this->m_routes )
    {
        list( route, false );
    }

    this->m_entry_count = count;
    this->m_focus       = focus;
    ++this->m_serial;
}

void movement_recorder::stop( )
{
    this->stop_recording( );
    this->stop_replay( );
}

void movement_recorder::toggle_recording( )
{
    if ( this->m_phase == recorder_phase_t::waiting || this->m_phase == recorder_phase_t::recording )
    {
        this->stop_recording( );
        return;
    }

    if ( !this->ready( ) )
    {
        return;
    }

    this->stop_replay( );

    if ( !this->m_take.m_frames.reserve( RECORDER_FRAME_LIMIT ) )
    {
        return;
    }

    ++this->m_take_serial;
    this->m_take.m_frames.clear( );
    this->m_take.m_interval = TICK_INTERVAL;
    this->m_has_take        = false;
    this->m_phase           = recorder_phase_t::waiting;
    this->publish_catalog( );
}

void movement_recorder::stop_recording( )
{
    if ( this->m_phase != recorder_phase_t::waiting && this->m_phase != recorder_phase_t::recording )
    {
        return;
    }

    const bool recorded{ this->m_phase == recorder_phase_t::recording && this->m_take.m_frames.size( ) >= 2 };

    this->m_phase = recorder_phase_t::idle;
    this->m_keys  = 0;

    if ( !recorded )
    {
        this->m_take.m_frames.clear( );
        return;
    }

    this->m_take.m_name.assign( ATLAS_XORSTR( "unsaved take" ).get( ) );
    this->m_take.m_class = this->m_class;
    measure( this->m_take );

    this->m_has_take = true;
    this->m_selected = this->m_take.m_name;
    this->publish_catalog( 0 );

    notify( notify_kind_t::success, ATLAS_XORSTR_STATIC( "Take ready. Name it and save it in Misc > Recorder." ) );
}

void movement_recorder::begin_replay( string_view name )
{
    if ( !this->ready( ) )
    {
        return;
    }

    route_t *route{ this->find( name ) };
    if ( !route )
    {
        notify( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Select a recording, or stand in a start marker." ) );
        return;
    }

    if ( route->m_class != g_cl.m_local->m_iClass( ) )
    {
        char message[ 64 ]{ };
        str::format_to( message, sizeof( message ), ATLAS_XORSTR( "This route was recorded as {}." ).get( ),
                        player_class_name( route->m_class ) );
        notify( notify_kind_t::warning, message );
        return;
    }

    if ( route->m_interval != TICK_INTERVAL )
    {
        notify( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "This route was recorded at another tick rate." ) );
        return;
    }

    this->stop_recording( );

    this->m_active = route;
    this->m_frame  = 0;
    this->m_phase  = recorder_phase_t::armed;
}

void movement_recorder::stop_replay( const char *reason )
{
    if ( this->m_phase != recorder_phase_t::armed && !this->replaying( ) )
    {
        return;
    }

#if defined( ATLAS_DEV )
    if ( this->m_phase == recorder_phase_t::replaying && this->m_frame )
    {
        DBGOUT( "recorder: stopped %s, %i of %i frames, aligned in %i, first error %.2f, peak error %.2f, mean error %.2f",
                this->m_active->m_name.c_str( ), this->m_frame, static_cast< i32 >( this->m_active->m_frames.size( ) ),
                this->m_align_commands, this->m_first_error, this->m_peak_error,
                this->m_error_sum / static_cast< f32 >( this->m_frame ) );
    }
#endif

    this->m_phase  = recorder_phase_t::idle;
    this->m_active = nullptr;
    this->m_frame  = 0;
    this->m_keys   = 0;

    if ( reason )
    {
        notify( notify_kind_t::warning, reason );
    }
}

void movement_recorder::try_start_replay( )
{
    const route_t &route{ *this->m_active };

    if ( this->start_wait( route ) != recorder_wait_t::none )
    {
        return;
    }

    this->m_user_held      = g_cl.m_cmd->buttons & MOVE_BUTTONS;
    this->m_align_commands = 0;

    if ( route.m_frames[ 0 ].m_velocity.length2d( ) >= 1.0f )
    {
        this->begin_frames( );
        return;
    }

    this->m_phase = recorder_phase_t::aligning;
}

void movement_recorder::begin_frames( )
{
    this->m_phase = recorder_phase_t::replaying;
    this->m_frame = 0;

#if defined( ATLAS_DEV )
    this->m_peak_error  = 0.0f;
    this->m_error_sum   = 0.0f;
    this->m_previous    = steer_sample_t{ };
    this->m_traced_clip = g_cl.m_weapon->m_iClip1( );

    DBGOUT( "recorder: replaying %s, %i frames, aligned in %i, tick %i", this->m_active->m_name.c_str( ),
            static_cast< i32 >( this->m_active->m_frames.size( ) ), this->m_align_commands, g_tf2.m_globals->tickcount );
#endif
}

void movement_recorder::align( )
{
    const route_t          &route{ *this->m_active };
    const recorder_frame_t &start{ route.m_frames[ 0 ] };

    if ( this->took_over( ) )
    {
        this->stop_replay( );
        return;
    }

    if ( this->start_wait( route ) != recorder_wait_t::none )
    {
        this->m_phase = recorder_phase_t::armed;
        return;
    }

    const recorder_physics_t physics{ live_physics( start.m_angles.y, false ) };
    const vec3               velocity{ this->m_start_velocity.x, this->m_start_velocity.y, 0.0f };
    const vec3               goal{ vec3{ start.m_velocity.x, start.m_velocity.y, 0.0f } +
                                   closing_velocity( physics, start.m_origin - this->m_start_origin ) };

    this->m_move = best_move( physics, velocity, goal, vec2{ } );

    const bool settled{ velocity.length2d( ) == 0.0f && this->m_move == vec2{ } };

    const f32  crossing{ 2.0f * math::sqrt( VEC_HULL_MAX.x * physics.m_ground.dt / command_authority( physics ) ) };
    const bool late{ static_cast< f32 >( ++this->m_align_commands ) * physics.m_ground.dt > 2.0f * crossing };

    if ( settled || late )
    {
        this->begin_frames( );
    }
}

void movement_recorder::steer( )
{
    const route_t &route{ *this->m_active };
    const bool     finished{ this->m_frame >= static_cast< i32 >( route.m_frames.size( ) ) };

    if ( finished || this->took_over( ) )
    {
        DBGOUT( "recorder: %s at frame %i, user buttons %x", finished ? "finished" : "taken over", this->m_frame,
                g_cl.m_cmd->buttons & MOVE_BUTTONS );

        this->stop_replay( );
        return;
    }

    const recorder_frame_t &frame{ route.m_frames[ this->m_frame ] };
    const vec3              error{ frame.m_origin - this->m_start_origin };

    if ( error.length2d( ) > VEC_HULL_MAX.x || this->on_other_surface( route ) )
    {
        DBGOUT( "recorder: left the route at frame %i, tick %i, cmd %i, error %.2f %.2f %.2f, live %s vel %.1f %.1f %.1f, "
                "recorded vel %.1f %.1f %.1f",
                this->m_frame, g_tf2.m_globals->tickcount, g_cl.m_cmd->command_number, error.x, error.y, error.z,
                g_cl.m_local->IsOnGround( ) ? "ground" : "air", this->m_start_velocity.x, this->m_start_velocity.y,
                this->m_start_velocity.z, frame.m_velocity.x, frame.m_velocity.y, frame.m_velocity.z );

        this->stop_replay( ATLAS_XORSTR_STATIC( "Replay stopped, you left the route." ) );
        return;
    }

#if defined( ATLAS_DEV )
    if ( !this->m_frame )
    {
        this->m_first_error = error.length2d( );
    }

    this->m_peak_error = math::bigger( this->m_peak_error, error.length2d( ) );
    this->m_error_sum += error.length2d( );
    this->m_sample = steer_sample_t{ error };
#endif

    this->m_move = this->choose_move( route, error );
}

vec2 movement_recorder::choose_move( const route_t &route, const vec3 &error )
{
    const recorder_frame_t &frame{ route.m_frames[ this->m_frame ] };
    const vec2              recorded{ frame.m_forward, frame.m_side };

    if ( this->m_frame + 1 >= static_cast< i32 >( route.m_frames.size( ) ) )
    {
        return recorded;
    }

    const recorder_frame_t &next{ route.m_frames[ this->m_frame + 1 ] };
    const i32               previous{ this->m_frame ? route.m_frames[ this->m_frame - 1 ].m_buttons : 0 };

    const bool               jumps{ ( frame.m_buttons & ~previous & IN_JUMP ) && !g_cl.m_local->IsDucked( ) };
    const recorder_physics_t physics{ live_physics( frame.m_angles.y, !g_cl.m_local->IsOnGround( ) || jumps ) };

    const vec3 velocity{ this->m_start_velocity.x, this->m_start_velocity.y, 0.0f };
    const vec3 target{ next.m_velocity.x, next.m_velocity.y, 0.0f };

    const vec3 residual{ target - command_velocity( physics, vec3{ frame.m_velocity.x, frame.m_velocity.y, 0.0f },
                                                    recorded.x, recorded.y ) };

    const vec2 move{ best_move( physics, velocity, target - residual + closing_velocity( physics, error ), recorded ) };

#if defined( ATLAS_DEV )
    const vec3 expected{ command_velocity( physics, velocity, move.x, move.y ) + residual };

    this->m_sample = steer_sample_t{ error, vec2{ expected.x, expected.y }, vec2{ residual.x, residual.y }, physics.m_air, true };
#endif

    return move;
}

void movement_recorder::apply_view( const recorder_frame_t &frame ) const
{
    CUserCmd &cmd{ *g_cl.m_cmd };

    cmd.viewangles = vec3{ frame.m_angles.x, frame.m_angles.y, 0.0f };

    if ( g_cfg.m_recorder.m_camera_follows && !g_tf2.m_input->CAM_IsThirdPerson( ) )
    {
        g_tf2.m_engine->SetViewAngles( cmd.viewangles );
    }
}

void movement_recorder::apply_alignment( )
{
    CUserCmd &cmd{ *g_cl.m_cmd };

    this->apply_view( this->m_active->m_frames[ 0 ] );

    cmd.forwardmove = this->m_move.x;
    cmd.sidemove    = this->m_move.y;
    cmd.buttons &= ~MOVE_BUTTONS;
}

void movement_recorder::apply_frame( )
{
    const recorder_frame_t &frame{ this->m_active->m_frames[ this->m_frame++ ] };
    CUserCmd               &cmd{ *g_cl.m_cmd };

    const i32 replayed{ g_cfg.m_recorder.m_replay_attacks ? MOVE_BUTTONS | IN_ATTACK | IN_ATTACK2 | IN_RELOAD : MOVE_BUTTONS };

    this->apply_view( frame );

    cmd.forwardmove = this->m_move.x;
    cmd.sidemove    = this->m_move.y;
    cmd.buttons     = ( cmd.buttons & ~replayed ) | ( frame.m_buttons & replayed );

    this->m_keys = command_keys( frame.m_forward, frame.m_side, frame.m_buttons & replayed );
    this->m_pressed |= this->m_keys;

#if defined( ATLAS_DEV )
    const vec3 &velocity{ this->m_start_velocity };
    const vec2  miss{ this->m_previous.m_modeled ? vec2{ velocity.x, velocity.y } - this->m_previous.m_expected : vec2{ } };
    const char *model{ !this->m_sample.m_modeled ? "none" : ( this->m_sample.m_air ? "air" : "ground" ) };

    DBGOUT( "recorder: f %i tick %i cmd %i ack %i %s %i | err %.2f %.2f %.2f | vel %.1f %.1f %.1f rec %.1f %.1f %.1f | "
            "miss %.1f %.1f res %.1f %.1f model %s | move %.0f %.0f rec %.0f %.0f | btn %x rec %x | %s%s",
            this->m_frame - 1, g_tf2.m_globals->tickcount, cmd.command_number, g_tf2.m_cl->command_ack,
            g_cl.m_send_packet ? "send" : "choke", g_tf2.m_cl->chokedcommands, this->m_sample.m_error.x,
            this->m_sample.m_error.y, this->m_sample.m_error.z, velocity.x, velocity.y, velocity.z, frame.m_velocity.x,
            frame.m_velocity.y, frame.m_velocity.z, miss.x, miss.y, this->m_sample.m_residual.x, this->m_sample.m_residual.y,
            model, cmd.forwardmove, cmd.sidemove, frame.m_forward, frame.m_side, cmd.buttons, frame.m_buttons,
            g_cl.m_local->IsOnGround( ) ? "ground" : "air", g_cl.m_local->IsDucked( ) ? " ducked" : "" );

    if ( ( frame.m_buttons & IN_ATTACK ) || g_cl.m_weapon->m_iClip1( ) != this->m_traced_clip )
    {
        trace_weapon( "f", this->m_frame - 1 );
        this->m_traced_clip = g_cl.m_weapon->m_iClip1( );
    }

    this->m_previous = this->m_sample;
#endif
}

void ATLAS_STDCALL movement_recorder::store_worker( void *context )
{
    movement_recorder &self{ *static_cast< movement_recorder * >( context ) };
    store_job_t       &job{ self.m_job };
    const string_view  map{ job.m_map.view( ) };

    switch ( job.m_kind )
    {
        case store_kind_t::list:
        {
            atl::vector< recorder_blob_t > blobs{ };
            job.m_success = store_list( map, blobs, job.m_status );

            for ( const recorder_blob_t &blob : blobs )
            {
                recorder_header_t header{ };
                route_t           route{ };

                if ( !unpack_route( blob.m_bytes, header, route.m_frames ) )
                {
                    continue;
                }

                route.m_name     = blob.m_name;
                route.m_interval = header.m_interval;
                route.m_class    = header.m_class;
                route.m_clip     = header.m_clip;
                measure( route );
                job.m_routes.push_back( static_cast< route_t && >( route ) );
            }

            break;
        }

        case store_kind_t::save:
        {
            const route_t          &route{ job.m_route };
            const recorder_header_t header{ RECORDER_MAGIC, RECORDER_VERSION, route.m_interval, route.m_class, static_cast< u32 >( route.m_frames.size( ) ),
                                            route.m_clip };
            atl::vector< u8 >       bytes{ };
            u32                     status{ };

            job.m_success = pack_route( header, route.m_frames, bytes ) && store_put( map, route.m_name.view( ), bytes, job.m_status );
            job.m_moved   = job.m_success && !job.m_previous.view( ).empty( ) && store_remove( map, job.m_previous.view( ), status );
            break;
        }

        case store_kind_t::remove: job.m_success = store_remove( map, job.m_route.m_name.view( ), job.m_status ); break;
    }

    _InterlockedExchange( &self.m_job_state, 2 );
}

void movement_recorder::poll_store( )
{
    if ( _InterlockedCompareExchange( &this->m_job_state, 2, 2 ) == 2 )
    {
        this->finish_store( );
        this->m_job = store_job_t{ };
        _InterlockedExchange( &this->m_job_state, 0 );
    }

    if ( !this->m_listing_due || this->m_job_state || this->m_map.view( ).empty( ) || !store_ready( ) )
    {
        return;
    }

    this->m_listing_due = false;
    this->m_job         = store_job_t{ };
    this->m_job.m_kind  = store_kind_t::list;
    this->m_job.m_map   = this->m_map;
    this->start_store( );
}

bool movement_recorder::store_idle( ) const
{
    if ( this->m_job_state )
    {
        notify( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Recordings are still syncing, try again in a moment." ) );
        return false;
    }

    if ( !store_ready( ) )
    {
        notify( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "No Cloud session." ) );
        return false;
    }

    return true;
}

void movement_recorder::start_store( )
{
    _InterlockedExchange( &this->m_job_state, 1 );

    if ( !atl::create_hidden_thread( &movement_recorder::store_worker, this ) )
    {
        _InterlockedExchange( &this->m_job_state, 0 );
        notify( notify_kind_t::error, ATLAS_XORSTR_STATIC( "Could not start the recordings worker." ) );
    }
}

void movement_recorder::finish_store( )
{
    const store_job_t &job{ this->m_job };

    if ( job.m_map.view( ) != this->m_map.view( ) )
    {
        return;
    }

    if ( job.m_success )
    {
        switch ( job.m_kind )
        {
            case store_kind_t::list: this->finish_list( ); break;
            case store_kind_t::save: this->finish_save( ); break;
            case store_kind_t::remove: this->finish_remove( ); break;
        }

        return;
    }

    switch ( job.m_kind )
    {
        case store_kind_t::list:
            notify( notify_kind_t::error, store_failure( job.m_status, ATLAS_XORSTR_STATIC( "Could not read the recordings." ) ) );
            break;

        case store_kind_t::save:
            notify( notify_kind_t::error, store_failure( job.m_status, ATLAS_XORSTR_STATIC( "Could not write the recording file." ) ) );
            break;

        case store_kind_t::remove:
            notify( notify_kind_t::error, store_failure( job.m_status, ATLAS_XORSTR_STATIC( "Could not delete the recording file." ) ) );
            break;
    }
}

void movement_recorder::finish_list( )
{
    if ( this->m_active != &this->m_take )
    {
        this->stop_replay( );
    }

    this->m_routes.clear( );

    for ( route_t &route : this->m_job.m_routes )
    {
        this->insert( static_cast< route_t && >( route ) );
    }

    this->publish_catalog( );
}

void movement_recorder::finish_save( )
{
    store_job_t &job{ this->m_job };

    this->stop_replay( );
    this->erase_route( job.m_route.m_name.view( ) );

    if ( job.m_moved )
    {
        this->erase_route( job.m_previous.view( ) );
    } else if ( !job.m_previous.view( ).empty( ) )
    {
        notify( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Saved under the new name, but the old one is still there." ) );
    }

    if ( job.m_take && job.m_take == this->m_take_serial && this->m_has_take )
    {
        this->m_has_take = false;
        this->m_take.m_frames.clear( );
    }

    this->m_selected = job.m_route.m_name;

    const usize index{ this->insert( static_cast< route_t && >( job.m_route ) ) };
    this->publish_catalog( this->m_has_take ? index + 1 : index );
}

void movement_recorder::finish_remove( )
{
    const string_view name{ this->m_job.m_route.m_name.view( ) };

    if ( this->m_active != &this->m_take )
    {
        this->stop_replay( );
    }

    this->erase_route( name );

    if ( this->m_selected.view( ) == name )
    {
        this->m_selected.assign( string_view{ } );
    }

    this->publish_catalog( );
}

void movement_recorder::save_route( string_view route, string_view name )
{
    if ( !configs::valid_name( name ) )
    {
        notify( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Names take lowercase letters, digits, - and _." ) );
        return;
    }

    route_t *source{ this->find( route ) };
    if ( !source )
    {
        notify( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Select a recording first." ) );
        return;
    }

    const bool from_take{ source == &this->m_take };
    if ( !from_take && source->m_name.view( ) == name )
    {
        return;
    }

    if ( from_take && !this->find( name ) && this->m_routes.size( ) >= RECORDER_ROUTE_CAPACITY )
    {
        notify( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "This map already holds 32 recordings." ) );
        return;
    }

    if ( !this->store_idle( ) )
    {
        return;
    }

    store_job_t &job{ this->m_job };
    job                    = store_job_t{ };
    job.m_kind             = store_kind_t::save;
    job.m_map              = this->m_map;
    job.m_take             = from_take ? this->m_take_serial : 0;
    job.m_route.m_interval = source->m_interval;
    job.m_route.m_class    = source->m_class;
    job.m_route.m_clip     = source->m_clip;
    job.m_route.m_length   = source->m_length;
    job.m_route.m_jumps    = source->m_jumps;
    job.m_route.m_name.assign( name );

    if ( !from_take )
    {
        job.m_previous = source->m_name;
    }

    if ( !job.m_route.m_frames.assign( source->m_frames.data( ), source->m_frames.size( ) ) )
    {
        notify( notify_kind_t::error, ATLAS_XORSTR_STATIC( "Could not copy the recording." ) );
        return;
    }

    this->start_store( );
}

void movement_recorder::remove_route( string_view name )
{
    route_t *route{ this->find( name ) };
    if ( !route )
    {
        notify( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Select a recording first." ) );
        return;
    }

    if ( route == &this->m_take )
    {
        this->stop_replay( );
        this->m_has_take = false;
        this->m_take.m_frames.clear( );

        if ( this->m_selected.view( ) == name )
        {
            this->m_selected.assign( string_view{ } );
        }

        this->publish_catalog( );
        return;
    }

    if ( !this->store_idle( ) )
    {
        return;
    }

    this->m_job        = store_job_t{ };
    this->m_job.m_kind = store_kind_t::remove;
    this->m_job.m_map  = this->m_map;
    this->m_job.m_route.m_name.assign( name );
    this->start_store( );
}

usize movement_recorder::insert( route_t &&route )
{
    if ( !this->m_routes.push_back( static_cast< route_t && >( route ) ) )
    {
        return RECORDER_NO_ENTRY;
    }

    usize index{ this->m_routes.size( ) - 1 };

    while ( index && str::compare_ignore_case( this->m_routes[ index - 1 ].m_name.c_str( ), this->m_routes[ index ].m_name.c_str( ) ) > 0 )
    {
        route_t moved{ static_cast< route_t && >( this->m_routes[ index ] ) };
        this->m_routes[ index ]     = static_cast< route_t && >( this->m_routes[ index - 1 ] );
        this->m_routes[ index - 1 ] = static_cast< route_t && >( moved );
        --index;
    }

    return index;
}

void movement_recorder::erase_route( string_view name )
{
    for ( usize index{ }; index < this->m_routes.size( ); ++index )
    {
        if ( this->m_routes[ index ].m_name.view( ) == name )
        {
            this->m_routes.erase( index );
            return;
        }
    }
}

void movement_recorder::measure( route_t &route )
{
    route.m_length = 0.0f;
    route.m_jumps  = 0;

    for ( usize index{ 1 }; index < route.m_frames.size( ); ++index )
    {
        const recorder_frame_t &frame{ route.m_frames[ index ] };
        const recorder_frame_t &previous{ route.m_frames[ index - 1 ] };

        route.m_length += frame.m_origin.dist_to( previous.m_origin );

        if ( ( frame.m_buttons & IN_JUMP ) && !( previous.m_buttons & IN_JUMP ) )
        {
            ++route.m_jumps;
        }
    }
}

movement_recorder::route_t *movement_recorder::find( string_view name )
{
    if ( name.empty( ) )
    {
        return nullptr;
    }

    if ( this->m_has_take && this->m_take.m_name.view( ) == name )
    {
        return &this->m_take;
    }

    for ( route_t &route : this->m_routes )
    {
        if ( route.m_name.view( ) == name )
        {
            return &route;
        }
    }

    return nullptr;
}

const movement_recorder::route_t *movement_recorder::standing_in( ) const
{
    if ( this->m_has_take && this->inside_start( this->m_take ) )
    {
        return &this->m_take;
    }

    for ( const route_t &route : this->m_routes )
    {
        if ( this->inside_start( route ) )
        {
            return &route;
        }
    }

    return nullptr;
}

const movement_recorder::route_t *movement_recorder::preview_route( )
{
    if ( this->m_phase == recorder_phase_t::armed || this->m_phase == recorder_phase_t::aligning )
    {
        return this->m_active;
    }

    if ( const route_t *standing{ this->standing_in( ) } )
    {
        return standing;
    }

    return this->find( this->m_selected.view( ) );
}

bool movement_recorder::ready( ) const
{
    if ( g_cfg.m_recorder.m_enabled && g_tf2.m_engine->IsInGame( ) && g_cl.m_local && g_cl.m_local->IsAlive( ) )
    {
        return true;
    }

    notify( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Turn the recorder on and spawn first." ) );
    return false;
}

bool movement_recorder::inside_start( const route_t &route ) const
{
    const vec3 delta{ route.m_frames[ 0 ].m_origin - g_cl.m_local->m_vecOrigin( ) };
    return delta.length2d( ) <= VEC_HULL_MAX.x && math::abs( delta.z ) <= g_cl.m_local->m_flStepSize( );
}

recorder_wait_t movement_recorder::start_wait( const route_t &route ) const
{
    if ( ( route.m_frames[ 0 ].m_origin - g_cl.m_local->m_vecOrigin( ) ).length2d( ) > VEC_HULL_MAX.x )
    {
        return recorder_wait_t::walk;
    }

    if ( !this->inside_start( route ) )
    {
        return recorder_wait_t::height;
    }

    if ( !g_cl.m_local->IsOnGround( ) )
    {
        return recorder_wait_t::land;
    }

    return g_cfg.m_recorder.m_replay_attacks && !weapon_matches( route.m_clip ) ? recorder_wait_t::weapon : recorder_wait_t::none;
}

bool movement_recorder::on_other_surface( const route_t &route ) const
{
    if ( !g_cl.m_local->IsOnGround( ) )
    {
        return false;
    }

    const i32 first{ math::bigger( this->m_frame - 1, 0 ) };
    const i32 last{ math::smaller( this->m_frame + 1, static_cast< i32 >( route.m_frames.size( ) ) - 1 ) };

    for ( i32 index{ first }; index <= last; ++index )
    {
        if ( math::abs( route.m_frames[ index ].m_origin.z - this->m_start_origin.z ) <= g_cl.m_local->m_flStepSize( ) )
        {
            return false;
        }
    }

    return true;
}

bool movement_recorder::took_over( )
{
    const i32  held{ g_cl.m_cmd->buttons & MOVE_BUTTONS };
    const bool pressed{ ( held & ~this->m_user_held ) != 0 };

    this->m_user_held &= held;
    return pressed;
}

void movement_recorder::draw_markers( ) const
{
    if ( this->m_has_take )
    {
        this->draw_marker( this->m_take, STATUS_WARNING );
    }

    for ( const route_t &route : this->m_routes )
    {
        this->draw_marker( route, g_cfg.m_recorder.m_marker_color );
    }
}

void movement_recorder::draw_marker( const route_t &route, const color_t &value ) const
{
    if ( &route == this->m_active && this->replaying( ) )
    {
        return;
    }

    const vec3 &start{ route.m_frames[ 0 ].m_origin };
    const u8    alpha{ marker_alpha( start.dist_to( g_cl.m_local->m_vecOrigin( ) ) ) };

    if ( !alpha )
    {
        return;
    }

    const bool    focused{ &route == this->m_active || this->inside_start( route ) };
    const color_t faded{ gui::with_alpha( value, alpha ) };

    this->draw_ring( start, VEC_HULL_MAX.x, focused ? faded : gui::with_alpha( faded, MARKER_IDLE_ALPHA ), focused,
                     RING_TICKS );
    ragebot::draw_path_segment( start + vec3{ 0.0f, 0.0f, 2.0f }, start + vec3{ 0.0f, 0.0f, LABEL_LIFT },
                                gui::with_alpha( faded, MARKER_LEADER_ALPHA ) );
    this->draw_label( route, value, alpha );
}

void movement_recorder::draw_label( const route_t &route, const color_t &value, u8 alpha ) const
{
    vec2 screen{ };
    if ( !g_cl.world_to_screen( route.m_frames[ 0 ].m_origin + vec3{ 0.0f, 0.0f, LABEL_LIFT }, screen ) )
    {
        return;
    }

    char meta[ 64 ]{ };
    str::format_to( meta, sizeof( meta ), ATLAS_XORSTR( "{} - {:.2f} s - {} {}" ).get( ), player_class_name( route.m_class ),
                    static_cast< f32 >( route.m_frames.size( ) - 1 ) * route.m_interval, route.m_jumps,
                    route.m_jumps == 1 ? ATLAS_XORSTR_STATIC( "jump" ) : ATLAS_XORSTR_STATIC( "jumps" ) );

    char    key[ 16 ]{ };
    char    hint[ 32 ]{ };
    color_t hint_color{ value };

    if ( &route == this->m_active )
    {
        const vec3 delta{ route.m_frames[ 0 ].m_origin - g_cl.m_local->m_vecOrigin( ) };

        hint_color = STATUS_WARNING;
        recorder_wait_text( this->start_wait( route ), delta.length2d( ), delta.z, hint );
    } else if ( this->m_phase == recorder_phase_t::idle && this->inside_start( route ) )
    {
        if ( g_cfg.m_recorder.m_replay_key.m_key )
        {
            str::copy( key, sizeof( key ), gui::key_name( g_cfg.m_recorder.m_replay_key.m_key ) );
            str::copy( hint, sizeof( hint ), ATLAS_XORSTR( "replay" ).get( ) );
        } else
        {
            str::copy( hint, sizeof( hint ), ATLAS_XORSTR( "replay from Misc > Recorder" ).get( ) );
        }
    }

    const i32 chip_width{ key[ 0 ] ? gui::estimate_tiny_text_width( key ) + LABEL_CHIP_GAP * 2 : 0 };
    const i32 hint_width{ hint[ 0 ] ? ( key[ 0 ] ? chip_width + LABEL_CHIP_GAP : 0 ) + gui::estimate_tiny_text_width( hint ) : 0 };
    const i32 text_width{ math::bigger( math::bigger( gui::estimate_text_width( route.m_name.view( ) ),
                                                      gui::estimate_tiny_text_width( meta ) ),
                                        hint_width ) };

    const i32 width{ 1 + LABEL_STRIPE + LABEL_PADDING * 2 + text_width };
    const i32 body{ LABEL_PADDING_Y * 2 + LABEL_NAME_ROW + LABEL_META_ROW };
    const i32 height{ hint[ 0 ] ? body + 1 + LABEL_HINT_ROW : body };
    const i32 left{ static_cast< i32 >( screen.x ) - width / 2 };
    const i32 top{ static_cast< i32 >( screen.y ) - height };
    const i32 text_left{ left + 1 + LABEL_STRIPE + LABEL_PADDING };

    g_draw.box( rect_t{ left, top, left + width, top + height }, gui::with_alpha( CARD_BACKGROUND, alpha ), true );
    g_draw.box( rect_t{ left, top, left + width - 1, top + height - 1 }, gui::with_alpha( CARD_BORDER, alpha ) );
    g_draw.box( rect_t{ left + 1, top + 1, left + 1 + LABEL_STRIPE, top + height - 1 }, gui::with_alpha( value, alpha ), true );

    const i32 meta_top{ top + LABEL_PADDING_Y + LABEL_NAME_ROW };

    g_draw.text( MENU_FONT( ), point_t{ text_left, label_text_y( top + LABEL_PADDING_Y, LABEL_NAME_ROW ) },
                 gui::with_alpha( MENU_TEXT, alpha ), route.m_name.view( ) );
    g_draw.text( MENU_FONT_TINY( ), point_t{ text_left, indicator_text_y( rect_t{ 0, meta_top, 0, meta_top + LABEL_META_ROW } ) },
                 gui::with_alpha( CONTROL_LABEL, alpha ), meta );

    if ( !hint[ 0 ] )
    {
        return;
    }

    const rect_t hint_row{ text_left, top + body + 1, left + width - LABEL_PADDING, top + height };
    i32          cursor{ text_left };

    g_draw.box( rect_t{ left + 1, top + body, left + width - 1, top + body + 1 }, gui::with_alpha( CARD_BORDER, alpha ), true );

    if ( key[ 0 ] )
    {
        const i32    chip_top{ hint_row.top + ( LABEL_HINT_ROW - LABEL_CHIP_HEIGHT ) / 2 };
        const rect_t chip{ cursor, chip_top, cursor + chip_width, chip_top + LABEL_CHIP_HEIGHT };

        g_draw.box( chip, gui::with_alpha( CONTROL_BACKGROUND, alpha ), true );
        g_draw.box( rect_t{ chip.left, chip.top, chip.right - 1, chip.bottom - 1 }, gui::with_alpha( hint_color, alpha ) );
        g_draw.text( MENU_FONT_TINY( ), point_t{ ( chip.left + chip.right ) / 2, indicator_text_y( chip ) },
                     gui::with_alpha( LIST_TEXT_ACTIVE, alpha ), key, false, render::text_align::center );

        cursor = chip.right + LABEL_CHIP_GAP;
    }

    g_draw.text( MENU_FONT_TINY( ), point_t{ cursor, indicator_text_y( hint_row ) }, gui::with_alpha( hint_color, alpha ), hint );
}

void movement_recorder::draw_paths( )
{
    const color_t &value{ g_cfg.m_recorder.m_route_color };

    if ( this->m_phase == recorder_phase_t::recording )
    {
        const i32 last{ static_cast< i32 >( this->m_take.m_frames.size( ) ) - 1 };
        this->draw_stretch( this->m_take, 0, last, value );
        this->draw_events( this->m_take, 0, last, ROUTE_EVENT_ALPHA );
        return;
    }

    if ( this->m_phase == recorder_phase_t::replaying )
    {
        this->draw_replay( );
        return;
    }

    const route_t *route{ this->preview_route( ) };
    if ( !route )
    {
        return;
    }

    const i32 last{ static_cast< i32 >( route->m_frames.size( ) ) - 1 };
    this->draw_stretch( *route, 0, last, gui::with_alpha( value, ROUTE_PREVIEW_ALPHA ) );
    this->draw_events( *route, 0, last, ROUTE_PREVIEW_EVENT_ALPHA );
    this->draw_ring( route->m_frames[ last ].m_origin, END_RING_RADIUS, value, false, 0 );
}

void movement_recorder::draw_replay( ) const
{
    const route_t &route{ *this->m_active };
    const color_t &value{ g_cfg.m_recorder.m_route_color };

    const i32 last{ static_cast< i32 >( route.m_frames.size( ) ) - 1 };
    const i32 at{ math::smaller( this->m_frame, last ) };
    const i32 ahead{ math::smaller( at + math::smaller( TIME_TO_TICKS( 1.0f ), static_cast< i32 >( ROUTE_AHEAD_LIMIT ) - 1 ), last ) };
    const i32 count{ ahead - at + 1 };

    vec3 window[ ROUTE_AHEAD_LIMIT ]{ };
    for ( i32 index{ }; index < count; ++index )
    {
        window[ index ] = route.m_frames[ at + index ].m_origin;
    }

    this->draw_stretch( route, 0, at, gui::with_alpha( g_cfg.m_recorder.m_traveled_color, ROUTE_TRAVELED_ALPHA ) );

    const ragebot::path_pen_t pen{ ragebot::path_pen( g_cfg.m_recorder.m_route_style, false ) };
    for ( i32 index{ 1 }; index < count; ++index )
    {
        ragebot::draw_path_step( pen, window, count, index, value );
    }

    this->draw_stretch( route, ahead, last, gui::with_alpha( value, ROUTE_REMAINDER_ALPHA ) );
    this->draw_events( route, 0, at, ROUTE_PAST_EVENT_ALPHA );
    this->draw_events( route, at + 1, last, ROUTE_EVENT_ALPHA );
    this->draw_ring( route.m_frames[ last ].m_origin, END_RING_RADIUS, value, true, RING_TICKS * at / last );
}

void movement_recorder::draw_stretch( const route_t &route, i32 first, i32 last, const color_t &value ) const
{
    const i32 stride{ ( last - first + ROUTE_DRAW_SEGMENTS - 1 ) / ROUTE_DRAW_SEGMENTS };

    for ( i32 from{ first }; from < last; from += stride )
    {
        const i32 to{ math::smaller( from + stride, last ) };
        ragebot::draw_path_segment( route.m_frames[ from ].m_origin, route.m_frames[ to ].m_origin, value );
    }
}

void movement_recorder::draw_events( const route_t &route, i32 first, i32 last, u8 alpha ) const
{
    const color_t jump{ gui::with_alpha( CHECKBOX_MARK_COLOR, alpha ) };
    const color_t attack{ gui::with_alpha( STATUS_WARNING, alpha ) };
    const i32     end{ static_cast< i32 >( route.m_frames.size( ) ) - 1 };

    for ( i32 index{ math::bigger( first, 1 ) }; index <= last; ++index )
    {
        const recorder_frame_t &frame{ route.m_frames[ index ] };
        const i32               pressed{ frame.m_buttons & ~route.m_frames[ index - 1 ].m_buttons };

        if ( pressed & IN_JUMP )
        {
            const vec3 along{ route.m_frames[ math::smaller( index + 1, end ) ].m_origin - route.m_frames[ index - 1 ].m_origin };
            const f32  length{ along.length2d( ) };
            const vec3 side{ length > 0.1f ? vec3{ -along.y / length, along.x / length, 0.0f } * ROUTE_CARET_SPREAD
                                           : vec3{ 0.0f, ROUTE_CARET_SPREAD, 0.0f } };
            const vec3 tip{ frame.m_origin + vec3{ 0.0f, 0.0f, ROUTE_CARET_HEIGHT } };

            ragebot::draw_path_segment( frame.m_origin + side, tip, jump );
            ragebot::draw_path_segment( tip, frame.m_origin - side, jump );
        }

        if ( g_cfg.m_recorder.m_replay_attacks && ( pressed & ( IN_ATTACK | IN_ATTACK2 ) ) )
        {
            ragebot::draw_path_segment( frame.m_origin - vec3{ ROUTE_BURST_SIZE, 0.0f, 0.0f },
                                        frame.m_origin + vec3{ ROUTE_BURST_SIZE, 0.0f, 0.0f }, attack );
            ragebot::draw_path_segment( frame.m_origin - vec3{ 0.0f, ROUTE_BURST_SIZE, 0.0f },
                                        frame.m_origin + vec3{ 0.0f, ROUTE_BURST_SIZE, 0.0f }, attack );
            ragebot::draw_path_segment( frame.m_origin - vec3{ 0.0f, 0.0f, ROUTE_BURST_SIZE },
                                        frame.m_origin + vec3{ 0.0f, 0.0f, ROUTE_BURST_SIZE }, attack );
        }
    }
}

void movement_recorder::draw_ring( const vec3 &center, f32 radius, const color_t &value, bool spinning, i32 lit ) const
{
    const vec3    lifted{ center + vec3{ 0.0f, 0.0f, 1.0f } };
    const color_t edge{ gui::with_alpha( value, RING_EDGE_ALPHA ) };

    for ( i32 segment{ }; segment < RING_SEGMENTS; ++segment )
    {
        const f32 from{ 2.0f * PI * static_cast< f32 >( segment ) / static_cast< f32 >( RING_SEGMENTS ) };
        const f32 to{ 2.0f * PI * static_cast< f32 >( segment + 1 ) / static_cast< f32 >( RING_SEGMENTS ) };

        ragebot::draw_path_segment( lifted + vec3{ math::cos( from ), math::sin( from ), 0.0f } * radius,
                                    lifted + vec3{ math::cos( to ), math::sin( to ), 0.0f } * radius, edge );
    }

    if ( !spinning )
    {
        return;
    }

    const f64 turns{ g_tf2.m_engine->Plat_FloatTime( ) * RING_TURNS_PER_SECOND };
    const f32 spin{ static_cast< f32 >( ( turns - static_cast< f64 >( static_cast< i32 >( turns ) ) ) * 2.0 * PI ) };

    for ( i32 tick{ }; tick < RING_TICKS; ++tick )
    {
        const f32  angle{ spin + 2.0f * PI * static_cast< f32 >( tick ) / static_cast< f32 >( RING_TICKS ) };
        const vec3 outward{ math::cos( angle ), math::sin( angle ), 0.0f };
        const bool on{ tick < lit };

        ragebot::draw_path_segment( lifted + outward * radius, lifted + outward * ( radius - ( on ? RING_TICK_LIT : RING_TICK_DIM ) ),
                                    on ? value : gui::with_alpha( value, RING_DIM_ALPHA ) );
    }
}
