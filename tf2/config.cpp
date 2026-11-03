#include "config.hpp"

#include "config_storage.hpp"
#include "features/chams.hpp"
#include "gui/notifications.hpp"
#include "gui/theme.hpp"
#include "preview.hpp"
#include "runtime/thread.hpp"
#include "script/script.hpp"

#include "config_fields.inl"

ATLAS_NO_DESTROY config g_cfg{ };

template< typename value_type >
static void write_config_value( json::writer &writer, value_type &value );

template< typename value_type, usize size >
static void write_config_value( json::writer &writer, value_type ( &value )[ size ] );

template< typename value_type >
static bool read_config_value( json::value node, value_type &value );

template< typename value_type, usize size >
static bool read_config_value( json::value node, value_type ( &value )[ size ] );

struct config_writer
{
    json::writer &m_writer;

    template< typename value_type >
    void operator( )( const char *name, value_type &value )
    {
        this->m_writer.key( name );
        write_config_value( this->m_writer, value );
    }
};

struct config_reader
{
    json::value m_node;
    bool        m_valid{ true };

    template< typename value_type >
    void operator( )( const char *name, value_type &value )
    {
        const json::value member{ this->m_node[ name ] };
        if ( member.valid( ) && !read_config_value( member, value ) )
        {
            this->m_valid = false;
        }
    }
};

template< typename value_type >
static void write_config_value( json::writer &writer, value_type &value )
{
    if constexpr ( __is_enum( value_type ) )
    {
        writer.write( static_cast< u32 >( value ) );
    } else if constexpr ( json::detail::is_number< value_type > )
    {
        writer.write( value );
    } else
    {
        writer.begin_object( );
        config_writer field{ writer };
        config_fields( value, field );
        writer.end_object( );
    }
}

template< typename value_type, usize size >
static void write_config_value( json::writer &writer, value_type ( &value )[ size ] )
{
    writer.begin_array( );
    for ( auto &element : value )
    {
        write_config_value( writer, element );
    }

    writer.end_array( );
}

template< typename value_type >
static bool read_config_value( json::value node, value_type &value )
{
    if constexpr ( json::detail::is_same< value_type, bool > )
    {
        if ( node.type( ) != json::type_t::boolean )
        {
            return false;
        }

        value = node.as< bool >( );
    } else if constexpr ( __is_enum( value_type ) )
    {
        if ( node.type( ) != json::type_t::number )
        {
            return false;
        }

        const i64 number{ node.as< i64 >( -1 ) };
        if ( number < 0 || number > config_enum_maximum< value_type > || node.as< f64 >( ) != number )
        {
            return false;
        }

        value = static_cast< value_type >( number );
    } else if constexpr ( json::detail::is_number< value_type > )
    {
        if ( node.type( ) != json::type_t::number )
        {
            return false;
        }

        const value_type number{ node.as< value_type >( ) };
        if ( static_cast< f64 >( number ) != node.as< f64 >( ) && !json::detail::is_real< value_type > )
        {
            return false;
        }

        value = number;
    } else
    {
        if ( node.type( ) != json::type_t::object )
        {
            return false;
        }

        config_reader field{ node };
        config_fields( value, field );
        if constexpr ( json::detail::is_same< value_type, bind_t > )
        {
            return field.m_valid && value.m_key <= CONFIG_BIND_KEY_MAXIMUM;
        }

        return field.m_valid;
    }

    return true;
}

template< typename value_type, usize size >
static bool read_config_value( json::value node, value_type ( &value )[ size ] )
{
    if ( node.type( ) != json::type_t::array || node.size( ) > size )
    {
        return false;
    }

    for ( usize i{ }; i < node.size( ); ++i )
    {
        if ( !read_config_value( node[ i ], value[ i ] ) )
        {
            return false;
        }
    }

    return true;
}

template< typename value_type >
static void clamp_config_vars( value_type &value, value_type minimum, value_type maximum )
{
    value = math::clamp( value, minimum, maximum );
}

bool valid_config( config &value )
{
    clamp_config_vars( value.m_ragebot.m_fov, 1, 180 );
    clamp_config_vars( value.m_ragebot.m_hitscan_draw_duration, 0.2f, 5.0f );
    clamp_config_vars( value.m_ragebot.m_projectile_path_fade_time, 0.2f, 3.0f );
    clamp_config_vars( value.m_projectile_path.m_duration, 0.2f, 5.0f );
    clamp_config_vars( value.m_esp.m_dormant_time, 1, 15 );
    clamp_config_vars( value.m_fog.m_start, 0, 8000 );
    clamp_config_vars( value.m_fog.m_end, 0, 16000 );
    clamp_config_vars( value.m_fog.m_density, 0, 100 );
    clamp_config_vars( value.m_weather.m_density, 0, 100 );
    clamp_config_vars( value.m_weather.m_wind_speed, 0, 500 );
    clamp_config_vars( value.m_weather.m_wind_direction, 0, 360 );
    clamp_config_vars( value.m_viewmodel.m_forward, -30, 30 );
    clamp_config_vars( value.m_viewmodel.m_right, -30, 30 );
    clamp_config_vars( value.m_viewmodel.m_up, -30, 30 );
    clamp_config_vars( value.m_viewmodel.m_bob, 0, 200 );
    clamp_config_vars( value.m_viewmodel.m_sway, 0, 200 );
    clamp_config_vars( value.m_viewmodel.m_aim_swing, 0, 400 );
    clamp_config_vars( value.m_tracers.m_duration, 100, 3000 );
    clamp_config_vars( value.m_arrows.m_radius, 60, 500 );
    clamp_config_vars( value.m_arrows.m_size, 8, 40 );
    clamp_config_vars( value.m_misc.m_fov, 60, 140 );
    clamp_config_vars( value.m_misc.m_thirdperson_distance, 30, 300 );
    clamp_config_vars( value.m_misc.m_thirdperson_side, -100, 100 );
    clamp_config_vars( value.m_misc.m_thirdperson_height, -100, 100 );
    clamp_config_vars( value.m_misc.m_thirdperson_smooth_time, 0, 1000 );
    clamp_config_vars( value.m_misc.m_freecam_speed, 100, 3000 );
    clamp_config_vars( value.m_misc.m_projectile_camera_distance, 0, 1000 );
    clamp_config_vars( value.m_misc.m_projectile_camera_width, 160, 800 );
    clamp_config_vars( value.m_misc.m_menu_sound_volume, 0, 100 );
    clamp_config_vars( value.m_radio.m_volume, 0, 100 );
    clamp_config_vars( value.m_soundboard.m_volume, 0, 500 );
    clamp_config_vars( value.m_radio.m_duck_level, 0, 100 );
    clamp_config_vars( value.m_radio.m_crossfade, 0, 2000 );
    clamp_config_vars( value.m_auto_det.m_radius_scale, 40, 100 );
    clamp_config_vars( value.m_auto_det.m_min_targets, 1, 6 );
    clamp_config_vars( value.m_misc.m_auto_strafe_retrack, 10.0f, 100.0f );
    clamp_config_vars( value.m_exploits.m_fake_latency_ms, 20, 900 );
    clamp_config_vars( value.m_exploits.m_ping_reducer_cmdrate, 1, 66 );
    clamp_config_vars( value.m_exploits.m_warp_ticks, 2, 22 );
    clamp_config_vars( value.m_queue.m_max_ping, 10, 1000 );
    clamp_config_vars( value.m_queue.m_delay_join_time, 15, 1000 );
    clamp_config_vars( value.m_recorder.m_marker_distance, 500, 6000 );

    if ( value.m_esp.m_target_group >= config::CHAM_ENTITY_COUNT ||
         ( value.m_esp.m_target_group != static_cast< usize >( config::cham_entity_t::world ) &&
           value.m_esp.m_edited >= config::ESP_TARGET_COUNT ) ||
         ( value.m_esp.m_target_group == static_cast< usize >( config::cham_entity_t::world ) &&
           value.m_esp.m_edited >= config::WORLD_COUNT ) ||
         value.m_esp.m_avatars >= 3 || value.m_esp.m_target_style >= config::TARGET_VISUAL_COUNT ||
         value.m_hitmarker.m_edited_event >= config::HITMARKER_EVENT_COUNT ||
         value.m_hitmarker.m_render_mode >= config::HITMARKER_MODE_COUNT ||
         value.m_auto_vacc.m_idle_resist >= 3 ||
         value.m_auto_vacc.m_reactivity >= config::auto_vacc_t::REACTIVITY_COUNT ||
         value.m_pills.m_crit_priority >= 2 ||
         value.m_chams.m_local_model >= config::LOCAL_MODEL_COUNT || value.m_chams.m_enemy_model >= config::ENEMY_MODEL_COUNT ||
         value.m_chams.m_backtrack_records >= config::BACKTRACK_COUNT || value.m_weather.m_type >= config::WEATHER_COUNT ||
         value.m_tracers.m_style >= config::TRACER_STYLE_COUNT || value.m_fog.m_mode >= config::FOG_COUNT ||
         value.m_misc.m_menu_particles >= gui::MENU_PARTICLES_COUNT || value.m_misc.m_auto_strafe_mode >= 2 ||
         value.m_misc.m_projectile_camera_mode >= config::PROJECTILE_CAMERA_COUNT ||
         value.m_misc.m_projectile_camera_hud >= config::PROJECTILE_CAMERA_HUD_COUNT ||
         value.m_rjump.m_priority >= config::RJUMP_PRIORITY_COUNT ||
         value.m_ragebot.m_projectile_path_style >= config::PATH_STYLE_COUNT ||
         value.m_ragebot.m_projectile_path_bounds_style >= config::PATH_BOUNDS_COUNT ||
         value.m_projectile_path.m_style >= config::PATH_STYLE_COUNT ||
         value.m_trajectory.m_style >= config::TRAJECTORY_STYLE_COUNT ||
         value.m_projectile_warning.m_style >= config::PROJECTILE_WARNING_STYLE_COUNT ||
         value.m_radio.m_indicator_style >= config::RADIO_INDICATOR_COUNT ||
         value.m_recorder.m_route_style >= config::PATH_STYLE_COUNT ||
         value.m_skins.m_wearable_slot >= config::SKIN_WEARABLE_SLOTS )
    {
        return false;
    }

    for ( auto &profile : value.m_esp.m_profiles )
    {
        if ( profile.m_health_style >= config::HEALTH_STYLE_COUNT )
        {
            return false;
        }

        clamp_config_vars( profile.m_health_divisions, 0, 10 );

        u32 seen{ };
        for ( u8 element : profile.m_order )
        {
            if ( element >= config::ESP_ELEMENT_COUNT || ( seen & ( 1u << element ) ) )
            {
                return false;
            }

            seen |= 1u << element;
        }
    }

    u16 groups{ };
    for ( usize index{ }; index < config::PLAYER_GROUP_COUNT; ++index )
    {
        const config::player_group_t &group{ value.m_player_groups[ index ] };
        if ( group.m_name[ 16 ] )
        {
            return false;
        }

        if ( group.m_name[ 0 ] )
        {
            groups = static_cast< u16 >( groups | ( 1u << index ) );
        }
    }

    for ( const auto &entry : value.m_player_tags )
    {
        if ( entry.m_tag > config::PLAYER_TAG_CHEATER || entry.m_alias[ 32 ] ||
             ( entry.m_groups & ~groups ) )
        {
            return false;
        }
    }

    return true;
}

bool config::load( const char *name )
{
    atl::vector< char > content{ };
    json::document      document{ };
    if ( !configs::transfer( configs::action::load, name, { }, content ) ||
         !document.parse( string_view{ content.data( ), content.size( ) } ) ||
         document.root( )[ ATLAS_XORSTR_STATIC( "version" ) ].as< i32 >( ) != 1 )
    {
        return false;
    }

    config candidate{ };
    if ( !read_config_value( document.root( )[ ATLAS_XORSTR_STATIC( "settings" ) ], candidate ) )
    {
        return false;
    }

    if ( candidate.m_esp.m_target_style == 3 )
    {
        candidate.m_esp.m_target_style = TARGET_VISUAL_BRACKETS;
    }

    if ( !valid_config( candidate ) )
    {
        return false;
    }

    if ( !configs::transfer( configs::action::activate, name, { }, content ) )
    {
        return false;
    }

    *this = candidate;

    const json::value scripts{ document.root( )[ ATLAS_XORSTR_STATIC( "scripts" ) ] };
    if ( scripts.type( ) == json::type_t::array )
    {
        g_scripts.request_unload_all( );

        for ( usize index{ }; index < scripts.size( ); ++index )
        {
            const string_view   script_name{ scripts[ index ].as< string_view >( ) };
            atl::vector< char > source{ };

            if ( !script::valid_name( script_name ) )
            {
                continue;
            }

            if ( !script::fetch( script_name, source ) )
            {
                g_scripts.log( script::log_kind_t::error, ATLAS_XORSTR( "could not read the script file" ).get( ) );
                continue;
            }

            g_scripts.request_run( script_name, string_view{ source.data( ), source.size( ) } );
        }
    }

    return true;
}

bool config::save( const char *name )
{
    json::writer writer{ };
    writer.begin_object( );
    writer.member( ATLAS_XORSTR_STATIC( "version" ), 1 );
    writer.key( ATLAS_XORSTR_STATIC( "settings" ) );
    write_config_value( writer, *this );

    script::name_t scripts[ script::SCRIPT_CAPACITY ]{ };
    const usize    script_count{ g_scripts.loaded_names( scripts, script::SCRIPT_CAPACITY ) };

    writer.key( ATLAS_XORSTR_STATIC( "scripts" ) );
    writer.begin_array( );
    for ( usize index{ }; index < script_count; ++index )
    {
        writer.write( scripts[ index ].view( ) );
    }

    writer.end_array( );

    writer.end_object( );
    atl::vector< char > output{ };
    return writer.ok( ) && configs::transfer( configs::action::save, name, writer.text( ), output );
}

static long                                 s_config_state{ };
static configs::action                      s_config_action{ };
static char                                 s_config_name[ 33 ]{ };
static char                                 s_config_names[ configs::max_configs ][ 33 ]{ };
static const char                          *s_config_labels[ configs::max_configs ]{ };
static usize                                s_config_count{ };
ATLAS_NO_DESTROY static config              s_config_pending{ };
ATLAS_NO_DESTROY static atl::vector< char > s_config_list{ };
static bool                                 s_config_success{ };
static bool                                 s_config_listed{ };

static bool read_config_list( )
{
    json::document document{ };
    if ( !document.parse( string_view{ s_config_list.data( ), s_config_list.size( ) } ) )
    {
        return false;
    }

    const json::value items{ document.root( )[ ATLAS_XORSTR_STATIC( "configs" ) ] };
    if ( items.type( ) != json::type_t::array || items.size( ) > configs::max_configs )
    {
        return false;
    }

    for ( usize i{ }; i < items.size( ); ++i )
    {
        if ( !configs::valid_name( items[ i ][ ATLAS_XORSTR_STATIC( "name" ) ].as< string_view >( ) ) )
        {
            return false;
        }
    }

    s_config_count = items.size( );
    for ( usize i{ }; i < configs::max_configs; ++i )
    {
        const string_view name{ i < s_config_count ? items[ i ][ ATLAS_XORSTR_STATIC( "name" ) ].as< string_view >( ) : string_view{ } };
        str::copy( s_config_names[ i ], sizeof( s_config_names[ i ] ), name.data( ), name.size( ) );
        s_config_labels[ i ] = s_config_names[ i ];
    }

    return true;
}

static void ATLAS_STDCALL config_worker( void * )
{
    atl::vector< char > output{ };
    switch ( s_config_action )
    {
        case configs::action::save: s_config_success = s_config_pending.save( s_config_name ); break;
        case configs::action::load: s_config_success = s_config_pending.load( s_config_name ); break;
        case configs::action::remove: s_config_success = configs::transfer( s_config_action, s_config_name, { }, output ); break;
        default: s_config_success = true; break;
    }

    s_config_listed = configs::transfer( configs::action::list, "", { }, s_config_list );
    _InterlockedExchange( &s_config_state, 2 );
}

static void notify_config_result( )
{
    const notify_kind_t kind{ s_config_success ? notify_kind_t::success : notify_kind_t::error };
    switch ( s_config_action )
    {
        case configs::action::save: push_notification( kind, ICON_SAVE, s_config_success ? ATLAS_XORSTR_STATIC( "Config saved" ) : ATLAS_XORSTR_STATIC( "Could not save config" ), s_config_name ); break;
        case configs::action::load: push_notification( kind, ICON_SAVE, s_config_success ? ATLAS_XORSTR_STATIC( "Config loaded" ) : ATLAS_XORSTR_STATIC( "Could not load config" ), s_config_name ); break;
        case configs::action::remove: push_notification( kind, ICON_SAVE, s_config_success ? ATLAS_XORSTR_STATIC( "Config deleted" ) : ATLAS_XORSTR_STATIC( "Could not delete config" ), s_config_name ); break;
        default: break;
    }

    if ( !s_config_listed )
    {
        push_notification( notify_kind_t::error, ICON_SAVE, ATLAS_XORSTR_STATIC( "Configs" ), ATLAS_XORSTR_STATIC( "Could not read the config list" ) );
    } else if ( s_config_action == configs::action::list )
    {
        push_notification( notify_kind_t::information, ICON_SAVE, ATLAS_XORSTR_STATIC( "Configs" ), ATLAS_XORSTR_STATIC( "Config list refreshed" ) );
    }
}

void configs::startup( )
{
    for ( usize i{ }; i < max_configs; ++i )
    {
        s_config_labels[ i ] = s_config_names[ i ];
    }

    if ( !transfer( action::list, "", { }, s_config_list ) || !read_config_list( ) )
    {
        push_notification( notify_kind_t::error, ICON_SAVE, ATLAS_XORSTR_STATIC( "Configs" ), ATLAS_XORSTR_STATIC( "Could not read the config list" ) );
        return;
    }

    json::document document{ };
    if ( !document.parse( string_view{ s_config_list.data( ), s_config_list.size( ) } ) )
    {
        return;
    }

    // the menu is built before this runs, so the result goes through poll( ) like a worker result
    // and the render thread applies it together with the picker, accent and skin editor refresh
    s_config_action  = action::activate; // nothing to load; poll( ) only publishes the list silently
    s_config_success = false;
    s_config_listed  = true;

    const string_view last{ document.root( )[ ATLAS_XORSTR_STATIC( "last_used" ) ].as< string_view >( ) };
    if ( valid_name( last ) )
    {
        str::copy( s_config_name, sizeof( s_config_name ), last.data( ), last.size( ) );
        s_config_action  = action::load;
        s_config_success = s_config_pending.load( s_config_name );
    }

    _InterlockedExchange( &s_config_state, 2 );
}

bool configs::request( action operation, string_view name )
{
    if ( operation != action::list && !valid_name( name ) )
    {
        push_notification( notify_kind_t::warning, ICON_SAVE, ATLAS_XORSTR_STATIC( "Configs" ), ATLAS_XORSTR_STATIC( "Enter a name or select a config" ) );
        return false;
    }

    if ( _InterlockedCompareExchange( &s_config_state, 1, 0 ) != 0 )
    {
        push_notification( notify_kind_t::warning, ICON_SAVE, ATLAS_XORSTR_STATIC( "Configs" ), ATLAS_XORSTR_STATIC( "A config operation is still running" ) );
        return false;
    }

    s_config_action = operation;
    str::copy( s_config_name, sizeof( s_config_name ), name.data( ), name.size( ) );

    s_config_pending = g_cfg;
    s_config_success = false;

    if ( !atl::create_hidden_thread( config_worker, nullptr ) )
    {
        _InterlockedExchange( &s_config_state, 0 );
        push_notification( notify_kind_t::error, ICON_SAVE, ATLAS_XORSTR_STATIC( "Configs" ), ATLAS_XORSTR_STATIC( "Could not start the config worker" ) );
        return false;
    }

    return true;
}

bool configs::poll( bool &loaded )
{
    loaded = false;
    if ( _InterlockedCompareExchange( &s_config_state, 3, 2 ) != 2 )
    {
        return false;
    }
    loaded = s_config_success && s_config_action == action::load;
    if ( loaded )
    {
        g_cfg = s_config_pending;
        reset_esp_preview( );
    }
    s_config_listed = s_config_listed && read_config_list( );
    notify_config_result( );
    _InterlockedExchange( &s_config_state, 0 );
    return true;
}

const char *const *configs::names( )
{
    return s_config_labels;
}

bool configs::has_name( usize index )
{
    return index < s_config_count;
}

usize configs::loaded_index( )
{
    for ( usize i{ }; i < s_config_count; ++i )
    {
        if ( str::equals( s_config_names[ i ], s_config_name ) )
        {
            return i;
        }
    }

    return 0;
}

usize configs::count( )
{
    return s_config_count;
}
