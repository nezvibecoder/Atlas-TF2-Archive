#pragma once

#include "link.hpp"

#include "gui/indicator.hpp"
#include "strafe-prediction.hpp"

inline constexpr usize RECORDER_ROUTE_CAPACITY{ 32 };
inline constexpr usize RECORDER_ENTRY_CAPACITY{ RECORDER_ROUTE_CAPACITY + 1 };
inline constexpr usize RECORDER_NAME_LENGTH{ 32 };
inline constexpr usize RECORDER_MAP_LENGTH{ 63 };
inline constexpr usize RECORDER_NO_ENTRY{ static_cast< usize >( -1 ) };
inline constexpr usize RECORDER_KEY_COUNT{ 8 };

inline constexpr u32   RECORDER_MAGIC{ 0x52545241 };
inline constexpr u32   RECORDER_VERSION{ 3 };
inline constexpr u32   RECORDER_VERSION_UNFLAGGED{ 1 };
inline constexpr u32   RECORDER_VERSION_CLIPLESS{ 2 };
inline constexpr i32   RECORDER_CLIP_UNKNOWN{ INT_MIN };
inline constexpr usize RECORDER_FRAME_LIMIT{ 20000 };

enum class recorder_phase_t : u8
{
    idle,
    waiting,
    recording,
    armed,
    aligning,
    replaying,
};

enum class recorder_wait_t : u8
{
    none,
    walk,
    height,
    land,
    weapon,
};

enum class recorder_request_t : u8
{
    record,
    replay,
    stop,
    save,
    remove,
    refresh,
    select,
};

enum : u32
{
    RECORDER_KEY_W    = 1u << 0,
    RECORDER_KEY_A    = 1u << 1,
    RECORDER_KEY_S    = 1u << 2,
    RECORDER_KEY_D    = 1u << 3,
    RECORDER_KEY_JUMP = 1u << 4,
    RECORDER_KEY_DUCK = 1u << 5,
    RECORDER_KEY_M1   = 1u << 6,
    RECORDER_KEY_M2   = 1u << 7,
};

enum : u32
{
    RECORDER_FRAME_PSILENT = 1u << 0,
};

struct recorder_frame_t
{
    vec3 m_origin{ };
    vec3 m_velocity{ };
    vec2 m_angles{ };
    f32  m_forward{ };
    f32  m_side{ };
    i32  m_buttons{ };
    u32  m_flags{ };
};

static_assert( sizeof( recorder_frame_t ) == 48 );

struct recorder_unflagged_frame_t
{
    vec3 m_origin{ };
    vec3 m_velocity{ };
    vec2 m_angles{ };
    f32  m_forward{ };
    f32  m_side{ };
    i32  m_buttons{ };
};

static_assert( sizeof( recorder_unflagged_frame_t ) == 44 );

struct recorder_header_t
{
    u32 m_magic{ RECORDER_MAGIC };
    u32 m_version{ RECORDER_VERSION };
    f32 m_interval{ };
    i32 m_class{ };
    u32 m_frames{ };
    i32 m_clip{ };
};

static_assert( sizeof( recorder_header_t ) == 24 );

inline constexpr usize RECORDER_HEADER_CLIPLESS{ __builtin_offsetof( recorder_header_t, m_clip ) };

struct recorder_blob_t
{
    fixed_string< RECORDER_NAME_LENGTH > m_name{ };
    atl::vector< u8 >                    m_bytes{ };
};

struct recorder_physics_t
{
    strafe_prediction::ground_physics m_ground{ };
    vec3                              m_forward{ };
    vec3                              m_right{ };
    f32                               m_yaw{ };
    f32                               m_accelerate{ };
    f32                               m_air_cap{ };
    f32                               m_back{ };
    f32                               m_back_min{ };
    bool                              m_air{ };
    bool                              m_ducked{ };
};

struct recorder_entry_t
{
    fixed_string< RECORDER_NAME_LENGTH > m_name{ };
    vec3                                 m_start{ };
    f32                                  m_length{ };
    i32                                  m_class{ };
    i32                                  m_commands{ };
    i32                                  m_jumps{ };
    bool                                 m_take{ };
};

struct recorder_status_t
{
    recorder_phase_t                     m_phase{ };
    fixed_string< RECORDER_MAP_LENGTH >  m_map{ };
    fixed_string< RECORDER_NAME_LENGTH > m_route{ };
    vec3                                 m_origin{ };
    f32                                  m_interval{ };
    f32                                  m_speed{ };
    f32                                  m_start_distance{ };
    f32                                  m_start_height{ };
    recorder_wait_t                      m_wait{ };
    i32                                  m_frame{ };
    i32                                  m_frames{ };
    u32                                  m_keys{ };
    usize                                m_routes{ };
    bool                                 m_has_take{ };
};

void recorder_wait_text( recorder_wait_t wait, f32 distance, f32 height, char ( &out )[ 32 ] );

class movement_recorder
{
public:
    void on_level_init( const char *map_name );
    void on_level_shutdown( );
    void on_pre_prediction( );
    void on_create_move_end( );
    void paint( );
    void paint_keys( );

    bool replaying( ) const;

    void              request( recorder_request_t kind, string_view route = { }, string_view name = { } );
    recorder_status_t status( ) const;
    bool              catalog( u32 &serial, recorder_entry_t ( &entries )[ RECORDER_ENTRY_CAPACITY ], usize &count,
                               usize &focus ) const;

private:
    struct route_t
    {
        fixed_string< RECORDER_NAME_LENGTH > m_name{ };
        atl::vector< recorder_frame_t >      m_frames{ };
        f32                                  m_interval{ };
        i32                                  m_class{ };
        i32                                  m_clip{ };
        f32                                  m_length{ };
        i32                                  m_jumps{ };
    };

    struct pending_t
    {
        recorder_request_t                   m_kind{ };
        fixed_string< RECORDER_NAME_LENGTH > m_route{ };
        fixed_string< RECORDER_NAME_LENGTH > m_name{ };
    };

    enum class store_kind_t : u8
    {
        list,
        save,
        remove,
    };

    struct store_job_t
    {
        store_kind_t                         m_kind{ };
        fixed_string< RECORDER_MAP_LENGTH >  m_map{ };
        fixed_string< RECORDER_NAME_LENGTH > m_previous{ };
        route_t                              m_route{ };
        atl::vector< route_t >               m_routes{ };
        u32                                  m_take{ };
        u32                                  m_status{ };
        bool                                 m_success{ };
        bool                                 m_moved{ };
    };

#if defined( ATLAS_DEV )
    struct steer_sample_t
    {
        vec3 m_error{ };
        vec2 m_expected{ };
        vec2 m_residual{ };
        bool m_air{ };
        bool m_modeled{ };
    };
#endif

    void service( );
    void watch( );
    void publish( );
    void publish_catalog( usize focus = RECORDER_NO_ENTRY );

    void stop( );
    void toggle_recording( );
    void stop_recording( );

    void begin_replay( string_view name );
    void stop_replay( const char *reason = nullptr );
    void try_start_replay( );
    void begin_frames( );
    void align( );
    void steer( );
    vec2 choose_move( const route_t &route, const vec3 &error );
    void apply_view( const recorder_frame_t &frame ) const;
    void apply_alignment( );
    void apply_frame( );

    static void ATLAS_STDCALL store_worker( void *context );

    void poll_store( );
    bool store_idle( ) const;
    void start_store( );
    void finish_store( );
    void finish_list( );
    void finish_save( );
    void finish_remove( );
    void save_route( string_view route, string_view name );
    void remove_route( string_view name );

    usize          insert( route_t &&route );
    void           erase_route( string_view name );
    static void    measure( route_t &route );
    route_t       *find( string_view name );
    const route_t *standing_in( ) const;
    const route_t *preview_route( );

    bool            ready( ) const;
    bool            inside_start( const route_t &route ) const;
    recorder_wait_t start_wait( const route_t &route ) const;
    bool            on_other_surface( const route_t &route ) const;
    bool            took_over( );

    void draw_markers( ) const;
    void draw_marker( const route_t &route, const color_t &value ) const;
    void draw_label( const route_t &route, const color_t &value, u8 alpha ) const;
    void draw_paths( );
    void draw_replay( ) const;
    void draw_stretch( const route_t &route, i32 first, i32 last, const color_t &value ) const;
    void draw_events( const route_t &route, i32 first, i32 last, u8 alpha ) const;
    void draw_ring( const vec3 &center, f32 radius, const color_t &value, bool spinning, i32 lit ) const;

    recorder_phase_t m_phase{ };
    route_t          m_take{ };
    bool             m_has_take{ };
    u32              m_take_serial{ };
    i32              m_class{ };

    route_t *m_active{ };
    i32      m_frame{ };
    vec3     m_start_origin{ };
    vec3     m_start_velocity{ };
    vec2     m_move{ };
    i32      m_align_commands{ };
    i32      m_user_held{ };

    u32  m_keys{ };
    u32  m_pressed{ };
    bool m_bind_held[ 2 ]{ };

    atl::vector< route_t >               m_routes{ };
    fixed_string< RECORDER_MAP_LENGTH >  m_map{ };
    fixed_string< RECORDER_NAME_LENGTH > m_selected{ };

    store_job_t   m_job{ };
    volatile long m_job_state{ };
    bool          m_listing_due{ };

    indicator_card_t m_key_card{ };
    u8               m_key_mix[ RECORDER_KEY_COUNT ]{ };

    mutable atl::mutex       m_lock{ };
    atl::vector< pending_t > m_requests{ };
    recorder_status_t        m_status{ };
    recorder_entry_t         m_entries[ RECORDER_ENTRY_CAPACITY ]{ };
    usize                    m_entry_count{ };
    usize                    m_focus{ RECORDER_NO_ENTRY };
    u32                      m_serial{ 1 };
    u32                      m_taps{ };

#if defined( ATLAS_DEV )
    f32            m_peak_error{ };
    f32            m_error_sum{ };
    f32            m_first_error{ };
    steer_sample_t m_sample{ };
    steer_sample_t m_previous{ };
    i32            m_traced_clip{ };
#endif
};

extern movement_recorder g_recorder;
