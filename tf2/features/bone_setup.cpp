#include "bone_setup.hpp"

#include "math/math.hpp"
#include "runtime/memory.hpp"

ATLAS_NO_DESTROY bone_setup g_bone_mgr{ };

void bone_setup::AccumulatePose( bone_setup_state_t &state, vec3 *pos, quaternion *q, i32 sequence,
                                 float cycle, float weight, float time ) const
{
    g_tf2.m_CBoneSetup_AccumulatePose
        .call_as< void, bone_setup_state_t *, vec3 *, quaternion *, i32, float, float, float,
                  void * >( &state, pos, q, sequence, cycle, weight, time, nullptr );
}

// what the client's animstate wrote on its last frame; replaying a stored tick means overwriting
// origin, angles and time with that tick's values, since these are the interpolated ones
bool bone_setup::capture( C_BaseAnimatingOverlay *entity, pose_snapshot_t &snapshot ) const
{
    if ( !entity )
    {
        return false;
    }

    snapshot.m_origin   = entity->m_vecOrigin( );
    snapshot.m_angles   = entity->GetRenderAngles( ); // players carry the animstate feet yaw here
    snapshot.m_time     = entity->m_flSimulationTime( );
    snapshot.m_sequence = entity->m_nSequence( );
    snapshot.m_cycle    = entity->m_flCycle( );
    snapshot.m_scale    = entity->m_flModelScale( );

    atl::memcpy( snapshot.m_pose_parameters, entity->m_flPoseParameter( ),
                 sizeof( snapshot.m_pose_parameters ) );
    atl::memcpy( snapshot.m_controllers, entity->m_flEncodedController( ),
                 sizeof( snapshot.m_controllers ) );

    const anim_overlay_t &overlays{ entity->m_AnimOverlay( ) };

    snapshot.m_layer_count = math::smaller( overlays.count( ), MAX_OVERLAYS );

    for ( i32 index{ }; index < snapshot.m_layer_count; ++index )
    {
        const C_AnimationLayer &layer{ overlays[ index ] };
        anim_layer_t           &target{ snapshot.m_layers[ index ] };

        target.m_sequence = layer.m_nSequence;
        target.m_order    = layer.m_nOrder;
        target.m_cycle    = layer.m_flCycle;
        target.m_weight   = layer.m_flWeight;
    }

    return true;
}

void bone_setup::accumulate( CStudioHdr *studio, const pose_snapshot_t &snapshot, i32 bone_mask,
                             vec3 *pos, quaternion *q )
{
    bone_setup_state_t state{ studio, bone_mask, snapshot.m_pose_parameters, nullptr };

    g_tf2.m_BoneSetup_InitPose.call_as< void, CStudioHdr *, vec3 *, quaternion *, i32 >(
        studio, pos, q, bone_mask );

    // @ the game's IBoneSetup ctor heap allocates exactly these 32 bytes on every call
    // and frees them in the dtor, so we keep it on the stack and call the inner entry points
    this->AccumulatePose( state, pos, q, snapshot.m_sequence, snapshot.m_cycle, 1.0f,
                          snapshot.m_time );

    i32 sorted[ MAX_OVERLAYS ];
    for ( i32 slot{ }; slot < MAX_OVERLAYS; ++slot )
    {
        sorted[ slot ] = -1;
    }

    const i32 count{ math::smaller( snapshot.m_layer_count, MAX_OVERLAYS ) };

    for ( i32 index{ }; index < count; ++index )
    {
        const anim_layer_t &layer{ snapshot.m_layers[ index ] };
        if ( layer.m_weight > 0.0f && layer.m_sequence >= 0 && layer.m_order >= 0 &&
             layer.m_order < count )
        {
            sorted[ layer.m_order ] = index;
        }
    }

    for ( i32 slot{ }; slot < count; ++slot )
    {
        if ( sorted[ slot ] < 0 )
        {
            continue;
        }

        const anim_layer_t &layer{ snapshot.m_layers[ sorted[ slot ] ] };

        this->AccumulatePose( state, pos, q, layer.m_sequence, layer.m_cycle, layer.m_weight,
                              snapshot.m_time );
    }

    bone_setup_state_t *handle{ &state };

    g_tf2.m_IBoneSetup_CalcAutoplaySequences
        .call_as< void, bone_setup_state_t **, vec3 *, quaternion *, float, void * >(
            &handle, pos, q, snapshot.m_time, nullptr );

    if ( studio->m_pStudioHdr->numbonecontrollers > 0 )
    {
        g_tf2.m_BoneSetup_CalcBoneAdj
            .call_as< void, CStudioHdr *, vec3 *, quaternion *, const float *, i32 >(
                studio, pos, q, snapshot.m_controllers, bone_mask );
    }
}

void bone_setup::build_matrices( const studiohdr_t *header, const pose_snapshot_t &snapshot,
                                 i32 bone_mask, const vec3 *pos, const quaternion *q,
                                 matrix3x4 *out ) const
{
    matrix3x4 root;
    math::angle_matrix( snapshot.m_angles, snapshot.m_origin, root );

    if ( snapshot.m_scale < 0.99999988f || snapshot.m_scale > 1.0000001f )
    {
        for ( i32 row{ }; row < 3; ++row )
        {
            root[ row ][ 0 ] *= snapshot.m_scale;
            root[ row ][ 1 ] *= snapshot.m_scale;
            root[ row ][ 2 ] *= snapshot.m_scale;
        }
    }

    matrix3x4 local;

    for ( i32 index{ }; index < header->numbones; ++index )
    {
        const mstudiobone_t *bone{ header->pBone( index ) };

        if ( !( bone->flags & bone_mask ) )
        {
            continue;
        }

        math::quaternion_matrix( q[ index ], pos[ index ], local );

        if ( bone->parent < 0 )
        {
            math::concat_transforms( root, local, out[ index ] );
            continue;
        }

        math::concat_transforms( out[ bone->parent ], local, out[ index ] );
    }
}

bool bone_setup::build( CStudioHdr *studio, const pose_snapshot_t &snapshot, i32 bone_mask,
                        matrix3x4 *out, vec3 *pos, quaternion *q )
{
    if ( !studio || !studio->m_pStudioHdr || !out || snapshot.m_sequence < 0 )
    {
        return false;
    }

    // server GetSkeleton bails before touching the pose when the model carries no sequences
    if ( !g_tf2.m_CStudioHdr_SequencesAvailable.call_as< bool, CStudioHdr * >( studio ) )
    {
        return false;
    }

    const studiohdr_t *header{ studio->m_pStudioHdr };

    if ( header->numbones <= 0 || header->numbones > MAX_STUDIO_BONES )
    {
        return false;
    }

    this->accumulate( studio, snapshot, bone_mask, pos, q );
    this->build_matrices( header, snapshot, bone_mask, pos, q, out );

    return true;
}

// the accessor is what the game reads bones through for the rest of the frame, and the attachment
// helper walks it to place the weapon, the muzzle flash and anything bolted to a bone; Skipping the
// original SetupBones without doing both leaves all of that frozen
void bone_setup::publish( C_BaseAnimatingOverlay *entity, CStudioHdr *studio, i32 bone_mask,
                          const slot_t &slot ) const
{
    bone_accessor_t &accessor{ entity->m_BoneAccessor( ) };

    if ( accessor.m_bones )
    {
        atl::memcpy( accessor.m_bones, slot.m_bones, sizeof( matrix3x4 ) * slot.m_count );
    }

    accessor.m_readable = bone_mask;
    accessor.m_writable = bone_mask;

    if ( bone_mask & BONE_USED_BY_ATTACHMENT )
    {
        g_tf2.m_CBaseAnimating_SetupBonesAttachmentHelper.call_as< bool, void *, CStudioHdr * >(
            entity, studio );
    }
}

void bone_setup::clear( )
{
    for ( slot_t &slot : this->m_slots )
    {
        slot.m_header        = nullptr;
        slot.m_version       = 0;
        slot.m_built_version = -1;
        slot.m_mask          = 0;
        slot.m_count         = 0;
    }

    this->m_shown.m_header  = nullptr;
    this->m_shown.m_version = 0;
}

bool bone_setup::stored_pose( C_TFPlayer *entity, const studiohdr_t *header, pose_snapshot_t &snapshot )
{
    return this->read( this->m_slots[ entity->entindex( ) ], header, snapshot );
}

bool bone_setup::shown_pose( const studiohdr_t *header, pose_snapshot_t &snapshot )
{
    return this->read( this->m_shown, header, snapshot );
}

void bone_setup::store_pose( C_TFPlayer *entity )
{
    this->store( entity, this->m_slots[ entity->entindex( ) ] );
}

void bone_setup::store_shown_pose( C_TFPlayer *entity )
{
    this->store( entity, this->m_shown );
}

bool bone_setup::read( slot_t &slot, const studiohdr_t *header, pose_snapshot_t &snapshot )
{
    while ( slot.m_busy.exchange( true ) )
    {
        _mm_pause( );
    }

    const bool stored{ slot.m_version > 0 && slot.m_header == header };
    if ( stored )
    {
        snapshot = slot.m_snapshot;
    }

    slot.m_busy.store( false );

    return stored;
}

void bone_setup::store( C_TFPlayer *entity, slot_t &slot )
{
    if ( entity->IsDormant( ) || !entity->IsAlive( ) || entity->m_nSequence( ) < 0 ||
         entity->InCond( ETFCond::TF_COND_TAUNTING ) )
    {
        return;
    }

    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( entity ) };

    if ( !studio || !studio->m_pStudioHdr )
    {
        return;
    }

    while ( slot.m_busy.exchange( true ) )
    {
        _mm_pause( );
    }

    if ( this->capture( entity, slot.m_snapshot ) )
    {
        // server sets player's abs angles at 0 pitch and roll
        // i guess that's why my skeletons were laying flat on the ground.
        slot.m_snapshot.m_angles.x = 0.0f;
        slot.m_snapshot.m_angles.z = 0.0f;

        slot.m_header = studio->m_pStudioHdr;
        ++slot.m_version;
    }

    slot.m_busy.store( false );
}

bool bone_setup::player_setup( C_TFPlayer *entity, matrix3x4 *out, i32 max_bones, i32 bone_mask )
{
    if ( entity->IsDormant( ) || !entity->IsAlive( ) || entity->m_nSequence( ) < 0 ||
         entity->InCond( ETFCond::TF_COND_TAUNTING ) )
    {
        return false;
    }

    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( entity ) };

    if ( !studio || !studio->m_pStudioHdr )
    {
        return false;
    }

    slot_t &slot{ this->m_slots[ entity->entindex( ) ] };

#if defined( ATLAS_DEV )
    bool contended{ };

    while ( slot.m_busy.exchange( true ) )
    {
        contended = true;
    }

    if ( contended )
    {
        DBGOUT( "bones[%d]: two threads asked for the same player", entity->entindex( ) );
    }
#else
    while ( slot.m_busy.exchange( true ) )
    {
        _mm_pause( );
    }
#endif

    bool result{ slot.m_version > 0 && slot.m_header == studio->m_pStudioHdr };

    if ( result )
    {
        // the game carries the masks it was asked for across frames for the same reason: a caller
        // after attachments must not be handed a skeleton that was built for hitboxes alone
        const i32 mask{ slot.m_built_version >= 0 ? slot.m_mask | bone_mask : bone_mask };
        bool      changed{ };
        if ( mask != slot.m_mask || slot.m_built_version != slot.m_version )
        {
            // MDLCACHE_CRITICAL_SECTION
            g_tf2.m_mdl_cache->BeginLock( );

            alignas( 16 ) vec3       pos[ MAX_STUDIO_BONES ];
            alignas( 16 ) quaternion q[ MAX_STUDIO_BONES ];

            result = this->build( studio, slot.m_snapshot, mask, slot.m_bones, pos, q );

            g_tf2.m_mdl_cache->EndLock( );

            if ( result )
            {
                slot.m_built_version = slot.m_version;
                slot.m_mask          = mask;
                slot.m_count         = math::smaller( slot.m_header->numbones, MAX_STUDIO_BONES );
                slot.m_built_origin  = slot.m_snapshot.m_origin;
                changed              = true;
            }
#if defined( ATLAS_DEV )
            else
            {
                DBGOUT( "bones[%d]: build failed seq %d", entity->entindex( ),
                        slot.m_snapshot.m_sequence );
            }
#endif
        }

        if ( result && entity == g_cl.m_local && entity->GetAbsOrigin( ) != slot.m_built_origin )
        {
            const vec3 offset{ entity->GetAbsOrigin( ) - slot.m_built_origin };
            for ( i32 index{ }; index < slot.m_count; ++index )
            {
                slot.m_bones[ index ][ 0 ][ 3 ] += offset.x;
                slot.m_bones[ index ][ 1 ][ 3 ] += offset.y;
                slot.m_bones[ index ][ 2 ][ 3 ] += offset.z;
            }

            slot.m_built_origin = entity->GetAbsOrigin( );
            changed             = true;
        }

        if ( changed )
        {
            this->publish( entity, studio, mask, slot );
        }
    }

    if ( result && out )
    {
        const bone_cache_t &cache{ entity->m_CachedBoneData( ) };
        result = cache.m_count > 0 && max_bones >= cache.m_count;

        if ( result )
        {
            atl::memcpy( out, cache.m_bones, sizeof( matrix3x4 ) * cache.m_count );
        }
    }

    slot.m_busy.store( false );

    return result;
}
