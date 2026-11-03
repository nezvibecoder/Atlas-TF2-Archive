#pragma once

#include "link.hpp"
#include "runtime/array.hpp"

inline constexpr usize TRACER_LINE_SLOTS{ 128 };

struct tracer_line_t
{
    vec3    m_start{ };
    vec3    m_end{ };
    color_t m_color_line{ };
    color_t m_color_impact{ };
    double  m_born{ };
    double  m_expire{ };
};

class bullet_tracers
{
public:
    void begin_bullet( C_TFPlayer *player, C_TFWeaponBase *weapon );
    void end_bullet( const FireBulletsInfo_t &info );
    bool owns_particle( ) const
    {
        return this->m_shooter != nullptr;
    }

    void paint( );
    void clear( );

private:
    vec3 muzzle( const vec3 &fallback ) const;
    vec3 impact( const FireBulletsInfo_t &info ) const;

    color_t color_for_line( ) const;
    color_t color_for_impact( ) const;

    void queue_line( const vec3 &start, const vec3 &end, const color_t &value, const color_t &impact_val );
    void queue_beam( const vec3 &start, const vec3 &end, color_t value ) const;

    f32 scaled_radius( );

    atl::array< tracer_line_t, TRACER_LINE_SLOTS > m_lines{ };
    usize                                          m_cursor{ };

    C_TFPlayer     *m_shooter{ };
    C_TFWeaponBase *m_weapon{ };
};

extern bullet_tracers g_tracers;
