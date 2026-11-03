#pragma once

#include "link.hpp"

class world_modulation
{
public:
    void think( );
    void on_level_init( );
    void on_level_shutdown( );
    void modulate_static_prop( const float *color, float *modulated ) const;

    bool m_drawing_static_props{ };

private:
    void    apply( color_t world, color_t sky ) const;
    color_t m_world{ 255, 255, 255 };
    color_t m_sky{ 255, 255, 255 };
    color_t m_props{ 255, 255, 255 };
};

extern world_modulation g_world_modulation;
