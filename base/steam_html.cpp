#include "steam_html.hpp"

#include "gui/gui.hpp"
#include "input/input.hpp"
#include "math/math.hpp"
#include "runtime/obfuscation.hpp"
#include "script/script.hpp"
#include "util/base64.hpp"
#include "util/windefs.hpp"

ATLAS_NO_DESTROY steam_html g_html{ };

#if defined( ATLAS_DEV )
#define HTML_TRACE( text ) g_scripts.log( script::log_kind_t::info, ATLAS_XORSTR( text ).get( ) )
#else
#define HTML_TRACE( text ) ( ( void )0 )
#endif

inline constexpr i32   HTML_BROWSER_READY{ 4501 };
inline constexpr i32   HTML_NEEDS_PAINT{ 4502 };
inline constexpr i32   HTML_START_REQUEST{ 4503 };
inline constexpr i32   HTML_JS_ALERT{ 4514 };
inline constexpr i32   HTML_JS_CONFIRM{ 4515 };
inline constexpr i32   HTML_FILE_OPEN_DIALOG{ 4516 };
inline constexpr usize HTML_SOURCE_LIMIT{ 1024u * 1024u };

inline constexpr i32 HTML_MODIFIER_ALT{ 1 << 0 };
inline constexpr i32 HTML_MODIFIER_CTRL{ 1 << 1 };
inline constexpr i32 HTML_MODIFIER_SHIFT{ 1 << 2 };

struct html_browser_ready_t
{
    u32 m_browser{ };
};

struct html_needs_paint_t
{
    u32       m_browser{ };
    const u8 *m_bgra{ };
    u32       m_wide{ };
    u32       m_tall{ };
    u32       m_update_x{ };
    u32       m_update_y{ };
    u32       m_update_wide{ };
    u32       m_update_tall{ };
    u32       m_scroll_x{ };
    u32       m_scroll_y{ };
    f32       m_page_scale{ };
    u32       m_page_serial{ };
};

struct html_start_request_t
{
    u32         m_browser{ };
    const char *m_url{ };
    const char *m_target{ };
    const char *m_post_data{ };
    bool        m_is_redirect{ };
};

struct html_js_dialog_t
{
    u32         m_browser{ };
    const char *m_message{ };
};

struct html_file_open_dialog_t
{
    u32         m_browser{ };
    const char *m_title{ };
    const char *m_initial_file{ };
};

static void ATLAS_THISCALL run_result( html_callback_t *, void *param, bool io_failure, u64 call )
{
    g_html.on_result( param, io_failure, call );
}

static void ATLAS_THISCALL run_callback( html_callback_t *self, void *param )
{
    g_html.on_callback( self->m_id, param );
}

static i32 ATLAS_THISCALL callback_size( html_callback_t *self )
{
    switch ( self->m_id )
    {
        case HTML_BROWSER_READY: return sizeof( html_browser_ready_t );
        case HTML_NEEDS_PAINT: return sizeof( html_needs_paint_t );
        case HTML_START_REQUEST: return sizeof( html_start_request_t );
        case HTML_FILE_OPEN_DIALOG: return sizeof( html_file_open_dialog_t );
        default: return sizeof( html_js_dialog_t );
    }
}

static constexpr html_callback_vtable_t HTML_CALLBACK_VTABLE{ &run_result, &run_callback, &callback_size };

ATLAS_FORCE_INLINE bool is_web_url( const char *url, usize size )
{
    auto http{ ATLAS_XORSTR( "http://" ) };
    auto https{ ATLAS_XORSTR( "https://" ) };
    return str::starts_with( url, size, http.get( ), 7 ) || str::starts_with( url, size, https.get( ), 8 );
}

ATLAS_FORCE_INLINE bool url_allowed( const char *url, bool web )
{
    const usize size{ str::length( url ) };

    auto data{ ATLAS_XORSTR( "data:" ) };
    auto about{ ATLAS_XORSTR( "about:" ) };
    return str::starts_with( url, size, data.get( ), 5 ) || str::starts_with( url, size, about.get( ), 6 ) ||
           ( web && is_web_url( url, size ) );
}

ATLAS_FORCE_INLINE usize head_offset( string_view source )
{
    usize at{ };

    while ( at < source.size( ) && ( source[ at ] == ' ' || source[ at ] == '\t' || source[ at ] == '\r' || source[ at ] == '\n' ) )
    {
        ++at;
    }

    auto doctype{ ATLAS_XORSTR( "<!doctype" ) };
    if ( !str::equals_ignore_case( source.data( ) + at, source.size( ) - at < 9 ? source.size( ) - at : 9, doctype.get( ), 9 ) )
    {
        return 0;
    }

    const usize close{ str::find( source.data( ), source.size( ), '>', at ) };
    return close == str::npos ? 0 : close + 1;
}

void steam_html::init( const util::pe_module &steam_api )
{
    const auto bind{ [ & ]< typename function_t >( const char *name, function_t &target )
    {
        util::address address{ steam_api.find_export( name ) };
        target = address.is_null( ) ? nullptr : address.get_as< function_t >( );
        return target != nullptr;
    } };

    using accessor_fn = void *( ATLAS_CDECL * )( );
    accessor_fn accessor{ };

    if ( !bind( ATLAS_XORSTR( "SteamAPI_SteamHTMLSurface_v005" ).get( ), accessor ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_Init" ).get( ), this->m_init ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_CreateBrowser" ).get( ), this->m_create_browser ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_RemoveBrowser" ).get( ), this->m_remove_browser ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_LoadURL" ).get( ), this->m_load_url ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_SetSize" ).get( ), this->m_set_size ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_MouseMove" ).get( ), this->m_mouse_move ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_MouseDown" ).get( ), this->m_mouse_down ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_MouseUp" ).get( ), this->m_mouse_up ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_MouseWheel" ).get( ), this->m_mouse_wheel ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_KeyDown" ).get( ), this->m_key_down ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_KeyUp" ).get( ), this->m_key_up ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_KeyChar" ).get( ), this->m_key_char ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_SetKeyFocus" ).get( ), this->m_set_key_focus ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_ExecuteJavascript" ).get( ), this->m_execute_javascript ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_AllowStartRequest" ).get( ), this->m_allow_start_request ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_JSDialogResponse" ).get( ), this->m_js_dialog_response ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse" ).get( ), this->m_file_load_dialog_response ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_RegisterCallback" ).get( ), this->m_register_callback ) ||
         !bind( ATLAS_XORSTR( "SteamAPI_RegisterCallResult" ).get( ), this->m_register_call_result ) )
    {
        return;
    }

    this->m_surface = accessor( );
}

bool steam_html::start( )
{
    if ( this->m_started )
    {
        return true;
    }

    if ( !this->m_init( this->m_surface ) )
    {
        HTML_TRACE( "html: ISteamHTMLSurface::Init failed" );
        return false;
    }

    static constexpr i32 IDS[]{ HTML_NEEDS_PAINT, HTML_START_REQUEST, HTML_JS_ALERT, HTML_JS_CONFIRM, HTML_FILE_OPEN_DIALOG };
    static_assert( sizeof( IDS ) / sizeof( IDS[ 0 ] ) == decltype( m_callbacks )::static_size );

    for ( usize index{ }; index < this->m_callbacks.size( ); ++index )
    {
        this->m_callbacks[ index ].m_vtable = &HTML_CALLBACK_VTABLE;
        this->m_register_callback( &this->m_callbacks[ index ], IDS[ index ] );
    }

    this->m_create_result.m_vtable = &HTML_CALLBACK_VTABLE;
    this->m_create_result.m_id     = HTML_BROWSER_READY;
    this->m_started                = true;
    return true;
}

steam_html::panel_t *steam_html::owner( u32 browser )
{
    for ( panel_t &panel : this->m_panels )
    {
        if ( panel.m_used && panel.m_state == panel_state_t::open && panel.m_browser == browser )
        {
            return &panel;
        }
    }

    return nullptr;
}

steam_html::panel_t *steam_html::find( html_handle_t handle )
{
    if ( handle.m_slot >= HTML_PANEL_CAPACITY )
    {
        return nullptr;
    }

    panel_t &panel{ this->m_panels[ handle.m_slot ] };
    return panel.m_used && panel.m_generation == handle.m_generation ? &panel : nullptr;
}

bool steam_html::build_url( string_view source, atl::vector< char > &url ) const
{
    if ( source.size( ) > HTML_SOURCE_LIMIT )
    {
        return false;
    }

    auto              bridge{ ATLAS_XORSTR( "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; "
                                            "script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src data:; "
                                            "font-src data:; media-src data:\"><script>window.atlas={send:function(m)"
                                            "{alert(String(m))},receive:null};</script>" ) };
    auto              scheme{ ATLAS_XORSTR( "data:text/html;charset=utf-8;base64," ) };
    const string_view prefix{ scheme.get( ) };
    const string_view shim{ bridge.get( ) };
    const usize       split{ head_offset( source ) };

    atl::vector< u8 > document{ };
    if ( !document.resize( source.size( ) + shim.size( ) ) )
    {
        return false;
    }

    atl::memcpy( document.data( ), source.data( ), split );
    atl::memcpy( document.data( ) + split, shim.data( ), shim.size( ) );
    atl::memcpy( document.data( ) + split + shim.size( ), source.data( ) + split, source.size( ) - split );

    url.clear( );
    return url.assign( prefix.data( ), prefix.size( ) ) && base64::append( document.data( ), document.size( ), url ) &&
           url.push_back( '\0' );
}

void steam_html::create_next( )
{
    if ( this->m_create_call )
    {
        return;
    }

    for ( u32 slot{ }; slot < HTML_PANEL_CAPACITY; ++slot )
    {
        panel_t &panel{ this->m_panels[ slot ] };

        if ( !panel.m_used || panel.m_state != panel_state_t::waiting )
        {
            continue;
        }

        const u64 call{ this->m_create_browser( this->m_surface, nullptr, nullptr ) };

        if ( !call )
        {
            HTML_TRACE( "html: CreateBrowser was refused" );
            panel.m_state = panel_state_t::failed;
            continue;
        }

        this->m_create_call = call;
        this->m_creating    = slot;
        panel.m_state       = panel_state_t::creating;
        this->m_register_call_result( &this->m_create_result, call );
        return;
    }
}

void steam_html::push_message( panel_t &panel, const char *message )
{
    if ( panel.m_message_count == HTML_MESSAGE_CAPACITY )
    {
        panel.m_message_head = ( panel.m_message_head + 1 ) % HTML_MESSAGE_CAPACITY;
        --panel.m_message_count;
    }

    fixed_string< HTML_MESSAGE_LENGTH > &slot{ panel.m_messages[ ( panel.m_message_head + panel.m_message_count ) % HTML_MESSAGE_CAPACITY ] };
    const usize                          size{ message ? str::length( message ) : 0 };

    slot.assign( string_view{ message, size < HTML_MESSAGE_LENGTH ? size : HTML_MESSAGE_LENGTH } );
    ++panel.m_message_count;
}

bool steam_html::set_document( panel_t &panel, string_view source ) const
{
    if ( !panel.m_web )
    {
        return this->build_url( source, panel.m_url );
    }

    if ( source.size( ) > HTML_SOURCE_LIMIT || !is_web_url( source.data( ), source.size( ) ) )
    {
        return false;
    }

    panel.m_url.clear( );
    return panel.m_url.assign( source.data( ), source.size( ) ) && panel.m_url.push_back( '\0' );
}

html_handle_t steam_html::open( u32 width, u32 height, string_view source, bool web )
{
    atl::scoped_lock lock{ this->m_lock };

    if ( !this->start( ) )
    {
        return html_handle_t{ };
    }

    for ( u32 slot{ }; slot < HTML_PANEL_CAPACITY; ++slot )
    {
        panel_t &panel{ this->m_panels[ slot ] };

        if ( panel.m_used )
        {
            continue;
        }

        panel.m_web = web;

        if ( !panel.m_pixels.resize( static_cast< usize >( width ) * height * 4 ) || !this->set_document( panel, source ) )
        {
            return html_handle_t{ };
        }

        atl::memset( panel.m_pixels.data( ), 0, panel.m_pixels.size( ) );

        panel.m_used          = true;
        panel.m_state         = panel_state_t::waiting;
        panel.m_painted       = false;
        panel.m_browser       = 0;
        panel.m_width         = width;
        panel.m_height        = height;
        panel.m_dirty         = rect_t{ };
        panel.m_mouse         = point_t{ -1, -1 };
        panel.m_held          = 0;
        panel.m_message_head  = 0;
        panel.m_message_count = 0;
        ++panel.m_generation;

        this->create_next( );
        return html_handle_t{ slot, panel.m_generation };
    }

    return html_handle_t{ };
}

bool steam_html::load( html_handle_t handle, string_view source )
{
    atl::scoped_lock lock{ this->m_lock };
    panel_t         *panel{ this->find( handle ) };

    if ( !panel || !this->set_document( *panel, source ) )
    {
        return false;
    }

    if ( panel->m_state == panel_state_t::open )
    {
        this->m_load_url( this->m_surface, panel->m_browser, panel->m_url.data( ), nullptr );
    }

    return true;
}

bool steam_html::send( html_handle_t handle, string_view text )
{
    atl::scoped_lock lock{ this->m_lock };
    panel_t         *panel{ this->find( handle ) };

    if ( !panel || panel->m_state != panel_state_t::open || text.size( ) > HTML_SOURCE_LIMIT )
    {
        return false;
    }

    auto              opening{ ATLAS_XORSTR( "window.atlas&&window.atlas.receive&&window.atlas.receive(new TextDecoder().decode(Uint8Array.from(atob('" ) };
    auto              closing{ ATLAS_XORSTR( "'),function(c){return c.charCodeAt(0)})))" ) };
    const string_view head{ opening.get( ) };
    const string_view tail{ closing.get( ) };

    atl::vector< char > script{ };
    if ( !script.assign( head.data( ), head.size( ) ) ||
         !base64::append( reinterpret_cast< const u8 * >( text.data( ) ), text.size( ), script ) ||
         !script.resize( script.size( ) + tail.size( ) + 1 ) )
    {
        return false;
    }

    atl::memcpy( script.data( ) + script.size( ) - tail.size( ) - 1, tail.data( ), tail.size( ) );
    script[ script.size( ) - 1 ] = '\0';

    this->m_execute_javascript( this->m_surface, panel->m_browser, script.data( ) );
    return true;
}

bool steam_html::receive( html_handle_t handle, fixed_string< HTML_MESSAGE_LENGTH > &message )
{
    atl::scoped_lock lock{ this->m_lock };
    panel_t         *panel{ this->find( handle ) };

    if ( !panel || !panel->m_message_count )
    {
        return false;
    }

    message               = panel->m_messages[ panel->m_message_head ];
    panel->m_message_head = ( panel->m_message_head + 1 ) % HTML_MESSAGE_CAPACITY;
    --panel->m_message_count;
    return true;
}

void steam_html::close( html_handle_t handle )
{
    atl::scoped_lock lock{ this->m_lock };
    panel_t         *panel{ this->find( handle ) };

    if ( !panel )
    {
        return;
    }

    if ( this->m_focus == handle.m_slot )
    {
        this->m_focus = HTML_PANEL_CAPACITY;
    }

    panel->m_used = false;
    panel->m_url.reset( );
    panel->m_pixels.reset( );

    switch ( panel->m_state )
    {
        case panel_state_t::open: this->m_remove_browser( this->m_surface, panel->m_browser ); break;
        case panel_state_t::creating: panel->m_state = panel_state_t::closing; return;
        default: break;
    }

    panel->m_state = panel_state_t::free;
}

void steam_html::on_result( void *param, bool io_failure, u64 call )
{
    atl::scoped_lock lock{ this->m_lock };

    if ( call != this->m_create_call || this->m_creating >= HTML_PANEL_CAPACITY )
    {
        return;
    }

    panel_t  &panel{ this->m_panels[ this->m_creating ] };
    const u32 browser{ io_failure ? 0u : static_cast< const html_browser_ready_t * >( param )->m_browser };

    this->m_create_call = 0;
    this->m_creating    = HTML_PANEL_CAPACITY;

    if ( panel.m_state == panel_state_t::closing )
    {
        if ( browser )
        {
            this->m_remove_browser( this->m_surface, browser );
        }

        panel.m_state = panel_state_t::free;
    } else if ( !browser )
    {
        HTML_TRACE( "html: browser creation failed" );
        panel.m_state = panel_state_t::failed;
    } else
    {
        HTML_TRACE( "html: browser ready" );
        panel.m_browser = browser;
        panel.m_state   = panel_state_t::open;
        this->m_set_size( this->m_surface, browser, panel.m_width, panel.m_height );
        this->m_load_url( this->m_surface, browser, panel.m_url.data( ), nullptr );
    }

    this->create_next( );
}

void steam_html::on_callback( i32 id, void *param )
{
    atl::scoped_lock lock{ this->m_lock };
    panel_t         *panel{ this->owner( *static_cast< const u32 * >( param ) ) };

    if ( !panel )
    {
        return;
    }

    switch ( id )
    {
        case HTML_NEEDS_PAINT:
        {
            const html_needs_paint_t &paint{ *static_cast< const html_needs_paint_t * >( param ) };

            if ( paint.m_wide != panel->m_width || paint.m_tall != panel->m_height || !paint.m_bgra )
            {
                return;
            }

            const u32   left{ paint.m_update_wide ? paint.m_update_x : 0 };
            const u32   top{ paint.m_update_tall ? paint.m_update_y : 0 };
            const u32   right{ paint.m_update_wide && paint.m_update_x + paint.m_update_wide < paint.m_wide ? paint.m_update_x + paint.m_update_wide : paint.m_wide };
            const u32   bottom{ paint.m_update_tall && paint.m_update_y + paint.m_update_tall < paint.m_tall ? paint.m_update_y + paint.m_update_tall : paint.m_tall };
            const usize stride{ static_cast< usize >( paint.m_wide ) * 4 };

            for ( u32 y{ top }; y < bottom; ++y )
            {
                const usize offset{ y * stride + static_cast< usize >( left ) * 4 };
                atl::memcpy( panel->m_pixels.data( ) + offset, paint.m_bgra + offset, static_cast< usize >( right - left ) * 4 );
            }

            rect_t &dirty{ panel->m_dirty };
            if ( dirty.right <= dirty.left || dirty.bottom <= dirty.top )
            {
                dirty = rect_t{ static_cast< i32 >( left ), static_cast< i32 >( top ), static_cast< i32 >( right ), static_cast< i32 >( bottom ) };
            } else
            {
                dirty.left   = math::smaller( dirty.left, static_cast< i32 >( left ) );
                dirty.top    = math::smaller( dirty.top, static_cast< i32 >( top ) );
                dirty.right  = math::bigger( dirty.right, static_cast< i32 >( right ) );
                dirty.bottom = math::bigger( dirty.bottom, static_cast< i32 >( bottom ) );
            }

            if ( !panel->m_painted )
            {
                HTML_TRACE( "html: first paint" );
            }

            panel->m_painted = true;
            break;
        }

        case HTML_START_REQUEST:
        {
            const html_start_request_t &request{ *static_cast< const html_start_request_t * >( param ) };
            const bool                  allowed{ request.m_url && url_allowed( request.m_url, panel->m_web ) };

            if ( !allowed )
            {
                HTML_TRACE( "html: blocked a navigation off the panel's document" );
            }

            this->m_allow_start_request( this->m_surface, panel->m_browser, allowed );
            break;
        }

        case HTML_JS_ALERT:
        {
            this->push_message( *panel, static_cast< const html_js_dialog_t * >( param )->m_message );
            this->m_js_dialog_response( this->m_surface, panel->m_browser, true );
            break;
        }

        case HTML_JS_CONFIRM: this->m_js_dialog_response( this->m_surface, panel->m_browser, false ); break;
        case HTML_FILE_OPEN_DIALOG: this->m_file_load_dialog_response( this->m_surface, panel->m_browser, nullptr ); break;
        default: break;
    }
}

bool steam_html::has_focus( ) const
{
    return this->m_focus < HTML_PANEL_CAPACITY && g_gui.open( );
}

void steam_html::forward_input( u32 slot, rect_t bounds )
{
    if ( !g_gui.open( ) )
    {
        this->m_focus = HTML_PANEL_CAPACITY;
        return;
    }

    u32     browser{ };
    point_t last{ };
    u8      held{ };

    {
        atl::scoped_lock lock{ this->m_lock };
        const panel_t   &panel{ this->m_panels[ slot ] };

        if ( !panel.m_used || panel.m_state != panel_state_t::open )
        {
            return;
        }

        browser = panel.m_browser;
        last    = panel.m_mouse;
        held    = panel.m_held;
    }

    const point_t mouse{ g_input.mouse( ) };
    const point_t local{ mouse.x - bounds.left, mouse.y - bounds.top };
    const bool    inside{ mouse.x >= bounds.left && mouse.x < bounds.right && mouse.y >= bounds.top && mouse.y < bounds.bottom };

    if ( ( inside || held ) && ( local.x != last.x || local.y != last.y ) )
    {
        this->m_mouse_move( this->m_surface, browser, local.x, local.y );
        last = local;
    }

    static constexpr u32 BUTTONS[]{ VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };

    for ( i32 button{ }; button < 3; ++button )
    {
        const u8 bit{ static_cast< u8 >( 1u << button ) };

        if ( g_input.pressed( BUTTONS[ button ] ) )
        {
            if ( inside )
            {
                this->m_mouse_down( this->m_surface, browser, button );
                held |= bit;

                if ( !button && this->m_focus != slot )
                {
                    this->m_focus = slot;
                    this->m_set_key_focus( this->m_surface, browser, true );
                }
            } else if ( !button && this->m_focus == slot )
            {
                this->m_focus = HTML_PANEL_CAPACITY;
                this->m_set_key_focus( this->m_surface, browser, false );
            }
        }

        if ( ( held & bit ) && !g_input.down( BUTTONS[ button ] ) )
        {
            this->m_mouse_up( this->m_surface, browser, button );
            held &= static_cast< u8 >( ~bit );
        }
    }

    if ( inside && g_input.wheel( ) )
    {
        this->m_mouse_wheel( this->m_surface, browser, g_input.wheel( ) * WHEEL_DELTA );
    }

    if ( this->m_focus == slot )
    {
        const i32 modifiers{ ( g_input.down( VK_MENU ) ? HTML_MODIFIER_ALT : 0 ) | ( g_input.down( VK_CONTROL ) ? HTML_MODIFIER_CTRL : 0 ) |
                             ( g_input.down( VK_SHIFT ) ? HTML_MODIFIER_SHIFT : 0 ) };

        for ( u32 key{ VK_BACK }; key < 0xFF; ++key )
        {
            if ( g_input.repeated( key ) )
            {
                this->m_key_down( this->m_surface, browser, key, modifiers, false );
            }

            if ( g_input.released( key ) )
            {
                this->m_key_up( this->m_surface, browser, key, modifiers );
            }
        }

        for ( usize index{ }; index < g_input.typed_count( ); ++index )
        {
            this->m_key_char( this->m_surface, browser, static_cast< u8 >( g_input.typed( index ) ), modifiers );
        }
    }

    atl::scoped_lock lock{ this->m_lock };
    panel_t         &panel{ this->m_panels[ slot ] };

    if ( panel.m_used && panel.m_browser == browser )
    {
        panel.m_mouse = last;
        panel.m_held  = held;
    }
}
