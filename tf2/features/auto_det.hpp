#pragma once

#include "link.hpp"
#include "proj_sim.hpp"
#include "splash.hpp"

class auto_det
{
public:
    void run( );
    void capture( );
    void on_network_update( );
    void clear( );

private:
    static constexpr usize MAX_FIRED{ 32 };
    static constexpr usize MAX_FLIGHTS{ 8 };
    static constexpr usize MAX_BLASTS{ 32 };
    static constexpr usize MAX_TARGETS{ 64 };

    enum class mode_t : u8
    {
        none,
        stickies,
        flare,
    };

    struct launcher_t
    {
        mode_t          m_mode{ mode_t::none };
        C_TFWeaponBase *m_weapon{ };
        f32             m_radius{ };
        f32             m_self_radius{ };
        f32             m_live_time{ };
        f32             m_fizzle_time{ };
        bool            m_view_gated{ };
    };

    struct fired_t
    {
        u32 m_handle{ INVALID_EHANDLE_INDEX };
        f32 m_time{ };
        i32 m_touched_tick{ -1 };
    };

    struct flight_t
    {
        u32  m_handle{ INVALID_EHANDLE_INDEX };
        i32  m_first_tick{ }; // simulation tick of m_path[ 0 ]
        i32  m_count{ };
        bool m_landed{ };
        bool m_attached{ };
        vec3 m_path[ projectile_sim::MAX_PATH_POINTS ]{ };
    };

    struct blast_t
    {
        C_BaseEntity *m_projectile{ };
        vec3          m_position{ };
        vec3          m_origin{ };
        f32           m_radius{ };
    };

    struct target_t
    {
        C_BaseEntity   *m_entity{ };
        C_TFPlayer     *m_player{ };
        vec3            m_origin{ };
        vec3            m_mins{ };
        vec3            m_maxs{ };
        vec3            m_spot{ };
        splash_target_t m_previous{ };
    };

    static launcher_t resolve( C_TFPlayer *local, C_TFWeaponBase *active );

    bool timing( );
    void locate( C_BaseEntity *projectile, const vec3 &launch, f32 gravity, trace_t &trace ) const;
    void collect_blasts( const launcher_t &launcher );
    bool add_sticky( C_BaseEntity *entity, const launcher_t &launcher );
    bool add_flare( C_BaseEntity *entity, const launcher_t &launcher );
    bool in_view( const blast_t &blast ) const;
    void collect_targets( );
    void add_player( C_TFPlayer *player );
    void add_dormant_player( C_TFPlayer *player );
    bool in_reach( const vec3 &mins, const vec3 &maxs, f32 slack ) const;
    void place( blast_t &blast ) const;
    bool hit( const target_t &target, f32 scale ) const;
    bool reached( const splash_target_t &target, f32 scale ) const;
    bool hurts_self( const launcher_t &launcher ) const;

    fired_t        *fired( C_BaseEntity *pipe, C_TFWeaponBase *launcher );
    void            bind( C_BaseEntity *pipe );
    bool            fly( C_BaseEntity *pipe, vec3 &position );
    const flight_t *replay( C_BaseEntity *pipe, const vec3 &origin, i32 seen ) const;
    flight_t       *resume( C_BaseEntity *pipe, const vec3 &origin, i32 seen, i32 ahead );
    static void     simulate( const projectile_setup_t &setup, i32 ticks, flight_t &flight );

    fired_t  m_fired[ MAX_FIRED ]{ };
    flight_t m_flights[ MAX_FLIGHTS ]{ };
    flight_t m_resumed{ };
    blast_t  m_blasts[ MAX_BLASTS ]{ };
    target_t m_targets[ MAX_TARGETS ]{ };
    target_t m_self{ };

    usize m_flight_next{ };
    usize m_blast_count{ };
    usize m_target_count{ };
    f32   m_command_time{ };
    i32   m_snapshot_tick{ };
    i32   m_server_tick{ };
};

extern auto_det g_auto_det;
