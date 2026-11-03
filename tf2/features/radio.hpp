#pragma once

#include "link.hpp"

#include "audio/stream.hpp"

enum : usize
{
    RADIO_DNB         = 0,
    RADIO_JUNGLE      = 1,
    RADIO_LIQUID_DNB  = 2,
    RADIO_HARDCORE    = 3,
    RADIO_TECHNO      = 4,
    RADIO_CLASSIC_RAP = 5,
    RADIO_VAPORWAVE   = 6,
    RADIO_NONSTOP_FM  = 7,
    RADIO_TEKK        = 8,
    RADIO_UNDERGROUND = 9,
    RADIO_THE_BEAT    = 10,

    RADIO_STATION_COUNT,
};

enum class radio_phase_t : u8
{
    off,
    connecting,
    buffering,
    playing,
    paused,
    failed,
};

inline constexpr usize   RADIO_BAND_COUNT{ 6 };
inline constexpr usize   RADIO_TITLE_CAPACITY{ AUDIO_TITLE_CAPACITY };
inline constexpr color_t RADIO_FAILED_COLOR{ 235, 64, 52 };

const char        *radio_station_name( usize station );
const char *const *radio_station_names( );

class radio
{
public:
    bool startup( );
    void run( );
    void on_frame( );
    void connect( i32 generation, usize station );
    void on_voice_status( i32 entity, bool talking );
    void on_level_shutdown( );

    void paint( ) const;

    void step_station( i32 delta );
    void toggle_paused( );
    bool paused( ) const;

    radio_phase_t phase( ) const;
    i32           buffer_percent( ) const;
    const char   *now_playing( ) const;

private:
    enum : usize
    {
        BIND_TOGGLE   = 0,
        BIND_NEXT     = 1,
        BIND_PREVIOUS = 2,
        BIND_LOUDER   = 3,
        BIND_QUIETER  = 4,
        BIND_COUNT    = 5,
    };

    void set_phase( radio_phase_t phase );
    void start_connect( usize station );
    bool poll_connect( );
    void silence( );
    void pump( );
    void apply_volume( );
    void pause_output( bool paused );
    void retire_fade( bool immediately );
    void poll_title( );
    void set_title( const char *title );
    void sample_bands( );

    bool pressed( usize slot, bind_t &bind );

    float wanted_volume( ) const;
    bool  ducking( ) const;
    bool  unfocused( ) const;

    audio_stream *m_stream{ };
    audio_stream *m_fading{ };
    u64           m_fade_deadline{ };
    u64           m_fade_in_until{ };
    u64           m_retry_at{ };
    usize         m_attempted{ RADIO_STATION_COUNT };

    atl::mutex           m_connect_lock{ };
    i32                  m_connect_generation{ };
    i32                  m_connect_ready{ };
    audio_stream        *m_connect_result{ };
    audio_stream_error_t m_connect_error{ };
    u64                  m_connect_elapsed{ };
    usize                m_connect_station{ RADIO_STATION_COUNT };
    bool                 m_connect_pending{ };
    usize                m_station{ RADIO_STATION_COUNT };
    bool                 m_started{ };
    bool                 m_output_paused{ };

    ConVar *m_game_volume{ };

    atl::atomic< i32 >  m_phase{ static_cast< i32 >( radio_phase_t::off ) };
    atl::atomic< i32 >  m_buffer{ };
    atl::atomic< bool > m_self_talking{ };
    atl::atomic< bool > m_others_talking{ };
    u64                 m_speaking{ };

    atl::atomic< bool > m_paused{ };

    mutable atl::mutex m_title_lock{ };
    char               m_title[ RADIO_TITLE_CAPACITY ]{ };
    atl::atomic< i32 > m_title_serial{ };
    atl::atomic< i32 > m_bands[ RADIO_BAND_COUNT ]{ };

    usize m_announced{ RADIO_STATION_COUNT };
    bool  m_bind_held[ BIND_COUNT ]{ };
};

extern radio g_radio;
