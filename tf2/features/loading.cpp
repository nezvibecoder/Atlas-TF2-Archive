#include "loading.hpp"

#include "config.hpp"
#include "global.hpp"
#include "gui/gui.hpp"
#include "gui/theme.hpp"
#include "gui/watermark.hpp"
#include "render/render.hpp"
#include "util/format.hpp"

ATLAS_NO_DESTROY loading g_loading{ };

constexpr u64     MIN_VISIBLE_MS{ 1400 }; // doing this cuz our cheat have a fucking ferrari engine
constexpr u64     LINGER_MS{ 400 };
constexpr float   FILL_GLIDE{ 140.0f };
constexpr u32     WAIT_PULSE_PERIOD{ 1600 };
constexpr u8      WAIT_PULSE_FLOOR{ 90 };
constexpr color_t SCREEN_DIM{ 8, 9, 11, 204 };

static const char *stage_caption( load_stage_t stage )
{
    switch ( stage )
    {
        case load_stage_t::render: return ATLAS_XORSTR_STATIC( "Starting drawsystem" );
        case load_stage_t::modules: return ATLAS_XORSTR_STATIC( "Waiting for the game" );
        case load_stage_t::interfaces: return ATLAS_XORSTR_STATIC( "Fetching game classes" );
        case load_stage_t::exports: return ATLAS_XORSTR_STATIC( "Fetching exports" );
        case load_stage_t::signatures: return ATLAS_XORSTR_STATIC( "Obtaining addresses" );
        case load_stage_t::indices: return ATLAS_XORSTR_STATIC( "Acquiring offsets" );
        case load_stage_t::convars: return ATLAS_XORSTR_STATIC( "Unlocking convars" );
        case load_stage_t::netvars: return ATLAS_XORSTR_STATIC( "Obtaining player offsets" );
        case load_stage_t::datamaps: return ATLAS_XORSTR_STATIC( "Applying fixes" );
        case load_stage_t::features: return ATLAS_XORSTR_STATIC( "Initializing features" );
        case load_stage_t::hooks: return ATLAS_XORSTR_STATIC( "Writing trampolines" );
        default: return ATLAS_XORSTR_STATIC( "Starting..." );
    }
}

void loading::enter( load_stage_t stage )
{
    this->m_stage.store( static_cast< i32 >( stage ) );
}

void loading::draw( )
{
    constexpr i32 total{ static_cast< i32 >( load_stage_t::count ) };

    if ( this->m_released && !this->m_shown )
    {
        return;
    }

    // the viewport is only known once a frame has gone through the present hook
    if ( g_cl.m_screen_width <= 0 || g_cl.m_screen_height <= 0 )
    {
        return;
    }

    const u64 now{ gui::clock( ) };
    const u32 delta{ gui::animation_delta( this->m_tick ) };

    if ( !this->m_started )
    {
        this->m_started = now;

        for ( i32 stage{ }; stage < total; ++stage )
        {
            this->m_caption_width = math::bigger( this->m_caption_width, gui::estimate_text_width( stage_caption( static_cast< load_stage_t >( stage ) ) ) );
        }
    }

    if ( g_cl.m_is_running && !this->m_released )
    {
        if ( !this->m_finished_at )
        {
            this->m_finished_at = now;
        }

        if ( now >= math::bigger( this->m_started + MIN_VISIBLE_MS, this->m_finished_at + LINGER_MS ) )
        {
            this->m_released = true;

            if ( g_cfg.m_misc.m_draw_watermark )
            {
                launch_watermark( );
            }
        }
    }

    gui::step_mix( this->m_shown, !this->m_released, delta, BACKGROUND_FADE_DURATION );

    const u8 shown{ gui::eased_mix( this->m_shown ) };

    g_render.box( rect_t{ 0, 0, g_cl.m_screen_width, g_cl.m_screen_height }, gui::with_alpha( SCREEN_DIM, shown ), true );

    if ( this->m_released && g_cfg.m_misc.m_draw_watermark )
    {
        return;
    }

    const i32 stage{ this->m_stage.load( ) };
    const i32 done{ math::clamp( stage + 1, 1, total ) };

    this->m_fill = gui::approach_float( this->m_fill, static_cast< f32 >( done ) / static_cast< f32 >( total ), delta, FILL_GLIDE );

    char counter[ 16 ]{ };
    str::format_to( counter, sizeof( counter ), ATLAS_XORSTR_STATIC( "{}/{}" ), done, total );

    watermark_boot_t boot{ };

    boot.m_caption       = stage_caption( static_cast< load_stage_t >( stage ) );
    boot.m_caption_width = this->m_caption_width;
    boot.m_counter       = counter;
    boot.m_fill          = this->m_fill;
    boot.m_opacity       = shown;

    boot.m_line = stage == static_cast< i32 >( load_stage_t::modules )
                      ? gui::mix_channel( WAIT_PULSE_FLOOR, 255, gui::pulse( now, WAIT_PULSE_PERIOD ) )
                      : u8{ 255 };

    draw_watermark_boot( g_cl.m_screen_width, g_cl.m_screen_height, boot );
}
