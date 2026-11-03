#include "input.hpp"

#include "libs/lazy_importer.hpp"
#include "render/d3d9.hpp"
#include "util/windefs.hpp"

#include "gui/gui.hpp"
#include "steam_html.hpp"

ATLAS_NO_DESTROY input_handler g_input{ };

ATLAS_FORCE_INLINE constexpr i16 low_word( i64 value )
{
    return static_cast< i16 >( static_cast< u16 >( value & 0xFFFF ) );
}

ATLAS_FORCE_INLINE constexpr i16 high_word( i64 value )
{
    return static_cast< i16 >( static_cast< u16 >( ( value >> 16 ) & 0xFFFF ) );
}

ATLAS_FORCE_INLINE constexpr u32 xbutton_key( usize wparam )
{
    u32 button{ static_cast< u32 >( high_word( static_cast< i64 >( wparam ) ) ) };

    if ( button == XBUTTON1 )
    {
        return VK_XBUTTON1;
    }

    if ( button == XBUTTON2 )
    {
        return VK_XBUTTON2;
    }

    return 0;
}

ATLAS_FORCE_INLINE bool input_message( u32 message )
{
    return message == WM_INPUT || ( message >= WM_KEYDOWN && message <= WM_UNICHAR ) ||
           ( message >= WM_MOUSEMOVE && message <= WM_MOUSEHWHEEL );
}

ATLAS_FORCE_INLINE pdiff set_window_procedure( void *window, pdiff procedure )
{
#if defined( __x86_64__ )
    return I( SetWindowLongPtrA )( ( HWND )window, GWLP_WNDPROC, procedure );
#else
    return I( SetWindowLongA )( ( HWND )window, GWLP_WNDPROC, procedure );
#endif
}

extern "C" pdiff ATLAS_STDCALL atlas_input_wndproc( void *window, u32 message, usize wparam, pdiff lparam )
{
    g_input.wndproc( window, message, wparam, lparam );

    if ( g_input.blocked( ) )
    {
        switch ( message )
        {
            case WM_MOUSEMOVE:
            case WM_MOUSELAST:
            case WM_MOUSEWHEEL:
            case WM_MOUSEACTIVATE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP: return 0;
            default:
            {
                if ( input_message( message ) && ( g_gui.focused_input( ) || g_html.has_focus( ) ) ) // TODO: Hoku; Would thread safety be an issue here? Meh...
                {
                    return 0;
                }

                break;
            }
        }
    }

    return g_input.call_original_wndproc( window, message, wparam, lparam );
}

bool input_handler::attach_device( void *device )
{
    render::d3d9::device9                       d3d_device{ device };
    render::d3d9::D3DDEVICE_CREATION_PARAMETERS parameters{ };

    if ( d3d_device.GetCreationParameters( &parameters ) < render::d3d9::D3D_OK || !parameters.focus_window )
    {
        return false;
    }

    return this->attach_window( parameters.focus_window );
}

bool input_handler::attach_window( void *window )
{
    if ( !window )
    {
        return false;
    }

    if ( this->m_window == window && this->m_original_wndproc )
    {
        return true;
    }

    this->detach( );

    pdiff previous{ set_window_procedure( window, reinterpret_cast< pdiff >( atlas_input_wndproc ) ) };

    if ( !previous )
    {
        return false;
    }

    this->m_window           = window;
    this->m_original_wndproc = reinterpret_cast< void * >( previous );
    this->reset( );
    this->m_focused = I( GetForegroundWindow )( ) == window;
    return true;
}

void input_handler::detach( )
{
    if ( this->m_window && this->m_original_wndproc )
    {
        set_window_procedure( this->m_window, reinterpret_cast< pdiff >( this->m_original_wndproc ) );
    }

    this->m_window           = nullptr;
    this->m_original_wndproc = nullptr;
    this->reset( );
}

void input_handler::reset( )
{
    this->m_keys.reset( );
    this->m_typed.reset( );
    this->m_typed_count      = 0;
    this->m_mouse            = point_t{ };
    this->m_wheel            = 0;
    this->m_focused          = false;
    this->m_blocked          = false;
    this->m_binds_suppressed = false;
}

void input_handler::begin_frame( )
{
    for ( input_button_t &button : this->m_keys )
    {
        button.m_repeat_edge    = button.m_pending_edge || button.m_pending_repeat;
        button.m_pressed_edge   = button.m_pending_edge;
        button.m_pending_edge   = false;
        button.m_pending_repeat = false;
    }
}

void input_handler::begin_command( )
{
    if ( this->m_batch_sealed )
    {
        this->m_batch_sealed = false;
        ++this->m_batch;
    }

    if ( this->m_batch_published == this->m_batch )
    {
        return; // every command of a batch acts on the same press, not on one press each
    }

    this->m_batch_published = this->m_batch;

    for ( input_button_t &button : this->m_keys )
    {
        button.m_batch_edge         = button.m_pending_batch_edge && !this->m_binds_suppressed;
        button.m_pending_batch_edge = false;
    }
}

void input_handler::seal_batch( )
{
    this->m_batch_sealed = true;
}

void input_handler::end_frame( )
{
    for ( input_button_t &button : this->m_keys )
    {
        if ( button.m_state == input_state::pressed )
        {
            button.m_state = button.m_down ? input_state::held : input_state::none;
            continue;
        }

        if ( button.m_state == input_state::released )
        {
            button.m_state = input_state::none;
        }
    }

    this->m_wheel       = 0;
    this->m_typed_count = 0;
}

pdiff input_handler::wndproc( void *window, u32 message, usize wparam, pdiff lparam )
{
    switch ( message )
    {
        case WM_ACTIVATEAPP:
        {
            this->m_focused = wparam != 0;

            if ( !this->m_focused )
            {
                this->m_keys.reset( );
                this->m_typed_count = 0;
            }
            break;
        }

        case WM_SETFOCUS: this->m_focused = true; break;

        case WM_KILLFOCUS:
        {
            this->m_focused     = false;
            this->m_typed_count = 0;
            this->m_keys.reset( );
            break;
        }

        case WM_CHAR:
        {
            if ( this->m_typed_count < this->m_typed.size( ) && wparam && wparam < 0x80 )
            {
                this->m_typed[ this->m_typed_count++ ] = static_cast< char >( wparam );
            }
            break;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            if ( wparam < this->m_keys.size( ) )
            {
                this->set_key( static_cast< u32 >( wparam ), true );
            }
            break;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            if ( wparam < this->m_keys.size( ) )
            {
                this->set_key( static_cast< u32 >( wparam ), false );
            }
            break;
        }

        case WM_MOUSEMOVE: this->set_mouse_position( lparam ); break;

        case WM_LBUTTONDOWN:
        {
            this->set_mouse_position( lparam );
            this->set_key( VK_LBUTTON, true );
            break;
        }

        case WM_LBUTTONUP:
        {
            this->set_mouse_position( lparam );
            this->set_key( VK_LBUTTON, false );
            break;
        }

        case WM_RBUTTONDOWN:
        {
            this->set_mouse_position( lparam );
            this->set_key( VK_RBUTTON, true );
            break;
        }

        case WM_RBUTTONUP:
        {
            this->set_mouse_position( lparam );
            this->set_key( VK_RBUTTON, false );
            break;
        }

        case WM_MBUTTONDOWN:
        {
            this->set_mouse_position( lparam );
            this->set_key( VK_MBUTTON, true );
            break;
        }

        case WM_MBUTTONUP:
        {
            this->set_mouse_position( lparam );
            this->set_key( VK_MBUTTON, false );
            break;
        }

        case WM_XBUTTONDOWN:
        {
            this->set_mouse_position( lparam );
            u32 key{ xbutton_key( wparam ) };

            if ( key )
            {
                this->set_key( key, true );
            }
            break;
        }

        case WM_XBUTTONUP:
        {
            this->set_mouse_position( lparam );
            u32 key{ xbutton_key( wparam ) };

            if ( key )
            {
                this->set_key( key, false );
            }
            break;
        }

        case WM_MOUSEWHEEL: this->m_wheel += static_cast< i32 >( high_word( static_cast< i64 >( wparam ) ) ) / WHEEL_DELTA; break;
        default: break;
    }

    return 1;
}

bool input_handler::pressed( u32 key ) const
{
    return key < this->m_keys.size( ) && this->focused( ) && this->m_keys[ key ].m_pressed_edge;
}

bool input_handler::pressed_batch( u32 key ) const
{
    return key < this->m_keys.size( ) && this->focused( ) && this->m_keys[ key ].m_batch_edge;
}

bool input_handler::repeated( u32 key ) const
{
    return key < this->m_keys.size( ) && this->focused( ) && this->m_keys[ key ].m_repeat_edge;
}

bool input_handler::held( u32 key ) const
{
    return key < this->m_keys.size( ) && this->focused( ) && this->m_keys[ key ].m_state == input_state::held;
}

bool input_handler::down( u32 key ) const
{
    return key < this->m_keys.size( ) && this->focused( ) && this->m_keys[ key ].m_down;
}

bool input_handler::down_batch( u32 key ) const
{
    return key < this->m_keys.size( ) && this->focused( ) && !this->m_binds_suppressed &&
           ( this->m_keys[ key ].m_down || this->m_keys[ key ].m_batch_edge );
}

bool input_handler::released( u32 key ) const
{
    return key < this->m_keys.size( ) && this->focused( ) && this->m_keys[ key ].m_state == input_state::released;
}

bool input_handler::is_pressed( u32 key ) const
{
    return this->pressed( key );
}

bool input_handler::is_held( u32 key ) const
{
    return this->held( key );
}

void input_handler::null_key( u32 key )
{
    if ( key < this->m_keys.size( ) )
    {
        this->m_keys[ key ] = input_button_t{ };
    }
}

bool input_handler::in_area( i32 x, i32 y, i32 width, i32 height ) const
{
    if ( !this->focused( ) )
    {
        return false;
    }

    return this->m_mouse.x >= x && this->m_mouse.x <= x + width &&
           this->m_mouse.y >= y && this->m_mouse.y <= y + height;
}

void input_handler::set_key( u32 key, bool down )
{
    if ( key >= this->m_keys.size( ) )
    {
        return;
    }

    input_button_t &button{ this->m_keys[ key ] };

    if ( down )
    {
        if ( button.m_down )
        {
            if ( button.m_state != input_state::pressed )
            {
                button.m_state = input_state::held;
            }

            button.m_pending_repeat = true;
            return;
        }

        button.m_state              = input_state::pressed;
        button.m_pending_edge       = true;
        button.m_pending_batch_edge = true;
        button.m_down               = true;
        return;
    }

    button.m_state = button.m_down ? input_state::released : input_state::none;
    button.m_down  = false;
}

void input_handler::set_mouse_position( pdiff lparam )
{
    this->m_mouse.x = low_word( lparam );
    this->m_mouse.y = high_word( lparam );
}

pdiff input_handler::call_original_wndproc( void *window, u32 message, usize wparam, pdiff lparam ) const
{
    return I( CallWindowProcA )( ( WNDPROC )this->m_original_wndproc, ( HWND )window, message, wparam, lparam );
}
