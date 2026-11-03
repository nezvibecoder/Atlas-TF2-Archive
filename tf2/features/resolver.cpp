#include "resolver.hpp"

#include "anims.hpp"
#include "lagcomp.hpp"

ATLAS_NO_DESTROY resolver g_resolver{ };

constexpr float REAL_OFFSETS[ RESOLVER_YAWS ]{ 0.0f, 180.0f, 180.0f, 135.0f, -135.0f, 90.0f, -90.0f };

constexpr i32 HALF_TURN{ 1 };
constexpr i32 HALF_TURN_MIRRORED{ 2 };

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/tf/tf_player.cpp#L751
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_playeranimstate.cpp#L378
constexpr float REAL_PITCH_SCALES[ RESOLVER_PITCHES ]{ -1.0f, 1.0f, 0.0f };

ATLAS_FORCE_INLINE bool is_headshot_weapon( C_TFWeaponBase *weapon )
{
    switch ( weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_CLASSIC:
        case TF_WEAPON_SNIPERRIFLE_DECAP: return true;
        case TF_WEAPON_REVOLVER: return weapon->As< C_TFRevolver * >( )->CanHeadshot( );
        default: return false;
    }
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/tf/tf_player.cpp#L8317
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_sniperrifle.cpp#L1004
ATLAS_FORCE_INLINE bool headshot_registers( C_TFWeaponBase *weapon, C_TFPlayer *target )
{
    switch ( weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_DECAP:
            return weapon->m_iItemDefinitionIndex( ) != Sniper_m_TheSydneySleeper && weapon->As< C_TFSniperRifle * >( )->IsReadyForHeadshot( );
        case TF_WEAPON_SNIPERRIFLE_CLASSIC: return weapon->As< C_TFSniperRifle * >( )->m_flChargedDamage( ) >= TF_WEAPON_SNIPERRIFLE_DAMAGE_MAX;
        case TF_WEAPON_REVOLVER:
        {
            C_TFRevolver *revolver{ weapon->As< C_TFRevolver * >( ) };
            return revolver->CanHeadshot( ) && revolver->IsReadyForHeadshot( ) && revolver->CanHeadshotTarget( target );
        }
        default: return false;
    }
}

ATLAS_FORCE_INLINE bool pitch_hidden( float sent )
{
    return math::abs( sent ) >= 90.0f;
}

ATLAS_FORCE_INLINE float hidden_yaw( i32 candidate, float interpolated, float sent )
{
    return candidate == 0 ? interpolated : math::normalize_yaw( sent + REAL_OFFSETS[ candidate ] );
}

void resolver::begin_batch( C_TFPlayer *player, const remote_anim_record_t &current, i32 ticks, bool continuous )
{
    const i32         index{ player->entindex( ) };
    resolver_track_t &track{ this->m_tracks[ index ] };

    if ( track.m_handle != current.m_handle )
    {
        track                  = resolver_track_t{ };
        track.m_handle         = current.m_handle;
        this->m_shots[ index ] = resolver_shot_t{ };
    }

    track.m_tracked      = g_cfg.m_resolver.m_enabled && player->m_iTeamNum( ) != g_cl.m_local->m_iTeamNum( );
    track.m_engaged      = track.m_tracked && g_cl.m_weapon && is_headshot_weapon( g_cl.m_weapon );
    track.m_hidden_yaw   = track.m_engaged && ticks > 1;
    track.m_hidden_pitch = track.m_engaged && pitch_hidden( current.m_angles.x );
    if ( !track.m_tracked )
    {
        track.m_seeded = false;
        return;
    }

    const i32 applied{ track.m_engaged ? track.m_yaw : 0 };
    if ( continuous && track.m_seeded && applied != track.m_applied )
    {
        CTFPlayerAnimState *anim_state{ player->m_PlayerAnimState( ) };
        anim_state->m_flGoalFeetYaw( )    = track.m_goal[ applied ];
        anim_state->m_flCurrentFeetYaw( ) = track.m_feet[ applied ];
    }

    track.m_applied    = applied;
    track.m_continuous = continuous;
    track.m_tick       = current.m_tick;
    track.m_sent_yaw   = current.m_angles.y;
}

vec3 resolver::command_angles( C_TFPlayer *player, const vec3 &interpolated, const vec3 &sent, bool endpoint, bool moving )
{
    resolver_track_t &track{ this->m_tracks[ player->entindex( ) ] };
    if ( !track.m_tracked )
    {
        return interpolated;
    }

    for ( i32 candidate{ }; candidate < RESOLVER_YAWS; ++candidate )
    {
        step_feet_yaw( track.m_goal[ candidate ], track.m_feet[ candidate ], endpoint ? sent.y : hidden_yaw( candidate, interpolated.y, sent.y ),
                       moving );
    }

    if ( !endpoint )
    {
        return vec3{ interpolated.x, hidden_yaw( track.m_applied, interpolated.y, sent.y ), 0.0f };
    }

    const float pitch{ track.m_hidden_pitch ? sent.x * REAL_PITCH_SCALES[ track.m_pitch ] : sent.x };

    return vec3{ pitch, sent.y, 0.0f };
}

void resolver::end_batch( C_TFPlayer *player )
{
    resolver_track_t &track{ this->m_tracks[ player->entindex( ) ] };
    if ( !track.m_tracked )
    {
        return;
    }

    CTFPlayerAnimState *anim_state{ player->m_PlayerAnimState( ) };

    if ( !track.m_continuous || !track.m_seeded )
    {
        for ( i32 candidate{ }; candidate < RESOLVER_YAWS; ++candidate )
        {
            track.m_goal[ candidate ] = anim_state->m_flGoalFeetYaw( );
            track.m_feet[ candidate ] = anim_state->m_flCurrentFeetYaw( );
        }

        track.m_seeded = true;
    } else
    {
        track.m_goal[ track.m_applied ] = anim_state->m_flGoalFeetYaw( );
        track.m_feet[ track.m_applied ] = anim_state->m_flCurrentFeetYaw( );
    }

    const i32 source{ track.m_applied == HALF_TURN_MIRRORED ? HALF_TURN_MIRRORED : HALF_TURN };
    const i32 mirror{ source == HALF_TURN ? HALF_TURN_MIRRORED : HALF_TURN };
    track.m_goal[ mirror ] = math::normalize_yaw( 2.0f * track.m_sent_yaw - track.m_goal[ source ] );
    track.m_feet[ mirror ] = math::normalize_yaw( 2.0f * track.m_sent_yaw - track.m_feet[ source ] );
}

void resolver::on_shot( C_TFPlayer *player, i32 tick, i32 hitbox, const vec3 &eye, const vec3 &point )
{
    const i32               index{ player->entindex( ) };
    const resolver_track_t &track{ this->m_tracks[ index ] };
    resolver_shot_t        &shot{ this->m_shots[ index ] };

    if ( !track.m_engaged ||
         ( shot.m_pending && !shot.m_acked ) ||
         ( !track.m_hidden_yaw && !track.m_hidden_pitch ) ||
         hitbox != HITBOX_HEAD ||
         player->IsUbered( ) || player->IsBonked( ) ||
         tick <= track.m_switch_tick ||
         !headshot_registers( g_cl.m_weapon, player ) ||
         !this->aimed_at_record_head( player, tick, eye, point ) )
    {
        return;
    }

    shot.m_command = g_cl.m_cmd->command_number;
    shot.m_health  = player->m_iHealth( );
    shot.m_weapon  = g_cl.m_weapon->GetWeaponID( );
    shot.m_pending = true;
    shot.m_acked   = false;
}

bool resolver::aimed_at_record_head( C_TFPlayer *player, i32 tick, const vec3 &eye, const vec3 &point ) const
{
    const i32 index{ player->entindex( ) };
    for ( i32 age{ }; const lag_record_t *record{ g_lagcomp.record( index, age ) }; ++age )
    {
        if ( record->m_tick != tick || record->m_bone_count <= 0 )
        {
            continue;
        }

        const mstudiohitboxset_t *set{ record->m_header->pHitboxSet( player->m_nHitboxSet( ) ) };
        const mstudiobbox_t      *head{ set->pHitbox( HITBOX_HEAD ) };

        vec3 centre{ };
        math::vector_transform( ( head->bbmin + head->bbmax ) * 0.5f, record->m_bones[ head->bone ], centre );

        return first_hitbox( set, record->m_bones, eye, point ) == HITBOX_HEAD && first_hitbox( set, record->m_bones, eye, centre ) == HITBOX_HEAD;
    }

    return false;
}

void resolver::on_player_hurt( IGameEvent *event )
{
    if ( !g_cl.m_local )
    {
        return;
    }

    const i32 attacker{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ATLAS_XORSTR( "attacker" ).get( ) ) ) };
    const i32 victim{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ATLAS_XORSTR( "userid" ).get( ) ) ) };
    if ( attacker != g_cl.m_local->entindex( ) || victim < 1 || victim >= MAX_TRACKED_PLAYERS )
    {
        return;
    }

    const resolver_shot_t &shot{ this->m_shots[ victim ] };
    if ( !shot.m_pending || event->GetInt( ATLAS_XORSTR( "weaponid" ).get( ) ) != shot.m_weapon )
    {
        return;
    }

    const i32 custom{ event->GetInt( ATLAS_XORSTR( "custom" ).get( ) ) };
    this->settle( victim, custom == TF_DMG_CUSTOM_HEADSHOT || custom == TF_DMG_CUSTOM_HEADSHOT_DECAPITATION );
}

void resolver::think( )
{
    if ( !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        return;
    }

    const i32 acked{ g_tf2.m_cl->command_ack };
    for ( i32 index{ 1 }; index < MAX_TRACKED_PLAYERS; ++index )
    {
        resolver_shot_t &shot{ this->m_shots[ index ] };
        if ( !shot.m_pending || shot.m_acked || acked < shot.m_command )
        {
            continue;
        }

        IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( index ) };
        C_BaseEntity  *base_entity{ client_entity ? client_entity->GetBaseEntity( ) : nullptr };
        if ( !base_entity || base_entity->IsDormant( ) )
        {
            shot = resolver_shot_t{ };
            continue;
        }

        C_TFPlayer *player{ base_entity->As< C_TFPlayer * >( ) };
        if ( player->IsAlive( ) && player->m_iHealth( ) >= shot.m_health )
        {
            this->settle( index, false );
            continue;
        }

        shot.m_acked = true;
    }
}

void resolver::settle( i32 index, bool head )
{
    resolver_track_t &track{ this->m_tracks[ index ] };
    this->m_shots[ index ] = resolver_shot_t{ };

    if ( head )
    {
        return;
    }

    if ( track.m_hidden_yaw )
    {
        track.m_yaw = ( track.m_yaw + 1 ) % RESOLVER_YAWS;
    }

    if ( track.m_hidden_pitch && ( !track.m_hidden_yaw || track.m_yaw == 0 ) )
    {
        track.m_pitch = ( track.m_pitch + 1 ) % RESOLVER_PITCHES;
    }

    track.m_switch_tick = track.m_tick;
}

void resolver::clear( )
{
    for ( i32 index{ }; index < MAX_TRACKED_PLAYERS; ++index )
    {
        this->m_tracks[ index ] = resolver_track_t{ };
        this->m_shots[ index ]  = resolver_shot_t{ };
    }
}

bool resolver::guessing( i32 index ) const
{
    const resolver_track_t &track{ this->m_tracks[ index ] };
    return track.m_hidden_yaw || track.m_hidden_pitch;
}
