#pragma once

#include "link.hpp"

class holdaim_mgr
{
public:
    void lock( const vec3 &angle );
    void on_create_move( );

    const vec3 *held( ) const
    {
        return m_angle_locked ? &m_angle : nullptr;
    }

    void reset( );

private:
    vec3 m_angle{ };
    bool m_angle_locked{ false };
};

extern holdaim_mgr g_holdaim_mgr;
