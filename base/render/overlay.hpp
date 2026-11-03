#pragma once

#include "d3d9.hpp"
#include "util/windefs.hpp"

namespace render
{
    enum : u32
    {
        OVERLAY_NONE     = 0,
        OVERLAY_STARTING = 1,
        OVERLAY_READY    = 2,
        OVERLAY_FAILED   = 3,
    };

    enum : u32
    {
        FRAME_IDLE      = 0,
        FRAME_IN_FLIGHT = 1,
        FRAME_COPYING   = 2,
        FRAME_COPIED    = 3,
    };

    class overlay
    {
    public:
        bool can_present( const d3d9::device9 &device );
        bool present( const d3d9::device9      &device,
                      const d3d9::surface9     &composite,
                      const d3d9::D3DVIEWPORT9 &viewport );
        void hide( );
        void release_device_resources( );
        void describe_failure( u32 *stage, u32 *error ) const;

    private:
        static void ATLAS_STDCALL thread_entry( void *argument );

        void run( );
        bool create_window( );
        void start_thread( );
        void cache_device_info( const d3d9::device9 &device );
        bool should_show( ) const;
        void set_visible( bool visible );
        bool ensure_buffers( const d3d9::device9 &device, u32 width, u32 height );
        void release_buffers( );
        bool issue_readback( const d3d9::device9 &device, const d3d9::surface9 &composite );
        bool lock_readback( const d3d9::D3DVIEWPORT9 &viewport );
        void copy_and_update( ) const;

        u32                  m_state{ OVERLAY_NONE };
        u32                  m_frame_state{ FRAME_IDLE };
        HWND                 m_window{ };
        HWND                 m_game_window{ };
        bool                 m_has_device_info{ };
        bool                 m_windowed{ };
        bool                 m_visible{ };
        d3d9::surface9       m_readback_surface{ };
        d3d9::query9         m_readback_query{ };
        d3d9::D3DLOCKED_RECT m_locked{ };
        POINT                m_origin{ };
        HDC                  m_bitmap_context{ };
        HBITMAP              m_bitmap{ };
        HGDIOBJ              m_previous_bitmap{ };
        u8                  *m_bitmap_bits{ };
        u32                  m_width{ };
        u32                  m_height{ };
        u32                  m_create_stage{ };
        u32                  m_create_error{ };
    };
}
