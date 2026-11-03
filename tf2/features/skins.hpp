#pragma once

#include "link.hpp"
#include "runtime/atomic.hpp"

namespace gui
{
    class dynamic_list_box;
}

struct row_t
{
    u32         m_value;
    const char *m_label;
};

struct twin_t // when a goy n calls you twin
{
    i32 m_stock;
    i32 m_upgradeable;
};

enum class role_t : u8
{
    none,
    melee,
    all_class,
    item1,
};

struct reskin_t
{
    i32         m_kind;
    i32         m_definition;
    const char *m_name;
};

struct melee_kind_t
{
    i32    m_kind;
    i32    m_class;
    role_t m_role;
};

struct shared_weapon_t
{
    i32  m_key{ -1 };
    i32  m_model{ -1 };
    u32  m_paintkit{ };
    u32  m_wear{ };
    u32  m_seed{ };
    bool m_australium{ };
    bool m_festivized{ };
    u32  m_unusual{ };
    u32  m_killstreak{ };
    u32  m_sheen{ };
    u32  m_killstreaker{ };

    bool operator==( const shared_weapon_t &other ) const = default;
};

struct shared_wearable_t
{
    i32 m_definition{ -1 };
    u32 m_unusual{ };
};

struct jiggle_bone_t
{
    vec3 m_tip{ };
    vec3 m_tip_velocity{ };
    vec3 m_base{ };
    vec3 m_base_last{ };
    vec3 m_base_velocity{ };
};

struct shared_loadout_t
{
    shared_weapon_t   m_weapons[ config::SKIN_WEAPON_COUNT ]{ };
    usize             m_weapon_count{ };
    shared_wearable_t m_wearables[ config::SKIN_WEARABLE_COUNT ]{ };
    usize             m_wearable_count{ };
};

class skins
{
public:
    static constexpr usize UNUSUAL_COUNT{ 6 };
    static constexpr usize KILLSTREAK_COUNT{ 4 };
    static constexpr usize SHEEN_COUNT{ 8 };
    static constexpr usize KILLSTREAKER_COUNT{ 8 };
    static constexpr usize WEAR_COUNT{ 5 };
    static constexpr usize FOOTPRINT_COUNT{ 8 };
    static constexpr usize JINGLE_COUNT{ 3 };
    static constexpr usize KIND_COUNT{ 37 };
    static constexpr usize MODEL_ROWS_MAX{ 24 };

    // the menu tree is built long before the game files are parsed, so the widgets take their
    // rows afterwards
    void bind_effect_list( gui::dynamic_list_box *list )
    {
        this->m_effect_list = list;
    }

    void bind_paintkit_list( gui::dynamic_list_box *list )
    {
        this->m_paintkit_list = list;
    }

    void bind_model_list( usize kind, gui::dynamic_list_box *list )
    {
        this->m_model_lists[ kind ] = list;
    }

    void request_game_tables( )
    {
        this->m_tables_requested.store( true );
    }

    void service_game_tables( );

    void  think( );
    void  on_event( IGameEvent *event );
    void  on_level_shutdown( );
    void  sync_editor( );
    void  reload_editor( );
    bool  attribute_hook( C_BaseEntity *initiator, const char *hook, float &value ) const;
    bool  holiday_active( i32 holiday ) const;
    void  tint_sheen( CProxyAnimatedWeaponSheen *proxy ) const;
    bool  mutes_sound( i32 entity_index, const char *name ) const;
    bool  on_ragdoll_created( C_TFRagdoll *ragdoll ) const;
    void  draw_botkiller( CStudioRender      *studio_render,
                          DrawModelResults_t *results,
                          DrawModelInfo_t    *info,
                          matrix3x4          *bone_to_world,
                          const vec3         *model_origin,
                          i32                 flags );
    void *model_for_index( i32 index ) const;
    i32   index_for_model( const char *name ) const;

    bool editing( ) const
    {
        return this->m_in_game && this->m_held_key >= 0;
    }

    bool in_game( ) const
    {
        return this->m_in_game;
    }

    usize held_kind( ) const
    {
        return this->m_held_kind;
    }

    const char *held_name( ) const
    {
        return this->m_held_name.data( );
    }

    bool held_australium_capable( ) const;
    bool held_festive_capable( ) const;
    bool held_paintkit_capable( ) const;

    bool editing_wearable( ) const
    {
        return this->m_in_game && this->wearable_key( ) >= 0;
    }

    const char *wearable_name( ) const;

    const shared_loadout_t &own_loadout( ) const
    {
        return this->m_own;
    }

    void sanitize( shared_weapon_t &weapon ) const;
    void sanitize( shared_wearable_t &wearable ) const;

private:
    static constexpr usize ORIGINAL_SLOTS{ 11 };

    struct original_t
    {
        u16  m_index{ };
        bool m_present{ };
        u32  m_raw_value{ };
    };

    struct seen_t
    {
        EHANDLE         m_handle{ };
        i32             m_server_definition{ };
        i32             m_definition{ };
        bool            m_wearable{ };
        shared_weapon_t m_skin{ };

        // what the server had in each attribute before it was first dressed
        original_t m_originals[ ORIGINAL_SLOTS ]{ };
        usize      m_original_count{ };

        bool dressed( ) const
        {
            return this->m_skin != shared_weapon_t{ };
        }
    };

    struct weapon_streak_t
    {
        i32 m_key{ -1 };
        i32 m_kills{ };
    };

    struct loaded_model_t
    {
        u64   m_hash{ };
        void *m_model{ };
    };

    struct gold_kill_t
    {
        i32   m_victim{ -1 };
        float m_expiry{ };
    };

    struct head_t
    {
        u64         m_hash{ };
        MDLHandle_t m_handle{ MDLHANDLE_INVALID };
    };

    struct botkiller_t
    {
        EHANDLE     m_weapon{ };
        MDLHandle_t m_head{ MDLHANDLE_INVALID };
        MDLHandle_t m_head_vm{ MDLHANDLE_INVALID };
    };

    static constexpr usize MAX_WEAPONS{ 48 };
    static constexpr usize SEEN_MAX{ MAX_TRACKED_PLAYERS * 8 };
    static constexpr usize STREAK_SLOTS{ 16 };
    static constexpr usize MODEL_SLOTS{ 96 };
    static constexpr usize GOLD_SLOTS{ 16 };
    static constexpr usize HEAD_SLOTS{ 64 };
    static constexpr usize BOTKILLER_SLOTS{ 32 };
    static constexpr usize HEAD_POSE_SLOTS{ 32 };
    static constexpr usize HEAD_BONES_MAX{ 16 };
    static constexpr usize HEAD_JIGGLE_MAX{ 4 };

    struct head_pose_t
    {
        unsigned long m_owner{ };
        float         m_time{ };
        jiggle_bone_t m_jiggles[ HEAD_JIGGLE_MAX ]{ };
    };

    const seen_t *dress_player( C_TFPlayer *player, const shared_loadout_t *loadout );
    void          dress_peers( );
    void          dress_weapon( C_TFWeaponBase *weapon, const shared_weapon_t &skin, seen_t &item );
    void          dress_wearable( C_TFWearable *wearable, u32 effect, seen_t &item );
    void          undress( C_EconEntity *entity, seen_t &item );
    void          undress_unclaimed( );
    void          rebuild_own( );

    void refresh_weapon( C_TFWeaponBase *weapon, const char *model );
    void refresh_wearable( C_TFWearable *wearable );
    bool set_attribute( CAttributeList &list, seen_t &item, u16 index, u32 raw_value );
    i32  register_model( const char *path );
    bool refresh_wanted( );
    void publish_held( C_TFPlayer *local, const seen_t *active );
    void publish_wearables( C_TFPlayer *local );
    void publish_lists( ) const;
    i32  wearable_key( ) const;

    void                  reset_streaks( );
    void                  on_class_change( IGameEvent *event );
    void                  record_gold_kill( i32 victim, bool gold );
    bool                  gold_victim( i32 victim ) const;
    i32                  *weapon_streak( i32 key );
    config::skin_entry_t *weapon_entry( i32 key );

    MDLHandle_t  head_handle( const char *path );
    head_pose_t &head_pose( unsigned long owner );

    static const seen_t *find_seen( const seen_t *records, usize count, const EHANDLE &handle );

    config::skin_entry_t     m_wanted_weapons[ config::SKIN_WEAPON_COUNT ]{ };
    config::wearable_entry_t m_wanted_wearables[ config::SKIN_WEARABLE_COUNT ]{ };
    config::spells_t         m_wanted_spells{ };

    shared_loadout_t m_own{ };
    bool             m_own_ready{ };

    // the weapons and wearables seen last frame and this frame, the local player's and every peer's
    seen_t m_seen[ SEEN_MAX ]{ };
    usize  m_seen_count{ };
    seen_t m_seen_next[ SEEN_MAX ]{ };
    usize  m_seen_next_count{ };

    loaded_model_t m_models[ MODEL_SLOTS ]{ };
    usize          m_model_count{ };

    head_t m_heads[ HEAD_SLOTS ]{ };
    usize  m_head_count{ };

    botkiller_t m_botkillers[ BOTKILLER_SLOTS ]{ };
    usize       m_botkiller_count{ };
    head_pose_t m_head_poses[ HEAD_POSE_SLOTS ]{ };

    gold_kill_t m_gold_kills[ GOLD_SLOTS ]{ };

    i32             m_class{ -1 };
    i32             m_streak{ };
    weapon_streak_t m_weapon_streaks[ STREAK_SLOTS ]{ };

    bool               m_in_game{ };
    i32                m_held_key{ -1 };
    usize              m_held_kind{ KIND_COUNT };
    fixed_string< 64 > m_held_name{ };
    i32                m_wearable_keys[ config::SKIN_WEARABLE_SLOTS ]{ -1, -1, -1, -1 };
    fixed_string< 64 > m_wearable_names[ config::SKIN_WEARABLE_SLOTS ]{ };

    i32 m_edit_key{ -2 };
    i32 m_edit_wearable_key{ -2 };

    gui::dynamic_list_box *m_effect_list{ };
    gui::dynamic_list_box *m_paintkit_list{ };
    gui::dynamic_list_box *m_model_lists[ KIND_COUNT ]{ };

    atl::atomic< bool > m_tables_requested{ false };
    atl::atomic< bool > m_tables_ready{ false };
};

extern skins g_skins;
