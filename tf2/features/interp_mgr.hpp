#pragma once

#include "link.hpp"

class interp_mgr
{
public:
    bool should_interpolate_local( );

private:
    i32 m_newest_command{ -1 };
    i32 m_command_tick{ -1 };
};

extern interp_mgr g_interp_mgr;
