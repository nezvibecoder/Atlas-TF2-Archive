#include "holdaim_mgr.hpp"

#include "anti_aim.hpp"

ATLAS_NO_DESTROY holdaim_mgr g_holdaim_mgr{ };

// when the server runs a batch of usercmds in one simulation tick, the first command that actually
// fires locks pl.v_angle (CTFPlayer::NoteWeaponFired), and CPlayerMove::RunCommand restores it for
// every later command of that tick (sv_maxusrcmdprocessticks_holdaim, default 1); The IN_ATTACK bit
// alone locks nothing;

void holdaim_mgr::lock( const vec3 &angle )
{
    if ( m_angle_locked )
    {
        return;
    }

    const i32 ticks{ g_tf2.sv_maxusrcmdprocessticks_holdaim->GetInt( ) };
    if ( ticks <= 0 )
    {
        return;
    }

    m_angle        = angle;
    m_angle_locked = true;

    DBGOUT( "holdaim_mgr: Locking va for this batch." );
}

void holdaim_mgr::on_create_move( )
{
    if ( !m_angle_locked )
    {
        return;
    }

    // Modulate the onshot for this va lock.
    vec3 temp_ang{ m_angle };
    g_anti_aim.on_va_lock( temp_ang );

    // the server restores the held angle for this command anyway; write it ourselves so the
    // outgoing command matches the shot the server actually takes..
    g_cl.m_cmd->viewangles = temp_ang;
    DBGOUT( "holdaim_mgr: Overriding va..." );
}

void holdaim_mgr::reset( )
{
    m_angle_locked = false;
    m_angle        = { };
}
