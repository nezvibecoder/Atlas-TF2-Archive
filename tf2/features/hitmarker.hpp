#pragma once

#include "link.hpp"

class hitmarker
{
public:
    void on_hurt( IGameEvent *event );
    void on_death( IGameEvent *event );
    void paint( ) const;
    void clear( );

private:
    struct hit_t
    {
        vec3   m_position{ };
        double m_time{ -1.0 };
        i32    m_victim{ };
        i32    m_damage{ };
        usize  m_event{ };
    };

    static constexpr usize CAPACITY{ 16 };

    void add( i32 victim, const vec3 &position, i32 damage, usize event );

    hit_t m_hits[ CAPACITY ]{ };
    usize m_next{ };
};

extern hitmarker g_hitmarker;
