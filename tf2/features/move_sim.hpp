#pragma once

#include "link.hpp"
#include "strafe-prediction.hpp"

inline constexpr i32 MAX_CATCH_UP_TICKS{ 32 };
inline constexpr f32 MAX_SIM_TIME{ 5.0f };
inline constexpr i32 MAX_WALK_TICKS{ MAX_CATCH_UP_TICKS + static_cast< i32 >( MAX_SIM_TIME * 66.67f ) + 1 };

struct move_sim_queries_t
{
    vec3    m_ground_origin{ }, m_ground_end{ }, m_water_origin{ };
    trace_t m_ground{ };
    i32     m_water_level{ };
    bool    m_has_ground{ }, m_has_water{ };
};

struct move_sim_player_t
{
    C_TFPlayer *m_player{ };
    vec3        m_origin{ };
};

enum class walk_aim : u8
{
    box,
    point,
    blast,
    trap,
};

struct move_sim_ctx_t
{
    move_sim_ctx_t( ) = default;
    move_sim_ctx_t( C_TFPlayer *player_to_pred );

    C_TFPlayer                *m_player_to_predict{ nullptr };     // The player to predict.
    bool                       m_has_been_predicted_once{ false }; // Have we predicted this player?
    bool                       m_prediction_broken{ false };       // Has prediction been broken at any point?
    u32                        m_pred_ticks{ 0 };                  // How many ticks have we predicted this player?
    vec3                       m_pred_origin{ };                   // The predicted origin of the player.
    vec3                       m_pred_velocity{ };                 // The predicted velocity of the player.
    vec3                       m_pred_mins{ };                     // The predicted BBOX mins of the player.
    vec3                       m_pred_maxs{ };                     // The predicted BBOX maxs of the player.
    u8                         m_pred_flags{ 0x0 };                // The predicted fFlags of the player.
    f32                        m_pred_surface_friction{ 0.f };     // The predicted surface friction coefficient of the player.
    vec3                       m_pred_base_velocity{ };
    i32                        m_pred_transport_ground_index{ -1 };
    i32                        m_pred_transport_ground_serial{ -1 };
    bool                       m_pred_transport_active{ false };
    bool                       m_pred_base_velocity_impulse{ false };
    bool                       m_pred_air_current{ false };        // Predicted TF_COND_AIR_CURRENT state.
    bool                       m_pred_lost_footing{ false };       // Predicted TF_COND_LOST_FOOTING state.
    f32                        m_max_speed{ 0.f };                 // Ground cap WalkMove rescales x,y back down to.
    f32                        m_wishspeed{ 0.f };                 // Speed the recovered input is asking for; 0 while coasting.
    f32                        m_walk_wishspeed{ 0.f };            // Ground input strength; kept separate for air/hop seeds.
    f32                        m_walk_turn{ };                     // World input turn when recovered, heading turn otherwise.
    bool                       m_walk_input{ };                    // Ground input was recovered; let acceleration steer it.
    bool                       m_walk_relative{ };                 // Ground controller keeps a fixed wish offset from velocity.
    bool                       m_walk_ground{ };                   // Seed used the ground estimator, rather than air/hop history.
    bool                       m_walk_keyed{ };
    f32                        m_walk_keys[ strafe_prediction::GROUND_WALKS ]{ };         // Chance of each walk on this tick; the read's chances at the seed.
    vec3                       m_walk_key_velocity[ strafe_prediction::GROUND_WALKS ]{ }; // Velocity given that walk is the one held now.
    vec3                       m_walk_key_offset[ strafe_prediction::GROUND_WALKS ]{ };   // Displacement from the forecast given that walk.
    vec3                       m_walk_anchor{ };                                          // Forecast origin the walk offsets were last taken from.
    i32                        m_walk_kept_key{ };                                        // The read key, which the last walk holds.
    f32                        m_walk_flip_scale{ };                                      // This walker's side changes over the population's.
    i32                        m_walk_kept_held{ };                                       // Ticks the kept walk's key has been held.
    f32                        m_walk_view_yaw{ };
    bool                       m_walk_unread{ };
    walk_aim                   m_walk_aim{ walk_aim::box };
    i32                        m_ground_agreement_ticks{ };        // Conditional model agreement, not a calibrated accuracy guarantee.
    bool                       m_charging{ };                      // The seed uses ChargeMove's forced forward input.
    i32                        m_charge_ticks{ };
    f32                        m_charge_exit_speed{ };
    f32                        m_charge_exit_cap{ };
    bool                       m_air_model{ };                     // The active movement regime uses the discrete air controller.
    i32                        m_air_seed_tick{ };
    f32                        m_air_speed_cap{ };
    bool                       m_ordinary_air_control{ };
    strafe_prediction::control m_air_control{ };
    bool                       m_air_turn_fades{ };   // The fitted air turn fades over the horizon; the zeroed default holds it.
    f32                        m_air_turn_kept{ };    // Share of the air controller's heading change kept on this tick.
    i32                        m_air_goal_side{ };    // +-1: the approach side a reached goal heading is left on; 0 holds it.
    i32                        m_air_goal_held{ };    // Ticks the forecast has held a reached goal heading.
    f32                        m_wish_delta{ 0.f };   // Signed angle from the seed heading to the wish direction, degrees.
    f32                        m_wish_yaw{ 0.f };     // Current world input direction.
    bool                       m_wish_valid{ false }; // Is there a direction worth steering with at all?
    i32                        m_wish_blast_age{ -1 };  // Ticks since the blast that ended the window; -1 without one.
    bool                       m_hopping{ false };      // The history shows a bunny hop cadence; run_tick re-jumps on it.
    i32                        m_hop_ground_ticks{ 0 }; // Ticks they stay grounded between hops, measured.
    i32                        m_ground_ticks{ 0 };     // Ticks grounded so far in the current stay.
    i32                        m_water_level{ 0 };      // Recomputed each tick; decides which move they get.
    bool                       m_water_control_valid{ false };
    u8                         m_water_control_level{ 0 };
    vec3                       m_water_control_direction{ };
    f32                        m_water_control_speed{ 0.f };
    bool                       m_dormant{ false };              // Seeded from a remembered origin; run_tick refuses to step it.
    move_sim_queries_t        *m_queries{ };
    const move_sim_player_t   *m_players{ };
    i32                        m_player_count{ };
    vec3                       m_track[ MAX_WALK_TICKS ]{ };
    bool                       m_track_air[ MAX_WALK_TICKS ]{ }; // m_air_model when that point was recorded
    i32                        m_track_count{ 0 };
};

class move_sim
{
public:
    bool run_tick( move_sim_ctx_t *ctx );
    void move_local( move_sim_ctx_t *ctx );
    void apply_parachute_reductions( move_sim_ctx_t *ctx );
    void observe_air( );
    void perform_player_move_trace( C_TFPlayer *player, const vec3 &origin, const vec3 &destination, trace_t &tr_out );
    void perform_hull_trace( C_TFPlayer *player, const vec3 &origin, const vec3 &destination,
                             const vec3 &mins, const vec3 &maxs, trace_t &tr_out );
    void perform_hull_trace( move_sim_ctx_t *ctx, const vec3 &origin, const vec3 &destination,
                             const vec3 &mins, const vec3 &maxs, trace_t &tr_out );
    u32  player_solid_mask( C_TFPlayer *player ) const;
    void categorize_position( move_sim_ctx_t *ctx );

private:
    void perform_player_move_trace( move_sim_ctx_t *ctx, const vec3 &origin, const vec3 &destination, trace_t &tr_out );

    void apply_gravity( move_sim_ctx_t *ctx );
    void apply_friction( move_sim_ctx_t *ctx );
    void check_velocity( move_sim_ctx_t *ctx );

    void walk_move( move_sim_ctx_t *ctx );
    void air_move( move_sim_ctx_t *ctx );
    void water_move( move_sim_ctx_t *ctx );
    void step_move( move_sim_ctx_t *ctx );
    void try_player_move( move_sim_ctx_t *ctx, f32 slide_redirect = 0.0f );
    void refresh_ground_surface( move_sim_ctx_t *ctx );
    void trace_ground( move_sim_ctx_t *ctx, const vec3 &end, trace_t &trace );
    bool touch_ground_in_quadrants( move_sim_ctx_t *ctx, const vec3 &start, const vec3 &end,
                                    trace_t &ground_trace );

    i32  check_water( move_sim_ctx_t *ctx );
    bool check_stuck_player( move_sim_ctx_t *ctx );
};

extern move_sim g_move_sim;
