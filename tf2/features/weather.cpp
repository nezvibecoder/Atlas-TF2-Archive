#include "weather.hpp"

ATLAS_NO_DESTROY weather g_weather{ };

static u8 density_alpha( i32 percent, float speed )
{
    const float ceiling{ math::smaller( 255.0f * speed / 600.0f, 255.0f ) };

    return static_cast< u8 >(
        static_cast< float >( math::clamp( percent, 0, 100 ) ) * ceiling / 100.0f );
}

constexpr look_t RAIN_LOOK{ 600.0f, 0.5f, 0.1f }; // the convar defaults, so rain stays vanilla
constexpr look_t SNOW_LOOK{ 80.0f, 0.7f, 0.1f };  // SNOW_SPEED and SNOW_TRACER_WIDTH

static const look_t &look_for( usize type )
{
    return type == config::WEATHER_SNOW ? SNOW_LOOK : RAIN_LOOK;
}

void weather::think( )
{
    const usize wanted{ g_cfg.m_weather.m_type };

    if ( wanted == config::WEATHER_OFF || !g_tf2.m_engine->IsInGame( ) )
    {
        this->destroy( );
        return;
    }

    if ( !this->alive( ) )
    {
        this->forget( );
    } else if ( this->m_type != wanted )
    {
        this->destroy( );
    }

    if ( !this->m_entity && !this->create( wanted ) )
    {
        return;
    }

    this->apply_density( );
    this->apply_convars( );
}

bool weather::alive( ) const
{
    if ( !this->m_networkable || !this->m_entity || !g_tf2.m_entity_list )
    {
        return false;
    }

    IClientEntity *occupant{ g_tf2.m_entity_list->GetClientEntity( MAX_EDICTS - 1 ) };

    return occupant &&
           occupant->GetBaseEntity( ) == static_cast< C_BaseEntity * >( this->m_entity );
}

void weather::forget( )
{
    this->m_networkable = nullptr;
    this->m_entity      = nullptr;
    this->m_type        = config::WEATHER_OFF;
}

void weather::on_level_init( )
{
    this->forget( );
}

void weather::on_level_shutdown( )
{
    // the engine frees every networkable in the list on its own level teardown, so the entity has
    // to go while it is still ours to release
    this->destroy( );
}

bool weather::create( usize type )
{
    ClientClass *client_class{ g_tf2.m_client->GetAllClasses( ) };

    while ( client_class && client_class->GetClassID( ) != ETFClassID::CPrecipitation )
    {
        client_class = client_class->m_pNext;
    }

    if ( !client_class || !client_class->m_pCreateFn )
    {
        return false;
    }

    // the last edict is the one slot the server never assigns, so a client only entity can sit
    // there without a networked one landing on top of it
    if ( g_tf2.m_entity_list->GetClientEntity( MAX_EDICTS - 1 ) )
    {
        return false;
    }

    IClientNetworkable *networkable{ client_class->m_pCreateFn( MAX_EDICTS - 1, 0 ) };

    if ( !networkable )
    {
        return false;
    }

    IClientUnknown *unknown{ networkable->GetIClientUnknown( ) };

    C_Precipitation *entity{ unknown ? static_cast< C_Precipitation * >( unknown->GetBaseEntity( ) )
                                     : nullptr };

    if ( !entity )
    {
        networkable->Release( );
        return false;
    }

    networkable->PreDataUpdate( DATA_UPDATE_CREATED );
    networkable->OnPreDataChanged( DATA_UPDATE_CREATED );

    // Precache latches the material, the fall speed and the tracer width off the type the first
    // time the entity simulates, and it only runs once, so the type has to be in place first
    entity->m_nPrecipType( ) = static_cast< i32 >( type ) - 1;
    entity->m_vecMins( )     = vec3{ -VOLUME_EXTENT, -VOLUME_EXTENT, -VOLUME_EXTENT };
    entity->m_vecMaxs( )     = vec3{ VOLUME_EXTENT, VOLUME_EXTENT, VOLUME_EXTENT };
    entity->m_clrRender( ).a = density_alpha( g_cfg.m_weather.m_density, look_for( type ).m_speed );

    networkable->OnDataChanged( DATA_UPDATE_CREATED );
    networkable->PostDataUpdate( DATA_UPDATE_CREATED );

    this->m_networkable = networkable;
    this->m_entity      = entity;
    this->m_type        = type;
    this->m_density     = g_cfg.m_weather.m_density;

    return true;
}

void weather::destroy( )
{
    if ( this->alive( ) )
    {
        this->m_networkable->Release( );
    }

    this->forget( );
    this->restore_convars( );
}

void weather::apply_density( )
{
    if ( this->m_density == g_cfg.m_weather.m_density )
    {
        return;
    }

    this->m_density                  = g_cfg.m_weather.m_density;
    this->m_entity->m_clrRender( ).a = density_alpha( this->m_density,
                                                      look_for( this->m_type ).m_speed );

    // m_flDensity is only recomputed inside OnDataChanged, and the created path would hand the
    // entity to the snow fall manager a second time
    this->m_networkable->OnDataChanged( DATA_UPDATE_DATATABLE_CHANGED );
}

void weather::apply_convars( )
{
    if ( !this->m_saved )
    {
        this->m_wind_speed     = g_tf2.cl_windspeed->GetFloat( );
        this->m_wind_direction = g_tf2.cl_winddir->GetFloat( );
        this->m_rain_speed     = g_tf2.r_rainspeed->GetFloat( );
        this->m_rain_width     = g_tf2.r_rainwidth->GetFloat( );
        this->m_rain_length    = g_tf2.r_rainlength->GetFloat( );
        this->m_saved          = true;
    }

    const look_t &look{ look_for( this->m_type ) };

    g_tf2.r_rainspeed->SetValue( look.m_speed );
    g_tf2.r_rainwidth->SetValue( look.m_width );
    g_tf2.r_rainlength->SetValue( look.m_length );

    g_tf2.cl_windspeed->SetValue( static_cast< float >( g_cfg.m_weather.m_wind_speed ) );
    g_tf2.cl_winddir->SetValue( static_cast< float >( g_cfg.m_weather.m_wind_direction ) );
}

void weather::restore_convars( )
{
    if ( !this->m_saved )
    {
        return;
    }

    g_tf2.cl_windspeed->SetValue( this->m_wind_speed );
    g_tf2.cl_winddir->SetValue( this->m_wind_direction );
    g_tf2.r_rainspeed->SetValue( this->m_rain_speed );
    g_tf2.r_rainwidth->SetValue( this->m_rain_width );
    g_tf2.r_rainlength->SetValue( this->m_rain_length );

    this->m_saved = false;
}
