#include "tracers.hpp"

#include "math/math.hpp"
#include "render/render.hpp"

ATLAS_NO_DESTROY bullet_tracers g_tracers{ };

static const char *effect_for( usize style, bool red )
{
    switch ( style )
    {
        case config::TRACER_MERASMUS:
            return ATLAS_XORSTR_STATIC( "merasmus_zap" );
        case config::TRACER_BIG_NASTY:
            return red ? ATLAS_XORSTR_STATIC( "bullet_bignasty_tracer01_red" )
                       : ATLAS_XORSTR_STATIC( "bullet_bignasty_tracer01_blue" );
        case config::TRACER_RAYGUN:
            return red ? ATLAS_XORSTR_STATIC( "bullet_tracer_raygun_red" )
                       : ATLAS_XORSTR_STATIC( "bullet_tracer_raygun_blue" );
        case config::TRACER_MACHINA:
            return red ? ATLAS_XORSTR_STATIC( "dxhr_sniper_rail_red" )
                       : ATLAS_XORSTR_STATIC( "dxhr_sniper_rail_blue" );
        case config::TRACER_LIGHTNING:
            return red ? ATLAS_XORSTR_STATIC( "spell_lightningball_hit_red" )
                       : ATLAS_XORSTR_STATIC( "spell_lightningball_hit_blue" );
        default:
            return nullptr;
    }
}

static float tracer_life( )
{
    return static_cast< float >( math::clamp( g_cfg.m_tracers.m_duration, 100, 3000 ) ) /
           1000.0f;
}

void bullet_tracers::begin_bullet( C_TFPlayer *player, C_TFWeaponBase *weapon )
{
    this->m_shooter = nullptr;
    this->m_weapon  = nullptr;

    if ( !g_cfg.m_tracers.m_enabled || !player || !g_cl.m_local )
    {
        return;
    }

    if ( player == g_cl.m_local )
    {
        if ( !g_tf2.m_prediction->m_bFirstTimePredicted )
        {
            return;
        }
    } else if ( g_cfg.m_tracers.m_local_only )
    {
        return;
    }

    this->m_shooter = player;
    this->m_weapon  = weapon;
}

void bullet_tracers::end_bullet( const FireBulletsInfo_t &info )
{
    C_TFPlayer *shooter{ this->m_shooter };

    if ( !shooter )
    {
        return;
    }

    const vec3    start{ this->muzzle( info.m_vecSrc ) };
    const vec3    end{ this->impact( info ) };
    const color_t value{ this->color_for_line( ) };
    const color_t impact_val{ this->color_for_impact( ) };

    this->m_shooter = nullptr;
    this->m_weapon  = nullptr;

    switch ( g_cfg.m_tracers.m_style )
    {
        case config::TRACER_LINES:
        {
            this->queue_line( start, end, value, impact_val );
            return;
        }

        case config::TRACER_BEAMS:
        {
            this->queue_beam( start, end, value );
            return;
        }

        default: break;
    }

    const char *effect{ effect_for( g_cfg.m_tracers.m_style, shooter->m_iTeamNum( ) == TF_TEAM_RED ) };

    if ( !effect )
    {
        return;
    }

    g_hooks.m_util_particle_tracer.fastcall< void >( effect, &start, &end, shooter->entindex( ), -1, false );
}

vec3 bullet_tracers::muzzle( const vec3 &fallback ) const
{
    if ( !this->m_weapon || g_tf2.m_input->CAM_IsThirdPerson( ) )
    {
        return fallback;
    }

    C_BaseAnimating *source{ this->m_weapon };
    if ( this->m_shooter == g_cl.m_local && !g_tf2.m_input->CAM_IsThirdPerson( ) )
    {
        if ( C_BaseEntity * view_model{ this->m_shooter->m_hViewModel( ).Get( ) } )
        {
            source = static_cast< C_BaseAnimating * >( view_model );
        }
    }

    const i32 attachment{ source->LookupAttachment( ATLAS_XORSTR( "muzzle" ).get( ) ) };
    if ( attachment <= 0 )
    {
        return fallback;
    }

    vec3 origin{ };

    return source->GetAttachment( attachment, origin ) ? origin : fallback;
}

vec3 bullet_tracers::impact( const FireBulletsInfo_t &info ) const
{
    trace_t             trace{ };
    CTraceFilterHitscan filter{ };
    Ray_t               ray{ };

    filter.m_pSkip = this->m_shooter;

    ray.Init( info.m_vecSrc, info.m_vecSrc + info.m_vecDirShooting * info.m_flDistance );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    return trace.endpos;
}

color_t bullet_tracers::color_for_line( ) const
{
    return this->m_shooter == g_cl.m_local ? g_cfg.m_tracers.m_local_color : g_cfg.m_tracers.m_entity_color;
}

color_t bullet_tracers::color_for_impact( ) const
{
    return this->m_shooter == g_cl.m_local ? g_cfg.m_tracers.m_local_impact_color : g_cfg.m_tracers.m_entity_impact_color;
}

void bullet_tracers::queue_line( const vec3 &start, const vec3 &end, const color_t &value, const color_t &impact_val )
{
    const double   now{ g_tf2.m_engine->Plat_FloatTime( ) };
    tracer_line_t &entry{ this->m_lines[ this->m_cursor ] };

    this->m_cursor = ( this->m_cursor + 1 ) % TRACER_LINE_SLOTS;

    entry.m_start        = start;
    entry.m_end          = end;
    entry.m_color_line   = value;
    entry.m_color_impact = impact_val;
    entry.m_born         = now;
    entry.m_expire       = now + static_cast< double >( tracer_life( ) );
}

void bullet_tracers::queue_beam( const vec3 &start, const vec3 &end, color_t value ) const
{
    BeamInfo_t info{ };

    info.m_vecStart     = start;
    info.m_vecEnd       = end;
    info.m_pszModelName = ATLAS_XORSTR_STATIC( "sprites/physbeam.vmt" );
    info.m_flLife       = tracer_life( );
    info.m_flWidth      = 2.f;
    info.m_flEndWidth   = 2.f;
    info.m_flBrightness = static_cast< float >( value.a );
    info.m_flSpeed      = -0.5f;
    info.m_nSegments    = 2;
    info.m_flRed        = static_cast< float >( value.r );
    info.m_flGreen      = static_cast< float >( value.g );
    info.m_flBlue       = static_cast< float >( value.b );

    if ( Beam_t * beam{ g_tf2.m_beams->CreateBeamPoints( info ) } )
    {
        g_tf2.m_beams->DrawBeam( beam );
    }
}

// Dynamically rescales the impact circle radius depending on screen resolution.
// ... because having it look smaller on bigger monitors looks dumb.
// And having it look massive on small monitors looks even more dumb.
// Scales based off of height because width would be too unreliable (what happens if we run 1366x768 and suddenly switch to 1024x768?)
f32 bullet_tracers::scaled_radius( )
{
    constexpr static f32 radius{ 5.0f };
    constexpr static f32 base_height_to_scale_from{ 1080.0f };

    const auto scalar{ ( f32 )g_cl.m_screen_height / base_height_to_scale_from };

    return radius * scalar;
}

void bullet_tracers::paint( )
{
    if ( !g_cfg.m_tracers.m_enabled || g_cfg.m_tracers.m_style != config::TRACER_LINES )
    {
        return;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    for ( const tracer_line_t &entry : this->m_lines )
    {
        const double span{ entry.m_expire - entry.m_born };

        if ( span <= 0.0 || entry.m_expire <= now )
        {
            continue;
        }

        const float left{ static_cast< float >( ( entry.m_expire - now ) / span ) };

        color_t value{ entry.m_color_line };
        color_t impact_val{ entry.m_color_impact };
        impact_val.a = value.a = static_cast< u8 >( static_cast< float >( value.a ) * left );

        g_cl.draw_3d_line( entry.m_start, entry.m_end, value, false );

        if ( g_cfg.m_tracers.m_impact_marker )
        {
            switch ( g_cfg.m_tracers.m_impact_style )
            {
                case config::IMPACT_STYLE_BOX:
                {
                    static constexpr vec3 hull{ 1.f, 1.f, 1.f };

                    vec3 angle_of_shot{ };
                    math::vector_angles( entry.m_start - entry.m_end, angle_of_shot );
                    g_cl.draw_3d_box( entry.m_end, -hull, hull, impact_val, angle_of_shot );
                    break;
                }

                case config::IMPACT_STYLE_CIRCLE: g_cl.draw_3d_overlay_circle( entry.m_end, impact_val, scaled_radius( ) ); break;
                default: Assert( 0 && "How the fuck did you get here?" );
            }
        }
    }
}

void bullet_tracers::clear( )
{
    this->m_lines.reset( );
    this->m_cursor = 0;

    this->m_shooter = nullptr;
    this->m_weapon  = nullptr;
}
