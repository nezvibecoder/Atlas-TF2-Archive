#pragma once

#include "link.hpp"

inline constexpr i32 HEAD_SCAN_POINTS{ 9 };

ATLAS_FORCE_INLINE void head_points( const mstudiobbox_t *head, const matrix3x4 *bones, vec3 *points )
{
    const vec3       center{ ( head->bbmin + head->bbmax ) * 0.5f };
    const vec3       quarter{ ( head->bbmax - head->bbmin ) * 0.25f };
    const matrix3x4 &head_bone{ bones[ head->bone ] };

    math::vector_transform( center, head_bone, points[ 0 ] );
    for ( i32 corner{ 0 }; corner < HEAD_SCAN_POINTS - 1; ++corner )
    {
        const vec3 offset{ corner & 1 ? quarter.x : -quarter.x, corner & 2 ? quarter.y : -quarter.y, corner & 4 ? quarter.z : -quarter.z };
        math::vector_transform( center + offset, head_bone, points[ corner + 1 ] );
    }
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/bone_setup.cpp#L5320
ATLAS_FORCE_INLINE i32 first_hitbox( const mstudiohitboxset_t *set, const matrix3x4 *bones, const vec3 &eye, const vec3 &point )
{
    i32 first{ -1 };
    f32 nearest{ FLT_MAX };

    for ( i32 hitbox{ 0 }; hitbox < set->numhitboxes; ++hitbox )
    {
        const mstudiobbox_t *box{ set->pHitbox( hitbox ) };

        f32 entry{ };
        if ( math::ray_hits_box( eye, point, bones[ box->bone ], box->bbmin, box->bbmax, 0.0f, &entry ) && entry < nearest )
        {
            nearest = entry;
            first   = hitbox;
        }
    }

    return first;
}

struct pose_snapshot_t
{
    vec3  m_origin{ };
    vec3  m_angles{ }; // what the animstate hands to SetLocalAngles
    i32   m_sequence{ -1 };
    float m_cycle{ };
    float m_time{ };
    float m_scale{ 1.0f };

    anim_layer_t m_layers[ MAX_OVERLAYS ]{ };
    i32          m_layer_count{ };

    float m_pose_parameters[ MAX_POSE_PARAMETERS ]{ };
    float m_controllers[ MAX_BONE_CONTROLLERS ]{ };
};

class bone_setup
{
public:
    bool capture( C_BaseAnimatingOverlay *entity, pose_snapshot_t &snapshot ) const;
    bool build( CStudioHdr *studio, const pose_snapshot_t &snapshot, i32 bone_mask, matrix3x4 *out,
                vec3 *pos, quaternion *q );
    bool player_setup( C_TFPlayer *entity, matrix3x4 *out, i32 max_bones, i32 bone_mask );
    bool stored_pose( C_TFPlayer *entity, const studiohdr_t *header, pose_snapshot_t &snapshot );
    bool shown_pose( const studiohdr_t *header, pose_snapshot_t &snapshot );
    void store_pose( C_TFPlayer *entity );
    void store_shown_pose( C_TFPlayer *entity );
    void clear( );

private:
    struct slot_t
    {
        matrix3x4           m_bones[ MAX_STUDIO_BONES ]{ };
        pose_snapshot_t     m_snapshot{ };
        vec3                m_built_origin{ };
        const studiohdr_t  *m_header{ };
        i32                 m_version{ };
        i32                 m_built_version{ -1 };
        i32                 m_mask{ };
        i32                 m_count{ };
        atl::atomic< bool > m_busy{ };
    };

    void store( C_TFPlayer *entity, slot_t &slot );
    bool read( slot_t &slot, const studiohdr_t *header, pose_snapshot_t &snapshot );
    void AccumulatePose( bone_setup_state_t &state, vec3 *pos, quaternion *q, i32 sequence,
                         float cycle, float weight, float time ) const;

    void accumulate( CStudioHdr *studio, const pose_snapshot_t &snapshot, i32 bone_mask, vec3 *pos,
                     quaternion *q );
    void build_matrices( const studiohdr_t *header, const pose_snapshot_t &snapshot, i32 bone_mask,
                         const vec3 *pos, const quaternion *q, matrix3x4 *out ) const;
    void publish( C_BaseAnimatingOverlay *entity, CStudioHdr *studio, i32 bone_mask,
                  const slot_t &slot ) const;

    slot_t m_slots[ MAX_TRACKED_PLAYERS ]{ };
    slot_t m_shown{ };
};

extern bone_setup g_bone_mgr;
