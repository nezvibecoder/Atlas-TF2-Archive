#pragma once

#include "runtime/array.hpp"
#include "runtime/mutex.hpp"
#include "runtime/vector.hpp"
#include "util/pe_module.hpp"
#include "util/string.hpp"

inline constexpr u32   HTML_PANEL_CAPACITY{ 8 };
inline constexpr u32   HTML_PANEL_MAX_SIDE{ 2048 };
inline constexpr usize HTML_MESSAGE_CAPACITY{ 32 };
inline constexpr usize HTML_MESSAGE_LENGTH{ 511 };

struct html_handle_t
{
    u32 m_slot{ HTML_PANEL_CAPACITY };
    u32 m_generation{ };
};

struct html_frame_t
{
    const u8 *m_pixels{ };
    u32       m_width{ };
    u32       m_height{ };
    u32       m_generation{ };
    rect_t    m_dirty{ };
};

struct html_callback_t;

struct html_callback_vtable_t
{
    void( ATLAS_THISCALL *m_run_result )( html_callback_t *self, void *param, bool io_failure, u64 call );
    void( ATLAS_THISCALL *m_run )( html_callback_t *self, void *param );
    i32( ATLAS_THISCALL *m_size )( html_callback_t *self );
};

struct html_callback_t
{
    const html_callback_vtable_t *m_vtable{ };
    u8                            m_flags{ };
    i32                           m_id{ };
};

class steam_html
{
public:
    void init( const util::pe_module &steam_api );

    bool ready( ) const
    {
        return this->m_surface != nullptr;
    }

    html_handle_t open( u32 width, u32 height, string_view source, bool web );
    bool          load( html_handle_t handle, string_view source );
    bool          send( html_handle_t handle, string_view text );
    bool          receive( html_handle_t handle, fixed_string< HTML_MESSAGE_LENGTH > &message );
    void          close( html_handle_t handle );

    void forward_input( u32 slot, rect_t bounds );
    bool has_focus( ) const;

    template< typename visitor_t >
    bool visit_frame( u32 slot, visitor_t &&visitor )
    {
        atl::scoped_lock lock{ this->m_lock };

        if ( slot >= HTML_PANEL_CAPACITY )
        {
            return false;
        }

        panel_t &panel{ this->m_panels[ slot ] };

        if ( !panel.m_used || !panel.m_painted )
        {
            return false;
        }

        const html_frame_t frame{ panel.m_pixels.data( ), panel.m_width, panel.m_height, panel.m_generation, panel.m_dirty };

        if ( !visitor( frame ) )
        {
            return false;
        }

        panel.m_dirty = rect_t{ };
        return true;
    }

    void on_callback( i32 id, void *param );
    void on_result( void *param, bool io_failure, u64 call );

private:
    enum class panel_state_t : u8
    {
        free,
        waiting,
        creating,
        open,
        closing,
        failed,
        _COUNT,
    };

    struct panel_t
    {
        panel_state_t                                                            m_state{ panel_state_t::free };
        bool                                                                     m_used{ };
        bool                                                                     m_painted{ };
        bool                                                                     m_web{ };
        u32                                                                      m_browser{ };
        u32                                                                      m_generation{ };
        u32                                                                      m_width{ };
        u32                                                                      m_height{ };
        atl::vector< char >                                                      m_url{ };
        atl::vector< u8 >                                                        m_pixels{ };
        rect_t                                                                   m_dirty{ };
        point_t                                                                  m_mouse{ -1, -1 };
        u8                                                                       m_held{ };
        atl::array< fixed_string< HTML_MESSAGE_LENGTH >, HTML_MESSAGE_CAPACITY > m_messages{ };
        usize                                                                    m_message_head{ };
        usize                                                                    m_message_count{ };
    };

    using init_fn         = bool( ATLAS_CDECL * )( void *surface );
    using create_fn       = u64( ATLAS_CDECL * )( void *surface, const char *user_agent, const char *css );
    using browser_fn      = void( ATLAS_CDECL * )( void *surface, u32 browser );
    using load_url_fn     = void( ATLAS_CDECL * )( void *surface, u32 browser, const char *url, const char *post_data );
    using set_size_fn     = void( ATLAS_CDECL * )( void *surface, u32 browser, u32 width, u32 height );
    using mouse_move_fn   = void( ATLAS_CDECL * )( void *surface, u32 browser, i32 x, i32 y );
    using mouse_button_fn = void( ATLAS_CDECL * )( void *surface, u32 browser, i32 button );
    using mouse_wheel_fn  = void( ATLAS_CDECL * )( void *surface, u32 browser, i32 delta );
    using key_down_fn     = void( ATLAS_CDECL * )( void *surface, u32 browser, u32 key, i32 modifiers, bool system_key );
    using key_fn          = void( ATLAS_CDECL * )( void *surface, u32 browser, u32 key, i32 modifiers );
    using flag_fn         = void( ATLAS_CDECL * )( void *surface, u32 browser, bool value );
    using script_fn       = void( ATLAS_CDECL * )( void *surface, u32 browser, const char *script );
    using file_dialog_fn  = void( ATLAS_CDECL * )( void *surface, u32 browser, const char **files );
    using register_fn     = void( ATLAS_CDECL * )( html_callback_t *callback, i32 id );
    using call_result_fn  = void( ATLAS_CDECL * )( html_callback_t *callback, u64 call );

    bool     start( );
    panel_t *owner( u32 browser );
    panel_t *find( html_handle_t handle );
    bool     build_url( string_view source, atl::vector< char > &url ) const;
    bool     set_document( panel_t &panel, string_view source ) const;
    void     create_next( );
    void     push_message( panel_t &panel, const char *message );

    void   *m_surface{ };
    init_fn m_init{ };
    bool    m_started{ };

    create_fn       m_create_browser{ };
    browser_fn      m_remove_browser{ };
    load_url_fn     m_load_url{ };
    set_size_fn     m_set_size{ };
    mouse_move_fn   m_mouse_move{ };
    mouse_button_fn m_mouse_down{ };
    mouse_button_fn m_mouse_up{ };
    mouse_wheel_fn  m_mouse_wheel{ };
    key_down_fn     m_key_down{ };
    key_fn          m_key_up{ };
    key_fn          m_key_char{ };
    flag_fn         m_set_key_focus{ };
    script_fn       m_execute_javascript{ };
    flag_fn         m_allow_start_request{ };
    flag_fn         m_js_dialog_response{ };
    file_dialog_fn  m_file_load_dialog_response{ };
    register_fn     m_register_callback{ };
    call_result_fn  m_register_call_result{ };

    atl::array< html_callback_t, 5 > m_callbacks{ };
    html_callback_t                  m_create_result{ };
    u64                              m_create_call{ };
    u32                              m_creating{ HTML_PANEL_CAPACITY };

    mutable atl::mutex                         m_lock{ };
    atl::array< panel_t, HTML_PANEL_CAPACITY > m_panels{ };
    u32                                        m_focus{ HTML_PANEL_CAPACITY };
};

extern steam_html g_html;
