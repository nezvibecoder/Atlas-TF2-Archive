#pragma once

#include "util/typedefs.hpp"

namespace gui
{
    enum : usize
    {
        MENU_PARTICLES_OFF       = 0,
        MENU_PARTICLES_SNOW      = 1,
        MENU_PARTICLES_NETWORK   = 2,
        MENU_PARTICLES_HALLOWEEN = 3,
        MENU_PARTICLES_COUNT     = 4,
    };

    void draw_menu_particles( rect_t screen, u8 opacity );
    void draw_menu_decorations( rect_t bounds, rect_t subnav, i32 unit, u8 opacity );
} // namespace gui
