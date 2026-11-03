#include "overlay.hpp"

#include "libs/lazy_importer.hpp"
#include "runtime/memory.hpp"
#include "runtime/obfuscation.hpp"
#include "runtime/thread.hpp"

namespace render
{
    inline constexpr usize NT_BUILD_NUMBER_ADDRESS{ 0x7FFE0260u };
    inline constexpr u32   EXCLUDE_FROM_CAPTURE_BUILD{ 19041 };
    inline constexpr u32   WM_OVERLAY_FRAME{ WM_APP };

    bool overlay::can_present( const d3d9::device9 &device )
    {
        if ( !this->m_has_device_info )
        {
            this->cache_device_info( device );
        }

        if ( !this->m_windowed )
        {
            return false;
        }

        if ( __atomic_load_n( &this->m_state, __ATOMIC_ACQUIRE ) == OVERLAY_NONE )
        {
            this->start_thread( );
        }

        return __atomic_load_n( &this->m_state, __ATOMIC_ACQUIRE ) != OVERLAY_FAILED;
    }

    bool overlay::present( const d3d9::device9      &device,
                           const d3d9::surface9     &composite,
                           const d3d9::D3DVIEWPORT9 &viewport )
    {
        if ( __atomic_load_n( &this->m_state, __ATOMIC_ACQUIRE ) != OVERLAY_READY )
        {
            return true;
        }

        if ( !this->should_show( ) )
        {
            this->set_visible( false );
            return true;
        }

        if ( !this->ensure_buffers( device, viewport.width, viewport.height ) )
        {
            return false;
        }

        switch ( __atomic_load_n( &this->m_frame_state, __ATOMIC_ACQUIRE ) )
        {
            case FRAME_COPIED:
                this->m_readback_surface.UnlockRect( );
                this->m_frame_state = FRAME_IDLE;
                this->set_visible( true );
                return this->issue_readback( device, composite );
            case FRAME_IDLE: return this->issue_readback( device, composite );
            case FRAME_IN_FLIGHT: return this->lock_readback( viewport );
            default: return true;
        }
    }

    void overlay::hide( )
    {
        if ( __atomic_load_n( &this->m_state, __ATOMIC_ACQUIRE ) != OVERLAY_READY )
        {
            return;
        }

        this->set_visible( false );
    }

    void overlay::release_device_resources( )
    {
        this->release_buffers( );
        this->m_has_device_info = false;
    }

    void overlay::describe_failure( u32 *stage, u32 *error ) const
    {
        *error = 0;

        if ( __atomic_load_n( &this->m_state, __ATOMIC_ACQUIRE ) == OVERLAY_FAILED )
        {
            *stage = this->m_create_stage;
            *error = this->m_create_error;
        } else if ( !this->m_has_device_info )
        {
            *stage = 1;
        } else if ( this->m_game_window )
        {
            *stage = 2;
        } else
        {
            *stage = 3;
        }
    }

    void ATLAS_STDCALL overlay::thread_entry( void *argument )
    {
        static_cast< overlay * >( argument )->run( );
    }

    void overlay::run( )
    {
        if ( !this->create_window( ) )
        {
            __atomic_store_n( &this->m_state, OVERLAY_FAILED, __ATOMIC_RELEASE );
            return;
        }

        __atomic_store_n( &this->m_state, OVERLAY_READY, __ATOMIC_RELEASE );

        MSG message{ };
        while ( I( GetMessageA )( &message, nullptr, 0, 0 ) > 0 )
        {
            if ( message.message == WM_OVERLAY_FRAME )
            {
                this->copy_and_update( );
                __atomic_store_n( &this->m_frame_state, FRAME_COPIED, __ATOMIC_RELEASE );
                continue;
            }

            I( DispatchMessageA )( &message );
        }
    }

    bool overlay::create_window( )
    {
        const u32 build{ *reinterpret_cast< volatile u32 * >( NT_BUILD_NUMBER_ADDRESS ) };

        if ( build < EXCLUDE_FROM_CAPTURE_BUILD )
        {
            this->m_create_stage = 4;
            this->m_create_error = build;
            return false;
        }

        HINSTANCE instance{ I( GetModuleHandleA )( nullptr ) };
        auto      class_name{ ATLAS_XORSTR( "OverlayWindow" ) };

        WNDCLASSEXA window_class{ };
        window_class.cbSize        = sizeof( WNDCLASSEXA );
        window_class.lpfnWndProc   = I( DefWindowProcA );
        window_class.hInstance     = instance;
        window_class.lpszClassName = class_name.get( );

        if ( !I( RegisterClassExA )( &window_class ) )
        {
            this->m_create_stage = 5;
            this->m_create_error = I( GetLastError )( );
            return false;
        }

        HWND window{ I( CreateWindowExA )( WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                                               WS_EX_TOOLWINDOW,
                                           class_name.get( ), nullptr, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                                           instance, nullptr ) };

        if ( !window )
        {
            this->m_create_stage = 6;
            this->m_create_error = I( GetLastError )( );
            return false;
        }

        if ( !I( SetWindowDisplayAffinity )( window, WDA_EXCLUDEFROMCAPTURE ) )
        {
            this->m_create_stage = 7;
            this->m_create_error = I( GetLastError )( );
            I( DestroyWindow )( window );
            return false;
        }

        this->m_window = window;
        return true;
    }

    void overlay::start_thread( )
    {
        __atomic_store_n( &this->m_state, OVERLAY_STARTING, __ATOMIC_RELEASE );

        if ( !atl::create_hidden_thread( &overlay::thread_entry, this ) )
        {
            this->m_create_stage = 8;
            __atomic_store_n( &this->m_state, OVERLAY_FAILED, __ATOMIC_RELEASE );
        }
    }

    void overlay::cache_device_info( const d3d9::device9 &device )
    {
        d3d9::D3DPRESENT_PARAMETERS parameters{ };

        if ( device.GetPresentParameters( &parameters ) < d3d9::D3D_OK )
        {
            return;
        }

        this->m_game_window     = static_cast< HWND >( parameters.device_window );
        this->m_windowed        = parameters.windowed != 0 && parameters.device_window != nullptr;
        this->m_has_device_info = true;
    }

    bool overlay::should_show( ) const
    {
        if ( I( IsIconic )( this->m_game_window ) )
        {
            return false;
        }

        return I( GetForegroundWindow )( ) == this->m_game_window;
    }

    void overlay::set_visible( bool visible )
    {
        if ( this->m_visible == visible )
        {
            return;
        }

        if ( visible )
        {
            I( ShowWindow )( this->m_window, SW_SHOWNOACTIVATE );
        } else
        {
            I( ShowWindow )( this->m_window, SW_HIDE );
        }

        this->m_visible = visible;
    }

    bool overlay::ensure_buffers( const d3d9::device9 &device, u32 width, u32 height )
    {
        if ( this->m_readback_surface && this->m_width == width && this->m_height == height )
        {
            return true;
        }

        this->release_buffers( );

        if ( device.CreateOffscreenPlainSurface( width, height, d3d9::D3DFMT_A8R8G8B8, d3d9::D3DPOOL_SYSTEMMEM,
                                                 &this->m_readback_surface ) < d3d9::D3D_OK ||
             device.CreateQuery( d3d9::D3DQUERYTYPE_EVENT, &this->m_readback_query ) < d3d9::D3D_OK )
        {
            return false;
        }

        BITMAPINFO info{ };
        info.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER );
        info.bmiHeader.biWidth       = static_cast< LONG >( width );
        info.bmiHeader.biHeight      = -static_cast< LONG >( height );
        info.bmiHeader.biPlanes      = 1;
        info.bmiHeader.biBitCount    = 32;
        info.bmiHeader.biCompression = BI_RGB;

        this->m_bitmap_context = I( CreateCompatibleDC )( nullptr );

        if ( !this->m_bitmap_context )
        {
            return false;
        }

        void *bits{ };
        this->m_bitmap = I( CreateDIBSection )( this->m_bitmap_context, &info, DIB_RGB_COLORS, &bits, nullptr, 0 );

        if ( !this->m_bitmap )
        {
            return false;
        }

        this->m_previous_bitmap = I( SelectObject )( this->m_bitmap_context, this->m_bitmap );
        this->m_bitmap_bits     = static_cast< u8 * >( bits );
        this->m_width           = width;
        this->m_height          = height;
        return true;
    }

    void overlay::release_buffers( )
    {
        while ( __atomic_load_n( &this->m_frame_state, __ATOMIC_ACQUIRE ) == FRAME_COPYING )
        {
            I( Sleep )( 0 );
        }

        if ( this->m_frame_state == FRAME_COPIED )
        {
            this->m_readback_surface.UnlockRect( );
        }

        this->m_frame_state = FRAME_IDLE;
        this->m_readback_query.Release( );
        this->m_readback_surface.Release( );

        if ( this->m_bitmap )
        {
            I( SelectObject )( this->m_bitmap_context, this->m_previous_bitmap );
            I( DeleteObject )( this->m_bitmap );
        }

        if ( this->m_bitmap_context )
        {
            I( DeleteDC )( this->m_bitmap_context );
        }

        this->m_bitmap_context  = nullptr;
        this->m_bitmap          = nullptr;
        this->m_previous_bitmap = nullptr;
        this->m_bitmap_bits     = nullptr;
        this->m_width           = 0;
        this->m_height          = 0;
    }

    bool overlay::issue_readback( const d3d9::device9 &device, const d3d9::surface9 &composite )
    {
        if ( device.GetRenderTargetData( &composite, &this->m_readback_surface ) < d3d9::D3D_OK ||
             this->m_readback_query.Issue( d3d9::D3DISSUE_END ) < d3d9::D3D_OK )
        {
            return false;
        }

        this->m_frame_state = FRAME_IN_FLIGHT;
        return true;
    }

    bool overlay::lock_readback( const d3d9::D3DVIEWPORT9 &viewport )
    {
        if ( this->m_readback_query.GetData( 0 ) != d3d9::D3D_OK )
        {
            return true;
        }

        this->m_origin = POINT{ static_cast< LONG >( viewport.x ), static_cast< LONG >( viewport.y ) };

        if ( !I( ClientToScreen )( this->m_game_window, &this->m_origin ) ||
             this->m_readback_surface.LockRect( &this->m_locked, nullptr, d3d9::D3DLOCK_READONLY ) < d3d9::D3D_OK )
        {
            return false;
        }

        __atomic_store_n( &this->m_frame_state, FRAME_COPYING, __ATOMIC_RELEASE );
        I( PostMessageA )( this->m_window, WM_OVERLAY_FRAME, 0, 0 );
        return true;
    }

    void overlay::copy_and_update( ) const
    {
        const usize row_size{ static_cast< usize >( this->m_width ) * 4u };

        for ( u32 row{ }; row < this->m_height; ++row )
        {
            atl::memcpy( this->m_bitmap_bits + row * row_size,
                         static_cast< const u8 * >( this->m_locked.bits ) + static_cast< usize >( row ) * this->m_locked.pitch,
                         row_size );
        }

        SIZE          size{ static_cast< LONG >( this->m_width ), static_cast< LONG >( this->m_height ) };
        POINT         source{ };
        POINT         origin{ this->m_origin };
        BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

        I( UpdateLayeredWindow )( this->m_window, nullptr, &origin, &size, this->m_bitmap_context, &source, 0, &blend,
                                  ULW_ALPHA );
    }
}
