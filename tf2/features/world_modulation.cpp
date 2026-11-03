#include "world_modulation.hpp"

#include "util/string.hpp"

ATLAS_NO_DESTROY world_modulation g_world_modulation{ };

static bool is_water( const char *name )
{
    const char *const water{ ATLAS_XORSTR_STATIC( "water" ) };
    return name && str::contains( name, str::length( name ), water, str::length( water ) );
}

void world_modulation::on_level_init( )
{
    this->m_world = color_t{ 255, 255, 255 };
    this->m_sky   = color_t{ 255, 255, 255 };
    this->m_props = color_t{ 255, 255, 255 };
}

void world_modulation::on_level_shutdown( )
{
    this->m_world = color_t{ 255, 255, 255 };
    this->m_sky   = color_t{ 255, 255, 255 };
    this->m_props = color_t{ 255, 255, 255 };
}

void world_modulation::think( )
{
    if ( !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    const bool    on{ g_cfg.m_world.m_modulate };
    const color_t world{ on ? g_cfg.m_world.m_color : color_t{ 255, 255, 255 } };
    const color_t sky{ on && g_cfg.m_world.m_skybox ? g_cfg.m_world.m_color
                                                    : color_t{ 255, 255, 255 } };
    const color_t props{ on && g_cfg.m_world.m_props ? g_cfg.m_world.m_color
                                                     : color_t{ 255, 255, 255 } };

    this->m_props = props;

    if ( world.gdi( ) == this->m_world.gdi( ) &&
         sky.gdi( ) == this->m_sky.gdi( ) )
    {
        return;
    }

    this->m_world = world;
    this->m_sky   = sky;

    this->apply( this->m_world, this->m_sky );
}

void world_modulation::modulate_static_prop( const float *color, float *modulated ) const
{
    modulated[ 0 ] = color[ 0 ] * static_cast< float >( this->m_props.r ) / 255.0f;
    modulated[ 1 ] = color[ 1 ] * static_cast< float >( this->m_props.g ) / 255.0f;
    modulated[ 2 ] = color[ 2 ] * static_cast< float >( this->m_props.b ) / 255.0f;
}

void world_modulation::apply( color_t world, color_t sky ) const
{
    CMaterialSystem       *materials{ g_tf2.m_material_system };
    const MaterialHandle_t invalid{ materials->InvalidMaterial( ) };

    const char *const world_group{ ATLAS_XORSTR_STATIC( "World textures" ) };
    const char *const sky_group{ ATLAS_XORSTR_STATIC( "SkyBox textures" ) };

    for ( MaterialHandle_t handle{ materials->FirstMaterial( ) }; handle != invalid;
          handle = materials->NextMaterial( handle ) )
    {
        IMaterial *material{ materials->GetMaterial( handle ) };
        if ( !material || material->IsErrorMaterial( ) || !material->IsPrecached( ) )
        {
            continue;
        }

        const char *group{ material->GetTextureGroupName( ) };

        if ( !group )
        {
            continue;
        }

        const char *name{ material->GetName( ) };

        if ( !name )
        {
            continue;
        }

        color_t value{ };

        if ( str::equals( group, world_group ) )
        {
            if ( is_water( name ) )
            {
                continue;
            }

            value = world;
        } else if ( str::equals( group, sky_group ) )
        {
            value = sky;
        } else
        {
            continue;
        }

        material->ColorModulate( static_cast< float >( value.r ) / 255.0f,
                                 static_cast< float >( value.g ) / 255.0f,
                                 static_cast< float >( value.b ) / 255.0f );
    }
}
