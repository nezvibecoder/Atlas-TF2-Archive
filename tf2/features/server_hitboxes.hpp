#pragma once

#include "link.hpp"

class server_hitboxes
{
public:
    void paint( ) const;

private:
    void draw_server( C_TFPlayer *player, const mstudiohitboxset_t *set ) const;
    void draw_client( C_TFPlayer *player, const mstudiohitboxset_t *set ) const;
};

extern server_hitboxes g_server_hitboxes;
