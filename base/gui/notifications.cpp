#include "render/render.hpp"
#include "runtime/array.hpp"
#include "util/string.hpp"

#include "gui/gui.hpp"
#include "gui/theme.hpp"

#include "gui/notifications.hpp"

constexpr usize TOAST_TITLE_LIMIT{ 128 };
constexpr usize TOAST_BODY_LIMIT{ 512 };

constexpr i32 TOAST_WIDTH{ 224 };
constexpr i32 TOAST_HEIGHT{ 36 };
constexpr i32 TOAST_MARGIN{ 12 };
constexpr i32 TOAST_GAP{ 6 };

constexpr i32 TOAST_BAR{ 2 };
constexpr i32 TOAST_PADDING{ 7 };
constexpr i32 TOAST_ICON_GAP{ 6 };

constexpr i32 TITLE_TOP{ 5 };
constexpr i32 TITLE_HEIGHT{ 13 };
constexpr i32 MESSAGE_TOP{ 18 };
constexpr i32 MESSAGE_HEIGHT{ 12 };
constexpr i32 TIMER_INSET{ 2 };

constexpr usize SLOT_CAPACITY{ 20 };

constexpr u64   TOAST_LIFETIME{ 4000 };
constexpr u64   HEAT_COOL{ 520 };
constexpr u32   ENTER_DURATION{ 150 };
constexpr u32   LEAVE_DURATION{ 110 };
constexpr i32   ENTER_SLIDE{ 14 };
constexpr float ROW_SLIDE_DURATION{ 110.0f };
constexpr float ROW_DROP{ 0.1f };

constexpr color_t TOAST_BACKGROUND{ 23, 24, 27, 244 };
constexpr color_t TOAST_BORDER{ 5, 5, 6 };
constexpr color_t TOAST_TITLE{ 232, 233, 236 };
constexpr color_t TOAST_MESSAGE{ 150, 151, 156 };
constexpr color_t TIMER_TRACK{ 16, 17, 20 };
constexpr color_t FLASH_TINT{ 245, 246, 247 };

constexpr color_t KIND_SUCCESS{ 126, 196, 107 };
constexpr color_t KIND_INFORMATION{ 110, 160, 210 };
constexpr color_t KIND_WARNING{ 226, 170, 74 };
constexpr color_t KIND_ERROR{ 239, 75, 72 };

struct toast_t
{
    // NOTE: Hoku; Unfortunately need to make these like this, otherwise we're pointing to the same char array that was allocated previously on the
    // stack and that's BAD!!!
    atl::string< TOAST_TITLE_LIMIT > m_title{ };
    atl::string< TOAST_BODY_LIMIT >  m_message{ };
    atl::string_view                 m_icon{ }; // This can be a string view since the pointer to the var will never change (we initialize it ourselves as a global).
    notify_kind_t                    m_kind{ };
    u32                              m_sequence{ };
    u64                              m_born{ };
    float                            m_row{ };
    u8                               m_alpha{ };
    bool                             m_active{ };
    bool                             m_used{ };
};

static atl::array< toast_t, SLOT_CAPACITY > s_slots{ };
static u32                                  s_sequence{ };
static u64                                  s_tick{ };

ATLAS_FORCE_INLINE constexpr color_t kind_color( notify_kind_t kind )
{
    switch ( kind )
    {
        case notify_kind_t::success: return KIND_SUCCESS;
        case notify_kind_t::warning: return KIND_WARNING;
        case notify_kind_t::error: return KIND_ERROR;
        default: return KIND_INFORMATION;
    }
}

ATLAS_FORCE_INLINE constexpr const char *kind_icon( notify_kind_t kind )
{
    switch ( kind )
    {
        case notify_kind_t::success: return ICON_SPARKLES;
        case notify_kind_t::warning: return ICON_ZAP;
        case notify_kind_t::error: return ICON_CROSS;
        default: return ICON_SETTINGS;
    }
}

inline void draw_toast( const toast_t &slot, i32 screen_width, i32 screen_height, u64 now )
{
    const u8 show{ gui::eased_out( slot.m_alpha ) };

    if ( !show )
    {
        return;
    }

    const i32 bar{ g_gui.scaled( TOAST_BAR ) };
    const i32 padding{ g_gui.scaled( TOAST_PADDING ) };
    const i32 text_inset{ 1 + bar + padding + g_gui.scaled( MENU_ICON_SIZE + TOAST_ICON_GAP ) };

    const i32 title_width{ g_render.get_text_width( MENU_FONT( ), slot.m_title ) };
    const i32 body_width{ g_render.get_text_width( MENU_FONT_TINY( ), slot.m_message ) };
    const i32 input_width{ math::bigger( title_width, body_width ) };
    const i32 width{ math::bigger( g_gui.scaled( TOAST_WIDTH ), text_inset + input_width + padding ) };
    const i32 height{ g_gui.scaled( TOAST_HEIGHT ) };
    const i32 margin{ g_gui.scaled( TOAST_MARGIN ) };
    const i32 step{ height + g_gui.scaled( TOAST_GAP ) };

    const i32 slide{ g_gui.scaled( ENTER_SLIDE ) * ( 255 - show ) / 255 };
    const i32 left{ screen_width - margin - width + slide };
    const i32 top{ margin + static_cast< i32 >( slot.m_row * static_cast< f32 >( step ) + 0.5f ) };

    if ( top + height > screen_height - margin )
    {
        return;
    }

    const u64 age{ now >= slot.m_born ? now - slot.m_born : 0 };

    float remaining{ };

    if ( slot.m_active && age < TOAST_LIFETIME )
    {
        remaining = 1.0f - static_cast< float >( age ) /
                               static_cast< float >( TOAST_LIFETIME );
    }

    u8 heat{ };

    if ( slot.m_active && age < HEAT_COOL )
    {
        heat = gui::eased_mix( static_cast< u8 >( 255u - age * 255u / HEAT_COOL ) );
    }

    const color_t accent{ kind_color( slot.m_kind ) };
    const color_t lit{ gui::mix_color( accent, FLASH_TINT,
                                       static_cast< u8 >( heat / 3u ) ) };

    const i32 right{ left + width };
    const i32 bottom{ top + height };

    g_render.box( rect_t{ left, top, right, bottom }, gui::with_alpha( TOAST_BACKGROUND, show ), true );
    g_render.box( rect_t{ left + 1, top + 1, left + 1 + bar, bottom - 1 }, gui::with_alpha( lit, show ), true );
    g_render.box( rect_t{ left, top, right - 1, bottom - 1 }, gui::with_alpha( TOAST_BORDER, show ) );

    const i32 content_left{ left + 1 + bar + padding };
    const i32 icon_top{ top + ( height - g_gui.scaled( MENU_ICON_SIZE ) ) / 2 };

    gui::draw_icon_at( slot.m_icon.data( ), point_t{ content_left, icon_top }, gui::with_alpha( lit, show ), MENU_ICON_SIZE );

    const i32 text_left{ left + text_inset };

    g_render.text( MENU_FONT( ),
                   point_t{ text_left, g_gui.text_center_y( top + g_gui.scaled( TITLE_TOP ), g_gui.scaled( TITLE_HEIGHT ) ) },
                   gui::with_alpha( TOAST_TITLE, show ), slot.m_title );

    g_render.text( MENU_FONT_TINY( ),
                   point_t{ text_left, g_gui.tiny_center_y( top + g_gui.scaled( MESSAGE_TOP ), g_gui.scaled( MESSAGE_HEIGHT ) ) },
                   gui::with_alpha( TOAST_MESSAGE, show ), slot.m_message );

    const i32 timer_left{ left + 1 + bar };
    const i32 timer_right{ right - 1 };
    const i32 timer_top{ bottom - g_gui.scaled( TIMER_INSET ) };
    const i32 timer_span{ timer_right - timer_left };

    if ( timer_span <= 0 )
    {
        return;
    }

    g_render.box( rect_t{ timer_left, timer_top, timer_right, timer_top + 1 },
                  gui::with_alpha( TIMER_TRACK, show ), true );

    const i32 filled{ static_cast< i32 >( static_cast< float >( timer_span ) * remaining +
                                          0.5f ) };

    if ( filled > 0 )
    {
        g_render.box( rect_t{ timer_left, timer_top, timer_left + filled, timer_top + 1 },
                      gui::with_alpha( accent, show ), true );
    }
}

void push_notification( notify_kind_t kind, const char *icon, const char *title,
                        const char *message )
{
    if ( !title || !message )
    {
        return;
    }

    const u64 now{ gui::clock( ) };
    toast_t  *chosen{ };

    for ( toast_t &slot : s_slots )
    {
        if ( !slot.m_used )
        {
            chosen = &slot;
            break;
        }
    }

    if ( !chosen )
    {
        for ( toast_t &slot : s_slots )
        {
            if ( !chosen || slot.m_sequence < chosen->m_sequence )
            {
                chosen = &slot;
            }
        }
    }

    *chosen            = toast_t{ };
    chosen->m_icon     = icon && icon[ 0 ] ? icon : kind_icon( kind );
    chosen->m_kind     = kind;
    chosen->m_sequence = ++s_sequence;
    chosen->m_born     = now;
    chosen->m_row      = -ROW_DROP;
    chosen->m_active   = true;
    chosen->m_used     = true;

    chosen->m_title.assign( title );
    chosen->m_message.assign( message );

    switch ( kind )
    {
        case notify_kind_t::success: gui::play_sound( gui::SOUND_SUCCESS ); break;
        case notify_kind_t::information: gui::play_sound( gui::SOUND_NOTIFY ); break;
        case notify_kind_t::warning: gui::play_sound( gui::SOUND_WARN ); break;
        case notify_kind_t::error: break;
    }
}

void push_notification( notify_kind_t kind, const char *title, const char *message )
{
    push_notification( kind, nullptr, title, message );
}

void draw_notifications( i32 screen_width, i32 screen_height )
{
    const u64 now{ gui::clock( ) };
    const u32 delta{ gui::animation_delta( s_tick ) };

    bool any{ };

    for ( toast_t &slot : s_slots )
    {
        if ( !slot.m_used )
        {
            continue;
        }

        if ( slot.m_active && now - slot.m_born >= TOAST_LIFETIME )
        {
            slot.m_active = false;
        }

        gui::step_mix( slot.m_alpha, slot.m_active, delta,
                       slot.m_active ? ENTER_DURATION : LEAVE_DURATION );

        if ( !slot.m_active && !slot.m_alpha )
        {
            slot = toast_t{ };
            continue;
        }

        any = true;
    }

    if ( !any )
    {
        return;
    }

    for ( toast_t &slot : s_slots )
    {
        if ( !slot.m_used || !slot.m_active )
        {
            continue;
        }

        i32 rank{ };

        for ( const toast_t &other : s_slots )
        {
            if ( other.m_used && other.m_active && other.m_sequence > slot.m_sequence )
            {
                ++rank;
            }
        }

        slot.m_row = gui::approach_float( slot.m_row, static_cast< float >( rank ), delta,
                                          ROW_SLIDE_DURATION );
    }

    for ( const toast_t &slot : s_slots )
    {
        if ( slot.m_used && !slot.m_active )
        {
            draw_toast( slot, screen_width, screen_height, now );
        }
    }

    for ( const toast_t &slot : s_slots )
    {
        if ( slot.m_used && slot.m_active )
        {
            draw_toast( slot, screen_width, screen_height, now );
        }
    }
}
