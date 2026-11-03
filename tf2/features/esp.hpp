#pragma once

#include "link.hpp"
#include "runtime/unordered_map.hpp"
#include "runtime/vector.hpp"

struct esp_entity_t
{
    C_BaseEntity        *m_ent{ };
    float                m_distance{ };
    ETFClassID           m_class_id{ };
    vec3                 m_origin{ };
    bool                 m_dormant{ };
    float                m_fade_time{ };
    bool                 m_dead{ };
    config::esp_target_t m_target{ };
    u32                  m_handle{ INVALID_EHANDLE_INDEX };
    u8                   m_tag{ };
    bool                 m_atlas{ };
    u8                   m_party{ };
    u16                  m_groups{ };
};

struct pickup_record_t
{
    u32   m_handle{ INVALID_EHANDLE_INDEX };
    usize m_kind{ config::PICKUP_COUNT };
    bool  m_respawns{ };
    bool  m_dormant{ };
    float m_vanished{ };
    float m_respawn{ };
};

struct pickup_event_t
{
    i32   m_player{ };
    usize m_kind{ };
};

// measures
inline constexpr usize   ESP_FLAG_COUNT{ 28 };
inline constexpr usize   ESP_BUILDING_FLAG_COUNT{ 2 };
inline constexpr i32     ESP_BLOCK_GAP{ 3 };
inline constexpr i32     ESP_BAR_GAP{ 4 };
inline constexpr i32     ESP_BAR_WIDTH{ 2 };
inline constexpr i32     ESP_BAR_STEP{ 5 };
inline constexpr i32     ESP_NAME_HEIGHT{ 13 };
inline constexpr i32     ESP_SMALL_HEIGHT{ 9 };
inline constexpr color_t ESP_BAR_BACKGROUND{ 16, 16, 18, 220 };

struct esp_layout_t
{
    rect_t m_box{ };
    i32    m_bar[ 4 ]{ ESP_BLOCK_GAP, ESP_BLOCK_GAP, ESP_BAR_GAP, ESP_BAR_GAP };
    i32    m_stack[ 4 ][ 3 ]{ };

    rect_t  claim_bar( config::esp_anchor_t anchor );
    point_t claim_block( config::esp_anchor_t anchor, config::esp_align_t align, i32 height );

    static render::text_align align_for( config::esp_anchor_t anchor );
};

struct flag_def_t
{
    const char *m_label{ };
    color_t     m_color{ };
};

struct visual_record_t
{
    i32   m_serial{ };
    float m_alpha{ };
    float m_fill{ };
    float m_overheal{ };
    float m_lag{ };
    float m_lag_hold{ };
    float m_ratio{ };
    float m_hp_pop{ };
    float m_ammo{ };
    float m_ammo_pop{ };
    float m_rockets{ };
    float m_rocket_pop{ };
    float m_blink{ };
    bool  m_seeded{ };

    float m_flag_pop[ ESP_FLAG_COUNT ]{ };
    float m_uber{ };
    float m_uber_pop{ };

    float m_arrow{ };
    float m_arrow_angle{ };
};

class esp
{
public:
    void think( );
    void gather_entities( );
    void paint( );
    void render_target( );

#if defined( ATLAS_DEV )
    void draw_debug( );
#endif // ATLAS_DEV

    void on_item_pickup( i32 player, usize kind );
    void on_level_shutdown( );
    void on_entity_deleted( C_BaseEntity *entity, u32 handle );

private:
    bool query_health( C_BaseEntity *entity, i32 &health, i32 &max_health ) const;

    u32              frame_delta( );
    visual_record_t *visual_for( C_BaseEntity *entity );

    void draw_player( const esp_entity_t &entry, const rect_t &bounds, u32 delta );
    void draw_building( const esp_entity_t &entry, const rect_t &bounds, u32 delta );
    void draw_projectile( const esp_entity_t &entry, const rect_t &bounds, u32 delta );
    void draw_objective( const esp_entity_t &entry, const rect_t &bounds, u32 delta );
    void draw_pack( const esp_entity_t &entry, const rect_t &bounds, u32 delta );
    void draw_npc( const esp_entity_t &entry, const rect_t &bounds, u32 delta );
    void draw_arrow( const esp_entity_t &entry, bool on_screen, u32 delta );

    pickup_record_t *find_pickup( u32 handle );
    pickup_record_t *track_pickup( u32 handle, C_BaseEntity *entity, ETFClassID class_id );
    void             resolve_pickup( const pickup_event_t &event );
    void             draw_health_bar( esp_layout_t &layout, config::esp_anchor_t anchor,
                                      visual_record_t &visual, i32 health, i32 max_health,
                                      usize style, color_t high, color_t low, color_t overheal_value,
                                      i32 divisions, u32 delta, u8 opacity );
    void             draw_uber_bar( esp_layout_t &layout, config::esp_anchor_t anchor,
                                    C_WeaponMedigun *medigun, visual_record_t &visual, u32 delta,
                                    u8 opacity );
    void             draw_flags( const esp_entity_t &entry, esp_layout_t &layout,
                                 config::esp_anchor_t anchor, config::esp_align_t align,
                                 visual_record_t &visual, u32 delta, u8 opacity );
    void             draw_bones( C_TFPlayer *player, color_t value, u8 opacity );

    atl::vector< esp_entity_t >                m_entities{ };
    atl::vector< pickup_record_t >             m_pickups{ };
    atl::vector< pickup_event_t >              m_pickup_events{ };
    atl::unordered_map< i32, visual_record_t > m_visuals{ };
    float                                      m_last_paint{ };
};

extern esp g_esp;
