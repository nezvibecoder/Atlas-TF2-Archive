#include "prediction.hpp"

#include "tickbase.hpp"
#include "doors.hpp"

ATLAS_NO_DESTROY engine_prediction g_prediction{ };

static void suppress_events( C_BaseEntity *host )
{
    IPredictionSystem *pSys{ g_tf2.m_pred_system->g_pPredictionSystems };

    while ( pSys )
    {
        pSys->m_bSuppressEvent = ( host != nullptr );
        pSys->m_pSuppressHost  = host;
        pSys->m_nStatusPushed  = 0;
        pSys                   = pSys->m_pNextSystem;
    }
}

static void copy_to_buffer( C_BaseEntity *entity, datamap_t *map, const datamap_copy_t &buffer )
{
    CPredictionCopy copy{ PC_EVERYTHING, buffer.m_data, PC_DATA_PACKED, entity, PC_DATA_NORMAL };
    copy.TransferData( ATLAS_XORSTR( "ATL_PREDICTION_SAVE" ).get( ), entity->entindex( ), map );
}

static void copy_from_buffer( C_BaseEntity *entity, datamap_t *map, const datamap_copy_t &buffer )
{
    CPredictionCopy copy{ PC_EVERYTHING, entity, PC_DATA_NORMAL, buffer.m_data, PC_DATA_PACKED };
    copy.TransferData( ATLAS_XORSTR( "ATL_PREDICTION_RESTORE" ).get( ), entity->entindex( ), map );
}

bool datamap_copy_t::ensure( usize needed )
{
    if ( !needed )
    {
        return false;
    }

    if ( this->m_data && this->m_size == needed )
    {
        return true;
    }

    void *block{ };
    if ( !this->m_data )
    {
        block = g_tf2.m_alloc->Alloc( needed );
    } else
    {
        block = g_tf2.m_alloc->Realloc( this->m_data, needed );
    }

    if ( !block )
    {
        // A failed Realloc leaves the old undersized block alive; drop it so a stale buffer is
        // never treated as matching a new packed_size
        this->release( );
        return false;
    }

    this->m_data = static_cast< u8 * >( block );
    this->m_size = needed;

    // @ the packed layout is not gapless, a 2-byte FIELD_SHORT at offset 100 leaves 102-103
    // untouched by TransferData, so those bytes keep whatever the allocator handed over; Two
    // separately allocated images of the same entity then disagree on the gaps alone, which reads
    // as a field change that never happened; Zeroing once per allocation makes the gaps
    // deterministic; the copy overwrites every byte it owns straight after, and this never runs
    // per command only when a block is first taken or a weapon switch resizes it
    atl::memset( this->m_data, 0, needed );

    return true;
}

void datamap_copy_t::release( )
{
    if ( !this->m_data )
    {
        return;
    }

    g_tf2.m_alloc->Free( this->m_data );

    this->m_data = nullptr;
    this->m_size = 0;
}

static bool save_entity_map( C_BaseEntity *entity, datamap_copy_t &buffer )
{
    datamap_t *map{ entity->GetPredDescMap( ) };

    if ( !map || map->packed_size <= 0 ||
         !buffer.ensure( static_cast< usize >( math::bigger( map->packed_size, 4 ) ) ) )
    {
        return false;
    }

    copy_to_buffer( entity, map, buffer );
    return true;
}

static void restore_entity_map( C_BaseEntity *entity, const datamap_copy_t &buffer )
{
    if ( datamap_t * map{ entity->GetPredDescMap( ) } )
    {
        copy_from_buffer( entity, map, buffer );
    }
}

static void strip_error_flag( C_BaseEntity *entity, hash_t field, typedescription_t *&desc )
{
    if ( desc || !entity )
    {
        return;
    }

    desc = g_netvars.find_datamap_field( entity->GetPredDescMap( ), field );
    Assert( desc != nullptr );
    if ( desc )
    {
        desc->flags |= FTYPEDESC_NOERRORCHECK;
    }
}

bool engine_prediction::save_data_maps( )
{
    if ( !save_entity_map( this->m_player, this->m_player_buffer ) )
    {
        return false;
    }

    if ( this->m_weapon && !save_entity_map( this->m_weapon, this->m_weapon_buffer ) )
    {
        return false;
    }

    for ( usize index{ }; index < MAX_VIEWMODELS; ++index )
    {
        if ( this->m_viewmodels[ index ] && !save_entity_map( this->m_viewmodels[ index ], this->m_viewmodel_buffers[ index ] ) )
        {
            return false;
        }
    }

    return true;
}

void engine_prediction::restore_data_maps( )
{
    restore_entity_map( this->m_player, this->m_player_buffer );

    if ( this->m_weapon )
    {
        restore_entity_map( this->m_weapon, this->m_weapon_buffer );
    }

    for ( usize index{ }; index < MAX_VIEWMODELS; ++index )
    {
        if ( this->m_viewmodels[ index ] )
        {
            restore_entity_map( this->m_viewmodels[ index ], this->m_viewmodel_buffers[ index ] );
        }
    }
}

void engine_prediction::post_think( )
{
    g_tf2.m_CBasePlayer_ItemPostFrame.call_as< void >( this->m_player );
}

void engine_prediction::update( )
{
    g_tf2.m_CL_RunPrediction_fn.call_as< void >( );
}

void engine_prediction::on_post_data_update( )
{
    if ( !g_cl.m_local )
    {
        this->m_network_command = -1;
        return;
    }

    const i32 command{ g_tf2.m_cl->command_ack };
    const i32 tickbase{ g_cl.m_local->m_nTickBase( ) };
    if ( command == this->m_network_command && tickbase != this->m_network_tickbase &&
         g_tf2.m_prediction->m_nCommandsPredicted > 0 )
    {
        // @::PhysicsSimulate can run a copy of the last command at the server's credit cap,
        // advancing the authoritative state without a new ACK; ComputeFirstCommandToExecute
        // otherwise restores an old predicted frame because no command was acknowledged
        g_tf2.m_prediction->m_nCommandsPredicted = 0;
    }
    this->m_network_command  = command;
    this->m_network_tickbase = tickbase;
}

i32 engine_prediction::network_command( ) const
{
    return this->m_network_command;
}

i32 engine_prediction::network_tickbase( ) const
{
    return this->m_network_tickbase;
}

bool engine_prediction::begin_weapon( )
{
    if ( !this->m_is_active || this->m_weapon_entered || !this->m_player || !this->m_weapon )
    {
        return false;
    }
    this->m_weapon_entered = true;
    return true;
}

bool engine_prediction::sync_weapon_input( )
{
    if ( !this->m_is_active || !g_cl.m_cmd || !this->m_player )
    {
        return false;
    }

    constexpr i32 mask{ IN_ATTACK | IN_ATTACK2 | IN_RELOAD };
    const i32     buttons{ ( this->m_player->m_nButtons( ) & ~mask ) | ( g_cl.m_cmd->buttons & mask ) };
    const i32     changed{ buttons ^ this->m_previous_buttons };
    // RunCommand already latched the original input; Recompute weapon edges from the previous
    // command, preserving movement/button changes made by player think and movement this tick;
    this->m_player->m_nButtons( )         = buttons;
    this->m_player->m_afButtonLast( )     = ( this->m_player->m_afButtonLast( ) & ~mask ) | ( this->m_previous_buttons & mask );
    this->m_player->m_afButtonPressed( )  = ( this->m_player->m_afButtonPressed( ) & ~mask ) | ( changed & buttons & mask );
    this->m_player->m_afButtonReleased( ) = ( this->m_player->m_afButtonReleased( ) & ~mask ) | ( changed & this->m_previous_buttons & mask );
    g_tf2.m_prediction->SetLocalViewAngles( g_cl.m_cmd->viewangles );
    return true;
}

bool engine_prediction::start( i32 sequence_number )
{
    if ( this->m_player || !g_cl.m_cmd || !g_cl.m_local || !g_cl.m_weapon ||
         !g_cl.m_local->IsAlive( ) || g_cl.m_cmd->weaponselect != 0 || sequence_number <= 0 )
    {
        return false;
    }

    /*
    // the particle cannon's SDK prediction table omits its secondary charge state/effects;
    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_PARTICLE_CANNON &&
         ( ( g_cl.m_cmd->buttons & IN_ATTACK2 ) ||
           g_cl.m_weapon->As< C_TFParticleCannon * >( )->m_flChargeBeginTime( ) > 0.0f ) ) { return false; }
    */

    this->m_player            = g_cl.m_local;
    this->m_weapon            = g_cl.m_weapon;
    this->m_weapon_entered    = false;
    this->m_weapon_fire_delay = 0.0f;
    this->m_previous_buttons  = this->m_player->m_nButtons( );
    for ( usize index{ }; index < MAX_VIEWMODELS; ++index )
    {
        this->m_viewmodels[ index ] = ( &this->m_player->m_hViewModel( ) )[ index ].Get( );
    }

    strip_error_flag( this->m_player, HASH_CT( "m_flInspectTime" ), this->m_masked_fields[ MASK_INSPECT_TIME ] );
    strip_error_flag( this->m_player, HASH_CT( "m_nModelIndex" ), this->m_masked_fields[ MASK_MODEL_INDEX ] );
    strip_error_flag( this->m_weapon, HASH_CT( "m_flTimeWeaponIdle" ), this->m_masked_fields[ MASK_WEAPON_IDLE ] );
    strip_error_flag( this->m_viewmodels[ 0 ], HASH_CT( "m_nSequence" ), this->m_masked_fields[ MASK_VIEWMODEL_SEQUENCE ] );
    strip_error_flag( this->m_viewmodels[ 0 ], HASH_CT( "m_nAnimationParity" ), this->m_masked_fields[ MASK_VIEWMODEL_PARITY ] );
    strip_error_flag( this->m_viewmodels[ 0 ], HASH_CT( "m_flPlaybackRate" ), this->m_masked_fields[ MASK_VIEWMODEL_PLAYBACK ] );

    if ( !this->save_data_maps( ) )
    {
        this->m_player = nullptr;
        this->m_weapon = nullptr;
        return false;
    }

    this->m_backup.store( );

    g_hooks.m_get_local_view_angles.thiscall< void >( g_tf2.m_prediction, &this->m_local_view_angles );

    g_tickbase.on_run_simulation( this->m_player, sequence_number );

    this->m_in_prediction        = g_tf2.m_prediction->m_bInPrediction;
    this->m_first_time_predicted = g_tf2.m_prediction->m_bFirstTimePredicted;

    g_tf2.m_prediction->m_bFirstTimePredicted = false;
    g_tf2.m_prediction->m_bInPrediction       = true;

    suppress_events( this->m_player );

    g_tf2.m_move_helper->SetHost( this->m_player );

    this->m_player->m_vecPreviouslyPredictedOrigin( ) = this->m_player->m_vecOrigin( );

    g_doors.pose( g_cl.m_cmd->command_number );

    this->m_is_active = true;

    // the server's copies of our last command run before this one (tick_manager::next_copy); The weapon preview is this
    // command's, so it stays closed through them..
    this->m_weapon_entered = true;
    for ( CUserCmd copy{ }; g_tickbase.next_copy( this->m_player, sequence_number, copy ); )
    {
        g_tf2.m_prediction->RunCommand( this->m_player, &copy, g_tf2.m_move_helper );
    }
    this->m_weapon_entered = false;

    g_tf2.m_prediction->RunCommand( this->m_player, g_cl.m_cmd, g_tf2.m_move_helper );
    this->m_is_active = false;

    g_doors.on_command_predicted( g_cl.m_cmd->command_number, this->m_player );
    g_doors.restore( );

    g_tf2.m_move_helper->SetHost( nullptr );

    suppress_events( nullptr );

    if ( const CTFWeaponInfo *info{ this->m_weapon ? this->m_weapon->GetWeaponInfo( ) : nullptr } )
    {
        const i32 mode{ math::clamp( this->m_weapon->m_iWeaponMode( ), 0, 1 ) };
        this->m_weapon_fire_delay = this->m_weapon->ApplyFireDelay( info->GetWeaponData( mode ).m_flTimeFireDelay );
    }

    return true;
}

void engine_prediction::end( )
{
    if ( !this->m_player ) // failsafe
    {
        return;
    }

    // m_nTickBase sits in the player's prediction datamap so this also rewinds
    // the tick RunCommand advanced and the engine replays the command from the base it expects
    this->restore_data_maps( );
    g_tf2.m_prediction->SetLocalViewAngles( this->m_local_view_angles );

    this->m_backup.restore( );

    g_tf2.m_prediction->m_bInPrediction       = this->m_in_prediction;
    g_tf2.m_prediction->m_bFirstTimePredicted = this->m_first_time_predicted;

    this->m_player = nullptr;
    this->m_weapon = nullptr;
}

void engine_prediction::shutdown( )
{
    this->m_player_buffer.release( );
    this->m_weapon_buffer.release( );
    for ( usize index{ }; index < MAX_VIEWMODELS; ++index )
    {
        this->m_viewmodel_buffers[ index ].release( );
        this->m_viewmodels[ index ] = nullptr;
    }

    this->m_is_active       = false;
    this->m_player          = nullptr;
    this->m_weapon          = nullptr;
    this->m_network_command = -1;
    this->m_weapon_entered  = false;

    for ( typedescription_t *&field : this->m_masked_fields )
    {
        if ( field )
        {
            field->flags &= ~FTYPEDESC_NOERRORCHECK;
            field = nullptr;
        }
    }
}
