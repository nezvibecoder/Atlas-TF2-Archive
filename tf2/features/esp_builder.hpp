#pragma once

#include "runtime/mutex.hpp"

#include "link.hpp"

class esp_builder
{
public:
    void paint( );
    void on_level_shutdown( );
    void on_level_init( );
    void on_device_lost( );

    void begin_frame( );
    void end_frame( );

    void set_viewport( rect_t bounds )
    {
        this->m_pending.m_viewport = bounds;
    }

    void set_profile( const char *model, const char *weapon, const char *stance, i32 team,
                      usize local_model = config::LOCAL_MODEL_PLAYER );

    void set_yaw( float degrees )
    {
        this->m_pending.m_yaw = degrees;
    }

    struct bone_segment_t
    {
        point_t m_from{ };
        point_t m_to{ };
    };

    struct frame_t
    {
        rect_t         m_viewport{ };
        rect_t         m_bounds{ };
        bone_segment_t m_skeleton[ MAX_STUDIO_BONES ]{ };
        i32            m_skeleton_count{ };
        u64            m_serial{ };
        u64            m_epoch{ };
        bool           m_valid{ };
        usize          m_local_model{ };
    };

    void snapshot( frame_t &out ) const
    {
        atl::scoped_lock lock{ this->m_publish };
        out = this->m_frame;
    }

    void *model_for_index( i32 index ) const
    {
        if ( index == WEAPON_MODEL_INDEX )
        {
            return this->m_weapon_model;
        }

        if ( index == ENTITY_MODEL_INDEX )
        {
            return this->m_active_model;
        }

        return nullptr;
    }

    const C_BaseEntity *entity( ) const
    {
        return this->m_entity;
    }

    bool on_setup_bones( const void *entity, matrix3x4 *out, i32 max_bones );
    void route( C_BaseEntity *entity, const DrawModelInfo_t *info, const matrix3x4 *bone_to_world, i32 &flags );

    bool drawing( ) const
    {
        return this->m_drawing;
    }

private:
    static constexpr i32 ENTITY_MODEL_INDEX{ 0x7FFE };
    static constexpr i32 WEAPON_MODEL_INDEX{ 0x7FFD };

    struct request_t
    {
        u64                    m_serial{ };
        rect_t                 m_viewport{ };
        const char            *m_model{ };
        const char            *m_weapon{ };
        const char            *m_stance{ };
        i32                    m_team{ TF_TEAM_RED };
        float                  m_yaw{ };
        usize                  m_local_model{ };
        config::cham_profile_t  m_chams{ };
        config::glow_profile_t  m_glow{ };
        config::cham_profile_t  m_weapon_chams{ };
        config::glow_profile_t  m_weapon_glow{ };
    };

    struct route_t
    {
        const config::cham_profile_t *m_chams{ };
        const config::glow_profile_t *m_glow{ };
        bool                          m_routed{ };
    };

    class capture_t final : public CFunctor
    {
    public:
        i32 AddRef( ) override
        {
            return _InterlockedIncrement( &this->m_references );
        }

        i32 Release( ) override
        {
            return _InterlockedDecrement( &this->m_references );
        }

        void operator()( ) override;

        bool acquire( )
        {
            return _InterlockedCompareExchange( &this->m_references, 1, 0 ) == 0;
        }

        esp_builder *m_owner{ };
        frame_t      m_frame{ };
        usize        m_target{ };

    private:
        volatile long m_references{ };
    };

    using base_flex_ctor_fn     = void *( ATLAS_FASTCALL * )( void * );
    using set_model_by_index_fn = void( ATLAS_FASTCALL * )( void *, i32 );

    bool          ensure_entity( );
    bool          ensure_weapon( );
    bool          assign_model( C_BaseEntity *entity, const char *path, void *&model, i32 index );
    void          detach_model( C_BaseEntity *entity, void *&model );
    C_BaseEntity *spawn_entity( C_BaseEntity *existing );
    i32           find_idle_sequence( );
    void          draw_model( C_BaseEntity *entity, const config::cham_profile_t &profile, const config::glow_profile_t &glow );
    color_t       faded( const color_t &color ) const;
    void          step_swap( float frametime );
    void          compute_bounds( const CViewSetup &view );
    void          project_skeleton( const VMatrix &world_to_pixels );
    void          invalidate_frame( );
    bool          ensure_targets( );
    void          publish_target( const frame_t &frame, usize target );

    mutable atl::mutex m_publish{ };
    request_t          m_pending{ };
    request_t          m_requested{ };
    frame_t            m_frame{ };
    capture_t          m_capture{ };
    route_t            m_route{ };
    ITexture          *m_targets[ 2 ]{ };
    usize              m_visible_target{ };
    u64                m_painted_request{ };
    u64                m_serial{ };
    rect_t             m_bounds{ };
    C_BaseEntity      *m_entity{ };
    C_BaseEntity      *m_weapon{ };
    void              *m_active_model{ };
    void              *m_weapon_model{ };
    ITexture          *m_cubemap{ };
    const char        *m_model_path{ };
    const char        *m_weapon_path{ };
    const char        *m_stance{ };
    const char        *m_next_model{ };
    const char        *m_next_weapon{ };
    const char        *m_next_stance{ };
    i32                m_team{ TF_TEAM_RED };
    i32                m_next_team{ TF_TEAM_RED };
    matrix3x4          m_player_bones[ MAX_STUDIO_BONES ]{ };
    bone_segment_t     m_skeleton[ MAX_STUDIO_BONES ]{ };
    i32                m_skeleton_count{ };
    float              m_fade{ 1.0f };
    double             m_animation_time{ };
    i32                m_idle_sequence{ -1 };
    bool               m_player_bones_valid{ };
    bool               m_bones_ready{ };
    bool               m_entity_ready{ };
    bool               m_weapon_ready{ };
    bool               m_bounds_valid{ };
    bool               m_drawing{ };
    usize              m_local_model{ };
    usize              m_next_local_model{ };
};

extern esp_builder g_esp_builder;
