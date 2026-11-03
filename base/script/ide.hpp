#pragma once

#include "script.hpp"

#include "gui/gui.hpp"

namespace script
{
    class ide final : public gui::baseobject
    {
    public:
        using action_fn = void ( * )( );

        ide( action_fn save, action_fn run, action_fn stop, action_fn clear );

        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;

        void open( string_view name, string_view source );
        void create( );
        void saved( string_view name );
        void run( string_view name );
        void stop( );

        string_view name( ) const
        {
            return this->m_name.view( );
        }

        string_view text( ) const
        {
            return this->m_editor.text( );
        }

        usize lines( ) const
        {
            return this->m_editor.lines( );
        }

        usize marked_line( ) const
        {
            return this->m_editor.marked_line( );
        }

    private:
        rect_t toolbar_bounds( ) const;
        rect_t editor_bounds( ) const;
        rect_t console_bounds( ) const;
        rect_t console_header( ) const;
        usize  console_rows( ) const;
        void   arrange_buttons( );
        void   follow_log( );
        void   think_console( );
        void   draw_toolbar( ) const;
        void   draw_console( ) const;

        gui::code_editor m_editor{ };
        gui::button      m_save;
        gui::button      m_run;
        gui::button      m_stop;
        gui::button      m_clear;
        name_t           m_name{ };
        u64              m_seen_serial{ };
        usize            m_console_scroll{ };
        usize            m_error_count{ };
        bool             m_console_open{ };
    };
} // namespace script
