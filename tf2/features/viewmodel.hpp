#pragma once

#include "link.hpp"

class viewmodel
{
public:
    void on_override_view( const CViewSetup *setup );
    void on_calc_view( vec3 &eye_position, vec3 &eye_angles );

    float view_model_fov( float original ) const;
    void  scale_bob( BobState_t *state ) const;

private:
    vec3 offset( float delta );
    vec3 sway( const vec3 &eye_angles, float delta );
    void aim( vec3 &eye_angles, const vec3 &eye_position, float delta );

    const CViewSetup *m_setup{ };
    float             m_game_fov{ };
    vec3              m_offset{ };
    vec3              m_sway_angle{ };
    vec3              m_aim_angle{ };
    float             m_aim_mix{ };
    double            m_last_time{ };
};

extern viewmodel g_viewmodel;
