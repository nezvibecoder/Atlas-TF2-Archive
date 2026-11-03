#include "trajectory.hpp"

#include "holdaim_mgr.hpp"
#include "ragebot.hpp"

#include "gui/gui.hpp"
#include "math/math.hpp"
#include "render/render.hpp"
#include "util/format.hpp"

ATLAS_NO_DESTROY trajectory g_trajectory{ };

constexpr f32 OCCLUDED_FADE{ 0.3f };
constexpr f32 FLIGHT_TIME_GLIDE{ 140.0f };

ATLAS_FORCE_INLINE usize path_style( usize style )
{
    switch ( style )
    {
        case config::TRAJECTORY_STYLE_SEPARATORS: return config::PATH_STYLE_SEPARATORS;
        case config::TRAJECTORY_STYLE_TIMELINE: return config::PATH_STYLE_TIMELINE;
        case config::TRAJECTORY_STYLE_COMET: return config::PATH_STYLE_COMET;
        case config::TRAJECTORY_STYLE_FLOW: return config::PATH_STYLE_FLOW;
        default: return config::PATH_STYLE_LINE;
    }
}

void trajectory::capture( )
{
    this->m_setup.m_valid = false;

    if ( !this->wanted( ) )
    {
        return;
    }

    const vec3 *held{ g_holdaim_mgr.held( ) };
    if ( !g_proj_sim.gather( g_cl.m_local, g_cl.m_weapon, held ? *held : g_cl.m_cmd->viewangles, this->m_setup ) ||
         this->m_setup.m_movetype == projectile_movetype::flame )
    {
        this->m_setup.m_valid = false;
        return;
    }

    this->m_setup_radius = g_cl.m_weapon->GetProjectileBlastRadius( );
}

void trajectory::on_create_move( )
{
    this->m_count   = 0;
    this->m_landing = trajectory_landing_t{ };

    if ( !this->m_setup.m_valid )
    {
        return;
    }

    const projectile_setup_t setup{ this->m_setup };
    this->m_setup.m_valid = false;

    const i32 ticks{ math::clamp( TIME_TO_TICKS( setup.m_lifetime ), 1, projectile_sim::MAX_PATH_POINTS - 1 ) };
    const i32 interval{ math::clamp( static_cast< i32 >( math::sqrt( static_cast< f32 >( ticks ) ) ), 1, 12 ) };

    impact_t impact{ };
    this->m_path[ 0 ]   = setup.m_position;
    this->m_count       = 1 + g_proj_sim.simulate( setup, &this->m_path[ 1 ], ticks, interval, impact );
    this->m_hull        = setup.m_hull;
    this->m_hull_centre = setup.m_hull_centre;
    this->m_radius      = this->m_setup_radius;
    this->m_landing     = trajectory_landing_t{ impact.m_position, impact.m_normal, impact.m_time, setup.m_angles.y,
                                                impact.m_hit };

    if ( !g_cfg.m_trajectory.m_enabled )
    {
        return;
    }

    if ( g_cfg.m_trajectory.m_dim_occluded )
    {
        this->mark_visibility( setup, interval );
    }

    this->m_splash_valid = impact.m_hit && g_cfg.m_trajectory.m_splash && this->m_radius > 0.0f;
    if ( this->m_splash_valid )
    {
        this->trace_splash( );
    }
}

void trajectory::paint( )
{
    const config::trajectory_t &settings{ g_cfg.m_trajectory };
    if ( !settings.m_enabled || this->m_count < 2 || !g_cl.m_local || !g_cl.m_weapon )
    {
        return;
    }

    if ( !g_cl.m_local->IsAlive( ) ||
         g_cl.m_local->IsGhost( ) ||
         g_cl.m_local->IsInKart( ) ||
         g_cl.m_local->IsTaunting( ) )
    {
        return;
    }

    const ragebot::path_pen_t pen{ ragebot::path_pen( path_style( settings.m_style ), settings.m_outline ) };
    const f32                 span{ static_cast< f32 >( this->m_count - 1 ) };

    for ( i32 at{ 1 }; at < this->m_count; ++at )
    {
        color_t value{ settings.m_gradient ? settings.m_color.blend( settings.m_gradient_color, static_cast< f32 >( at ) / span )
                                           : settings.m_color };

        if ( settings.m_dim_occluded && !this->m_visible[ at ] )
        {
            value.a = static_cast< u8 >( static_cast< f32 >( value.a ) * OCCLUDED_FADE );
        }

        ragebot::draw_path_step( pen, this->m_path, this->m_count, at, value );
    }

    if ( this->m_landing.m_hit )
    {
        this->draw_markers( );
    }
}

void trajectory::clear( )
{
    this->m_setup.m_valid = false;
    this->m_count         = 0;
    this->m_splash_valid  = false;
    this->m_landing       = trajectory_landing_t{ };
}

bool trajectory::wanted( ) const
{
    return g_cfg.m_trajectory.m_enabled ||
           ( g_cfg.m_misc.m_projectile_camera && g_cfg.m_misc.m_projectile_camera_mode == config::PROJECTILE_CAMERA_IMPACT );
}

void trajectory::mark_visibility( const projectile_setup_t &setup, i32 interval )
{
    CViewSetup view{ };
    if ( !g_tf2.m_client->GetPlayerView( view ) )
    {
        return;
    }

    const f32                     reach_sqr{ setup.m_hull.length_sqr( ) };
    CTraceFilterWorldAndPropsOnly filter{ };

    for ( i32 first{ }; first < this->m_count; first += interval )
    {
        const i32 last{ math::smaller( first + interval, this->m_count ) - 1 };

        trace_t trace{ };
        Ray_t   ray{ };
        ray.Init( view.origin, this->m_path[ last ] );
        g_tf2.m_trace->TraceRay( ray, MASK_VISIBLE, &filter, &trace );

        const bool seen{ ( trace.endpos - this->m_path[ last ] ).length_sqr( ) <= reach_sqr };
        for ( i32 at{ first }; at <= last; ++at )
        {
            this->m_visible[ at ] = seen;
        }
    }
}

void trajectory::trace_splash( )
{
    const trajectory_landing_t   &landing{ this->m_landing };
    const vec3                    origin{ landing.m_position + landing.m_normal };
    const vec3                    reference{ math::abs( landing.m_normal.z ) > 0.9f ? vec3{ 1.0f, 0.0f, 0.0f } : vec3{ 0.0f, 0.0f, 1.0f } };
    const vec3                    across{ landing.m_normal.cross( reference ).normalized( ) };
    const vec3                    along{ landing.m_normal.cross( across ) };
    CTraceFilterWorldAndPropsOnly filter{ };

    for ( i32 point{ }; point < SPLASH_POINTS; ++point )
    {
        const f32 turn{ static_cast< f32 >( point ) / static_cast< f32 >( SPLASH_POINTS ) * PI * 2.0f };

        trace_t trace{ };
        Ray_t   ray{ };
        ray.Init( origin, origin + ( across * math::cos( turn ) + along * math::sin( turn ) ) * this->m_radius );
        g_tf2.m_trace->TraceRay( ray, MASK_RADIUS_DAMAGE, &filter, &trace );

        this->m_splash[ point ] = trace.endpos;
    }
}

void trajectory::draw_markers( )
{
    const config::trajectory_t &settings{ g_cfg.m_trajectory };
    const trajectory_landing_t &landing{ this->m_landing };
    const color_t              &marker{ settings.m_marker_color };

    if ( settings.m_splash && this->m_splash_valid )
    {
        for ( i32 point{ }; point < SPLASH_POINTS; ++point )
        {
            g_cl.draw_3d_line( this->m_splash[ point ], this->m_splash[ ( point + 1 ) % SPLASH_POINTS ], marker, settings.m_outline );
        }
    }

    if ( settings.m_impact_box )
    {
        g_cl.draw_3d_box( landing.m_position + this->m_hull_centre, -this->m_hull, this->m_hull, marker );
    }

    const u32 delta{ gui::animation_delta( this->m_shown_tick ) };
    this->m_shown_time = gui::approach_float( this->m_shown_time, landing.m_time, delta, FLIGHT_TIME_GLIDE );

    vec2 screen{ };
    if ( settings.m_flight_time && g_cl.world_to_screen( landing.m_position, screen ) )
    {
        char label[ 16 ]{ };
        str::format_to( label, sizeof( label ), ATLAS_XORSTR_STATIC( "{:.2f} s" ), static_cast< f64 >( this->m_shown_time ) );
        g_draw.text( MENU_FONT( ), point_t{ static_cast< i32 >( screen.x ) + 10, static_cast< i32 >( screen.y ) - 16 },
                     settings.m_color, label, true );
    }
}
