#include "hitmarker.hpp"

#include "render/render.hpp"
#include "util/format.hpp"

ATLAS_NO_DESTROY hitmarker g_hitmarker{ };

ATLAS_FORCE_INLINE color_t hitmarker_color( usize event )
{
    const config::hitmarker_t &settings{ g_cfg.m_hitmarker };
    return settings.m_override_color[ event ] ? settings.m_event_color[ event ] : settings.m_color;
}

void hitmarker::add( i32 victim, const vec3 &position, i32 damage, usize event )
{
    hit_t &hit{ this->m_hits[ this->m_next ] };
    hit.m_position = position;
    hit.m_time     = g_tf2.m_engine->Plat_FloatTime( );
    hit.m_victim   = victim;
    hit.m_damage   = damage;
    hit.m_event    = event;
    this->m_next   = ( this->m_next + 1 ) % CAPACITY;
}

void hitmarker::on_hurt( IGameEvent *event )
{
    if ( !g_cfg.m_hitmarker.m_enabled || !g_cl.m_local )
    {
        return;
    }

    const i32 attacker{ g_tf2.m_engine->GetPlayerForUserID(
        event->GetInt( ATLAS_XORSTR( "attacker" ).get( ) ) ) };
    const i32 victim{ g_tf2.m_engine->GetPlayerForUserID(
        event->GetInt( ATLAS_XORSTR( "userid" ).get( ) ) ) };
    const i32 damage{ event->GetInt( ATLAS_XORSTR( "damageamount" ).get( ) ) };
    if ( attacker != g_cl.m_local->entindex( ) || victim <= 0 || victim == attacker || damage <= 0 )
    {
        return;
    }

    IClientEntity *client{ g_tf2.m_entity_list->GetClientEntity( victim ) };
    C_BaseEntity  *player{ client ? client->GetBaseEntity( ) : nullptr };
    if ( !player )
    {
        return;
    }

    const bool critical{ event->GetBool( ATLAS_XORSTR( "crit" ).get( ) ) ||
                         event->GetBool( ATLAS_XORSTR( "minicrit" ).get( ) ) };
    this->add( victim, player->WorldSpaceCenter( ), damage,
               critical ? config::HITMARKER_CRITICAL : config::HITMARKER_NORMAL );
}

void hitmarker::on_death( IGameEvent *event )
{
    if ( !g_cfg.m_hitmarker.m_enabled || !g_cl.m_local ||
         ( event->GetInt( ATLAS_XORSTR( "death_flags" ).get( ) ) & TF_DEATH_FEIGN_DEATH ) )
    {
        return;
    }

    const i32 attacker{ g_tf2.m_engine->GetPlayerForUserID(
        event->GetInt( ATLAS_XORSTR( "attacker" ).get( ) ) ) };
    const i32 victim{ g_tf2.m_engine->GetPlayerForUserID(
        event->GetInt( ATLAS_XORSTR( "userid" ).get( ) ) ) };
    if ( attacker != g_cl.m_local->entindex( ) || victim <= 0 || victim == attacker )
    {
        return;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    hit_t       *recent{ };
    for ( hit_t &hit : this->m_hits )
    {
        const double age{ now - hit.m_time };
        if ( hit.m_victim == victim && age >= 0.0 && age <= 0.35 &&
             ( !recent || hit.m_time > recent->m_time ) )
        {
            recent = &hit;
        }
    }

    if ( recent )
    {
        recent->m_event = config::HITMARKER_KILL;
        recent->m_time  = now;
        return;
    }

    IClientEntity *client{ g_tf2.m_entity_list->GetClientEntity( victim ) };
    C_BaseEntity  *player{ client ? client->GetBaseEntity( ) : nullptr };
    if ( player )
    {
        this->add( victim, player->WorldSpaceCenter( ), 0, config::HITMARKER_KILL );
    }
}

void hitmarker::paint( ) const
{
    if ( !g_cfg.m_hitmarker.m_enabled || !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const bool   screen_mode{ g_cfg.m_hitmarker.m_render_mode == config::HITMARKER_2D };
    const hit_t *latest{ };
    if ( screen_mode )
    {
        for ( const hit_t &hit : this->m_hits )
        {
            const double age{ now - hit.m_time };
            const float  duration{ hit.m_event == config::HITMARKER_KILL ? 0.85f : 0.65f };
            if ( age >= 0.0 && age < duration && ( !latest || hit.m_time > latest->m_time ) )
            {
                latest = &hit;
            }
        }
    }

    for ( const hit_t &hit : this->m_hits )
    {
        if ( screen_mode && &hit != latest )
        {
            continue;
        }

        const double age{ now - hit.m_time };
        const float  duration{ hit.m_event == config::HITMARKER_KILL ? 0.85f : 0.65f };
        if ( age < 0.0 || age >= duration )
        {
            continue;
        }

        vec2 screen{ };
        if ( screen_mode )
        {
            screen = vec2{ static_cast< f32 >( g_cl.m_screen_width / 2 ),
                           static_cast< f32 >( g_cl.m_screen_height / 2 ) };
        } else if ( !g_cl.world_to_screen( hit.m_position, screen ) )
        {
            continue;
        }

        const float progress{ static_cast< float >( age ) / duration };
        const float inner{ 3.0f + progress * 4.0f };
        const float outer{ 9.0f + progress * ( hit.m_event == config::HITMARKER_KILL ? 10.0f : 7.0f ) };
        color_t     color{ hitmarker_color( hit.m_event ) };
        color.a = static_cast< u8 >( static_cast< float >( color.a ) * ( 1.0f - progress ) );

        for ( i32 x{ -1 }; x <= 1; x += 2 )
        {
            for ( i32 y{ -1 }; y <= 1; y += 2 )
            {
                const vec2 direction{ static_cast< float >( x ) * 0.7071f,
                                      static_cast< float >( y ) * 0.7071f };
                g_draw.line( screen + direction * inner, screen + direction * outer, color, 2.0f );
            }
        }

        if ( g_cfg.m_hitmarker.m_show_damage && hit.m_damage > 0 )
        {
            char damage[ 16 ]{ };
            str::format_to( damage, sizeof( damage ), ATLAS_XORSTR( "-{}" ).get( ), hit.m_damage );
            const point_t at{ static_cast< i32 >( screen.x + 13.0f ),
                              static_cast< i32 >( screen.y - 19.0f - progress * 23.0f ) };
            g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), at, color, damage, true );
        }
    }
}

void hitmarker::clear( )
{
    for ( hit_t &hit : this->m_hits )
    {
        hit = hit_t{ };
    }
    this->m_next = 0;
}
