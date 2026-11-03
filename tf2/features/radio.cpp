#include "base.hpp"

#include "features/radio.hpp"

#include "gui/gui.hpp"
#include "gui/indicator.hpp"
#include "render/render.hpp"
#include "util/format.hpp"

ATLAS_NO_DESTROY radio g_radio{ };

static const char *station_url( usize station )
{
    switch ( station )
    {
        case RADIO_DNB: return ATLAS_XORSTR_STATIC( "https://sc2.dubplate.fm/radio/8030/dnb/uhifi" );
        case RADIO_JUNGLE: return ATLAS_XORSTR_STATIC( "https://energy1058.radioca.st/stream" );
        case RADIO_LIQUID_DNB: return ATLAS_XORSTR_STATIC( "https://radiorecord.hostingradio.ru/liquidfunk96.aacp" );
        case RADIO_HARDCORE: return ATLAS_XORSTR_STATIC( "https://stream.hardcoreradio.nl:9000/hcr.ogg" );
        case RADIO_TECHNO: return ATLAS_XORSTR_STATIC( "https://schranz.in/schranz" );
        case RADIO_CLASSIC_RAP: return ATLAS_XORSTR_STATIC( "https://radiorecord.hostingradio.ru/rapclassics96.aacp" );
        case RADIO_VAPORWAVE: return ATLAS_XORSTR_STATIC( "https://radio.plaza.one/ogg" );
        case RADIO_NONSTOP_FM: return ATLAS_XORSTR_STATIC( "https://stream.zeno.fm/hace1hc4vwzuv" );
        case RADIO_TEKK: return ATLAS_XORSTR_STATIC( "https://stream.zeno.fm/oj65ts503spvv" );
        case RADIO_UNDERGROUND: return ATLAS_XORSTR_STATIC( "https://54house.fm:9013/stream" );
        case RADIO_THE_BEAT: return ATLAS_XORSTR_STATIC( "https://audio.gtaradio.net/4/beat_95" );
        default: return nullptr;
    }
}

inline constexpr i32 RADIO_VOLUME_STEP{ 5 };
inline constexpr u64 RADIO_RETRY_DELAY{ 3000 };
inline constexpr u64 RADIO_BUFFER_READY{ 75 };
inline constexpr i32 RADIO_VOICE_MAX_PLAYERS{ 64 }; // one bit each in m_speaking

static u32 ATLAS_STDCALL radio_connect_thread( void *parameter )
{
    const usize payload{ reinterpret_cast< usize >( parameter ) };

    g_radio.connect( static_cast< i32 >( payload >> 32 ),
                     static_cast< usize >( payload & 0xFFFFFFFFull ) );
    return 0;
}

static u32 ATLAS_STDCALL radio_thread( void *parameter )
{
    ATLAS_UNUSED_PARAM( parameter );

    g_radio.run( );
    return 0;
}

const char *radio_station_name( usize station )
{
    switch ( station )
    {
        case RADIO_DNB: return ATLAS_XORSTR_STATIC( "Drum and bass" );
        case RADIO_JUNGLE: return ATLAS_XORSTR_STATIC( "Jungle" );
        case RADIO_LIQUID_DNB: return ATLAS_XORSTR_STATIC( "Liquid DnB" );
        case RADIO_HARDCORE: return ATLAS_XORSTR_STATIC( "Hardcore" );
        case RADIO_TECHNO: return ATLAS_XORSTR_STATIC( "Techno" );
        case RADIO_CLASSIC_RAP: return ATLAS_XORSTR_STATIC( "Classic rap" );
        case RADIO_VAPORWAVE: return ATLAS_XORSTR_STATIC( "Vaporwave" );
        case RADIO_NONSTOP_FM: return ATLAS_XORSTR_STATIC( "NonStop FM" );
        case RADIO_TEKK: return ATLAS_XORSTR_STATIC( "Tekk" );
        case RADIO_UNDERGROUND: return ATLAS_XORSTR_STATIC( "Underground" );
        case RADIO_THE_BEAT: return ATLAS_XORSTR_STATIC( "The Beat 102.7" );
        default: return ATLAS_XORSTR_STATIC( "No station tuned" );
    }
}

const char *const *radio_station_names( )
{
    ATLAS_NO_DESTROY static const char *names[ RADIO_STATION_COUNT ]{ };

    if ( !names[ 0 ] )
    {
        for ( usize index{ }; index < RADIO_STATION_COUNT; ++index )
        {
            names[ index ] = radio_station_name( index );
        }
    }

    return names;
}

bool radio::startup( )
{
    this->m_game_volume = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "volume" ).get( ) );

    if ( !atl::create_hidden_thread( reinterpret_cast< atl::thread_entry_fn >( radio_thread ), nullptr ) )
    {
        DBGOUT( "radio: stream thread did not start" );
        return false;
    }

    return true;
}

void radio::set_phase( radio_phase_t phase )
{
    const i32 value{ static_cast< i32 >( phase ) };

    // the panel reads this every frame, so only write when it actually moves
    if ( this->m_phase.load( ) != value )
    {
        this->m_phase.store( value );
    }
}

void radio::run( )
{
    while ( g_cl.m_is_running )
    {
        if ( !g_cfg.m_radio.m_enabled )
        {
            this->silence( );
        } else if ( this->m_paused )
        {
            if ( !this->m_output_paused )
            {
                this->pause_output( true );
                this->m_output_paused = true;
            }

            this->set_phase( radio_phase_t::paused );
        } else
        {
            if ( this->m_output_paused )
            {
                this->pause_output( false );
                this->m_output_paused = false;
            }

            const usize wanted{ g_cfg.m_radio.m_station };
            if ( wanted != this->m_attempted )
            {
                this->m_attempted = wanted;
                this->m_retry_at  = 0;
            }

            this->poll_connect( );
            const bool superseded{ this->m_connect_pending && this->m_connect_station != wanted };

            if ( this->m_station != wanted && ( !this->m_connect_pending || superseded ) &&
                 I( GetTickCount64 )( ) >= this->m_retry_at )
            {
                this->start_connect( wanted );
            }

            this->apply_volume( );
            this->pump( );
        }

        this->retire_fade( false );
        I( Sleep )( 50 );
    }

    this->silence( );
}

void radio::start_connect( usize station )
{
    i32 generation{ };

    {
        atl::scoped_lock guard{ this->m_connect_lock };

        generation            = ++this->m_connect_generation;
        this->m_connect_ready = 0;

        if ( this->m_connect_result )
        {
            this->m_connect_result->release( );
            this->m_connect_result = nullptr;
        }
    }

    this->m_connect_pending = true;
    this->m_connect_station = station;

    this->set_phase( radio_phase_t::connecting );
    this->m_buffer.store( 0 );

    // the worker gets its own copy of both, so two overlapping requests cannot read each other's
    const usize payload{ ( static_cast< usize >( static_cast< u32 >( generation ) ) << 32 ) |
                         static_cast< u32 >( station ) };

    if ( !atl::create_hidden_thread( reinterpret_cast< atl::thread_entry_fn >( radio_connect_thread ),
                                     reinterpret_cast< void * >( payload ) ) )
    {
        DBGOUT( "radio: could not start a connect thread for %s", radio_station_name( station ) );

        this->m_connect_pending = false;
        this->m_retry_at        = I( GetTickCount64 )( ) + RADIO_RETRY_DELAY;
        this->set_phase( radio_phase_t::failed );
        return;
    }
}

void radio::connect( i32 generation, usize station )
{
    const char *url{ station_url( station ) };
    const u64   started{ I( GetTickCount64 )( ) };

    audio_stream_error_t error{ audio_stream_error_t::url };
    audio_stream        *stream{ url ? audio_stream::open( url, error ) : nullptr };

    const u64 elapsed{ I( GetTickCount64 )( ) - started };

    atl::scoped_lock guard{ this->m_connect_lock };

    // a newer request went out while this one was stuck, so this stream belongs to nobody
    if ( this->m_connect_generation != generation )
    {
        if ( stream )
        {
            stream->release( );
        }

        return;
    }

    this->m_connect_result  = stream;
    this->m_connect_error   = error;
    this->m_connect_elapsed = elapsed;
    this->m_connect_ready   = generation;
}

bool radio::poll_connect( )
{
    audio_stream        *stream{ };
    audio_stream_error_t error{ };
    u64                  elapsed{ };

    {
        atl::scoped_lock guard{ this->m_connect_lock };

        if ( !this->m_connect_ready || this->m_connect_ready != this->m_connect_generation )
        {
            return false;
        }

        stream  = this->m_connect_result;
        error   = this->m_connect_error;
        elapsed = this->m_connect_elapsed;

        this->m_connect_ready  = 0;
        this->m_connect_result = nullptr;
    }

    this->m_connect_pending = false;

    if ( !stream )
    {
        this->m_retry_at = I( GetTickCount64 )( ) + RADIO_RETRY_DELAY;
        this->set_phase( radio_phase_t::failed );

        DBGOUT( "radio: %s failed after %ums, audio error %d, retrying in %ums",
                radio_station_name( this->m_connect_station ), static_cast< u32 >( elapsed ),
                static_cast< i32 >( error ), static_cast< u32 >( RADIO_RETRY_DELAY ) );
        return true;
    }

    const i32 crossfade{ g_cfg.m_radio.m_crossfade };
    if ( this->m_stream && crossfade > 0 )
    {
        this->retire_fade( true ); // only one stream may be on its way out at a time

        this->m_fading        = this->m_stream;
        this->m_fade_deadline = I( GetTickCount64 )( ) + static_cast< u64 >( crossfade );

        this->m_fading->slide( 0.0f, static_cast< u32 >( crossfade ) );
    } else if ( this->m_stream )
    {
        this->m_stream->release( );
    }

    this->m_stream  = stream;
    this->m_station = this->m_connect_station;
    this->m_started = false;
    this->set_title( "" );

    // a crossfading stream comes up silent and is slid in by pump( ) once it actually starts
    this->m_stream->slide( crossfade > 0 ? 0.0f : this->wanted_volume( ), 0 );
    return true;
}

void radio::silence( )
{
    {
        atl::scoped_lock guard{ this->m_connect_lock };

        // orphan anything still in flight; the worker frees its own stream on the way out
        ++this->m_connect_generation;

        this->m_connect_ready = 0;

        if ( this->m_connect_result )
        {
            this->m_connect_result->release( );
            this->m_connect_result = nullptr;
        }
    }

    this->m_connect_pending = false;

    this->retire_fade( true );

    this->set_phase( radio_phase_t::off );
    this->m_buffer.store( 0 );

    this->m_retry_at  = 0;
    this->m_attempted = RADIO_STATION_COUNT;

    if ( !this->m_stream )
    {
        return;
    }

    this->m_stream->release( );

    this->m_stream  = nullptr;
    this->m_station = RADIO_STATION_COUNT;
    this->m_started = false;
    this->set_title( "" );
}

void radio::pump( )
{
    if ( !this->m_stream )
    {
        return;
    }

    if ( this->m_connect_pending )
    {
        return;
    }

    if ( this->m_started )
    {
        const audio_stream_state_t active{ this->m_stream->state( ) };

        if ( active == audio_stream_state_t::stalled )
        {
            this->set_phase( radio_phase_t::buffering );
            this->m_buffer.store( math::clamp( this->m_stream->buffer_percent( ), 0, 100 ) );
            return;
        }

        if ( active == audio_stream_state_t::stopped )
        {
            DBGOUT( "radio: %s dropped the connection", radio_station_name( this->m_station ) );

            this->m_stream->release( );

            this->m_stream  = nullptr;
            this->m_station = RADIO_STATION_COUNT;
            this->m_started = false;

            this->m_buffer.store( 0 );
            this->set_title( "" );
            this->set_phase( radio_phase_t::connecting );
            return;
        }

        this->set_phase( radio_phase_t::playing );
        this->m_buffer.store( 100 );
        this->poll_title( );
        this->sample_bands( );
        return;
    }

    const i32 filled{ this->m_stream->buffer_percent( ) };

    if ( static_cast< u64 >( filled ) <= RADIO_BUFFER_READY && this->m_stream->connected( ) )
    {
        this->set_phase( radio_phase_t::buffering );
        this->m_buffer.store( filled );
        return;
    }

    this->m_stream->play( );
    this->m_started = true;

    if ( const i32 crossfade{ g_cfg.m_radio.m_crossfade }; crossfade > 0 )
    {
        this->m_stream->slide( this->wanted_volume( ), static_cast< u32 >( crossfade ) );

        this->m_fade_in_until = I( GetTickCount64 )( ) + static_cast< u64 >( crossfade );
    }

    this->set_phase( radio_phase_t::playing );
    this->m_buffer.store( 100 );
}

void radio::apply_volume( )
{
    if ( !this->m_stream || I( GetTickCount64 )( ) < this->m_fade_in_until )
    {
        return;
    }

    this->m_stream->slide( this->wanted_volume( ), 0 );
}

void radio::pause_output( bool paused )
{
    if ( this->m_stream )
    {
        this->m_stream->pause( paused );
    }

    if ( this->m_fading )
    {
        this->m_fading->pause( paused );
    }
}

void radio::retire_fade( bool immediately )
{
    if ( !this->m_fading )
    {
        return;
    }

    if ( !immediately && I( GetTickCount64 )( ) < this->m_fade_deadline )
    {
        return;
    }

    this->m_fading->release( );
    this->m_fading = nullptr;
}

void radio::set_title( const char *title )
{
    atl::scoped_lock guard{ this->m_title_lock };

    if ( str::equals( title, static_cast< const char * >( this->m_title ) ) )
    {
        return;
    }

    str::copy( this->m_title, sizeof( this->m_title ), title );
    this->m_title_serial.store( this->m_title_serial.load( ) + 1 );
}

void radio::poll_title( )
{
    char title[ RADIO_TITLE_CAPACITY ]{ };

    this->m_stream->title( title, sizeof( title ) );
    this->set_title( title );
}

void radio::sample_bands( )
{
    constexpr usize edges[ RADIO_BAND_COUNT + 1 ]{ 0, 2, 4, 8, 16, 32, 64 };
    constexpr float gain{ 1.6f };

    f32 spectrum[ AUDIO_SPECTRUM_BINS ]{ };

    this->m_stream->spectrum( spectrum );

    for ( usize band{ }; band < RADIO_BAND_COUNT; ++band )
    {
        float peak{ };

        for ( usize bin{ edges[ band ] }; bin < edges[ band + 1 ]; ++bin )
        {
            peak = math::bigger( peak, spectrum[ bin ] );
        }

        this->m_bands[ band ].store(
            static_cast< i32 >( math::clamp( math::sqrt( peak ) * gain, 0.0f, 1.0f ) * 255.0f ) );
    }
}

float radio::wanted_volume( ) const
{
    if ( this->unfocused( ) )
    {
        return 0.0f;
    }

    float volume{ static_cast< float >( g_cfg.m_radio.m_volume ) / 100.0f };

    if ( this->ducking( ) )
    {
        volume *= static_cast< float >( g_cfg.m_radio.m_duck_level ) / 100.0f;
    }

    if ( !g_cfg.m_radio.m_scale_to_game_volume || !this->m_game_volume )
    {
        return volume;
    }

    return math::remap_clamped( volume, 0.0f, 1.0f, 0.0f, this->m_game_volume->GetFloat( ) );
}

bool radio::ducking( ) const
{
    if ( !g_cfg.m_radio.m_duck_on_voice )
    {
        return false;
    }

    return ( g_cfg.m_radio.m_duck_on_self && this->m_self_talking ) ||
           ( g_cfg.m_radio.m_duck_on_others && this->m_others_talking );
}

bool radio::unfocused( ) const
{
    return g_cfg.m_radio.m_mute_unfocused && I( GetForegroundWindow )( ) != g_input.window( );
}

void radio::on_voice_status( i32 entity, bool talking )
{
    // CVoiceStatus::UpdateSpeakerStatus is what the game does with these: -1 is us holding the
    // voice key, -2 is only the server acknowledging our voice, and a positive index is another
    // player; -3 is a vox threshold the retail build does not compile
    if ( entity == -1 )
    {
        this->m_self_talking.store( talking );
        return;
    }

    if ( entity < 1 || entity > RADIO_VOICE_MAX_PLAYERS )
    {
        return;
    }

    const u64 bit{ 1ull << ( entity - 1 ) };
    if ( talking )
    {
        this->m_speaking |= bit;
    } else
    {
        this->m_speaking &= ~bit;
    }

    this->m_others_talking.store( this->m_speaking != 0 );
}

void radio::on_level_shutdown( )
{
    this->m_speaking = 0;
    this->m_self_talking.store( false );
    this->m_others_talking.store( false );
}

void radio::on_frame( )
{
    if ( this->pressed( BIND_TOGGLE, g_cfg.m_radio.m_key ) )
    {
        g_cfg.m_radio.m_enabled = !g_cfg.m_radio.m_enabled;
    }

    if ( this->pressed( BIND_NEXT, g_cfg.m_radio.m_next_key ) )
    {
        this->step_station( 1 );
    }

    if ( this->pressed( BIND_PREVIOUS, g_cfg.m_radio.m_previous_key ) )
    {
        this->step_station( -1 );
    }

    if ( this->pressed( BIND_LOUDER, g_cfg.m_radio.m_louder_key ) )
    {
        g_cfg.m_radio.m_volume = math::clamp( g_cfg.m_radio.m_volume + RADIO_VOLUME_STEP, 0, 100 );
    }

    if ( this->pressed( BIND_QUIETER, g_cfg.m_radio.m_quieter_key ) )
    {
        g_cfg.m_radio.m_volume = math::clamp( g_cfg.m_radio.m_volume - RADIO_VOLUME_STEP, 0, 100 );
    }

    if ( !g_cfg.m_radio.m_enabled )
    {
        this->m_announced = RADIO_STATION_COUNT;
        return;
    }

    if ( this->m_announced != g_cfg.m_radio.m_station )
    {
        this->m_announced = g_cfg.m_radio.m_station;

        push_notification( notify_kind_t::information, ATLAS_XORSTR_STATIC( "Station changed" ),
                           radio_station_name( g_cfg.m_radio.m_station ) );
    }
}

bool radio::pressed( usize slot, bind_t &bind )
{
    const bool active{ gui::bind_active( bind ) };
    const bool edge{ active && !this->m_bind_held[ slot ] };

    this->m_bind_held[ slot ] = active;
    return edge;
}

void radio::step_station( i32 delta )
{
    const i32 count{ static_cast< i32 >( RADIO_STATION_COUNT ) };

    i32 next{ static_cast< i32 >( g_cfg.m_radio.m_station ) + delta };

    while ( next < 0 )
    {
        next += count;
    }

    g_cfg.m_radio.m_station = static_cast< usize >( next % count );
}

void radio::toggle_paused( )
{
    this->m_paused.store( !this->m_paused.load( ) );
}

bool radio::paused( ) const
{
    return this->m_paused;
}

radio_phase_t radio::phase( ) const
{
    return static_cast< radio_phase_t >( this->m_phase.load( ) );
}

i32 radio::buffer_percent( ) const
{
    return this->m_buffer.load( );
}

const char *radio::now_playing( ) const
{
    if ( this->phase( ) == radio_phase_t::off )
    {
        return radio_station_name( RADIO_STATION_COUNT );
    }

    return radio_station_name( g_cfg.m_radio.m_station );
}

constexpr i32   RADIO_HUD_WIDTH{ 184 };
constexpr i32   RADIO_HUD_ROW_HEIGHT{ INDICATOR_WELL_HEIGHT };
constexpr i32   RADIO_HUD_ROW_STEP{ RADIO_HUD_ROW_HEIGHT + 6 };
constexpr i32   RADIO_HUD_DETAILED_HEIGHT{ INDICATOR_HEIGHT + RADIO_HUD_ROW_STEP };
constexpr i32   RADIO_HUD_TEXT_LEFT{ 26 };
constexpr i32   RADIO_HUD_BAR_WIDTH{ 3 };
constexpr i32   RADIO_HUD_BAR_GAP{ 2 };
constexpr i32   RADIO_HUD_BAR_HEIGHT{ 10 };
constexpr float RADIO_HUD_BAR_ATTACK{ 60.0f };
constexpr float RADIO_HUD_BAR_RELEASE{ 260.0f };
constexpr u64   RADIO_HUD_SCROLL_HOLD{ 1500 };
constexpr i32   RADIO_HUD_SCROLL_SPEED{ 24 };
constexpr i32   RADIO_HUD_SCROLL_GAP{ 28 };
constexpr u32   RADIO_HUD_FLASH{ 900 };

enum class radio_status_t : usize
{
    connecting,
    buffering,
    playing,
    ducked,
    paused,
    failed,
    count,
};

struct radio_look_t
{
    const char *m_label{ };
    color_t     m_text{ };
    color_t     m_ink{ };
};

static indicator_card_t s_radio_card{ };
static u8               s_radio_mix[ static_cast< usize >( radio_status_t::count ) ]{ };
static float            s_radio_bars[ RADIO_BAND_COUNT ]{ };
static u8               s_radio_ghost{ };
static u8               s_radio_flash{ };
static i32              s_radio_serial{ };
static u64              s_radio_title_at{ };

static radio_look_t radio_look( radio_status_t status )
{
    switch ( status )
    {
        case radio_status_t::playing: return radio_look_t{ ATLAS_XORSTR_STATIC( "playing" ), g_theme.m_accent, g_theme.m_accent };
        case radio_status_t::ducked: return radio_look_t{ ATLAS_XORSTR_STATIC( "ducked" ), STATUS_WARNING, STATUS_WARNING };
        case radio_status_t::paused: return radio_look_t{ ATLAS_XORSTR_STATIC( "paused" ), MENU_TEXT_DIM, MENU_TEXT_DIM };
        case radio_status_t::failed: return radio_look_t{ ATLAS_XORSTR_STATIC( "failed" ), RADIO_FAILED_COLOR, RADIO_FAILED_COLOR };
        case radio_status_t::buffering: return radio_look_t{ ATLAS_XORSTR_STATIC( "buffering" ), CONTROL_VALUE, MENU_TEXT_DIM };
        default: return radio_look_t{ ATLAS_XORSTR_STATIC( "connecting" ), CONTROL_VALUE, MENU_TEXT_DIM };
    }
}

ATLAS_FORCE_INLINE u8 radio_mix( radio_status_t status )
{
    return gui::eased_mix( s_radio_mix[ static_cast< usize >( status ) ] );
}

void radio::paint( ) const
{
    const bool wanted{ g_cfg.m_radio.m_enabled && g_cfg.m_radio.m_indicator };

    if ( !wanted && !s_radio_card.m_enter )
    {
        hide_indicator( s_radio_card );

        for ( u8 &mix : s_radio_mix )
        {
            mix = 0;
        }

        for ( float &level : s_radio_bars )
        {
            level = 0.0f;
        }

        s_radio_ghost = 0;
        return;
    }

    const radio_phase_t phase{ this->phase( ) };
    const bool          ducked{ phase == radio_phase_t::playing && this->ducking( ) };

    radio_status_t status{ radio_status_t::connecting };

    switch ( phase )
    {
        case radio_phase_t::buffering: status = radio_status_t::buffering; break;
        case radio_phase_t::playing: status = ducked ? radio_status_t::ducked : radio_status_t::playing; break;
        case radio_phase_t::paused: status = radio_status_t::paused; break;
        case radio_phase_t::failed: status = radio_status_t::failed; break;
        default: break;
    }

    const float volume{ static_cast< float >( g_cfg.m_radio.m_volume ) / 100.0f };
    const float heard{ ducked ? volume * static_cast< float >( g_cfg.m_radio.m_duck_level ) / 100.0f : volume };
    float       fill{ heard };

    if ( status == radio_status_t::connecting || status == radio_status_t::buffering )
    {
        fill = static_cast< float >( math::clamp( this->buffer_percent( ), 0, 100 ) ) / 100.0f;
    } else if ( status == radio_status_t::failed )
    {
        fill = 0.0f;
    }

    const bool detailed{ g_cfg.m_radio.m_indicator_style == config::RADIO_INDICATOR_DETAILED };

    s_radio_card.m_at = g_cfg.m_radio.m_indicator_position;
    const u32 delta{ step_indicator( s_radio_card, fill, g_cl.m_screen_width, g_cl.m_screen_height, RADIO_HUD_WIDTH,
                                     detailed ? RADIO_HUD_DETAILED_HEIGHT : INDICATOR_HEIGHT, wanted ) };
    g_cfg.m_radio.m_indicator_position = s_radio_card.m_at;

    for ( usize index{ }; index < static_cast< usize >( radio_status_t::count ); ++index )
    {
        gui::step_mix( s_radio_mix[ index ], index == static_cast< usize >( status ), delta, WIDGET_TOGGLE_DURATION );
    }

    gui::step_mix( s_radio_ghost, ducked, delta, WIDGET_TOGGLE_DURATION );

    const u8 warning{ radio_mix( radio_status_t::ducked ) };
    const u8 failure{ radio_mix( radio_status_t::failed ) };
    const u8 live{ static_cast< u8 >( math::smaller( radio_mix( radio_status_t::playing ) + warning, 255 ) ) };

    const color_t edge{ gui::mix_color( gui::mix_color( gui::mix_color( g_theme.m_input_focus_border, g_theme.m_accent, live ),
                                                        STATUS_WARNING, warning ),
                                        RADIO_FAILED_COLOR, failure ) };
    const color_t wash{ gui::mix_color( g_theme.m_selected_background, g_theme.m_row_selected_background, live ) };

    const indicator_frame_t frame{ draw_indicator_frame( s_radio_card, ATLAS_XORSTR_STATIC( "radio" ), CARD_TITLE_TEXT ) };
    const u8                enter{ frame.m_enter };

    const rect_t first{ frame.m_content.left, frame.m_content.top, frame.m_content.right,
                        frame.m_content.top + RADIO_HUD_ROW_HEIGHT };
    const rect_t second{ first.left, first.top + RADIO_HUD_ROW_STEP, first.right, first.bottom + RADIO_HUD_ROW_STEP };

    draw_indicator_well( first, enter );

    if ( detailed )
    {
        const rect_t inner{ first.left + 1, first.top + 1, first.right - 1, first.bottom - 1 };
        const i32    washed{ static_cast< i32 >( s_radio_card.m_fill * static_cast< float >( inner.right - inner.left ) +
                                                 0.5f ) };
        const i32    fill_edge{ math::smaller( inner.left + washed, inner.right - 1 ) };

        g_render.box( rect_t{ inner.left, inner.top, inner.left + washed, inner.bottom },
                      gui::with_alpha( wash, enter ), true );
        g_render.box( rect_t{ fill_edge, inner.top, fill_edge + 1, inner.bottom }, gui::with_alpha( edge, enter ),
                      true );

        const u8 ghost{ gui::eased_mix( s_radio_ghost ) };

        if ( ghost )
        {
            const i32 mark{ inner.left + static_cast< i32 >( volume * static_cast< float >( inner.right - inner.left ) ) };

            for ( i32 top{ inner.top }; top < inner.bottom; top += 3 )
            {
                g_render.box( rect_t{ mark, top, mark + 1, math::smaller( top + 2, inner.bottom ) },
                              gui::with_alpha( gui::with_alpha( CONTROL_LABEL, ghost ), enter ), true );
            }
        }
    }

    draw_indicator_stripe( first, enter, edge );

    char      name[ 64 ]{ };
    const i32 bars_width{ static_cast< i32 >( RADIO_BAND_COUNT ) * ( RADIO_HUD_BAR_WIDTH + RADIO_HUD_BAR_GAP ) -
                          RADIO_HUD_BAR_GAP };
    const i32 percent_room{ detailed ? gui::estimate_tiny_text_width( "100%" ) : bars_width + 5 };
    gui::fit_text( name, radio_station_name( g_cfg.m_radio.m_station ),
                   first.right - 10 - percent_room - ( first.left + RADIO_HUD_TEXT_LEFT ) );

    g_render.text( MENU_FONT_TINY( ), point_t{ first.left + RADIO_HUD_TEXT_LEFT, indicator_text_y( first ) },
                   gui::with_alpha( CONTROL_VALUE, enter ), name );

    for ( usize index{ }; index < static_cast< usize >( radio_status_t::count ); ++index )
    {
        const radio_status_t layer{ static_cast< radio_status_t >( index ) };
        const u8             mix{ radio_mix( layer ) };

        if ( !mix )
        {
            continue;
        }

        const radio_look_t look{ radio_look( layer ) };
        const bool         working{ layer == radio_status_t::connecting || layer == radio_status_t::buffering };
        const u8           pulse{ working ? indicator_glyph_alpha( 255 ) : u8{ 255 } };

        g_render.text( MENU_FONT_TINY( ), frame.m_state, gui::with_alpha( gui::with_alpha( look.m_text, mix ), enter ),
                       look.m_label, false, render::text_align::right );
        gui::draw_icon_at( ICON_RADIO, point_t{ first.left + 6, first.top + 2 },
                           gui::with_alpha( gui::with_alpha( look.m_ink, gui::mix_channel( 0, pulse, mix ) ), enter ) );
    }

    if ( detailed )
    {
        char percent[ 8 ]{ };
        str::format_to( percent, sizeof( percent ), ATLAS_XORSTR_STATIC( "{}%" ),
                        static_cast< i32 >( heard * 100.0f + 0.5f ) );

        g_render.text( MENU_FONT_TINY( ), point_t{ first.right - 5, indicator_text_y( first ) },
                       gui::with_alpha( gui::mix_color( gui::mix_color( MENU_TEXT_DIM, CHECKBOX_MARK_COLOR, live ),
                                                        STATUS_WARNING, warning ),
                                        enter ),
                       percent, false, render::text_align::right );
    } else
    {
        const color_t bars{ gui::mix_color( gui::mix_color( MENU_TEXT_DIM, g_theme.m_accent, live ), STATUS_WARNING,
                                            warning ) };
        const i32     bottom{ first.top + ( RADIO_HUD_ROW_HEIGHT + RADIO_HUD_BAR_HEIGHT ) / 2 };

        for ( usize band{ }; band < RADIO_BAND_COUNT; ++band )
        {
            const float target{ status == radio_status_t::playing || status == radio_status_t::ducked
                                    ? static_cast< float >( this->m_bands[ band ].load( ) ) / 255.0f
                                    : 0.0f };

            s_radio_bars[ band ] = gui::approach_float( s_radio_bars[ band ], target, delta,
                                                        target > s_radio_bars[ band ] ? RADIO_HUD_BAR_ATTACK
                                                                                      : RADIO_HUD_BAR_RELEASE );

            const i32 height{ 1 + static_cast< i32 >( s_radio_bars[ band ] * static_cast< float >( RADIO_HUD_BAR_HEIGHT - 1 ) +
                                                      0.5f ) };
            const i32 left{ first.right - 6 - bars_width +
                            static_cast< i32 >( band ) * ( RADIO_HUD_BAR_WIDTH + RADIO_HUD_BAR_GAP ) };

            g_render.box( rect_t{ left, bottom - height, left + RADIO_HUD_BAR_WIDTH, bottom },
                          gui::with_alpha( bars, enter ), true );
        }
    }

    if ( !detailed )
    {
        return;
    }

    draw_indicator_well( second, enter );
    draw_indicator_stripe( second, enter, edge );
    gui::draw_icon_at( ICON_MUSIC, point_t{ second.left + 6, second.top + 2 },
                       gui::with_alpha( CONTROL_LABEL, enter ) );

    char title[ RADIO_TITLE_CAPACITY ]{ };

    {
        atl::scoped_lock guard{ this->m_title_lock };
        str::copy( title, sizeof( title ), static_cast< const char * >( this->m_title ) );
    }

    const i32 serial{ this->m_title_serial.load( ) };

    if ( serial != s_radio_serial )
    {
        s_radio_serial   = serial;
        s_radio_flash    = 255;
        s_radio_title_at = gui::clock( );
    }

    gui::step_mix( s_radio_flash, false, delta, RADIO_HUD_FLASH );

    const i32 text_left{ second.left + RADIO_HUD_TEXT_LEFT };
    const i32 text_right{ second.right - 5 };
    const i32 text_y{ indicator_text_y( second ) };

    if ( title[ 0 ] && ( status == radio_status_t::playing || status == radio_status_t::ducked ) )
    {
        const i32 width{ gui::estimate_tiny_text_width( title ) };
        const i32 cycle{ width + RADIO_HUD_SCROLL_GAP };
        const u64 elapsed{ gui::clock( ) - s_radio_title_at };
        i32       shift{ };

        if ( width > text_right - text_left && elapsed > RADIO_HUD_SCROLL_HOLD )
        {
            shift = static_cast< i32 >( ( elapsed - RADIO_HUD_SCROLL_HOLD ) * RADIO_HUD_SCROLL_SPEED / 1000 ) % cycle;
        }

        const color_t color{ gui::with_alpha( gui::mix_color( CONTROL_VALUE, g_theme.m_accent,
                                                              gui::eased_mix( s_radio_flash ) ),
                                              enter ) };

        g_render.set_clip( rect_t{ text_left, second.top, text_right, second.bottom } );
        g_render.text( MENU_FONT_TINY( ), point_t{ text_left - shift, text_y }, color, title );

        if ( shift )
        {
            g_render.text( MENU_FONT_TINY( ), point_t{ text_left - shift + cycle, text_y }, color, title );
        }

        g_render.reset_clip( );
        return;
    }

    char line[ 32 ]{ };

    switch ( status )
    {
        case radio_status_t::connecting: str::format_to( line, sizeof( line ), ATLAS_XORSTR_STATIC( "connecting" ) ); break;
        case radio_status_t::buffering:
            str::format_to( line, sizeof( line ), ATLAS_XORSTR_STATIC( "buffering {}%" ), this->buffer_percent( ) );
            break;
        case radio_status_t::paused: str::format_to( line, sizeof( line ), ATLAS_XORSTR_STATIC( "paused" ) ); break;
        case radio_status_t::failed: str::format_to( line, sizeof( line ), ATLAS_XORSTR_STATIC( "retrying" ) ); break;
        default: str::format_to( line, sizeof( line ), ATLAS_XORSTR_STATIC( "live stream" ) ); break;
    }

    g_render.text( MENU_FONT_TINY( ), point_t{ text_left, text_y }, gui::with_alpha( CONTROL_LABEL, enter ), line );
}
