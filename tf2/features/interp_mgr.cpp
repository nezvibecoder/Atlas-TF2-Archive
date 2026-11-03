#include "interp_mgr.hpp"

ATLAS_NO_DESTROY interp_mgr g_interp_mgr{ };

// BaseInterpolatePart1 puts a predictable at ( m_nFinalPredictedTick - 1 + interpolation_amount ) ticks, between its last
// two predicted commands; On a client tick that makes no command (a recharge, a shift) the fraction restarts on the same
// final tick and the local walks a tick back, so it keeps the newest prediction until the next command.
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/client/c_baseentity.cpp#L2853-L2862
bool interp_mgr::should_interpolate_local( )
{
    const i32 newest{ g_tf2.m_cl->lastoutgoingcommand + g_tf2.m_cl->chokedcommands };
    if ( newest != this->m_newest_command )
    {
        this->m_newest_command = newest;
        this->m_command_tick   = g_tf2.m_globals->tickcount;
    }

    return g_tf2.m_globals->tickcount == this->m_command_tick;
}
