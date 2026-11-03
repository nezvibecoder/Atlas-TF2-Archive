#pragma once

#include "link.hpp"

inline constexpr usize MAX_DOORS{ 128 };
inline constexpr usize MAX_DOOR_TRIGGERS{ 128 };
inline constexpr usize MAX_DOOR_LINKS{ 4 };
inline constexpr usize MAX_DOOR_PLAYERS{ 128 };
inline constexpr usize MAX_DOOR_FILTERS{ 64 };
inline constexpr usize MAX_DOOR_GATED{ 256 };
inline constexpr usize MAX_LUMP_OUTPUTS{ 256 };
inline constexpr usize DOOR_FRAME_RING{ 128 };

enum class door_state_t : u8
{
    closed,
    opening,
    open,
    closing,
    unknown,
};

struct door_trigger_t
{
    vec3 m_mins{ };
    vec3 m_maxs{ };
    i32  m_team{ };
};

struct door_link_t
{
    u8   m_trigger{ };
    bool m_all{ };
    bool m_opens{ };
    bool m_closes{ };
};

struct door_members_t
{
    u64 m_bits[ MAX_DOOR_PLAYERS / 64 ]{ };
};

struct door_t
{
    i32          m_model{ -1 };
    vec3         m_pos1{ };
    vec3         m_pos2{ };
    vec3         m_dir{ };
    vec3         m_mins{ };
    vec3         m_maxs{ };
    f32          m_step{ };
    bool         m_reverses{ };
    bool         m_predicted{ };
    door_link_t  m_links[ MAX_DOOR_LINKS ]{ };
    u8           m_link_count{ };

    u32          m_handle{ INVALID_EHANDLE_INDEX };
    i32          m_snapshot_tick{ -1 };
    f32          m_simtime{ };
    vec3         m_received{ };
    door_state_t m_state{ door_state_t::unknown };

    vec3         m_saved{ };
    bool         m_posed{ };
};

struct door_player_t
{
    vec3 m_origin[ 2 ]{ };
    i32  m_tick[ 2 ]{ -1, -1 };
    i32  m_seen{ -1 };
    f32  m_simtime{ };
    vec3 m_mins{ };
    vec3 m_maxs{ };
    i32  m_team{ };
};

struct door_local_t
{
    vec3 m_origin{ };
    vec3 m_mins{ };
    vec3 m_maxs{ };
    i32  m_frame{ -1 };
};

struct door_lump_entity_t
{
    string_view m_classname{ };
    string_view m_targetname{ };
    string_view m_parentname{ };
    string_view m_filtername{ };
    string_view m_model{ };
    string_view m_origin{ };
    string_view m_movedir{ };
    string_view m_speed{ };
    string_view m_lip{ };
    string_view m_wait{ };
    string_view m_spawnflags{ };
    string_view m_force_closed{ };
    string_view m_team{ };
    string_view m_negated{ };
    string_view m_control_point{ };
    string_view m_start_disabled{ };
    string_view m_output_names[ MAX_LUMP_OUTPUTS ]{ };
    string_view m_outputs[ MAX_LUMP_OUTPUTS ]{ };
    usize       m_output_count{ };
};

struct door_lump_names_t
{
    string_view m_doors[ MAX_DOORS ]{ };
    string_view m_filters[ MAX_DOOR_FILTERS ]{ };
    i32         m_filter_teams[ MAX_DOOR_FILTERS ]{ };
    usize       m_filter_count{ };
    string_view m_gated[ MAX_DOOR_GATED ]{ };
    usize       m_gated_count{ };
};

class doors
{
public:
    void on_post_data_update( );
    void pose( i32 command );
    void on_command_predicted( i32 command, C_BasePlayer *player );
    void restore( );
    void clear( );

private:
    bool           load_map( );
    void           read_door( const door_lump_entity_t &entity, door_lump_names_t &names );
    void           read_trigger( const door_lump_entity_t &entity, const door_lump_names_t &names );
    void           unpredict_targets( const door_lump_entity_t &entity, const door_lump_names_t &names );

    void           bind( );
    void           observe( i32 tick );
    void           observe_players( i32 tick );

    vec3           pose_at( const door_t &door, i32 frame ) const;
    void           seed_members( const door_t &door, door_state_t state, door_members_t *before ) const;
    void           step( const door_t &door, i32 frame, vec3 &position, door_state_t &state ) const;
    void           fire_links( const door_t &door, i32 frame, door_members_t *before, door_state_t &state ) const;
    door_members_t members( const door_trigger_t &trigger, i32 frame ) const;
    bool           blocks( const door_t &door, const vec3 &position, i32 frame ) const;
    bool           local_at( i32 frame, door_local_t &out ) const;
    vec3           player_at( const door_player_t &player, i32 frame ) const;

    door_t         m_doors[ MAX_DOORS ]{ };
    usize          m_door_count{ };
    door_trigger_t m_triggers[ MAX_DOOR_TRIGGERS ]{ };
    usize          m_trigger_count{ };
    door_player_t  m_players[ MAX_DOOR_PLAYERS ]{ };
    u8             m_player_list[ MAX_DOOR_PLAYERS ]{ };
    usize          m_player_count{ };
    door_local_t   m_local[ DOOR_FRAME_RING ]{ };
    i32            m_local_index{ };
    i32            m_local_team{ };
    i32            m_snapshot_tick{ -1 };
    i32            m_previous_tick{ -1 };
    bool           m_team_win{ };
    bool           m_loaded{ };
};

extern doors g_doors;
