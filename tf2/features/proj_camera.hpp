#pragma once

#include "link.hpp"

#include "proj_sim.hpp"

struct follow_sample_t
{
    vec3 m_origin{ };
    f32  m_time{ };
};

struct camera_frame_t
{
    rect_t m_body{ };
    vec3   m_target{ };
    vec3   m_shooter{ };
    vec3   m_camera{ };
    bool   m_blitted{ };
};

class projectile_camera
{
public:
    void capture( );
    void on_create_move( );
    void render( CViewRender *view_render, const CViewSetup &view );
    void paint( );
    void on_level_shutdown( );

    bool drawing( ) const
    {
        return this->m_drawing;
    }

private:
    class publish_t final : public CFunctor
    {
    public:
        i32 AddRef( ) override
        {
            return _InterlockedIncrement( &this->m_references );
        }

        i32 Release( ) override
        {
            return _InterlockedDecrement( &this->m_references );
        }

        void operator()( ) override;

        bool acquire( )
        {
            return _InterlockedCompareExchange( &this->m_references, 1, 0 ) == 0;
        }

        projectile_camera *m_owner{ };
        camera_frame_t     m_frame{ };

    private:
        volatile long m_references{ };
    };

    u32  newest_projectile( );
    bool place( );
    bool place_follow( );
    bool place_impact( );
    bool place_landing( );
    void place_behind( const vec3 &target, const vec3 &heading );
    void place_above( const vec3 &target, f32 yaw );
    void publish( IMatRenderContext *render_context, const camera_frame_t &frame );
    void create_resources( );
    void destroy_resources( );

    ITexture  *m_texture{ };
    IMaterial *m_material{ };

    vec3           m_origin{ };
    vec3           m_angles{ };
    vec3           m_target{ };
    rect_t         m_body{ };
    bool           m_active{ };
    bool           m_drawing{ };
    publish_t      m_publish[ 4 ]{ };
    camera_frame_t m_presented{ };

    u32             m_seen[ MAX_EDICTS ]{ };
    u32             m_followed{ INVALID_EHANDLE_INDEX };
    follow_sample_t m_samples[ 2 ]{ };
    i32             m_sample_count{ };

    projectile_setup_t m_setup{ };
    vec3               m_capture_angles{ };
    vec3               m_path[ projectile_sim::MAX_PATH_POINTS ]{ };

    u32  m_landing_handle{ INVALID_EHANDLE_INDEX };
    vec3 m_landing_target{ };
    f32  m_landing_yaw{ };
    f64  m_landing_deadline{ };
    f64  m_landing_linger{ };
};

extern projectile_camera g_proj_camera;
