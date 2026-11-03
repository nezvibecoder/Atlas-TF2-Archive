#pragma once

#include "runtime/array.hpp"

enum class input_state : u8
{
    none,
    pressed,
    held,
    released,
};

struct input_button_t
{
    input_state m_state{ input_state::none };
    bool        m_down{ };
    bool        m_pending_edge{ };
    bool        m_pressed_edge{ };
    bool        m_pending_batch_edge{ };
    bool        m_batch_edge{ };
    bool        m_pending_repeat{ };
    bool        m_repeat_edge{ };
};

class input_handler
{
public:
    bool attach_device( void *device );
    bool attach_window( void *window );
    void detach( );
    void reset( );
    void begin_frame( );
    void end_frame( );
    void begin_command( );
    void seal_batch( );
    pdiff wndproc( void *window, u32 message, usize wparam, pdiff lparam );
    pdiff call_original_wndproc( void *window, u32 message, usize wparam, pdiff lparam ) const;
    bool pressed( u32 key ) const;
    bool pressed_batch( u32 key ) const;
    bool repeated( u32 key ) const;
    bool held( u32 key ) const;
    bool down( u32 key ) const;
    bool down_batch( u32 key ) const;
    bool released( u32 key ) const;
    bool is_pressed( u32 key ) const;
    bool is_held( u32 key ) const;
    bool in_area( i32 x, i32 y, i32 width, i32 height ) const;
    void null_key( u32 key );
    void block( bool blocked )
    {
        this->m_blocked = blocked;
    }

    bool blocked( ) const
    {
        return this->m_blocked;
    }

    void suppress_binds( bool suppressed )
    {
        this->m_binds_suppressed = suppressed;
    }

    point_t mouse( ) const
    {
        return this->m_mouse;
    }

    i32 wheel( ) const
    {
        return this->m_wheel;
    }

    usize typed_count( ) const
    {
        return this->m_typed_count;
    }

    char typed( usize index ) const
    {
        return index < this->m_typed_count ? this->m_typed[ index ] : 0;
    }

    bool focused( ) const
    {
        return this->m_focused;
    }

    void *window( ) const
    {
        return this->m_window;
    }

    u64 batch( ) const
    {
        return this->m_batch;
    }

private:
    void set_key( u32 key, bool down );
    void set_mouse_position( pdiff lparam );

    atl::array< input_button_t, 256 > m_keys{ };
    atl::array< char, 16 >            m_typed{ };
    usize                             m_typed_count{ };
    void                             *m_window{ };
    void                             *m_original_wndproc{ };
    u64                               m_batch{ 1 };
    u64                               m_batch_published{ };
    bool                              m_batch_sealed{ };
    point_t                           m_mouse{ };
    i32                               m_wheel{ };
    bool                              m_focused{ };
    bool                              m_blocked{ };
    bool                              m_binds_suppressed{ };
};

extern input_handler g_input;
