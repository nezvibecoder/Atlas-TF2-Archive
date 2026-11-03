#include "projectile_warning.hpp"

#include "entity_cache.hpp"
#include "esp.hpp"
#include "networking.hpp"
#include "splash.hpp"
#include "tickbase.hpp"

#include "math/math.hpp"
#include "render/icons.hpp"
#include "render/render.hpp"
#include "util/format.hpp"

ATLAS_NO_DESTROY projectile_warning g_projectile_warning{ };

constexpr i32     TRACE_INTERVAL{ 12 };
constexpr f32     LAUNCH_ROLL{ 1200.0f };
constexpr f32     BADGE_RADIUS{ 15.0f };
constexpr f32     BADGE_RING{ 2.5f };
constexpr i32     PILL_HEIGHT{ 19 };
constexpr i32     LABEL_DROP{ 2 };
constexpr f32     RADAR_RADIUS{ 80.0f };
constexpr f32     RADAR_SPAN{ 0.38f };
constexpr f32     EDGE_MARGIN{ 48.0f };
constexpr f32     PULSE_WINDOW{ 0.7f };
constexpr i32     ZONE_POINTS{ 32 };
constexpr i32     TRAIL_SEGMENTS{ 24 };
constexpr f32     HU_TO_METERS{ 0.01905f };
constexpr color_t PLATE{ 18, 18, 20, 200 };
constexpr color_t TRACK{ 255, 255, 255, 36 };
constexpr color_t INK{ 255, 255, 255, 255 };

ATLAS_FORCE_INLINE warning_kind kind_of( C_BaseEntity *projectile, ETFClassID class_id )
{
    switch ( class_id )
    {
        case ETFClassID::CTFProjectile_Rocket:
        case ETFClassID::CTFProjectile_SentryRocket:
        case ETFClassID::CTFProjectile_EnergyBall: return warning_kind::rocket;
        case ETFClassID::CTFProjectile_Flare: return warning_kind::flare;
        case ETFClassID::CTFProjectile_Arrow:
        case ETFClassID::CTFProjectile_HealingBolt: return warning_kind::arrow;
        case ETFClassID::CTFGrenadePipebombProjectile: break;
        default: return warning_kind::none;
    }

    switch ( projectile->As< C_TFGrenadePipebombProjectile * >( )->m_iType( ) )
    {
        case TF_GL_MODE_REMOTE_DETONATE: return warning_kind::sticky;
        default: return warning_kind::none;
    }
}

ATLAS_FORCE_INLINE i32 deflections( C_BaseEntity *projectile, warning_kind kind )
{
    return kind == warning_kind::sticky ? projectile->As< C_TFGrenadePipebombProjectile * >( )->m_iDeflected( )
                                        : projectile->As< C_TFProjectile_Rocket * >( )->m_iDeflected( );
}

ATLAS_FORCE_INLINE vec3 launch_velocity( C_BaseEntity *projectile, warning_kind kind )
{
    return kind == warning_kind::sticky ? projectile->As< C_TFGrenadePipebombProjectile * >( )->m_vInitialVelocity( )
                                        : projectile->As< C_TFProjectile_Rocket * >( )->m_vInitialVelocity( );
}

ATLAS_FORCE_INLINE void cell_of( const vec3 &origin, vec3 &low, vec3 &high )
{
    low  = origin;
    high = origin;

    for ( i32 axis{ }; axis < 3; ++axis )
    {
        if ( origin[ axis ] >= 0.0f )
        {
            high[ axis ] += 1.0f;
        }

        if ( origin[ axis ] <= 0.0f )
        {
            low[ axis ] -= 1.0f;
        }
    }
}

ATLAS_FORCE_INLINE vec3 into_cell( const vec3 &position, const vec3 &origin )
{
    vec3 low{ }, high{ };
    cell_of( origin, low, high );

    vec3 out{ };
    for ( i32 axis{ }; axis < 3; ++axis )
    {
        out[ axis ] = math::clamp( position[ axis ], low[ axis ], high[ axis ] );
    }

    return out;
}

ATLAS_FORCE_INLINE void forget_flight( warning_threat_t &threat )
{
    threat.m_count   = 0;
    threat.m_landed  = false;
    threat.m_resting = false;
}

ATLAS_FORCE_INLINE color_t faded( color_t value, f32 opacity )
{
    value.a = static_cast< u8 >( static_cast< f32 >( value.a ) * opacity );
    return value;
}

ATLAS_FORCE_INLINE void draw_segment( const vec3 &from, const vec3 &to, color_t color, f32 thickness )
{
    vec2 start{ }, end{ };
    if ( g_cl.clip_segment_to_screen( from, to, start, end ) )
    {
        g_draw.line( start, end, color, thickness );
    }
}

ATLAS_FORCE_INLINE f32 view_bearing( const vec3 &world )
{
    const auto &m{ g_cl.m_w2s };
    vec2        forward{ m[ 3 ][ 0 ], m[ 3 ][ 1 ] };
    forward.normalize( );

    const vec2 right{ forward.y, -forward.x };
    const vec3 offset{ world - g_cl.m_local->GetAbsOrigin( ) };
    return math::atan2( offset.x * right.x + offset.y * right.y, offset.x * forward.x + offset.y * forward.y );
}

ATLAS_FORCE_INLINE vec2 whole_pixel( vec2 point )
{
    return vec2{ static_cast< f32 >( static_cast< i32 >( point.x + 0.5f ) ), static_cast< f32 >( static_cast< i32 >( point.y + 0.5f ) ) };
}

ATLAS_FORCE_INLINE bool anchor( const vec3 &world, vec2 &out, f32 &pointing )
{
    const vec2 middle{ static_cast< f32 >( g_cl.m_screen_width ) * 0.5f, static_cast< f32 >( g_cl.m_screen_height ) * 0.5f };

    vec2 screen{ };
    vec2 radial{ };
    if ( g_cl.world_to_screen( world, screen ) )
    {
        if ( screen.x > EDGE_MARGIN && screen.x < middle.x * 2.0f - EDGE_MARGIN &&
             screen.y > EDGE_MARGIN && screen.y < middle.y * 2.0f - EDGE_MARGIN )
        {
            out = whole_pixel( screen );
            return false;
        }

        radial = vec2{ screen.x - middle.x, screen.y - middle.y };
    } else
    {
        const f32 bearing{ view_bearing( world ) };
        radial = vec2{ math::sin( bearing ), -math::cos( bearing ) };
    }

    radial.normalize( );

    const f32 reach_x{ radial.x != 0.0f ? ( middle.x - EDGE_MARGIN ) / math::abs( radial.x ) : FLT_MAX };
    const f32 reach_y{ radial.y != 0.0f ? ( middle.y - EDGE_MARGIN ) / math::abs( radial.y ) : FLT_MAX };
    const f32 reach{ math::smaller( reach_x, reach_y ) };

    out      = whole_pixel( vec2{ middle.x + radial.x * reach, middle.y + radial.y * reach } );
    pointing = math::atan2( radial.y, radial.x );
    return true;
}

ATLAS_FORCE_INLINE void draw_pointer( vec2 centre, f32 pointing, f32 distance, color_t color )
{
    const vec2    along{ math::cos( pointing ), math::sin( pointing ) };
    const vec2    across{ -along.y, along.x };
    const vec2    tip{ centre.x + along.x * ( distance + 7.0f ), centre.y + along.y * ( distance + 7.0f ) };
    const vec2    base{ centre.x + along.x * distance, centre.y + along.y * distance };
    const vec2    points[ 3 ]{ tip, { base.x + across.x * 5.0f, base.y + across.y * 5.0f }, { base.x - across.x * 5.0f, base.y - across.y * 5.0f } };
    const color_t colors[ 3 ]{ color, color, color };
    g_draw.polygon( points, colors, 3 );
}

ATLAS_FORCE_INLINE void draw_atlas_icon( u32 slot, u32 kind, u32 size, vec2 centre, color_t color )
{
    const i32 half{ static_cast< i32 >( size / 2 ) };
    g_draw.icon( slot, kind, point_t{ static_cast< i32 >( centre.x ) - half, static_cast< i32 >( centre.y ) - half }, color );
}

ATLAS_FORCE_INLINE void draw_pill( vec2 top_centre, const char *label, bool timer )
{
    const i32 cap{ ( PILL_HEIGHT + 1 ) / 2 };
    const i32 timer_width{ timer ? static_cast< i32 >( render::icons::SIZE_SMALL ) + 4 : 0 };
    const i32 width{ g_render.get_text_width( ATLAS_XORSTR( "ESP Name" ).get( ), label ) + timer_width + 18 };
    const i32 left{ static_cast< i32 >( top_centre.x ) - width / 2 };
    const i32 top{ static_cast< i32 >( top_centre.y ) };
    const f32 radius{ static_cast< f32 >( PILL_HEIGHT ) * 0.5f };
    const f32 middle{ static_cast< f32 >( top ) + radius };

    g_draw.box( rect_t{ left + cap, top, left + width - cap, top + PILL_HEIGHT }, PLATE, true );
    g_draw.arc( vec2{ static_cast< f32 >( left + cap ), middle }, radius, radius, PI * 0.5f, PI, PLATE );
    g_draw.arc( vec2{ static_cast< f32 >( left + width - cap ), middle }, radius, radius, -PI * 0.5f, PI, PLATE );

    if ( timer )
    {
        g_draw.icon( render::icons::ID_TIMER, render::icons::KIND_SMALL,
                     point_t{ left + 9, top + ( PILL_HEIGHT - static_cast< i32 >( render::icons::SIZE_SMALL ) ) / 2 }, INK );
    }

    g_draw.text( ATLAS_XORSTR( "ESP Name" ).get( ), point_t{ left + 9 + timer_width, top + ( PILL_HEIGHT - ESP_NAME_HEIGHT ) / 2 + LABEL_DROP }, INK, label );
}

ATLAS_FORCE_INLINE u32 icon_of( warning_kind kind )
{
    switch ( kind )
    {
        case warning_kind::rocket: return render::icons::ID_ROCKET;
        case warning_kind::sticky: return render::icons::ID_SUN;
        case warning_kind::arrow: return render::icons::ID_ARROW_UP_RIGHT;
        default: return render::icons::ID_FLAME;
    }
}

void projectile_warning::on_network_update( )
{
    if ( !this->should_run( ) )
    {
        this->m_count = 0;
        return;
    }

    for ( i32 index{ }; index < this->m_count; ++index )
    {
        this->m_threats[ index ].m_live = false;
    }

    const i32 team{ g_cl.m_local->m_iTeamNum( ) };

    for ( const auto *cached : g_entity_cache.projectiles( ) )
    {
        C_BaseEntity *projectile{ cached->m_entity };
        if ( !cached->networked( ) || projectile->IsDormant( ) || projectile->m_iTeamNum( ) == team )
        {
            continue;
        }

        const warning_kind kind{ kind_of( projectile, cached->m_class_id ) };
        if ( kind == warning_kind::none ||
             ( kind != warning_kind::sticky && projectile->m_MoveType( ) != MOVETYPE_FLY && projectile->m_MoveType( ) != MOVETYPE_FLYGRAVITY ) )
        {
            continue;
        }

        warning_threat_t *threat{ this->track( cached->m_handle, kind, projectile ) };
        if ( !threat )
        {
            continue;
        }

        threat->m_live = true;
        this->update( *threat, projectile, cached->m_class_id );
    }

    this->drop_gone( );
}

void projectile_warning::paint( )
{
    if ( !this->should_run( ) || !this->m_count )
    {
        return;
    }

    const i32 frame{ g_tickbase.send_frame( ) };
    const i32 now{ frame < 0 ? TIME_TO_TICKS( g_tf2.m_globals->curtime ) : frame + g_networking.send_delay( ).ticks( ) };

    const config::proj_warning_t &settings{ g_cfg.m_projectile_warning };

    if ( settings.m_style == config::PROJECTILE_WARNING_RADAR )
    {
        const vec2 middle{ static_cast< f32 >( g_cl.m_screen_width ) * 0.5f, static_cast< f32 >( g_cl.m_screen_height ) * 0.5f };
        g_draw.arc( middle, RADAR_RADIUS + 1.0f, 2.0f, 0.0f, PI * 2.0f, TRACK );
    }

    for ( i32 index{ }; index < this->m_count; ++index )
    {
        const warning_threat_t &threat{ this->m_threats[ index ] };
        if ( ( !threat.m_landed && !threat.m_direct ) ||
             ( settings.m_only_threats && !threat.m_danger ) ||
             ( threat.m_resting && threat.m_kind == warning_kind::sticky && !settings.m_resting_stickies ) )
        {
            continue;
        }

        C_BaseEntity *projectile{ g_entity_cache.resolve( threat.m_handle ) };
        if ( !projectile )
        {
            continue;
        }

        this->draw_threat( threat, projectile, now );
    }
}

void projectile_warning::on_level_shutdown( )
{
    this->m_count = 0;
}

bool projectile_warning::should_run( ) const
{
    return g_cfg.m_projectile_warning.m_enabled && g_cl.m_local && g_cl.m_local->IsAlive( );
}

warning_threat_t *projectile_warning::track( u32 handle, warning_kind kind, C_BaseEntity *projectile )
{
    for ( i32 index{ }; index < this->m_count; ++index )
    {
        if ( this->m_threats[ index ].m_handle == handle )
        {
            return &this->m_threats[ index ];
        }
    }

    if ( this->m_count >= MAX_THREATS )
    {
        return nullptr;
    }

    warning_threat_t &threat{ this->m_threats[ this->m_count++ ] };

    threat              = warning_threat_t{ };
    threat.m_handle     = handle;
    threat.m_kind       = kind;
    threat.m_spawn_time = projectile->m_flSimulationTime( );
    return &threat;
}

void projectile_warning::update( warning_threat_t &threat, C_BaseEntity *projectile, ETFClassID class_id )
{
    const vec3 origin{ projectile->m_vecOrigin( ) };
    const i32  deflected{ deflections( projectile, threat.m_kind ) };

    const bool touched{ threat.m_kind == warning_kind::sticky && projectile->As< C_TFGrenadePipebombProjectile * >( )->m_bTouched( ) };

    if ( deflected != threat.m_deflected || ( threat.m_resting && !touched ) )
    {
        threat.m_deflected    = deflected;
        threat.m_first_origin = origin;
        threat.m_first_time   = projectile->m_flSimulationTime( );
        threat.m_first_tick   = TIME_TO_TICKS( threat.m_first_time );
        threat.m_seen         = -1;
        threat.m_radius       = this->blast_radius( projectile, class_id );
        forget_flight( threat );
    }

    const i32 seen{ TIME_TO_TICKS( projectile->m_flSimulationTime( ) ) };
    if ( seen != threat.m_seen )
    {
        threat.m_seen = seen;

        if ( touched )
        {
            this->rest( threat, projectile );
        } else if ( !this->follows( threat, origin ) )
        {
            this->seed( threat, projectile, origin );
        }
    }

    this->meet( threat );
}

bool projectile_warning::follows( warning_threat_t &threat, const vec3 &origin ) const
{
    const i32 index{ threat.m_seen - threat.m_seed_tick };
    if ( threat.m_resting || index < 0 || index >= threat.m_count )
    {
        return false;
    }

    vec3 low{ }, high{ };
    cell_of( origin, low, high );

    const vec3 &point{ threat.m_path[ index ] };
    vec3        offset_low{ };
    vec3        offset_high{ };
    for ( i32 axis{ }; axis < 3; ++axis )
    {
        offset_low[ axis ]  = math::bigger( threat.m_offset_low[ axis ], low[ axis ] - point[ axis ] );
        offset_high[ axis ] = math::smaller( threat.m_offset_high[ axis ], high[ axis ] - point[ axis ] );

        if ( offset_low[ axis ] > offset_high[ axis ] )
        {
            return false;
        }
    }

    threat.m_offset_low  = offset_low;
    threat.m_offset_high = offset_high;
    return true;
}

void projectile_warning::seed( warning_threat_t &threat, C_BaseEntity *projectile, const vec3 &origin )
{
    const f32 span{ projectile->m_flSimulationTime( ) - threat.m_first_time };
    const i32 index{ threat.m_seen - threat.m_seed_tick };

    vec3 position{ origin };
    vec3 velocity{ };
    if ( !threat.m_resting && threat.m_count >= 3 && index >= 1 )
    {
        const i32  at{ math::smaller( index, threat.m_count - 2 ) };
        const f32  beyond{ TICKS_TO_TIME( index - at ) };
        const f32  gravity{ this->gravity_of( threat, projectile ) };
        vec3       carried{ ( threat.m_path[ at + 1 ] - threat.m_path[ at - 1 ] ) / ( TICK_INTERVAL * 2.0f ) };
        const vec3 estimate{ threat.m_path[ at ] + ( threat.m_offset_low + threat.m_offset_high ) * 0.5f + carried * beyond -
                             vec3{ 0.0f, 0.0f, gravity * beyond * beyond * 0.5f } };

        carried.z -= gravity * beyond;
        position = into_cell( estimate, origin );
        velocity = carried + ( position - estimate ) / TICKS_TO_TIME( index );
    } else if ( span > 0.0f )
    {
        velocity = this->chord_velocity( threat, projectile, span );

        if ( threat.m_kind == warning_kind::rocket && threat.m_deflected > 0 )
        {
            velocity = velocity.normalized( ) * launch_velocity( projectile, threat.m_kind ).length( );
        }
    } else if ( threat.m_deflected == 0 && threat.m_first_time == threat.m_spawn_time )
    {
        velocity = launch_velocity( projectile, threat.m_kind );
    } else
    {
        forget_flight( threat );
        return;
    }

    projectile_setup_t setup{ };
    if ( !this->setup_flight( threat, projectile, position, velocity, setup ) )
    {
        forget_flight( threat );
        return;
    }

    impact_t impact{ };
    threat.m_path[ 0 ]    = position;
    threat.m_count        = 1 + g_proj_sim.simulate( setup, &threat.m_path[ 1 ], warning_threat_t::MAX_POINTS, TRACE_INTERVAL, impact );
    threat.m_seed_tick    = threat.m_seen;
    threat.m_resting      = false;
    threat.m_hull_mins    = setup.m_hull_centre - setup.m_hull;
    threat.m_hull_maxs    = setup.m_hull_centre + setup.m_hull;
    threat.m_landing      = threat.m_path[ threat.m_count - 1 ];
    threat.m_normal       = impact.m_hit ? impact.m_normal : vec3{ 0.0f, 0.0f, 1.0f };
    threat.m_landed       = impact.m_hit && !( impact.m_trace.surface.flags & SURF_SKY );
    threat.m_landing_tick = threat.m_seed_tick + ( impact.m_hit ? TIME_TO_TICKS( impact.m_time ) : threat.m_count - 1 );

    vec3 low{ }, high{ };
    cell_of( origin, low, high );
    threat.m_offset_low  = low - position;
    threat.m_offset_high = high - position;
}

void projectile_warning::rest( warning_threat_t &threat, C_BaseEntity *projectile ) const
{
    threat.m_resting      = true;
    threat.m_count        = 0;
    threat.m_landed       = true;
    threat.m_landing      = projectile->m_vecOrigin( );
    threat.m_normal       = vec3{ 0.0f, 0.0f, 1.0f };
    threat.m_landing_tick = this->arm_tick( threat, projectile );
}

void projectile_warning::meet( warning_threat_t &threat ) const
{
    threat.m_direct      = false;
    threat.m_impact      = threat.m_landing;
    threat.m_impact_tick = threat.m_landing_tick;

    if ( threat.m_kind != warning_kind::sticky && !threat.m_resting )
    {
        const vec3 origin{ g_cl.m_local->GetAbsOrigin( ) };
        const vec3 mins{ origin + g_cl.m_local->m_vecMins( ) - threat.m_hull_maxs };
        const vec3 maxs{ origin + g_cl.m_local->m_vecMaxs( ) - threat.m_hull_mins };

        for ( i32 index{ math::bigger( threat.m_seen - threat.m_seed_tick, 0 ) }; index + 1 < threat.m_count; ++index )
        {
            const vec3 &from{ threat.m_path[ index ] };
            const vec3 &to{ threat.m_path[ index + 1 ] };
            const f32   fraction{ math::segment_box_fraction( from, to, mins, maxs ) };
            if ( fraction == FLT_MAX )
            {
                continue;
            }

            threat.m_direct      = true;
            threat.m_impact      = from + ( to - from ) * fraction;
            threat.m_impact_tick = threat.m_seed_tick + index + 1;
            break;
        }
    }

    threat.m_danger = threat.m_direct || ( ( threat.m_landed || threat.m_resting ) && this->reaches_us( threat.m_impact, threat.m_radius ) );
}

bool projectile_warning::setup_flight( const warning_threat_t &threat, C_BaseEntity *projectile, const vec3 &position,
                                       const vec3 &velocity, projectile_setup_t &out ) const
{
    if ( threat.m_kind == warning_kind::sticky )
    {
        return g_proj_sim.resume_sticky( projectile, position, velocity, vec3{ LAUNCH_ROLL, 0.0f, 0.0f }, out );
    }

    out = projectile_setup_t{ };

    out.m_position    = position;
    out.m_speed       = velocity.length( );
    out.m_gravity     = this->gravity_of( threat, projectile ) / g_tf2.sv_gravity->GetFloat( );
    out.m_lifetime    = TICKS_TO_TIME( warning_threat_t::MAX_POINTS );
    out.m_movetype    = out.m_gravity > 0.0f ? projectile_movetype::fly_gravity : projectile_movetype::fly;
    out.m_hull        = ( projectile->m_vecMaxs( ) - projectile->m_vecMins( ) ) * 0.5f;
    out.m_hull_centre = ( projectile->m_vecMaxs( ) + projectile->m_vecMins( ) ) * 0.5f;
    out.m_valid       = out.m_speed > 0.0f;

    math::vector_angles( velocity, out.m_angles );
    return out.m_valid;
}

vec3 projectile_warning::chord_velocity( const warning_threat_t &threat, C_BaseEntity *projectile, f32 span ) const
{
    vec3 velocity{ ( projectile->m_vecOrigin( ) - threat.m_first_origin ) / span };
    velocity.z -= this->gravity_of( threat, projectile ) * span * 0.5f;
    return velocity;
}

f32 projectile_warning::gravity_of( const warning_threat_t &threat, C_BaseEntity *projectile ) const
{
    if ( threat.m_kind == warning_kind::sticky )
    {
        return g_tf2.sv_gravity->GetFloat( );
    }

    if ( projectile->m_MoveType( ) != MOVETYPE_FLYGRAVITY )
    {
        return 0.0f;
    }

    const f32 scale{ projectile->m_flGravity( ) };
    return g_tf2.sv_gravity->GetFloat( ) * ( scale != 0.0f ? scale : 1.0f );
}

f32 projectile_warning::blast_radius( C_BaseEntity *projectile, ETFClassID class_id ) const
{
    switch ( class_id )
    {
        case ETFClassID::CTFProjectile_SentryRocket: return TF_ROCKET_RADIUS;
        case ETFClassID::CTFProjectile_Arrow:
        case ETFClassID::CTFProjectile_HealingBolt: return 0.0f;

        case ETFClassID::CTFProjectile_Flare:
        {
            C_BaseEntity *launcher{ projectile->As< C_TFProjectile_Flare * >( )->m_hLauncher( ).Get( ) };
            return launcher ? launcher->As< C_TFWeaponBase * >( )->GetProjectileBlastRadius( ) : 0.0f;
        }

        case ETFClassID::CTFGrenadePipebombProjectile:
        {
            C_BaseEntity *launcher{ projectile->As< C_TFGrenadePipebombProjectile * >( )->m_hLauncher( ).Get( ) };
            return g_cl.attrib_hook( launcher, TF_ROCKET_RADIUS, ATLAS_XORSTR( "mult_explosion_radius" ).get( ) );
        }

        default:
        {
            C_BaseEntity *launcher{ projectile->As< C_TFProjectile_Rocket * >( )->m_hLauncher( ).Get( ) };
            return g_cl.attrib_hook( launcher, TF_ROCKET_RADIUS, ATLAS_XORSTR( "mult_explosion_radius" ).get( ) );
        }
    }
}

i32 projectile_warning::arm_tick( const warning_threat_t &threat, C_BaseEntity *projectile ) const
{
    C_BaseEntity *launcher{ projectile->As< C_TFGrenadePipebombProjectile * >( )->m_hLauncher( ).Get( ) };
    return TIME_TO_TICKS( threat.m_spawn_time + g_cl.attrib_hook( launcher, g_tf2.tf_grenadelauncher_livetime->GetFloat( ),
                                                                  ATLAS_XORSTR( "sticky_arm_time" ).get( ) ) );
}

bool projectile_warning::reaches_us( const vec3 &point, f32 radius ) const
{
    C_TFPlayer           *local{ g_cl.m_local };
    const vec3            origin{ local->GetAbsOrigin( ) };
    const splash_target_t target{ local, origin, origin + local->m_vecMins( ), origin + local->m_vecMaxs( ),
                                  splash::player_eye( local, origin, local->m_fFlags( ) ) };
    return splash::reaches( point, radius, target );
}

void projectile_warning::drop_gone( )
{
    i32 kept{ };
    for ( i32 index{ }; index < this->m_count; ++index )
    {
        if ( !this->m_threats[ index ].m_live )
        {
            continue;
        }

        if ( kept != index )
        {
            this->m_threats[ kept ] = this->m_threats[ index ];
        }

        ++kept;
    }

    this->m_count = kept;
}

void projectile_warning::draw_threat( const warning_threat_t &threat, C_BaseEntity *projectile, i32 now ) const
{
    const config::proj_warning_t &settings{ g_cfg.m_projectile_warning };

    const color_t color{ threat.m_danger ? settings.m_danger_color : settings.m_color };
    const vec3    position{ projectile->GetAbsOrigin( ) };
    const i32     left{ math::bigger( threat.m_impact_tick - now, 0 ) };
    const i32     total{ math::bigger( threat.m_impact_tick - threat.m_first_tick, 1 ) };
    const f32     progress{ math::clamp( static_cast< f32 >( left ) / static_cast< f32 >( total ), 0.0f, 1.0f ) };
    const f32     remaining{ TICKS_TO_TIME( left ) };

    char label[ 16 ]{ };
    if ( threat.m_resting && threat.m_kind == warning_kind::sticky && !left )
    {
        str::format_to( label, sizeof( label ), ATLAS_XORSTR_STATIC( "armed" ) );
    } else
    {
        str::format_to( label, sizeof( label ), ATLAS_XORSTR_STATIC( "{:.1f}s" ), static_cast< f64 >( remaining ) );
    }

    switch ( settings.m_style )
    {
        case config::PROJECTILE_WARNING_BADGE: this->draw_badge( threat, position, color, progress, remaining, label ); break;
        case config::PROJECTILE_WARNING_RADAR: this->draw_radar( position, color, progress, label ); break;

        case config::PROJECTILE_WARNING_TRAIL:
        {
            this->draw_trail( threat, position, color, 2.0f );
            this->draw_zone( threat, faded( color, 0.5f ), 0.0f );
            this->draw_impact_mark( threat, color );
            break;
        }

        case config::PROJECTILE_WARNING_ZONE:
        {
            this->draw_zone( threat, color, 1.0f - progress );

            vec2 centre{ };
            f32  pointing{ };
            anchor( threat.m_impact, centre, pointing );
            draw_pill( vec2{ centre.x, centre.y - static_cast< f32 >( PILL_HEIGHT / 2 ) }, label, true );
            break;
        }

        default:
        {
            this->draw_trail( threat, position, faded( color, 0.45f ), 1.5f );
            this->draw_zone( threat, faded( color, 0.35f ), 0.0f );
            this->draw_badge( threat, position, color, progress, remaining, label );
            break;
        }
    }
}

void projectile_warning::draw_trail( const warning_threat_t &threat, const vec3 &from, color_t color, f32 thickness ) const
{
    if ( threat.m_count < 2 )
    {
        return;
    }

    const i32 shown{ TIME_TO_TICKS( g_tf2.m_globals->curtime ) - 1 - threat.m_seed_tick };
    const i32 first{ math::clamp( shown + 1, 1, threat.m_count - 1 ) };
    const i32 last{ math::clamp( threat.m_impact_tick - threat.m_seed_tick, first, threat.m_count - 1 ) };
    const i32 stride{ math::bigger( ( last - first ) / TRAIL_SEGMENTS, 1 ) };

    vec3 previous{ from };
    for ( i32 index{ first }; index < last; index += stride )
    {
        draw_segment( previous, threat.m_path[ index ], color, thickness );
        previous = threat.m_path[ index ];
    }

    draw_segment( previous, threat.m_impact, color, thickness );
}

void projectile_warning::draw_zone( const warning_threat_t &threat, color_t color, f32 fill ) const
{
    if ( threat.m_radius <= 0.0f )
    {
        return;
    }

    const vec3 normal{ threat.m_direct ? vec3{ 0.0f, 0.0f, 1.0f } : threat.m_normal };
    const vec3 reference{ math::abs( normal.z ) > 0.9f ? vec3{ 1.0f, 0.0f, 0.0f } : vec3{ 0.0f, 0.0f, 1.0f } };
    const vec3 across{ normal.cross( reference ).normalized( ) };
    const vec3 along{ normal.cross( across ) };
    const vec3 centre{ threat.m_impact + normal };
    const f32  inner{ threat.m_radius * fill };

    vec3 edge_from{ centre + across * threat.m_radius };
    vec3 fill_from{ centre + across * inner };
    for ( i32 point{ 1 }; point <= ZONE_POINTS; ++point )
    {
        const f32  turn{ static_cast< f32 >( point ) / static_cast< f32 >( ZONE_POINTS ) * PI * 2.0f };
        const vec3 direction{ across * math::cos( turn ) + along * math::sin( turn ) };
        const vec3 edge_to{ centre + direction * threat.m_radius };
        const vec3 fill_to{ centre + direction * inner };

        draw_segment( edge_from, edge_to, color, 1.5f );
        if ( inner > 0.0f )
        {
            draw_segment( fill_from, fill_to, faded( color, 0.6f ), 3.0f );
        }

        edge_from = edge_to;
        fill_from = fill_to;
    }
}

void projectile_warning::draw_badge( const warning_threat_t &threat, const vec3 &position, color_t color, f32 progress,
                                     f32 remaining, const char *label ) const
{
    vec2       centre{ };
    f32        pointing{ };
    const bool clamped{ anchor( position, centre, pointing ) };

    if ( threat.m_danger && remaining < PULSE_WINDOW )
    {
        const f64 beats{ g_tf2.m_engine->Plat_FloatTime( ) * 4.0 };
        const f32 beat{ static_cast< f32 >( beats - static_cast< f64 >( static_cast< i64 >( beats ) ) ) };
        g_draw.arc( centre, BADGE_RADIUS + 4.0f + beat * 10.0f, 2.0f, 0.0f, PI * 2.0f, faded( color, 0.6f * ( 1.0f - beat ) ) );
    }

    g_draw.arc( centre, BADGE_RADIUS, BADGE_RADIUS, 0.0f, PI * 2.0f, PLATE );
    g_draw.arc( centre, BADGE_RADIUS, BADGE_RING, 0.0f, PI * 2.0f, TRACK );
    g_draw.arc( centre, BADGE_RADIUS, BADGE_RING, -PI * 0.5f, PI * 2.0f * progress, color );
    draw_atlas_icon( icon_of( threat.m_kind ), render::icons::KIND_LARGE, render::icons::SIZE_LARGE, centre, color );

    if ( clamped )
    {
        draw_pointer( centre, pointing, BADGE_RADIUS + 3.0f, color );
    }

    draw_pill( vec2{ centre.x, centre.y + BADGE_RADIUS + 6.0f }, label, true );
}

void projectile_warning::draw_radar( const vec3 &position, color_t color, f32 progress, const char *label ) const
{
    const vec2 middle{ static_cast< f32 >( g_cl.m_screen_width ) * 0.5f, static_cast< f32 >( g_cl.m_screen_height ) * 0.5f };
    const f32  bearing{ view_bearing( position ) };
    const vec2 radial{ math::sin( bearing ), -math::cos( bearing ) };
    const f32  facing{ math::atan2( radial.y, radial.x ) };
    const f32  width{ 3.0f + 6.0f * ( 1.0f - progress ) };

    g_draw.arc( middle, RADAR_RADIUS + width * 0.5f, width, facing - RADAR_SPAN * 0.5f, RADAR_SPAN, color );
    draw_pointer( middle, facing, RADAR_RADIUS + width * 0.5f + 2.0f, color );

    const f32 out{ RADAR_RADIUS + 34.0f };
    draw_pill( vec2{ middle.x + radial.x * out, middle.y + radial.y * out - static_cast< f32 >( PILL_HEIGHT / 2 ) }, label, true );
}

void projectile_warning::draw_impact_mark( const warning_threat_t &threat, color_t color ) const
{
    vec2 centre{ };
    f32  pointing{ };
    anchor( threat.m_impact, centre, pointing );

    g_draw.arc( centre, 13.0f, 13.0f, 0.0f, PI * 2.0f, PLATE );
    g_draw.arc( centre, 13.0f, 1.5f, 0.0f, PI * 2.0f, color );
    draw_atlas_icon( render::icons::ID_TRIANGLE_ALERT, render::icons::KIND_SMALL, render::icons::SIZE_SMALL, centre, color );

    char distance[ 16 ]{ };
    str::format_to( distance, sizeof( distance ), ATLAS_XORSTR_STATIC( "{:.1f} m" ),
                    static_cast< f64 >( g_cl.m_local->GetAbsOrigin( ).dist_to( threat.m_impact ) * HU_TO_METERS ) );
    g_draw.text( ATLAS_XORSTR( "ESP Name" ).get( ), point_t{ static_cast< i32 >( centre.x ), static_cast< i32 >( centre.y ) + 16 }, INK,
                 distance, true, render::text_align::center );
}
