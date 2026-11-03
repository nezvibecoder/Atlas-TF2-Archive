#pragma once

#include "link.hpp"
#include "model_render.hpp"

struct placed_t
{
    vec3  m_origin{ };
    float m_speed{ };
};

class chams
{
public:
    enum : usize
    {
        CHAM_OFF = 0,
        CHAM_FLAT,
        CHAM_SHADED,
        CHAM_FRESNEL,
        CHAM_WIREFRAME,
        CHAM_LAVA,
        CHAM_PEARL,
        CHAM_BUBBLE,
        CHAM_LSD,
        CHAM_NETWORK,
        CHAM_SOAP,
        CHAM_AURORA,
        CHAM_AQUARIUM,
        CHAM_SKY,
        CHAM_TEXTURED,
        CHAM_COUNT,
        CHAM_ORIGINAL = CHAM_COUNT,
    };
    static constexpr usize CHAM_OVERLAY_COUNT{ CHAM_TEXTURED };

    void         render_main( );
    bool         on_draw_model( const DrawModelResults_t *results, const DrawModelInfo_t *info, const matrix3x4 *bone_to_world, i32 &flags );
    static usize local_model_kind( C_BaseEntity *entity );
    matrix3x4   *record_bones( matrix3x4 *live ) const
    {
        return this->m_record_bones ? this->m_record_bones : live;
    }

    bool        begin_local_model( C_BaseEntity *entity, const DrawModelInfo_t &info, const matrix3x4 *bone_to_world, i32 &flags );
    static bool route_layers( C_BaseEntity             *entity,
                              const DrawModelInfo_t    &info,
                              const matrix3x4          *bone_to_world,
                              model_render::placement_t placement,
                              usize                     base,
                              const color_t            &base_color,
                              usize                     overlay,
                              const color_t            &overlay_color,
                              i32                      &flags );
    void       end_local_model( CStudioRender      *thisptr,
                                DrawModelResults_t *results,
                                DrawModelInfo_t    *info,
                                matrix3x4          *bone_to_world,
                                float              *flex_weights,
                                float              *flex_delayed_weights,
                                const vec3         *model_origin,
                                i32                 flags );
    void       on_level_init( );
    void       on_level_shutdown( );
    void       on_entity_deleted( u32 handle );
    void       on_device_lost( );
    IMaterial *material_for( usize mode );

    bool       m_enabled{ true };
    bool       m_xqz{ true };
    bool       m_driving{ };
    matrix3x4  m_record_scratch[ MAX_STUDIO_BONES ]{ };
    matrix3x4 *m_record_bones{ };
    float      m_backtrack_mix{ };
    double     m_backtrack_time{ };
    bool       m_local_override{ };
    usize      m_local_overlay{ };
    float      m_local_modulation[ 3 ]{ };
    float      m_local_blend{ 1.0f };
    color_t    m_local_color{ };
    color_t    m_local_overlay_color{ };

private:
    struct draw_t
    {
        u32                    m_handle{ };
        config::cham_profile_t m_style{ };
        bool                   m_d3d{ };
        bool                   m_engine_base{ };
    };

    struct player_style_t
    {
        u32                    m_handle{ };
        config::cham_profile_t m_style{ };
        bool                   m_checked{ };
        bool                   m_allowed{ };
    };

    void       collect( );
    bool       decide( C_BaseEntity *entity, ETFClassID class_id, config::cham_profile_t &style );
    bool       player_style( C_TFPlayer *player, config::cham_profile_t &style ) const;
    IMaterial *resolve_material( usize mode ) const;
    void       animate_lava( );
    void       resolve_lava_vars( );
    void       paint_ramp( );
    void       create_materials( );
    void       destroy_materials( );
    void       setup_pass( IMaterial *material, const color_t &color ) const;
    void       setup_local_pass( IMaterial *material, const color_t &color ) const;
    void       run_sweep( bool occluded );
    void       sweep_backtrack( bool occluded );
    void       draw_records( C_TFPlayer *player, i32 index, usize base, usize overlay );
    void       draw_fake( );
    void       queue_draws( );
    void       queue_engine_bases( );
    bool       queue_backtrack( bool layers );
    void       queue_layers( u32 instance, const config::cham_profile_t &style, bool visible );
    bool       route_in_place( C_BaseEntity *entity, const DrawModelInfo_t &info, const matrix3x4 *bone_to_world, i32 &flags );
    bool       backtrack_applies( ) const;
    void       advance_backtrack_fade( );
    void       draw_layer( C_BaseEntity *entity, u32 handle, usize mode, const color_t &color );
    void       draw_entity( C_BaseEntity *entity, u32 handle );

    template< typename F >
    void for_each_backtrack_player( F &&visit );
    template< typename F >
    void for_each_record( C_TFPlayer *player, i32 index, F &&visit );

    bool drawn( i32 index ) const;
    void mark_drawn( i32 index, bool value );

    u64 m_drawn[ MAX_EDICTS / 64 ]{ };

    draw_t         m_draws[ MAX_EDICTS ]{ };
    usize          m_draw_count{ };
    bool           m_has_occluded{ };
    bool           m_queued_occluded{ };
    player_style_t m_player_styles[ MAX_TRACKED_PLAYERS ]{ };
    bool           m_records_queued[ MAX_TRACKED_PLAYERS ]{ };
    matrix3x4      m_fake_bones[ MAX_STUDIO_BONES ]{ };

    IMaterial *m_flat{ };
    IMaterial *m_shaded{ };
    IMaterial *m_fresnel{ };
    IMaterial *m_wireframe{ };
    IMaterial *m_lava{ };
    IMaterial *m_pearl{ };
    IMaterial *m_bubble{ };
    IMaterial *m_lsd{ };
    IMaterial *m_base_texture{ };
    ITexture  *m_ramp{ };
    bool       m_ramp_dirty{ };

    IMaterialVar *m_lava_frame{ };
    IMaterialVar *m_lava_blend{ };
    IMaterialVar *m_lava_scale{ };
    void         *m_lava_shader{ };
};

extern chams g_chams;
