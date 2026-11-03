#pragma once

#include "link.hpp"
#include "runtime/vector.hpp"

class glow
{
public:
    void render_main( );
    bool begin_local_model( C_BaseEntity *entity, const DrawModelInfo_t &info, const matrix3x4 *bone_to_world, i32 &flags );
    void render_local_model( C_BaseEntity                 *entity,
                             const config::glow_profile_t &profile,
                             CStudioRender                *thisptr,
                             DrawModelResults_t           *results,
                             DrawModelInfo_t              *info,
                             matrix3x4                    *bone_to_world,
                             float                        *flex_weights,
                             float                        *flex_delayed_weights,
                             const vec3                   *model_origin,
                             i32                           flags );
    void render_preview( IMatRenderContext            *render_context,
                         const config::glow_profile_t &profile,
                         float                         fade,
                         C_BaseEntity                 *entity,
                         C_BaseEntity                 *weapon,
                         i32                           width,
                         i32                           height );
    void on_level_init( );
    void on_level_shutdown( );
    void on_entity_deleted( C_BaseEntity *entity );

    bool m_enabled{ true };
    bool m_driving{ };

private:
    static constexpr usize PICKUP_PASS{ config::ESP_TARGET_COUNT + 2 };
    static constexpr usize INTEL_PASS{ PICKUP_PASS + 1 };
    static constexpr usize NPC_PASS{ INTEL_PASS + 1 };
    static constexpr usize GLOW_PASS_COUNT{ NPC_PASS + 1 };

    struct pass_t
    {
        const config::glow_profile_t *m_profile{ };
        color_t                       m_color{ };
        atl::vector< C_BaseEntity * > m_entities{ };
        bool                          m_local{ };
        bool                          m_preview{ };
    };

    struct local_draw_t
    {
        CStudioRender      *m_studio_render{ };
        DrawModelResults_t *m_results{ };
        DrawModelInfo_t    *m_info{ };
        matrix3x4          *m_bone_to_world{ };
        float              *m_flex_weights{ };
        float              *m_flex_delayed_weights{ };
        const vec3         *m_model_origin{ };
        i32                 m_flags{ };
    };

    struct player_pass_t
    {
        u32   m_handle{ };
        usize m_pass{ };
        bool  m_checked{ };
        bool  m_allowed{ };
    };

    bool decide( C_BaseEntity *entity, ETFClassID class_id, usize &pass );
    bool player_pass( C_TFPlayer *player, usize &pass ) const;
    void set_pass_modulation( const float modulation[ 3 ], float alpha ) const;
    void redraw_local( ) const;
    void collect( );
    void queue_d3d( );
    void create_resources( );
    void destroy_resources( );
    void draw_entities( );
    void begin_mask( IMatRenderContext *render_context ) const;
    void mask_pass( IMatRenderContext *render_context );
    void color_pass( IMatRenderContext *render_context, const pass_t &pass, i32 width, i32 height );

    pass_t       m_passes[ GLOW_PASS_COUNT ]{ };
    pass_t       m_local_pass{ };
    pass_t       m_preview_pass{ };
    local_draw_t m_local_draw{ };
    player_pass_t m_player_passes[ MAX_TRACKED_PLAYERS ]{ };
    bool          m_players_enabled{ };

    IMaterial    *m_glow_color{ };
    IMaterial    *m_halo{ };
    IMaterial    *m_blur_x{ };
    IMaterial    *m_blur_y{ };
    IMaterialVar *m_bloom_amount{ };
    void         *m_blur_y_shader{ };
    ITexture     *m_buffer1{ };
    ITexture     *m_buffer2{ };
};

extern glow g_glow;
