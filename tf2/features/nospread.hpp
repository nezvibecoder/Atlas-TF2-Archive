#pragma once

#include "link.hpp"
#include "player_perf.hpp"

static constexpr i32    SAMPLE_RECORDS{ 8 };
static constexpr i32   MAX_BULLETS{ 16 };
static constexpr usize  WINDOW_SIZE{ 128 };
static constexpr usize  WINDOW_TRIM{ 16 };
static constexpr usize  WINDOW_MINIMUM{ SAMPLE_RECORDS * 2 };
static constexpr usize MAX_CANDIDATES{ 32 };
static constexpr double KERNEL_WIDTH{ 3.4641016 };
static constexpr usize ACK_RING{ 64 };
static constexpr usize ACK_RECENT{ 8 };

static constexpr i32 SPREAD_SEEDS{ 256 };

struct seed_candidate_t
{
    i32   m_seed{ };
    float m_weight{ };
};

struct spread_table_t
{
    vec2 m_offset[ SPREAD_SEEDS + MAX_BULLETS ]{ };
    bool m_filled{ };
};

struct acked_packet_t
{
    i32 m_command{ };
    i32 m_tick{ };
    double m_time{ };
};

enum class seed_phase_t
{
    empty,
    acquiring,
    tracking,
    uncertain,
};

class no_spread
{
public:
    void pre_predict( );
    void create_move( );
    void clear( );
    void on_perf( const perf_dump_t &dump );
    void on_packet( CNetChannel *channel );
    void request( );

    float pre_last_fire( ) const
    {
        return this->m_pre_last_fire;
    }

    usize candidates( i32 delay, seed_candidate_t *out ) const;
    bool  fixed_spread( ) const;
    float weapon_spread( float base ) const;

    static f32  axis_roll( f32 xx, f32 yy, f32 xy, f32 target );
    static void free_rolls( const vec3 &sent, f32 &low, f32 &high );
    static f32  nearest_free_roll( f32 wanted, const vec3 &sent );
    static bool rolls_freely( const vec3 &sent );

    const spread_table_t &spread_table( ) const;

    i32 seed( ) const
    {
        return this->m_seed;
    }

    bool synced( ) const
    {
        return this->m_phase == seed_phase_t::tracking &&
               g_tf2.m_engine->Plat_FloatTime( ) - this->m_sample_time < 5.0;
    }

    void paint( ) const;

private:
    void update_acks( CNetChannel *channel );
    void sample( const perf_record_t *records, i32 count );
    i32  pair( const perf_record_t *records, i32 count, i32 newest, double *raws ) const;
    void push_offset( double offset );
    void estimate( );

    bool uses_clock_seed( ) const;
    bool should_run( ) const;

    double tick_seconds( i32 tick ) const;
    double stamp_for( i32 tick ) const;

    const acked_packet_t *acked( i32 command ) const;
    void  remember( i32 command, i32 tick, double time );
    usize arrival_votes( i32 delay, i32 *ticks, float *votes ) const;

    void apply( float spread, i32 bullets, float first_variance, const seed_candidate_t *candidates,
                usize count );
    f32  pattern_roll( float ( *offsets_x )[ MAX_BULLETS ], float ( *offsets_y )[ MAX_BULLETS ],
                       const seed_candidate_t *candidates, usize count, i32 bullets ) const;

    double m_window[ WINDOW_SIZE ]{ };
    usize  m_window_count{ };
    usize  m_window_next{ };

    double m_offset{ };
    double m_sigma{ };
    double m_grid{ };
    double m_request_time{ };
    double m_newest_sample{ };
    double m_sample_time{ };
    float  m_step{ };

    perf_record_t m_responses[ SAMPLE_RECORDS ]{ };
    i32           m_response_count{ };
    bool          m_response_pending{ };

    double m_moved[ SAMPLE_RECORDS ]{ };
    i32    m_moved_count{ };

    float m_pre_last_fire{ };

    i32 m_rejected{ };
    i32 m_request_command{ };

    acked_packet_t m_acks[ ACK_RING ]{ };
    usize          m_ack_count{ };
    usize          m_ack_next{ };
    i32            m_ack_command{ -1 };
    i32            m_last_ack{ -1 };
    i32            m_last_tick{ -1 };
    i32            m_tick_base{ -1 };

    bool m_pending{ };
    seed_phase_t m_phase{ seed_phase_t::empty };

    i32 m_seed{ };
};

extern no_spread g_nospread;
