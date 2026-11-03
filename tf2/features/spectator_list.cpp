#include "spectator_list.hpp"

#include "gui/gui.hpp"
#include "gui/indicator.hpp"
#include "math/math.hpp"
#include "render/render.hpp"
#include "util/format.hpp"
#include "util/string.hpp"

#include "link.hpp"
#include "streamer_mode.hpp"

constexpr i32 SPECTATOR_WIDTH{ 184 };
constexpr i32 SPECTATOR_ROW_HEIGHT{ INDICATOR_WELL_HEIGHT };
constexpr i32 SPECTATOR_ROW_GAP{ 6 };
constexpr i32 SPECTATOR_ROW_STEP{ SPECTATOR_ROW_HEIGHT + SPECTATOR_ROW_GAP };
constexpr u32 SPECTATOR_AVATAR_SIZE{ 15 };
constexpr i32 SPECTATOR_PLAYER_LIMIT{ 64 };
constexpr u32 SPECTATOR_ROW_ENTER{ 180 };
constexpr u32 SPECTATOR_ROW_LEAVE{ 160 };

struct spectator_row_t
{
    char  m_name[ 64 ]{ };
    i32   m_user_id{ };
    i32   m_mode{ };
    i32   m_target_row{ };
    u32   m_avatar{ };
    float m_row{ };
    u8    m_alpha{ };
    u8    m_mode_flash{ };
    bool  m_used{ };
    bool  m_active{ };
};

static indicator_card_t s_spectator_card{ };
static spectator_row_t s_spectator_rows[ SPECTATOR_PLAYER_LIMIT + 1 ]{ };
static u64             s_spectator_tick{ };
static float           s_spectator_height{ };
static bool            s_spectator_preview{ };

ATLAS_FORCE_INLINE const char *spectator_mode_label( i32 mode )
{
    switch ( mode )
    {
        case OBS_MODE_DEATHCAM: return ATLAS_XORSTR_STATIC( "death" );
        case OBS_MODE_FREEZECAM: return ATLAS_XORSTR_STATIC( "freezecam" );
        case OBS_MODE_IN_EYE: return ATLAS_XORSTR_STATIC( "1st" );
        case OBS_MODE_CHASE: return ATLAS_XORSTR_STATIC( "3rd" );
        default: return nullptr;
    }
}

void draw_spectator_list( )
{
    if ( !g_cfg.m_misc.m_spectator_list )
    {
        hide_indicator( s_spectator_card );
        for ( spectator_row_t &row : s_spectator_rows )
        {
            row = spectator_row_t{ };
        }
        s_spectator_height = 0.0f;
        s_spectator_preview = false;
        return;
    }

    for ( spectator_row_t &row : s_spectator_rows )
    {
        row.m_active = false;
    }

    i32 count{ };
    if ( g_tf2.m_engine->IsInGame( ) && g_cl.m_local )
    {
        const i32 local_index{ g_cl.m_local->entindex( ) };
        const i32 max_clients{ math::smaller( g_tf2.m_engine->GetMaxClients( ), SPECTATOR_PLAYER_LIMIT ) };

        for ( i32 index{ 1 }; index <= max_clients; ++index )
        {
            if ( index == local_index )
            {
                continue;
            }

            IClientEntity *client{ g_tf2.m_entity_list->GetClientEntity( index ) };
            if ( !client || client->IsDormant( ) )
            {
                continue;
            }

            C_BaseEntity *entity{ client->GetBaseEntity( ) };
            if ( !entity )
            {
                continue;
            }

            C_TFPlayer *player{ entity->As< C_TFPlayer * >( ) };
            const i32   mode{ player->m_iObserverMode( ) };
            if ( !spectator_mode_label( mode ) ||
                 player->m_hObserverTarget( ).Get( ) != g_cl.m_local )
            {
                continue;
            }

            player_info_t info{ };
            if ( !g_tf2.m_engine->GetPlayerInfo( index, &info ) || info.ishltv || info.isreplay )
            {
                continue;
            }

            spectator_row_t &row{ s_spectator_rows[ index ] };
            if ( !row.m_used || row.m_user_id != info.userID )
            {
                row           = spectator_row_t{ };
                row.m_used    = true;
                row.m_user_id = info.userID;
                row.m_mode    = mode;
                row.m_row     = static_cast< float >( count );
                row.m_alpha   = s_spectator_card.m_enter ? 0 : 255;
            } else if ( row.m_mode != mode )
            {
                row.m_mode       = mode;
                row.m_mode_flash = 255;
            }

            row.m_active     = true;
            row.m_target_row = count++;

            const bool hidden{ g_streamer_mode.should_hide_player( index ) };
            row.m_avatar = hidden ? 0 : info.friendsID;

            const char *name{ hidden ? g_streamer_mode.get_ideal_player_name( index ) : info.name };
            str::copy( row.m_name, sizeof( row.m_name ), name );
        }
    }

    if ( count )
    {
        s_spectator_preview = false;
    } else if ( g_gui.open( ) )
    {
        s_spectator_preview = true;
    }

    const u32 delta{ gui::animation_delta( s_spectator_tick ) };
    i32       rows{ count ? count : 1 };
    bool      fading{ };

    for ( spectator_row_t &row : s_spectator_rows )
    {
        if ( !row.m_used )
        {
            continue;
        }

        gui::step_mix( row.m_alpha, row.m_active, delta,
                       row.m_active ? SPECTATOR_ROW_ENTER : SPECTATOR_ROW_LEAVE );
        gui::step_mix( row.m_mode_flash, false, delta, WIDGET_TOGGLE_DURATION );

        if ( !row.m_active && !row.m_alpha )
        {
            row = spectator_row_t{ };
            continue;
        }

        if ( row.m_active )
        {
            row.m_row = gui::approach_float( row.m_row, static_cast< float >( row.m_target_row ),
                                              delta, 110.0f );
            rows = math::bigger( rows, static_cast< i32 >( row.m_row ) + 1 );
        } else
        {
            rows   = math::bigger( rows, static_cast< i32 >( row.m_row ) + 1 );
            fading = true;
        }
    }

    const bool visible{ g_gui.open( ) || count > 0 };
    if ( !visible && !s_spectator_card.m_enter )
    {
        hide_indicator( s_spectator_card );
        s_spectator_height = 0.0f;
        s_spectator_preview = false;
        return;
    }

    if ( g_cl.m_screen_width < SPECTATOR_WIDTH || g_cl.m_screen_height < INDICATOR_HEIGHT )
    {
        return;
    }

    const i32 max_rows{ 1 + ( g_cl.m_screen_height - INDICATOR_HEIGHT ) / SPECTATOR_ROW_STEP };
    rows = math::smaller( rows, max_rows );

    const i32 target_height{ INDICATOR_HEIGHT + ( rows - 1 ) * SPECTATOR_ROW_STEP };
    s_spectator_height = s_spectator_height > static_cast< float >( target_height )
                             ? gui::approach_float( s_spectator_height, static_cast< float >( target_height ),
                                                    delta, 140.0f )
                             : static_cast< float >( target_height );
    s_spectator_height = math::smaller( s_spectator_height, static_cast< float >( g_cl.m_screen_height ) );

    s_spectator_card.m_at = g_cfg.m_misc.m_spectator_list_position;
    step_indicator( s_spectator_card, 0.0f, g_cl.m_screen_width, g_cl.m_screen_height,
                    SPECTATOR_WIDTH, static_cast< i32 >( s_spectator_height + 0.5f ), visible );
    g_cfg.m_misc.m_spectator_list_position = s_spectator_card.m_at;

    const indicator_frame_t frame{ draw_indicator_frame( s_spectator_card,
                                                         ATLAS_XORSTR_STATIC( "spectators" ),
                                                         CARD_TITLE_TEXT ) };
    const u8                enter{ frame.m_enter };

    char status[ 24 ]{ };
    if ( count )
    {
        str::format_to( status, sizeof( status ), ATLAS_XORSTR_STATIC( "{} watching" ), count );
    }

    g_render.text( MENU_FONT_TINY( ), frame.m_state,
                   gui::with_alpha( count ? g_theme.m_accent : CONTROL_VALUE, enter ),
                   count ? status : s_spectator_preview ? ATLAS_XORSTR_STATIC( "preview" ) : ATLAS_XORSTR_STATIC( "" ),
                   false, render::text_align::right );

    g_render.set_clip( frame.m_content );

    if ( !count && !fading && s_spectator_preview )
    {
        const rect_t preview{ frame.m_content };
        g_render.box( preview, gui::with_alpha( CONTROL_BACKGROUND, enter ), true );
        g_render.box( rect_t{ preview.left, preview.top, preview.right - 1, preview.bottom - 1 },
                      gui::with_alpha( CONTROL_BORDER, enter ) );
        gui::draw_icon_at( ICON_EYE, point_t{ preview.left + 5, preview.top + 2 },
                           gui::with_alpha( MENU_TEXT_DIM, enter ) );
        g_render.text( MENU_FONT_TINY( ), point_t{ preview.left + 22, preview.top + 4 },
                       gui::with_alpha( MENU_TEXT_MUTED, enter ),
                       ATLAS_XORSTR_STATIC( "No spectators" ) );
    }

    for ( const spectator_row_t &row : s_spectator_rows )
    {
        if ( !row.m_used || !row.m_alpha )
        {
            continue;
        }

        const u8  alpha{ gui::mix_channel( 0, enter, gui::eased_out( row.m_alpha ) ) };
        const i32 slide{ -6 * ( 255 - gui::eased_out( row.m_alpha ) ) / 255 };
        const i32 top{ frame.m_content.top + static_cast< i32 >( row.m_row * SPECTATOR_ROW_STEP + 0.5f ) };
        const rect_t well{ frame.m_content.left + slide, top,
                           frame.m_content.right + slide, top + SPECTATOR_ROW_HEIGHT };
        const i32 text_y{ top + ( SPECTATOR_ROW_HEIGHT - TINY_FONT_INK_HEIGHT ) / 2 -
                          TINY_FONT_INK_TOP };

        g_render.box( well, gui::with_alpha( CONTROL_BACKGROUND, alpha ), true );
        g_render.box( rect_t{ well.left, well.top, well.right - 1, well.bottom - 1 },
                      gui::with_alpha( CONTROL_BORDER, alpha ) );
        g_render.box( rect_t{ well.left + 1, well.top + 1, well.left + 2, well.bottom - 1 },
                      gui::with_alpha( g_theme.m_accent, alpha ), true );

        const rect_t avatar{ well.left + 5, well.top + 1, well.left + 20, well.bottom - 1 };
        if ( row.m_avatar )
        {
            g_render.text( MENU_FONT( ), point_t{ avatar.left, avatar.top },
                           gui::with_alpha( CONTROL_VALUE, alpha ), ATLAS_XORSTR_STATIC( " " ), false,
                           render::text_align::left, 0.0f, row.m_avatar, render::AVATAR_SQUARE,
                           SPECTATOR_AVATAR_SIZE );
            g_render.box( avatar, gui::with_alpha( CONTROL_BORDER_HOVER, alpha ) );
        } else
        {
            gui::draw_icon_at( ICON_EYE, point_t{ avatar.left + 1, avatar.top + 1 },
                               gui::with_alpha( MENU_TEXT_DIM, alpha ) );
        }

        const char *mode_label{ spectator_mode_label( row.m_mode ) };
        char        name[ 64 ]{ };
        const i32   name_room{ well.right - 11 - gui::estimate_tiny_text_width( mode_label ) -
                              ( well.left + 26 ) };
        gui::fit_text( name, row.m_name, name_room );
        g_render.text( MENU_FONT_TINY( ), point_t{ well.left + 26, text_y },
                       gui::with_alpha( CONTROL_VALUE, alpha ), name );

        const color_t mode_color{ gui::mix_color( CONTROL_LABEL, g_theme.m_accent,
                                                  gui::eased_mix( row.m_mode_flash ) ) };
        g_render.text( MENU_FONT_TINY( ), point_t{ well.right - 5, text_y },
                       gui::with_alpha( mode_color, alpha ), mode_label, false,
                       render::text_align::right );
    }

    g_render.reset_clip( );
}
