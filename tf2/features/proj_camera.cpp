#include "proj_camera.hpp"

#include "entity_cache.hpp"
#include "holdaim_mgr.hpp"
#include "trajectory.hpp"

#include "gui/gui.hpp"
#include "gui/indicator.hpp"
#include "math/math.hpp"
#include "render/render.hpp"
#include "util/format.hpp"

ATLAS_NO_DESTROY projectile_camera g_proj_camera{ };

constexpr i32 RETICLE_GAP{ 4 };
constexpr i32 RETICLE_ARM{ 7 };

constexpr f64 STICKY_LINGER{ 1.5 }; // لعبة جيدة!

constexpr color_t BOMBER_INK{ 235, 235, 235, 235 };
constexpr i32     BOMBER_LINE_HEIGHT{ 12 };
constexpr i32     BEARING_SOUTH{ 48 * 3600 + 52 * 60 + 31 };
constexpr i32     BEARING_WEST{ 123 * 3600 + 23 * 60 + 33 };
constexpr f32     METRES_PER_UNIT{ 0.01905f };

static indicator_card_t s_camera_card{ };

ATLAS_FORCE_INLINE void draw_reticle( point_t centre )
{
    const rect_t marks[]{
        rect_t{ centre.x - RETICLE_GAP - RETICLE_ARM, centre.y, centre.x - RETICLE_GAP, centre.y + 1 },
        rect_t{ centre.x + RETICLE_GAP + 1, centre.y, centre.x + RETICLE_GAP + 1 + RETICLE_ARM, centre.y + 1 },
        rect_t{ centre.x, centre.y - RETICLE_GAP - RETICLE_ARM, centre.x + 1, centre.y - RETICLE_GAP },
        rect_t{ centre.x, centre.y + RETICLE_GAP + 1, centre.x + 1, centre.y + RETICLE_GAP + 1 + RETICLE_ARM },
        rect_t{ centre.x, centre.y, centre.x + 1, centre.y + 1 },
    };

    for ( const rect_t &mark : marks )
    {
        g_render.box( rect_t{ mark.left - 1, mark.top - 1, mark.right + 1, mark.bottom + 1 }, color_t{ 0, 0, 0, 160 }, true );
    }

    for ( const rect_t &mark : marks )
    {
        g_render.box( mark, g_theme.m_accent, true );
    }
}

ATLAS_FORCE_INLINE usize format_bearing( char *out, usize capacity, const char *axis, i32 seconds )
{
    const i32   minutes{ seconds / 60 % 60 };
    const i32   rest{ seconds % 60 };
    const usize degrees{ str::format_to( out, capacity, ATLAS_XORSTR_STATIC( "{}: {}" ), axis, seconds / 3600 ) };

    str::format_to( out + degrees, capacity - degrees, ATLAS_XORSTR_STATIC( " {}{}'{}{}\"" ), minutes / 10, minutes % 10,
                    rest / 10, rest % 10 );
    return degrees;
}

ATLAS_FORCE_INLINE void draw_bomber_hud( const camera_frame_t &frame )
{
    const rect_t &body{ frame.m_body };
    const point_t centre{ ( body.left + body.right ) / 2, ( body.top + body.bottom ) / 2 };
    const i32     width{ body.right - body.left };
    const i32 height{ body.bottom - body.top };

    const i32    half_width{ width * 18 / 100 };
    const i32    half_height{ height * 20 / 100 };
    const i32    reach_x{ half_width * 6 / 10 };
    const i32    reach_y{ half_height / 2 };
    const rect_t brackets{ centre.x - half_width, centre.y - half_height, centre.x + half_width, centre.y + half_height };

    g_render.set_clip( body );

    const point_t corners[]{ point_t{ brackets.left, brackets.top }, point_t{ brackets.right, brackets.top },
                             point_t{ brackets.left, brackets.bottom }, point_t{ brackets.right, brackets.bottom } };
    for ( const point_t &corner : corners )
    {
        const i32 inward_x{ corner.x == brackets.left ? reach_x : -reach_x };
        const i32 inward_y{ corner.y == brackets.top ? reach_y : -reach_y };
        g_render.line( corner, point_t{ corner.x + inward_x, corner.y }, BOMBER_INK );
        g_render.line( corner, point_t{ corner.x, corner.y + inward_y }, BOMBER_INK );
    }

    const i32 gap{ 2 + width / 50 };
    const i32 arm{ 3 + width / 25 };
    g_render.line( point_t{ centre.x - gap - arm, centre.y }, point_t{ centre.x - gap, centre.y }, BOMBER_INK );
    g_render.line( point_t{ centre.x + gap, centre.y }, point_t{ centre.x + gap + arm, centre.y }, BOMBER_INK );
    g_render.line( point_t{ centre.x, centre.y - gap - arm }, point_t{ centre.x, centre.y - gap }, BOMBER_INK );
    g_render.line( point_t{ centre.x, centre.y + gap }, point_t{ centre.x, centre.y + gap + arm }, BOMBER_INK );

    g_render.text( MENU_FONT_MONO( ), point_t{ body.left + 5, body.top + 4 }, BOMBER_INK,
                   ATLAS_XORSTR_STATIC( ">camera_drone_a01" ), true );
    g_render.box( rect_t{ brackets.left, brackets.top - 9, brackets.left + 3, brackets.top - 6 }, BOMBER_INK, true );
    g_render.text( MENU_FONT_MONO( ), point_t{ brackets.left + 6, brackets.top - 14 }, BOMBER_INK, ATLAS_XORSTR_STATIC( "ARM" ), true );
    g_render.text( MENU_FONT_MONO( ), point_t{ brackets.right, brackets.bottom + 4 }, BOMBER_INK, ATLAS_XORSTR_STATIC( "LOCKED" ),
                   true, render::text_align::right );

    const vec3 &target{ frame.m_target };
    const vec3  to_target{ target - frame.m_shooter };
    char        readout[ 5 ][ 32 ]{ };
    usize       degree_at[ 5 ]{ };
    str::format_to( readout[ 0 ], sizeof( readout[ 0 ] ), ATLAS_XORSTR_STATIC( "DST:{}" ),
                    static_cast< i32 >( to_target.length2d( ) * METRES_PER_UNIT ) );
    str::format_to( readout[ 1 ], sizeof( readout[ 1 ] ), ATLAS_XORSTR_STATIC( "RNG:{:.1f}" ),
                    static_cast< f64 >( to_target.length( ) * METRES_PER_UNIT ) );
    str::format_to( readout[ 2 ], sizeof( readout[ 2 ] ), ATLAS_XORSTR_STATIC( "ELV:{}m" ),
                    static_cast< i32 >( ( frame.m_camera.z - target.z ) * METRES_PER_UNIT ) );
    degree_at[ 3 ] = format_bearing( readout[ 3 ], sizeof( readout[ 3 ] ), ATLAS_XORSTR_STATIC( "S" ),
                                     BEARING_SOUTH + static_cast< i32 >( target.y * METRES_PER_UNIT ) );
    degree_at[ 4 ] = format_bearing( readout[ 4 ], sizeof( readout[ 4 ] ), ATLAS_XORSTR_STATIC( "W" ),
                                     BEARING_WEST + static_cast< i32 >( target.x * METRES_PER_UNIT ) );

    i32 widest{ };
    for ( const char *line : readout )
    {
        widest = math::bigger( widest, g_render.get_text_width( MENU_FONT_MONO( ), line ) );
    }

    const i32 left{ body.right - 5 - widest };
    i32       top{ body.bottom - 4 - static_cast< i32 >( sizeof( readout ) / sizeof( readout[ 0 ] ) ) * BOMBER_LINE_HEIGHT };
    for ( usize line{ }; line < sizeof( readout ) / sizeof( readout[ 0 ] ); ++line )
    {
        g_render.text( MENU_FONT_MONO( ), point_t{ left, top }, BOMBER_INK, readout[ line ], true );

        if ( degree_at[ line ] )
        {
            const i32 ring{ left + g_render.get_text_width( MENU_FONT_MONO( ), string_view{ readout[ line ], degree_at[ line ] } ) + 1 };
            g_render.box( rect_t{ ring, top + 2, ring + 2, top + 4 }, BOMBER_INK );
        }

        top += BOMBER_LINE_HEIGHT;
    }

    g_render.reset_clip( );
}

ATLAS_FORCE_INLINE C_BaseEntity *projectile_owner( const entity_cache::entry_t &cached )
{
    switch ( cached.m_class_id )
    {
        case ETFClassID::CTFGrenadePipebombProjectile:
        case ETFClassID::CTFStunBall:
        case ETFClassID::CTFProjectile_Cleaver:
        case ETFClassID::CTFProjectile_Jar:
        case ETFClassID::CTFProjectile_JarMilk:
        case ETFClassID::CTFProjectile_JarGas: return cached.m_entity->As< C_TFGrenadePipebombProjectile * >( )->m_hThrower( ).Get( );
        default: return cached.m_entity->m_hOwnerEntity( ).Get( );
    }
}

ATLAS_FORCE_INLINE bool is_sticky( C_BaseEntity *projectile )
{
    if ( projectile->GetClientClass( )->GetClassID( ) != ETFClassID::CTFGrenadePipebombProjectile )
    {
        return false;
    }

    switch ( projectile->As< C_TFGrenadePipebombProjectile * >( )->m_iType( ) )
    {
        case TF_GL_MODE_REMOTE_DETONATE:
        case TF_GL_MODE_REMOTE_DETONATE_PRACTICE: return true;
        default: return false;
    }
}

ATLAS_FORCE_INLINE bool is_stuck_sticky( C_BaseEntity *projectile )
{
    return is_sticky( projectile ) && projectile->As< C_TFGrenadePipebombProjectile * >( )->m_bTouched( );
}

void projectile_camera::capture( )
{
    this->m_setup.m_valid = false;

    if ( !g_cfg.m_misc.m_projectile_camera || g_cfg.m_misc.m_projectile_camera_mode != config::PROJECTILE_CAMERA_LANDING ||
         !g_cl.m_will_fire )
    {
        return;
    }

    const vec3 *held{ g_holdaim_mgr.held( ) };
    this->m_capture_angles = held ? *held : g_cl.m_cmd->viewangles;
    if ( !g_proj_sim.gather( g_cl.m_local, g_cl.m_weapon, this->m_capture_angles, this->m_setup, g_cl.m_cmd->command_number ) ||
         this->m_setup.m_movetype == projectile_movetype::flame )
    {
        this->m_setup.m_valid = false;
    }
}

void projectile_camera::on_create_move( )
{
    if ( !this->m_setup.m_valid )
    {
        return;
    }

    const projectile_setup_t setup{ this->m_setup };
    this->m_setup.m_valid = false;

    if ( !g_cl.m_will_fire )
    {
        return;
    }

    const vec3 moved{ this->m_capture_angles - g_cl.m_cmd->viewangles };
    for ( i32 axis{ }; axis < 3; ++axis )
    {
        if ( math::abs( math::normalize_yaw( moved[ axis ] ) ) > 0.001f )
        {
            return;
        }
    }

    const i32 ticks{ math::clamp( TIME_TO_TICKS( setup.m_lifetime ), 1, projectile_sim::MAX_PATH_POINTS ) };
    const i32 interval{ math::clamp( static_cast< i32 >( math::sqrt( static_cast< f32 >( ticks ) ) ), 1, 12 ) };

    impact_t impact{ };
    g_proj_sim.simulate( setup, this->m_path, ticks, interval, impact );

    if ( !impact.m_hit )
    {
        return;
    }

    this->m_landing_target   = impact.m_position;
    this->m_landing_yaw      = setup.m_angles.y;
    this->m_landing_handle   = INVALID_EHANDLE_INDEX;
    this->m_landing_deadline = g_tf2.m_engine->Plat_FloatTime( ) + static_cast< f64 >( impact.m_time );
}

void projectile_camera::publish_t::operator()( )
{
    this->m_owner->m_presented = this->m_frame;
}

void projectile_camera::publish( IMatRenderContext *render_context, const camera_frame_t &frame )
{
    for ( publish_t &job : this->m_publish )
    {
        if ( !job.acquire( ) )
        {
            continue;
        }

        job.m_owner = this;
        job.m_frame = frame;

        if ( ICallQueue *queue{ render_context->GetCallQueue( ) } )
        {
            queue->QueueFunctor( &job );
        } else
        {
            job( );
        }

        job.Release( );
        return;
    }
}

void projectile_camera::render( CViewRender *view_render, const CViewSetup &view )
{
    this->m_active = g_cfg.m_misc.m_projectile_camera && this->place( );

    const rect_t body{ this->m_body };
    const i32    width{ body.right - body.left };
    const i32    height{ body.bottom - body.top };
    if ( !this->m_active || width <= 0 || height <= 0 )
    {
        return;
    }

    if ( !this->m_material )
    {
        this->create_resources( );

        if ( !this->m_material )
        {
            return;
        }
    }

    CViewSetup camera{ view };
    camera.x               = 0;
    camera.y               = 0;
    camera.width           = width;
    camera.height          = height;
    camera.m_flAspectRatio = static_cast< f32 >( width ) / static_cast< f32 >( height );
    camera.fov             = 90.0f;
    camera.origin          = this->m_origin;
    camera.angles          = this->m_angles;

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };

    render_context->PushRenderTargetAndViewport( );
    render_context->SetRenderTarget( this->m_texture );

    this->m_drawing = true;
    g_hooks.m_render_view.thiscall< void >( view_render, &camera, 1 | 2, 0 );
    this->m_drawing = false;

    render_context->PopRenderTargetAndViewport( );

    render_context->DrawScreenSpaceRectangle( this->m_material, body.left, body.top, width, height, 0.0f, 0.0f,
                                              static_cast< f32 >( width - 1 ), static_cast< f32 >( height - 1 ),
                                              g_cl.m_screen_width, g_cl.m_screen_height );

    this->publish( render_context, camera_frame_t{ body, this->m_target, g_cl.m_local->GetAbsOrigin( ), this->m_origin, true } );

    render_context->Release( );
}

void projectile_camera::paint( )
{
    const camera_frame_t shown{ this->m_presented };
    this->m_presented.m_blitted = false;

    const bool visible{ g_cfg.m_misc.m_projectile_camera && ( this->m_active || g_gui.open( ) ) };
    if ( !visible && !s_camera_card.m_enter )
    {
        hide_indicator( s_camera_card );
        this->m_body = rect_t{ };
        return;
    }

    const i32 width{ g_cfg.m_misc.m_projectile_camera_width };
    const i32 card_width{ width + 2 };
    if ( g_cl.m_screen_width < card_width )
    {
        this->m_body = rect_t{ };
        return;
    }

    const i32 height{ width * g_cl.m_screen_height / g_cl.m_screen_width };
    const i32 card_height{ height + INDICATOR_TITLE_HEIGHT + 3 };
    if ( g_cl.m_screen_height < card_height )
    {
        this->m_body = rect_t{ };
        return;
    }

    s_camera_card.m_at = g_cfg.m_misc.m_projectile_camera_position;
    step_indicator( s_camera_card, 0.0f, g_cl.m_screen_width, g_cl.m_screen_height, card_width, card_height, visible );
    g_cfg.m_misc.m_projectile_camera_position = s_camera_card.m_at;

    draw_indicator_frame( s_camera_card, ATLAS_XORSTR_STATIC( "projectile camera" ), CARD_TITLE_TEXT, !shown.m_blitted );

    const point_t at{ s_camera_card.m_at };
    const rect_t  body{ s_camera_card.m_enter == 255 ? rect_t{ at.x + 1, at.y + INDICATOR_TITLE_HEIGHT + 2, at.x + 1 + width,
                                                               at.y + INDICATOR_TITLE_HEIGHT + 2 + height }
                                                     : rect_t{ } };

    if ( shown.m_blitted )
    {
        if ( g_cfg.m_misc.m_projectile_camera_hud == config::PROJECTILE_CAMERA_HUD_BOMBER )
        {
            draw_bomber_hud( shown );
        } else
        {
            draw_reticle( point_t{ ( shown.m_body.left + shown.m_body.right ) / 2, ( shown.m_body.top + shown.m_body.bottom ) / 2 } );
        }
    }

    this->m_body = body;
}

void projectile_camera::on_level_shutdown( )
{
    this->destroy_resources( );

    atl::memset( this->m_seen, 0, sizeof( this->m_seen ) );
    this->m_followed         = INVALID_EHANDLE_INDEX;
    this->m_landing_handle   = INVALID_EHANDLE_INDEX;
    this->m_landing_deadline = 0.0;
    this->m_landing_linger   = 0.0;
    this->m_active           = false;
}

u32 projectile_camera::newest_projectile( )
{
    u32 newest{ INVALID_EHANDLE_INDEX };

    for ( const auto *cached : g_entity_cache.projectiles( ) )
    {
        if ( !cached->networked( ) || this->m_seen[ cached->m_index ] == cached->m_handle ||
             projectile_owner( *cached ) != g_cl.m_local )
        {
            continue;
        }

        this->m_seen[ cached->m_index ] = cached->m_handle;
        newest                          = cached->m_handle;
    }

    return newest;
}

bool projectile_camera::place( )
{
    if ( !g_cl.m_local )
    {
        return false;
    }

    switch ( g_cfg.m_misc.m_projectile_camera_mode )
    {
        case config::PROJECTILE_CAMERA_FOLLOW: return this->place_follow( );
        case config::PROJECTILE_CAMERA_IMPACT: return this->place_impact( );
        case config::PROJECTILE_CAMERA_LANDING: return this->place_landing( );
        default: return false;
    }
}

bool projectile_camera::place_follow( )
{
    const u32 newest{ this->newest_projectile( ) };
    if ( newest != INVALID_EHANDLE_INDEX )
    {
        this->m_followed     = newest;
        this->m_sample_count = 0;
    }

    C_BaseEntity *projectile{ g_entity_cache.resolve( this->m_followed ) };
    if ( !projectile || projectile->IsDormant( ) || is_stuck_sticky( projectile ) )
    {
        return false;
    }

    const vec3 origin{ projectile->m_vecOrigin( ) };
    if ( this->m_sample_count == 0 || origin != this->m_samples[ 1 ].m_origin )
    {
        this->m_samples[ 0 ] = this->m_samples[ 1 ];
        this->m_samples[ 1 ] = follow_sample_t{ origin, projectile->m_flSimulationTime( ) };
        this->m_sample_count = math::smaller( this->m_sample_count + 1, 2 );
    }

    if ( this->m_sample_count < 2 )
    {
        return false;
    }

    const follow_sample_t &from{ this->m_samples[ 0 ] };
    const follow_sample_t &to{ this->m_samples[ 1 ] };
    const vec3             travel{ to.m_origin - from.m_origin };
    const f32              span{ to.m_time - from.m_time };

    const f32 fraction{ span > 0.0f ? math::clamp( ( g_tf2.m_globals->curtime - span - from.m_time ) / span, 0.0f, 1.0f )
                                    : 1.0f };

    this->place_behind( from.m_origin + travel * fraction, travel.normalized( ) );
    return true;
}

bool projectile_camera::place_impact( )
{
    const trajectory_landing_t &landing{ g_trajectory.landing( ) };
    if ( !landing.m_hit || !g_cl.m_local->IsAlive( ) || !g_cl.m_weapon )
    {
        return false;
    }

    this->place_above( landing.m_position, landing.m_yaw );
    return true;
}

bool projectile_camera::place_landing( )
{
    const u32 newest{ this->newest_projectile( ) };
    const f64 now{ g_tf2.m_engine->Plat_FloatTime( ) };

    if ( this->m_landing_deadline > 0.0 )
    {
        if ( newest != INVALID_EHANDLE_INDEX )
        {
            this->m_landing_handle   = newest;
            this->m_landing_deadline = 0.0;
        } else if ( now > this->m_landing_deadline )
        {
            this->m_landing_deadline = 0.0;
            return false;
        }
    }

    if ( this->m_landing_deadline <= 0.0 )
    {
        if ( C_BaseEntity *projectile{ g_entity_cache.resolve( this->m_landing_handle ) } )
        {
            this->m_landing_linger = is_sticky( projectile ) ? now + STICKY_LINGER : 0.0;
        } else if ( now > this->m_landing_linger )
        {
            return false;
        }
    }

    this->place_above( this->m_landing_target, this->m_landing_yaw );
    return true;
}

void projectile_camera::place_behind( const vec3 &target, const vec3 &heading )
{
    trace_t                       trace{ };
    Ray_t                         ray{ };
    CTraceFilterWorldAndPropsOnly filter{ };

    ray.Init( target, target - heading * static_cast< f32 >( g_cfg.m_misc.m_projectile_camera_distance ) );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    this->m_origin = trace.endpos;
    this->m_target = target;
    math::vector_angles( heading, this->m_angles );
}

void projectile_camera::place_above( const vec3 &target, f32 yaw )
{
    trace_t                       trace{ };
    Ray_t                         ray{ };
    CTraceFilterWorldAndPropsOnly filter{ };

    ray.Init( target, target + vec3{ 0.0f, 0.0f, static_cast< f32 >( g_cfg.m_misc.m_projectile_camera_distance ) } );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    this->m_origin = trace.endpos;
    this->m_target = target;
    this->m_angles = vec3{ 90.0f, yaw, 0.0f };
}

void projectile_camera::create_resources( )
{
    if ( !this->m_texture )
    {
        this->m_texture = g_tf2.m_material_system->CreateNamedRenderTargetTextureEx(
            ATLAS_XORSTR( "atlas_projectile_camera" ).get( ), g_cl.m_screen_width, g_cl.m_screen_height, RT_SIZE_LITERAL,
            IMAGE_FORMAT_RGB888, MATERIAL_RT_DEPTH_SHARED, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
            CREATERENDERTARGETFLAGS_HDR );
        if ( !this->m_texture )
        {
            return;
        }
        this->m_texture->IncrementReferenceCount( );
    }

    KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "UnlitGeneric" ).get( ) ) };
    kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "atlas_projectile_camera" ).get( ) );
    this->m_material = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_projectile_camera_view" ).get( ), kv );
}

void projectile_camera::destroy_resources( )
{
    if ( this->m_material )
    {
        this->m_material->DecrementReferenceCount( );
        this->m_material->DeleteIfUnreferenced( );
        this->m_material = nullptr;
    }

    if ( this->m_texture )
    {
        this->m_texture->DecrementReferenceCount( );
        this->m_texture->DeleteIfUnreferenced( );
        this->m_texture = nullptr;
    }
}
