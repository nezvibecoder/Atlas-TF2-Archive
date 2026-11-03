#pragma once

#include "link.hpp"

struct lua_State;

class script_api final : public IGameEventListener2
{
public:
    static void open( lua_State *state );

    void paint( );

    void create_move_pre_pred( CUserCmd *cmd );
    void create_move( CUserCmd *cmd );
    void create_move_post_pred( CUserCmd *cmd );

    void frame_stage( ClientFrameStage_t stage );

    void level_init( );
    void level_shutdown( );

    void shutdown( );

    bool listen( const char *name );

    void FireGameEvent( IGameEvent *event ) override;
    int  GetEventDebugID( ) override;

    CUserCmd *command( ) const
    {
        return this->m_cmd;
    }

    IGameEvent *event( ) const
    {
        return this->m_event;
    }

    ClientFrameStage_t stage( ) const
    {
        return this->m_stage;
    }

private:
    CUserCmd          *m_cmd{ };
    IGameEvent        *m_event{ };
    ClientFrameStage_t m_stage{ FRAME_UNDEFINED };
    bool               m_listening{ };
};

extern script_api g_script_api;

void name_menu_elements( );
