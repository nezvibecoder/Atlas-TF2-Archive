#pragma once

#include "link.hpp"

constexpr float VOLUME_EXTENT{ 16384.0f };

struct look_t
{
    float m_speed;
    float m_width;
    float m_length;
};

class weather
{
public:
    void think( );
    void on_level_init( );
    void on_level_shutdown( );

private:
    bool create( usize type );
    void destroy( );
    void forget( );
    bool alive( ) const;

    void apply_density( );
    void apply_convars( );
    void restore_convars( );

    IClientNetworkable *m_networkable{ };
    C_Precipitation    *m_entity{ };

    usize m_type{ };
    i32   m_density{ };

    float m_wind_speed{ };
    float m_wind_direction{ };
    float m_rain_speed{ };
    float m_rain_width{ };
    float m_rain_length{ };
    bool  m_saved{ };
};

extern weather g_weather;
