#include "proj_sim.hpp"

#include "crits.hpp"
#include "holdaim_mgr.hpp"
#include "networking.hpp"
#include "ragebot.hpp"
#include "splash.hpp"
#include "tickbase.hpp"

#include "gui/gui.hpp"
#include "math/math.hpp"

ATLAS_NO_DESTROY projectile_sim g_proj_sim{ };

// the shipped binary rand wrappers tail-call vstdlib RandomInt(0, 32767);
ATLAS_FORCE_INLINE vec3 projectile_angle_spread( i32 command, float spread, bool skip_angles = false )
{
    if ( command == 0 || spread == 0.0f )
    {
        return vec3{ };
    }
    struct angle_sample_t
    {
        i32  m_command{ };
        vec2 m_fractions{ };
    };
    static angle_sample_t cache[ 2 ]{ };
    angle_sample_t       &sample{ cache[ skip_angles ? 1 : 0 ] };
    if ( command != sample.m_command )
    {
        g_crits.seed_launch_random( command, skip_angles );
        sample.m_fractions = { static_cast< float >( g_tf2.m_random.RandomInt( 0, 0x7FFF ) ) / 32767.0f,
                               static_cast< float >( g_tf2.m_random.RandomInt( 0, 0x7FFF ) ) / 32767.0f };
        sample.m_command   = command;
    }
    return vec3{ -spread + sample.m_fractions.x * ( spread * 2.0f ),
                 -spread + sample.m_fractions.y * ( spread * 2.0f ), 0.0f };
}

ATLAS_FORCE_INLINE float detonate_poll( float fuse )
{
    const float period{ ROUND_TO_TICKS( 0.2f ) };
    return period * static_cast< float >( static_cast< i32 >( fuse / period ) + 1 );
}

ATLAS_FORCE_INLINE const char *model_path( projectile_model model )
{
    switch ( model )
    {
        case projectile_model::rocket: return ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_rocket.mdl" );
        case projectile_model::pipe: return ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_grenade_grenadelauncher.mdl" );
        case projectile_model::sticky: return ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_stickybomb.mdl" );
        case projectile_model::sticky_practice: return ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_stickybomb2.mdl" );
        case projectile_model::cannonball: return ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_cannonball.mdl" );
        case projectile_model::cleaver: return ATLAS_XORSTR_STATIC( "models/workshop_partner/weapons/c_models/c_sd_cleaver/c_sd_cleaver.mdl" );
        case projectile_model::baseball: return ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_baseball.mdl" );
        case projectile_model::ornament: return ATLAS_XORSTR_STATIC( "models/weapons/c_models/c_xms_festive_ornament.mdl" );
        case projectile_model::jarate: return ATLAS_XORSTR_STATIC( "models/weapons/c_models/urinejar.mdl" );
        case projectile_model::madmilk: return ATLAS_XORSTR_STATIC( "models/workshop/weapons/c_models/c_madmilk/c_madmilk.mdl" );
        case projectile_model::breadmonster: return ATLAS_XORSTR_STATIC( "models/weapons/c_models/c_breadmonster/c_breadmonster.mdl" );
        case projectile_model::breadmonster_milk: return ATLAS_XORSTR_STATIC( "models/weapons/c_models/c_breadmonster/c_breadmonster_milk.mdl" );
        case projectile_model::gascan: return ATLAS_XORSTR_STATIC( "models/weapons/c_models/c_gascan/c_gascan.mdl" );
        case projectile_model::sandvich: return ATLAS_XORSTR_STATIC( "models/weapons/c_models/c_sandwich/c_sandwich.mdl" );
        default: return nullptr;
    }
}

struct key_block_t
{
    const char *m_name{ };
    usize       m_name_length{ };
    const char *m_body{ };
    usize       m_body_length{ };

    bool named( const char *name ) const
    {
        return str::equals_ignore_case( this->m_name, this->m_name_length, name, str::length( name ) );
    }
};

static bool next_block( const char *text, usize length, usize &cursor, key_block_t &out )
{
    const auto blank{ [ & ]( usize at ) { return static_cast< u8 >( text[ at ] ) <= ' '; } };
    while ( cursor < length && blank( cursor ) )
    {
        ++cursor;
    }

    const usize name{ cursor };
    while ( cursor < length && text[ cursor ] != '{' && !blank( cursor ) )
    {
        ++cursor;
    }

    const usize name_end{ cursor };
    while ( cursor < length && text[ cursor ] != '{' )
    {
        ++cursor;
    }

    if ( cursor >= length || name_end == name )
    {
        return false;
    }

    const usize open{ ++cursor };
    while ( cursor < length && text[ cursor ] != '}' )
    {
        ++cursor;
    }

    if ( cursor >= length )
    {
        return false;
    }

    out = key_block_t{ text + name, name_end - name, text + open, cursor - open };
    ++cursor;
    return true;
}

static bool read_value( const key_block_t &block, const char *key, const char *&value, usize &value_length )
{
    const char *text{ block.m_body };
    const usize length{ block.m_body_length };
    const usize key_length{ str::length( key ) };

    for ( usize at{ }; at + key_length + 2 < length; ++at )
    {
        if ( text[ at ] != '"' || text[ at + 1 + key_length ] != '"' ||
             !str::equals_ignore_case( text + at + 1, key_length, key, key_length ) )
        {
            continue;
        }

        usize cursor{ at + key_length + 2 };
        while ( cursor < length && text[ cursor ] != '"' )
        {
            ++cursor;
        }

        const usize start{ cursor + 1 };
        usize       end{ start };
        while ( end < length && text[ end ] != '"' )
        {
            ++end;
        }

        if ( end >= length )
        {
            return false;
        }

        value        = text + start;
        value_length = end - start;
        return true;
    }

    return false;
}

static bool read_key( const key_block_t &block, const char *key, float &out )
{
    const char *text{ };
    usize       length{ };
    if ( !read_value( block, key, text, length ) )
    {
        return false;
    }

    usize cursor{ };
    float sign{ 1.0f };
    if ( cursor < length && text[ cursor ] == '-' )
    {
        sign = -1.0f;
        ++cursor;
    }

    float value{ };
    while ( cursor < length && str::is_digit( text[ cursor ] ) )
    {
        value = value * 10.0f + static_cast< float >( text[ cursor ] - '0' );
        ++cursor;
    }

    if ( cursor < length && text[ cursor ] == '.' )
    {
        ++cursor;

        float scale{ 0.1f };
        while ( cursor < length && str::is_digit( text[ cursor ] ) )
        {
            value += static_cast< float >( text[ cursor ] - '0' ) * scale;
            scale *= 0.1f;
            ++cursor;
        }
    }

    out = sign * value;
    return true;
}

static bool read_int( const key_block_t &block, const char *key, i32 &out )
{
    const char *text{ };
    usize       length{ };
    if ( !read_value( block, key, text, length ) || length == 0 )
    {
        return false;
    }

    i32 value{ };
    for ( usize at{ }; at < length; ++at )
    {
        if ( !str::is_digit( text[ at ] ) )
        {
            return false;
        }
        value = value * 10 + ( text[ at ] - '0' );
    }

    out = value;
    return true;
}

static i32 s_model_indices[ static_cast< usize >( projectile_model::sandvich ) + 1 ]{ };
static i32 model_index_of( projectile_model model )
{
    const usize slot{ static_cast< usize >( model ) };
    if ( model == projectile_model::none || slot >= ( sizeof( s_model_indices ) / sizeof( s_model_indices[ 0 ] ) ) )
    {
        return -1;
    }

    if ( s_model_indices[ slot ] != 0 )
    {
        return s_model_indices[ slot ];
    }

    const char *path{ model_path( model ) };
    if ( !path || !g_tf2.m_model_info )
    {
        s_model_indices[ slot ] = -1;
        return -1;
    }

    if ( g_tf2.m_model_loader )
    {
        g_tf2.m_model_loader->GetModelForName( path, FMODELLOADER_CLIENT );
    }

    const i32 index{ g_tf2.m_model_info->GetModelIndex( path ) };
    s_model_indices[ slot ] = index > 0 ? index : -1;

    return s_model_indices[ slot ];
}

static collide_info_t s_collides[ static_cast< usize >( projectile_model::sandvich ) + 1 ]{ };
static void           forget_collides( )
{
    for ( collide_info_t &entry : s_collides )
    {
        entry = collide_info_t{ };
    }

    for ( i32 &index : s_model_indices )
    {
        index = 0;
    }
}

static const collide_info_t *resolve_collide( projectile_model model )
{
    const usize slot{ static_cast< usize >( model ) };
    if ( model == projectile_model::none || slot >= ( sizeof( s_collides ) / sizeof( s_collides[ 0 ] ) ) )
    {
        return nullptr;
    }

    collide_info_t &info{ s_collides[ slot ] };
    if ( info.m_attempted )
    {
        return info.m_resolved ? &info : nullptr;
    }

    info.m_attempted = true;

    const i32 model_index{ model_index_of( model ) };

    if ( model_index < 0 )
    {
        return nullptr;
    }

    vcollide_t *collide{ g_tf2.m_model_info->GetVCollide( model_index ) };
    if ( !collide )
    {
        return nullptr;
    }

    if ( collide->solidCount == 0 || !collide->solids || !collide->solids[ 0 ] )
    {
        return nullptr;
    }

    info.m_collide  = collide->solids[ 0 ];
    info.m_params   = g_PhysDefaultObjectParams;
    info.m_material = -1; // vphysics reads -1 as "default", as PhysModelCreate passes it

    const char *text{ collide->pKeyValues };
    const usize length{ text ? str::length( text ) : 0 };
    usize       cursor{ };
    key_block_t block{ };
    while ( next_block( text, length, cursor, block ) )
    {
        if ( !block.named( ATLAS_XORSTR( "solid" ).get( ) ) )
        {
            continue;
        }

        float parsed{ };
        if ( read_key( block, ATLAS_XORSTR( "mass" ).get( ), parsed ) )
            info.m_params.mass = parsed;
        if ( read_key( block, ATLAS_XORSTR( "inertia" ).get( ), parsed ) )
            info.m_params.inertia = parsed;
        if ( read_key( block, ATLAS_XORSTR( "damping" ).get( ), parsed ) )
            info.m_params.damping = parsed;
        if ( read_key( block, ATLAS_XORSTR( "rotdamping" ).get( ), parsed ) )
            info.m_params.rotdamping = parsed;
        if ( read_key( block, ATLAS_XORSTR( "volume" ).get( ), parsed ) )
            info.m_params.volume = parsed;

        const char *surface{ };
        usize       surface_length{ };
        char        name[ sizeof( solid_t::surfaceprop ) ]{ };
        if ( read_value( block, ATLAS_XORSTR( "surfaceprop" ).get( ), surface, surface_length ) && surface_length < sizeof( name ) )
        {
            atl::memcpy( name, surface, surface_length );
            info.m_material = g_tf2.m_physics_surface_props->GetSurfaceIndex( name );
        }
        break;
    }

    info.m_params.enableCollisions = true;
    info.m_params.pGameData        = nullptr;
    info.m_params.pName            = nullptr;

    vec3 mins{ }, maxs{ };
    g_tf2.m_physics_collision->CollideGetAABB( &mins, &maxs, info.m_collide, vec3( ), vec3( ) );

    info.m_half_extents = ( maxs - mins ) * 0.5f;
    info.m_centre       = ( maxs + mins ) * 0.5f;
    info.m_resolved     = true;

    return &info;
}

void projectile_sim::apply_launch_randomness( projectile_setup_t &out, C_TFWeaponBase *weapon, i32 command, bool angular )
{
    switch ( out.m_model )
    {
        case projectile_model::pipe:
        case projectile_model::sticky:
        case projectile_model::sticky_practice:
        case projectile_model::cannonball: break;
        default: return;
    }

    const bool                  spins{ g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "grenade_no_spin" ).get( ) ) == 0.0f };
    static i32                  cached_command{ };
    static bool                 cached_angular{ };
    static crits::pipe_launch_t cached_launch{ };

    if ( command != cached_command || angular != cached_angular )
    {
        cached_command = command;
        cached_angular = angular;
        cached_launch  = command != 0 ? g_crits.pipe_launch_for( command, angular ) : crits::pipe_launch_t{ };
    }

    out.m_jitter_right     = cached_launch.m_jitter_right;
    out.m_jitter_up        = cached_launch.m_jitter_up;
    out.m_angular_velocity = vec3( );

    if ( spins )
    {
        // grenade_no_spin zeroes the lot
        out.m_angular_velocity = vec3( 600.0f, static_cast< float >( cached_launch.m_spin ), 0.0f );
    }
}

float projectile_sim::sv_gravity( ) const
{
    return g_tf2.sv_gravity->GetFloat( );
}

void projectile_sim::snap_to_eye( C_TFPlayer *player, vec3 &position ) const
{
    trace_t            trace{ };
    Ray_t              ray{ };
    CTraceFilterSimple filter{ player, COLLISION_GROUP_NONE };

    ray.Init( player->Weapon_ShootPosition( ), position );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID_BRUSHONLY, &filter, &trace );

    position = trace.endpos;
}

vec3 projectile_sim::get_fire_offset( C_TFWeaponBase *gun, bool ducked )
{
    Assert( gun != nullptr );
    switch ( gun->GetWeaponID( ) )
    {
        case TF_WEAPON_COMPOUND_BOW:
        case TF_WEAPON_CROSSBOW:
        case TF_WEAPON_SHOTGUN_BUILDING_RESCUE: return vec3( 23.5f, 8.0f, -3.0f );
        case TF_WEAPON_SYRINGEGUN_MEDIC: return vec3( 16.0f, 6.0f, -8.0f );
        case TF_WEAPON_FLAREGUN:
        case TF_WEAPON_FLAREGUN_REVENGE:
        case TF_WEAPON_ROCKETLAUNCHER:
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT: return vec3( 23.5f, 12.0f, ducked ? 8.0f : -3.0f );
        case TF_WEAPON_RAYGUN:
        case TF_WEAPON_DRG_POMSON:
        case TF_WEAPON_PARTICLE_CANNON: return vec3( 23.5f, 8.0f, ducked ? 8.0f : -3.0f );
        default: return vec3{ };
    }
}

bool projectile_sim::gather( C_TFPlayer *player, C_TFWeaponBase *weapon, const vec3 &angles,
                             projectile_setup_t &out, i32 command, f32 charge_time,
                             const projectile_target_t *target )
{
    if ( !player || !weapon )
    {
        return false;
    }

    out = projectile_setup_t{ };

    const i32   id{ weapon->GetWeaponID( ) };
    const bool  pipe{ id == TF_WEAPON_GRENADELAUNCHER || id == TF_WEAPON_PIPEBOMBLAUNCHER ||
                      id == TF_WEAPON_CANNON };
    const float attribute_spread{ g_cl.attrib_hook( weapon, 0.0f,
                                                    ATLAS_XORSTR( "projectile_spread_angle" ).get( ) ) };
    const float angular_spread{ pipe ? math::bigger( attribute_spread, 0.0f ) : attribute_spread };
    if ( g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "auto_fires_full_clip_all_at_once" ).get( ) ) != 0.0f )
    {
        return false;
    }
    if ( angular_spread != 0.0f && !pipe )
    {
        C_TFGameRules *rules{ TFGameRules( ) };
        if ( rules && rules->GameModeUsesUpgrades( ) && weapon->m_iClip1( ) == 1 &&
             !weapon->m_bFiringWholeClip( ) &&
             g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "can_overload" ).get( ) ) != 0.0f &&
             g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "auto_fires_full_clip" ).get( ) ) != 0.0f )
        {
            return false;
        }
    }
    const vec3 firing_angles{ angles + projectile_angle_spread( command, angular_spread ) };

    const bool  ducking{ player->IsDucked( ) };
    const bool  precision{ player->InCond( TF_COND_RUNE_PRECISION ) };
    const float charge_now{ TICKS_TO_TIME( player->m_nTickBase( ) ) };

    switch ( weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_ROCKETLAUNCHER:
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
        {
            g_cl.projectile_fire_setup( player, weapon, firing_angles, get_fire_offset( weapon, ducking ), false,
                                        out.m_position, out.m_angles, target );
            snap_to_eye( player, out.m_position );

            out.m_speed    = precision ? 3000.0f
                                       : g_cl.attrib_hook( weapon, 1100.0f, ATLAS_XORSTR( "mult_projectile_speed" ).get( ) );
            out.m_gravity  = 0.0f;
            out.m_movetype = projectile_movetype::fly;
            out.m_lifetime = 5.0f;
            break;
        }

        case TF_WEAPON_PARTICLE_CANNON:
        {
            g_cl.projectile_fire_setup( player, weapon, firing_angles, get_fire_offset( weapon, ducking ), false,
                                        out.m_position, out.m_angles, target );
            snap_to_eye( player, out.m_position );

            out.m_speed    = 1100.0f;
            out.m_gravity  = 0.0f;
            out.m_movetype = projectile_movetype::fly;
            out.m_lifetime = 5.0f;
            break;
        }

        case TF_WEAPON_RAYGUN:
        case TF_WEAPON_DRG_POMSON:
        {
            g_cl.projectile_fire_setup( player, weapon, firing_angles, get_fire_offset( weapon, ducking ), false,
                                        out.m_position, out.m_angles, target );

            if ( weapon->GetWeaponID( ) == TF_WEAPON_DRG_POMSON )
            {
                out.m_position.z -= 13.0f; // autism
            }
            snap_to_eye( player, out.m_position );

            out.m_speed    = 1200.0f;
            out.m_gravity  = 0.0f;
            out.m_hull     = vec3( 1.0f, 1.0f, 1.0f );
            out.m_movetype = projectile_movetype::fly;
            out.m_lifetime = 5.0f;
            break;
        }

        case TF_WEAPON_FLAREGUN:
        case TF_WEAPON_FLAREGUN_REVENGE:
        {
            g_cl.projectile_fire_setup( player, weapon, firing_angles, get_fire_offset( weapon, ducking ), false,
                                        out.m_position, out.m_angles, target );

            out.m_speed    = g_cl.attrib_hook( weapon, 2000.0f, ATLAS_XORSTR( "mult_projectile_speed" ).get( ) );
            out.m_gravity  = g_cl.attrib_hook( weapon, 0.3f, ATLAS_XORSTR( "mult_projectile_speed" ).get( ) );
            out.m_movetype = projectile_movetype::fly_gravity;
            out.m_lifetime = 5.0f;
            break;
        }

        case TF_WEAPON_FLAMETHROWER:
        {
            vec3 forward{ }, right{ };
            math::angle_vectors( angles, &forward, &right );

            out.m_position = player->Weapon_ShootPosition( ) + ( forward * 40.0f + right * 5.0f ) * player->m_flModelScale( );
            out.m_angles   = angles;

            out.m_speed    = g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "flame_speed" ).get( ) );
            out.m_drag     = g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "flame_drag" ).get( ) );
            out.m_rise     = g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "flame_up_speed" ).get( ) );
            out.m_lifetime = g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "flame_lifetime" ).get( ) ) -
                             g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "flame_random_life_time_offset" ).get( ) );
            out.m_carry    = player->m_vecVelocity( );
            out.m_gravity  = 0.0f;
            out.m_movetype = projectile_movetype::flame;

            const f32 radius{ g_tf2.tf_flamethrower_boxsize->GetFloat( ) };
            out.m_hull = vec3( radius, radius, radius );
            break;
        }

        case TF_WEAPON_FLAME_BALL:
        {
            vec3 forward{ }, right{ }, up{ };
            math::angle_vectors( angles, &forward, &right, &up );

            const vec3 shoot_position{ player->Weapon_ShootPosition( ) };
            const f32  distance{ g_tf2.tf_fireball_distance->GetFloat( ) };

            out.m_position = shoot_position + up * -9.0f + right * ( g_cl.m_vm_flipped ? -7.0f : 7.0f ) + forward * 3.0f;
            math::vector_angles( shoot_position + forward * distance - out.m_position, out.m_angles );

            out.m_speed    = g_tf2.tf_fireball_speed->GetFloat( );
            out.m_gravity  = 0.0f;
            out.m_hull     = vec3( 1.0f, 1.0f, 1.0f );
            out.m_movetype = projectile_movetype::fly;
            out.m_lifetime = math::smaller( distance / out.m_speed, g_tf2.tf_fireball_max_lifetime->GetFloat( ) );
            break;
        }

        case TF_WEAPON_COMPOUND_BOW:
        {
            g_cl.projectile_fire_setup( player, weapon, firing_angles, get_fire_offset( weapon, ducking ), false,
                                        out.m_position, out.m_angles, target );

            // @ CTFCompoundBow::GetCurrentCharge
            const float begin{ weapon->As< C_TFPipebombLauncher * >( )->m_flChargeBeginTime( ) };
            const float held{ begin > 0.0f ? charge_now - begin : 0.0f };
            if ( ( charge_time >= 0.0f ? charge_time : held ) >= 5.0f )
            {
                out.m_angles += projectile_angle_spread( command, 6.0f, angular_spread != 0.0f );
            }
            const float charge{ math::smaller( charge_time >= 0.0f ? charge_time : held, 1.0f ) };
            const float max_charge{ weapon->ApplyFireDelay( 1.0f ) };

            out.m_speed    = math::remap_clamped( charge, 0.0f, max_charge, 1800.0f, 2600.0f );
            out.m_gravity  = math::remap_clamped( charge, 0.0f, max_charge, 0.5f, 0.1f );
            out.m_hull     = vec3( 1.0f, 1.0f, 1.0f );
            out.m_movetype = projectile_movetype::fly_gravity;
            out.m_lifetime = 10.0f;
            break;
        }

        case TF_WEAPON_CROSSBOW:
        case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
        {
            g_cl.projectile_fire_setup( player, weapon, firing_angles, get_fire_offset( weapon, ducking ), false,
                                        out.m_position, out.m_angles, target );

            const bool  scaled_bolt{ weapon->GetWeaponID( ) == TF_WEAPON_CROSSBOW };
            const float bolt_hull{ scaled_bolt ? 1.0f * 3.0f : 1.0f };

            // both weapons hardcode a 0.75 charge fraction rather than ramping
            out.m_speed    = 2400.0f;
            out.m_gravity  = 0.2f;
            out.m_hull     = vec3( bolt_hull, bolt_hull, bolt_hull );
            out.m_movetype = projectile_movetype::fly_gravity;
            out.m_lifetime = 10.0f;
            break;
        }

        case TF_WEAPON_SYRINGEGUN_MEDIC:
        {
            g_cl.projectile_fire_setup( player, weapon, firing_angles, get_fire_offset( weapon, ducking ), true,
                                        out.m_position, out.m_angles, target );

            if ( command != 0 )
            {
                static i32  cached_command{ };
                static vec2 jitter{ };
                static bool cached_angular{ };
                const bool  angular{ angular_spread != 0.0f };
                if ( command != cached_command || angular != cached_angular )
                {
                    cached_command = command;
                    cached_angular = angular;
                    g_crits.seed_launch_random( command, angular );
                    jitter = { g_tf2.m_random.RandomFloat( -1.5f, 1.5f ),
                               g_tf2.m_random.RandomFloat( -1.5f, 1.5f ) };
                }
                out.m_angles.x += jitter.x;
                out.m_angles.y += jitter.y;
            }

            out.m_speed    = 1000.0f;
            out.m_gravity  = 0.3f;
            out.m_hull     = vec3( 1.0f, 1.0f, 1.0f );
            out.m_movetype = projectile_movetype::fly_gravity;
            out.m_lifetime = 5.0f;
            break;
        }

        case TF_WEAPON_GRENADELAUNCHER:
        case TF_WEAPON_CANNON:
        {
            if ( !g_cl.projectile_grenade_setup( player, weapon, firing_angles, out.m_position ) )
            {
                out.m_blocked = true;
            }

            const bool cannon{ weapon->GetWeaponID( ) == TF_WEAPON_CANNON };

            out.m_angles = firing_angles;
            out.m_speed  = g_cl.attrib_hook( weapon,
                                             precision ? 3000.0f
                                                       : g_cl.attrib_hook( weapon, 1200.0f,
                                                                           ATLAS_XORSTR( "mult_projectile_speed" ).get( ) ),
                                             ATLAS_XORSTR( "mult_projectile_range" ).get( ) );

            out.m_gravity  = 1.0f;
            out.m_up_kick  = 200.0f;
            out.m_movetype = projectile_movetype::vphysics;
            out.m_model    = cannon ? projectile_model::cannonball : projectile_model::pipe;
            if ( const collide_info_t *info{ resolve_collide( out.m_model ) } )
            {
                out.m_hull        = info->m_half_extents;
                out.m_hull_centre = info->m_centre;
            } else
            {
                return false; // model is not loaded; refuse rather than invent a shape
            }

            if ( cannon )
            {
                const float fuse{ g_cl.attrib_hook(
                    weapon, 0.0f, ATLAS_XORSTR( "grenade_launcher_mortar_mode" ).get( ) ) };
                const float detonate{ weapon->As< C_TFGrenadeLauncher * >( )->m_flDetonateTime( ) };

                const float remaining{ charge_time >= 0.0f ? fuse - charge_time
                                       : detonate > 0.0f   ? detonate - charge_now
                                                           : fuse };

                out.m_lifetime = fuse > 0.0f ? detonate_poll( math::bigger( remaining, 0.0f ) ) : 2.0f;
            } else
            {
                out.m_lifetime = detonate_poll( g_cl.attrib_hook( weapon, 2.0f, ATLAS_XORSTR( "fuse_mult" ).get( ) ) );
            }

            apply_launch_randomness( out, weapon, command, angular_spread > 0.0f );
            break;
        }

        case TF_WEAPON_PIPEBOMBLAUNCHER:
        {
            if ( !g_cl.projectile_grenade_setup( player, weapon, firing_angles, out.m_position ) )
            {
                out.m_blocked = true;
            }

            const float begin{ weapon->As< C_TFPipebombLauncher * >( )->m_flChargeBeginTime( ) };
            const float held{ begin > 0.0f ? charge_now - begin : 0.0f };
            const float charge{ charge_time >= 0.0f ? charge_time : held };
            const float max_charge{ g_cl.attrib_hook( weapon, 4.0f, ATLAS_XORSTR( "stickybomb_charge_rate" ).get( ) ) };

            out.m_angles = firing_angles;
            out.m_speed  = g_cl.attrib_hook( weapon,
                                             math::remap_clamped( charge, 0.0f, max_charge, 900.0f, 2400.0f ),
                                             ATLAS_XORSTR( "mult_projectile_range" ).get( ) );

            out.m_gravity  = 1.0f;
            out.m_up_kick  = 200.0f;
            out.m_movetype = projectile_movetype::vphysics;
            out.m_model    = projectile_model::sticky;
            if ( const collide_info_t *info{ resolve_collide( out.m_model ) } )
            {
                out.m_hull        = info->m_half_extents;
                out.m_hull_centre = info->m_centre;
            } else
            {
                return false; // model is not loaded; refuse rather than invent a shape
            }
            out.m_lifetime = 8.0f;

            apply_launch_randomness( out, weapon, command, angular_spread > 0.0f );
            break;
        }

        case TF_WEAPON_JAR:
        case TF_WEAPON_JAR_MILK:
        case TF_WEAPON_JAR_GAS:
        {
            if ( !g_cl.projectile_grenade_setup( player, weapon, angles, out.m_position ) )
            {
                out.m_blocked = true;
                return false;
            }

            const bool gas{ weapon->GetWeaponID( ) == TF_WEAPON_JAR_GAS };

            out.m_angles           = angles;
            out.m_speed            = gas ? 2000.0f : 1000.0f;
            out.m_gravity          = 1.0f;
            out.m_up_kick          = 200.0f;
            out.m_angular_velocity = vec3( 300.0f, 0.0f, 0.0f );
            out.m_movetype         = projectile_movetype::vphysics;
            out.m_model            = gas ? projectile_model::gascan
                                         : ( weapon->GetWeaponID( ) == TF_WEAPON_JAR_MILK
                                                 ? projectile_model::madmilk
                                                 : projectile_model::jarate );
            if ( const collide_info_t *info{ resolve_collide( out.m_model ) } )
            {
                out.m_hull        = info->m_half_extents;
                out.m_hull_centre = info->m_centre;
            } else
            {
                return false;
            }
            out.m_lifetime = 2.2f;
            break;
        }

        case TF_WEAPON_CLEAVER:
        {
            if ( !g_cl.projectile_grenade_setup( player, weapon, angles, out.m_position ) )
            {
                out.m_blocked = true;
                return false;
            }

            out.m_angles           = angles;
            out.m_speed            = 3000.0f;
            out.m_gravity          = 1.0f;
            out.m_angular_velocity = vec3( 0.0f, 500.0f, 0.0f );
            out.m_movetype         = projectile_movetype::vphysics;
            out.m_model            = projectile_model::cleaver;
            if ( const collide_info_t *info{ resolve_collide( out.m_model ) } )
            {
                out.m_hull        = info->m_half_extents;
                out.m_hull_centre = info->m_centre;
            } else
            {
                return false;
            }
            out.m_lifetime = 2.2f;
            break;
        }

        default:
        {
            return false;
        }
    }

    out.m_valid = true;
    return true;
}

// a sticky already in flight, from its networked state instead of a launch; the spin is not networked,
// so spin is the caller's estimate in local degrees per second
bool projectile_sim::resume_sticky( C_BaseEntity *pipe, const vec3 &position, const vec3 &velocity, const vec3 &spin,
                                    projectile_setup_t &out ) const
{
    const collide_info_t *info{ resolve_collide( projectile_model::sticky ) };
    if ( !info )
    {
        return false;
    }

    vec3 forward{ }, right{ }, up{ };
    math::angle_vectors( pipe->m_angRotation( ), &forward, &right, &up );

    out = projectile_setup_t{ };

    // arm_physics builds the velocity back on the same basis
    out.m_position     = position;
    out.m_angles       = pipe->m_angRotation( );
    out.m_speed        = velocity.dot( forward );
    out.m_jitter_right = velocity.dot( right );
    out.m_up_kick      = velocity.dot( up );
    out.m_gravity      = 1.0f;
    out.m_lifetime     = 8.0f;
    out.m_movetype     = projectile_movetype::vphysics;
    out.m_model        = projectile_model::sticky;
    out.m_hull         = info->m_half_extents;
    out.m_hull_centre  = info->m_centre;

    out.m_angular_velocity = spin * 0.5f;
    out.m_valid            = true;
    return true;
}

class projectile_contact final : public IPhysicsCollisionEvent
{
public:
    void watch( IPhysicsObject *object )
    {
        this->m_object = object;
        this->m_hit    = false;
    }

    void PreCollision( vcollisionevent_t * ) override { }

    void PostCollision( vcollisionevent_t *event ) override
    {
        if ( this->m_hit )
        {
            return;
        }

        const bool first{ event->pObjects[ 0 ] == this->m_object };
        if ( !first && event->pObjects[ 1 ] != this->m_object )
        {
            return;
        }

        vec3 normal{ };
        event->pInternalData->GetSurfaceNormal( normal );
        event->pInternalData->GetContactPoint( this->m_point );

        this->m_normal  = first ? -normal : normal; // out of the surface, toward the projectile
        this->m_surface = event->surfaceProps[ first ? 1 : 0 ];
        this->m_hit     = true;
    }

    void Friction( IPhysicsObject *, float, int, int, IPhysicsCollisionData * ) override { }
    void StartTouch( IPhysicsObject *, IPhysicsObject *, IPhysicsCollisionData * ) override { }
    void EndTouch( IPhysicsObject *, IPhysicsObject *, IPhysicsCollisionData * ) override { }
    void FluidStartTouch( IPhysicsObject *, IPhysicsFluidController * ) override { }
    void FluidEndTouch( IPhysicsObject *, IPhysicsFluidController * ) override { }
    void PostSimulationFrame( ) override { }
    void ObjectEnterTrigger( IPhysicsObject *, IPhysicsObject * ) override { }
    void ObjectLeaveTrigger( IPhysicsObject *, IPhysicsObject * ) override { }

    IPhysicsObject *m_object{ };
    vec3            m_point{ };
    vec3            m_normal{ };
    i32             m_surface{ };
    bool            m_hit{ };
};

static constinit projectile_contact s_contact{ };

class solid_defaults final : public IVPhysicsKeyHandler
{
public:
    void ParseKeyValue( void *, const char *, const char * ) override { }

    void SetDefaults( void *data ) override
    {
        static_cast< solid_t * >( data )->params = g_PhysDefaultObjectParams;
    }
};

static bool build_world( IPhysicsEnvironment *environment )
{
    const vcollide_t *world{ g_tf2.m_model_info->GetVCollide( 1 ) };
    if ( !world || world->solidCount < 1 || !world->solids[ 0 ] )
    {
        return false;
    }

    objectparams_t params{ g_PhysDefaultObjectParams };
    const i32      surface{ g_tf2.m_physics_surface_props->GetSurfaceIndex( ATLAS_XORSTR( "default" ).get( ) ) };
    environment->CreatePolyObjectStatic( world->solids[ 0 ], surface, vec3( ), vec3( ), &params );

    const char *text{ world->pKeyValues };
    const usize length{ text ? str::length( text ) : 0 };
    usize       cursor{ };
    key_block_t block{ };
    bool        terrain{ };
    while ( next_block( text, length, cursor, block ) )
    {
        if ( block.named( ATLAS_XORSTR( "virtualterrain" ).get( ) ) )
        {
            terrain = true;
            continue;
        }

        i32 solid{ };
        if ( !( block.named( ATLAS_XORSTR( "solid" ).get( ) ) || block.named( ATLAS_XORSTR( "staticsolid" ).get( ) ) ) ||
             !read_int( block, ATLAS_XORSTR( "index" ).get( ), solid ) || solid == 0 )
        {
            continue;
        }

        if ( solid >= world->solidCount || !world->solids[ solid ] )
        {
            terrain = true;
            continue;
        }

        // CCollisionEvent::ShouldCollide drops a pair whose contents miss the entity's solid mask, MASK_SOLID for
        // grenades, so they fly through the playerclip and monsterclip solids
        i32 contents{ CONTENTS_SOLID }; // CSolidSetDefaults::SetDefaults
        read_int( block, ATLAS_XORSTR( "contents" ).get( ), contents );
        if ( !( contents & MASK_SOLID ) )
        {
            continue;
        }

        environment->CreatePolyObjectStatic( world->solids[ solid ], surface, vec3( ), vec3( ), &params );
    }

    if ( terrain )
    {
        for ( i32 index{ }; index < VIRTUAL_TERRAIN_COUNT; ++index )
        {
            if ( const CPhysCollide *displacement{ g_tf2.m_model_info->GetCollideForVirtualTerrain( index ) } )
            {
                environment->CreatePolyObjectStatic( displacement, surface, vec3( ), vec3( ), &params );
            }
        }
    }

    solid_defaults defaults{ };
    g_tf2.m_static_prop_mgr->CreateVPhysicsRepresentations( environment, &defaults, nullptr );
    return true;
}

bool projectile_sim::arm_physics( const projectile_setup_t &setup )
{
    if ( !this->m_environment )
    {
        this->m_environment = g_tf2.m_physics->CreateEnvironment( );
        if ( !this->m_environment )
        {
            return false;
        }

        physics_performanceparams_t performance{ };
        performance.Defaults( );
        performance.maxCollisionsPerObjectPerTimestep = 10; // CPhysicsHook::LevelInitPreEntity
        this->m_environment->SetPerformanceSettings( &performance );
        this->m_environment->SetAirDensity( 2.0f );
        this->m_environment->SetCollisionEventHandler( &s_contact );

        if ( !build_world( this->m_environment ) )
        {
            this->release_physics( );
            return false;
        }
    }

    const collide_info_t *info{ resolve_collide( setup.m_model ) };
    if ( !info )
    {
        return false;
    }

    if ( this->m_object && this->m_object->m_pCollide != info->m_collide )
    {
        this->m_environment->DestroyObject( this->m_object );
        this->m_object = nullptr;
    }

    if ( !this->m_object )
    {
        objectparams_t params{ info->m_params };
        this->m_object = this->m_environment->CreatePolyObject( info->m_collide, info->m_material, setup.m_position,
                                                                setup.m_angles, &params );
        if ( !this->m_object )
        {
            return false;
        }
    }

    vec3 forward{ }, right{ }, up{ };
    math::angle_vectors( setup.m_angles, &forward, &right, &up );

    vec3 velocity{ forward * setup.m_speed + up * setup.m_up_kick +
                   right * setup.m_jitter_right + up * setup.m_jitter_up };

    if ( setup.m_model == projectile_model::cleaver )
    {
        // CTFCleaver::GetVelocityVector normalises a fixed forward/up blend instead of using the
        // launch direction directly
        velocity = forward * 10.0f + up;
        velocity.normalize( );
        velocity *= setup.m_speed;
    }

    if ( const float gravity{ -( this->sv_gravity( ) * setup.m_gravity ) }; gravity != this->m_environment_gravity )
    {
        this->m_environment->SetGravity( vec3( 0.0f, 0.0f, gravity ) );
        this->m_environment_gravity = gravity;
    }

    if ( TICK_INTERVAL != this->m_environment_timestep )
    {
        this->m_environment->SetSimulationTimestep( TICK_INTERVAL );
        this->m_environment_timestep = TICK_INTERVAL;
    }

    const AngularImpulse spin{ setup.m_angular_velocity * 2.0f };

    this->m_object->SetPosition( setup.m_position, setup.m_angles, true );
    this->m_object->SetVelocity( &velocity, &spin );
    this->m_object->Wake( );

    this->m_environment->ResetSimulationClock( );
    this->m_environment->Simulate( TICK_INTERVAL );

    this->m_object->SetPosition( setup.m_position, setup.m_angles, true );
    this->m_object->SetVelocity( &velocity, &spin );
    this->m_object->Wake( );

    s_contact.watch( this->m_object ); // whatever the warm-up step touched is not part of the flight
    return true;
}

void projectile_sim::release_physics( )
{
    if ( this->m_environment && this->m_object )
    {
        this->m_environment->DestroyObject( this->m_object );
        this->m_object = nullptr;
    }

    if ( this->m_environment )
    {
        g_tf2.m_physics->DestroyEnvironment( this->m_environment );
        this->m_environment = nullptr;
    }

    this->m_environment_gravity  = 0.0f;
    this->m_environment_timestep = 0.0f;
}

void projectile_sim::shutdown( )
{
    this->clear_path( );
    this->release_physics( );
    forget_collides( );
    g_splash.clear( );
}

i32 projectile_sim::simulate( const projectile_setup_t &setup, vec3 *path, i32 max_ticks, i32 trace_interval, impact_t &impact, C_BaseEntity *target, projectile_work_t *work, const projectile_target_t *intercept )
{
    impact = impact_t{ };

    if ( !setup.m_valid || setup.m_blocked || !path || max_ticks <= 0 )
    {
        return 0;
    }

    const i32 limit{ math::smaller( max_ticks, TIME_TO_TICKS( setup.m_lifetime ) ) };
    if ( limit <= 0 )
    {
        return 0;
    }

    const bool physics{ setup.m_movetype == projectile_movetype::vphysics };
    const bool sticky{ setup.m_model == projectile_model::sticky || setup.m_model == projectile_model::sticky_practice };
    if ( work && !work->flight( ) )
    {
        impact.m_deferred = true;
        return 0;
    }
    if ( physics && work && !work->step( ) )
    {
        impact.m_deferred = true;
        return 0;
    }
    if ( physics && !this->arm_physics( setup ) )
    {
        return 0;
    }

    const collide_info_t *info{ physics ? resolve_collide( setup.m_model ) : nullptr };
    const CPhysCollide   *collide{ info ? info->m_collide : nullptr };

    if ( physics && !collide )
    {
        return 0;
    }

    vec3 forward{ };
    math::angle_vectors( setup.m_angles, &forward );

    vec3 position{ setup.m_position };

    vec3        velocity{ forward * setup.m_speed };
    const float gravity{ this->sv_gravity( ) * setup.m_gravity };
    const float dt{ TICK_INTERVAL };

    const vec3  drift{ forward * setup.m_carry.dot( forward ) + vec3( 0.0f, 0.0f, setup.m_rise ) };
    const float keep{ math::bigger( 1.0f - setup.m_drag * dt, 0.0f ) };

    CTraceFilterProjectile filter{ };
    filter.m_pSkip         = g_cl.m_local;
    filter.m_pSkipTarget   = target;
    filter.m_players_block = sticky && target != nullptr;
    filter.m_entities_only = physics;

    i32 written{ };

    struct step_t
    {
        vec3 m_from{ };
        vec3 m_to{ };
        vec3 m_hull{ };
        vec3 m_centre{ };
        vec3 m_angles{ };
    };

    constexpr i32 MAX_TRACE_INTERVAL{ 16 };

    const i32 interval{ math::clamp( trace_interval, 1, MAX_TRACE_INTERVAL ) };

    step_t steps[ MAX_TRACE_INTERVAL ]{ };
    i32    step_count{ };

    // index of the step the projectile stopped on, or -1 when the stretch was clear
    const auto sweep_steps{ [ & ]( ) -> i32
    {
        const auto cast{ [ & ]( const vec3 &from, const vec3 &to, const vec3 &mins,
                                const vec3 &maxs, trace_t &out )
        {
            Ray_t ray{ };

            if ( mins.is_zero( ) && maxs.is_zero( ) )
            {
                ray.Init( from, to );
            } else
            {
                ray.Init( from, to, mins, maxs );
            }

            g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &out );
        } };

        const auto walk{ [ & ]( ) -> i32
        {
            for ( i32 at{ }; at < step_count; ++at )
            {
                const step_t &step{ steps[ at ] };
                filter.m_skip_team = filter.m_players_block && TICKS_TO_TIME( written - step_count + at ) < 0.25f ? g_cl.m_local->m_iTeamNum( ) : 0;

                trace_t fine{ };
                cast( step.m_from, step.m_to, step.m_centre - step.m_hull,
                      step.m_centre + step.m_hull, fine );

                bool exact{ false };
                if ( sticky )
                {
                    const vec3 low{ math::smaller( step.m_from.x, step.m_to.x ), math::smaller( step.m_from.y, step.m_to.y ), math::smaller( step.m_from.z, step.m_to.z ) };
                    const vec3 high{ math::bigger( step.m_from.x, step.m_to.x ), math::bigger( step.m_from.y, step.m_to.y ), math::bigger( step.m_from.z, step.m_to.z ) };
                    const vec3 mins{ low + step.m_centre - step.m_hull };
                    const vec3 maxs{ high + step.m_centre + step.m_hull };
                    if ( fine.fraction < 1.0f || fine.startsolid )
                    {
                        const trace_t engine{ fine };
                        exact = g_splash.sweep( step.m_from, step.m_to, step.m_angles, collide, mins, maxs, fine, filter, work );
                        if ( work && work->m_exhausted )
                        {
                            impact.m_deferred = true;
                            return at;
                        }
                        if ( !exact )
                        {
                            fine.startsolid = true;
                        } else if ( fine.fraction >= 1.0f && !fine.startsolid && engine.fraction < 1.0f && !engine.startsolid &&
                                    !( engine.m_pEnt && engine.m_pEnt->GetClientClass( )->GetClassID( ) == ETFClassID::CTFPlayer ) )
                        {
                            fine  = engine;
                            exact = false;
                        }
                    }
                }

                if ( fine.fraction < 1.0f || fine.startsolid )
                {
                    impact.m_hit      = true;
                    impact.m_normal   = fine.plane.normal;
                    impact.m_position = fine.endpos;
                    impact.m_time     = ( static_cast< f32 >( written - step_count + at ) + fine.fraction ) * dt;
                    impact.m_trace    = fine;

                    if ( sticky && exact && splash::can_attach( fine ) )
                    {
                        const vec3 support{ g_tf2.m_physics_collision->CollideGetExtent( collide, fine.endpos, step.m_angles, -fine.plane.normal ) };
                        const vec3 contact{ fine.endpos + fine.plane.normal * ( support - fine.endpos ).dot( fine.plane.normal ) };
                        Ray_t      surface_ray{ };
                        trace_t    surface{ };
                        surface_ray.Init( contact + fine.plane.normal, contact - fine.plane.normal );
                        g_tf2.m_trace->TraceRay( surface_ray, MASK_SOLID, &filter, &surface );
                        if ( surface.fraction < 1.0f && !surface.startsolid && !surface.allsolid &&
                             surface.m_pEnt == fine.m_pEnt && surface.plane.normal.dot( fine.plane.normal ) > 0.99f )
                        {
                            impact.m_trace    = surface;
                            impact.m_attached = splash::can_attach( surface );
                        }
                    }

                    return at;
                }
            }

            return -1;
        } };

        if ( step_count == 1 )
        {
            return walk( );
        }

        const vec3 &start{ steps[ 0 ].m_from };
        const vec3 &end{ steps[ step_count - 1 ].m_to };

        // a tick's own sweep starts and ends within the sag of the chord and its interior is a
        // blend of the two, so growing the chord's box by that sag covers all of it
        vec3 sag{ };
        vec3 mins{ steps[ 0 ].m_centre - steps[ 0 ].m_hull };
        vec3 maxs{ steps[ 0 ].m_centre + steps[ 0 ].m_hull };

        for ( i32 at{ }; at < step_count; ++at )
        {
            const step_t &step{ steps[ at ] };

            const vec3 low{ step.m_centre - step.m_hull };
            const vec3 high{ step.m_centre + step.m_hull };

            mins = vec3( math::smaller( mins.x, low.x ), math::smaller( mins.y, low.y ),
                         math::smaller( mins.z, low.z ) );
            maxs = vec3( math::bigger( maxs.x, high.x ), math::bigger( maxs.y, high.y ),
                         math::bigger( maxs.z, high.z ) );

            if ( at < step_count - 1 && setup.m_movetype != projectile_movetype::fly )
            {
                const vec3 off{ math::axis_offset_from_segment( step.m_to, start, end ) };

                sag = vec3( math::bigger( sag.x, off.x ), math::bigger( sag.y, off.y ),
                            math::bigger( sag.z, off.z ) );
            }
        }

        trace_t coarse{ };
        filter.m_skip_team = 0;
        cast( start, end, mins - sag, maxs + sag, coarse );

        if ( coarse.fraction >= 1.0f && !coarse.startsolid )
        {
            return -1;
        }

        return walk( ); // the widened box touched something; only the real path can say what
    } };

    for ( i32 tick{ }; tick < limit; ++tick )
    {
        if ( work && !work->step( ) )
        {
            impact.m_deferred = true;
            impact.m_time     = TICKS_TO_TIME( written );
            return written;
        }
        const vec3 previous{ position };

        vec3 sweep_hull{ setup.m_hull };
        vec3 sweep_centre{ setup.m_hull_centre };
        vec3 sweep_angles{ setup.m_angles };
        bool landed{ };

        if ( physics )
        {
            this->m_environment->Simulate( dt );
            this->m_object->GetPosition( &position, &sweep_angles );
            landed = s_contact.m_hit;
            vec3 mins{ }, maxs{ };
            g_tf2.m_physics_collision->CollideGetAABB( &mins, &maxs, collide, vec3( ), sweep_angles );

            sweep_hull   = ( maxs - mins ) * 0.5f;
            sweep_centre = ( maxs + mins ) * 0.5f;
        } else if ( setup.m_movetype == projectile_movetype::fly_gravity )
        {
            // CBaseEntity::PhysicsAddGravityMove advances .z on the average of the velocity before
            // and after the tick, so it is trapezoidal
            // . . . does the word "trapezoidal" even exist?
            const float next_z{ velocity.z - gravity * dt };

            position.x += velocity.x * dt;
            position.y += velocity.y * dt;
            position.z += ( ( velocity.z + next_z ) * 0.5f ) * dt;

            velocity.z = next_z;
        } else if ( setup.m_movetype == projectile_movetype::flame )
        {
            position += ( velocity + drift ) * dt;
            velocity *= keep;
        } else
        {
            position += velocity * dt;
        }

        f32 target_fraction{ FLT_MAX };
        if ( intercept )
        {
            target_fraction = math::segment_box_fraction( previous - intercept->m_track[ tick ], position - intercept->m_track[ tick + 1 ],
                                                          intercept->m_mins, intercept->m_maxs );
            if ( target_fraction != FLT_MAX && landed )
            {
                const vec3 along{ position - previous };
                if ( along.dot( s_contact.m_point - previous ) < along.length_sqr( ) * target_fraction )
                {
                    target_fraction = FLT_MAX;
                } else
                {
                    landed = false;
                }
            }
            if ( target_fraction != FLT_MAX )
            {
                position = previous + ( position - previous ) * target_fraction;
            }
        }
        steps[ step_count++ ] = step_t{ previous, position, sweep_hull, sweep_centre, sweep_angles };

        path[ written ] = position;
        ++written;

        if ( target_fraction == FLT_MAX && !landed && step_count < interval && tick < limit - 1 )
        {
            continue;
        }

        const i32 stopped{ sweep_steps( ) };
        if ( impact.m_deferred )
        {
            impact.m_time = TICKS_TO_TIME( written );
            return written;
        }

        if ( stopped >= 0 )
        {
            if ( target_fraction != FLT_MAX && stopped == step_count - 1 )
            {
                impact.m_time = tick * dt + ( impact.m_time - tick * dt ) * target_fraction;
            }
            // everything past the impact was integrated on speculation
            written             = written - step_count + stopped + 1;
            path[ written - 1 ] = impact.m_position;

            break;
        }

        if ( landed )
        {
            impact.m_hit      = true;
            impact.m_position = position;
            impact.m_normal   = s_contact.m_normal;
            impact.m_time     = TICKS_TO_TIME( written );

            Ray_t                         ray{ };
            CTraceFilterWorldAndPropsOnly world{ };
            ray.Init( s_contact.m_point + s_contact.m_normal, s_contact.m_point - s_contact.m_normal );
            g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &world, &impact.m_trace );

            impact.m_attached = sticky && g_tf2.m_physics_surface_props->GetSurfaceData( s_contact.m_surface )->game.material != 'X';
            break;
        }

        if ( target_fraction != FLT_MAX )
        {
            impact.m_target_hit = true;
            impact.m_position   = position;
            impact.m_time       = ( tick + target_fraction ) * dt;
            break;
        }

        step_count = 0;
    }

    return written;
}

void projectile_sim::compensate( )
{
    if ( g_holdaim_mgr.held( ) || g_ragebot.aimed_projectile_this_command( ) ||
         !g_cl.will_fire_this_tick( ) || g_cl.m_cmd->viewangles.z != 0.0f )
    {
        return;
    }

    const i32   id{ g_cl.m_weapon->GetWeaponID( ) };
    const bool  pipe{ id == TF_WEAPON_GRENADELAUNCHER || id == TF_WEAPON_PIPEBOMBLAUNCHER ||
                      id == TF_WEAPON_CANNON };
    const float angular_spread{ g_cl.attrib_hook( g_cl.m_weapon, 0.0f,
                                                  ATLAS_XORSTR( "projectile_spread_angle" ).get( ) ) };
    const bool  bow{ id == TF_WEAPON_COMPOUND_BOW &&
                     g_cl.m_weapon->As< C_TFPipebombLauncher * >( )->m_flChargeBeginTime( ) > 0.0f &&
                     TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) -
                             g_cl.m_weapon->As< C_TFPipebombLauncher * >( )->m_flChargeBeginTime( ) >=
                         5.0f };
    if ( !pipe && id != TF_WEAPON_SYRINGEGUN_MEDIC && !bow && angular_spread == 0.0f )
    {
        return;
    }

    projectile_setup_t launch{ };
    const vec3         original{ g_cl.m_cmd->viewangles };
    const i32          command{ g_cl.m_cmd->command_number };
    if ( !this->gather( g_cl.m_local, g_cl.m_weapon, original, launch, command ) || launch.m_blocked )
    {
        return;
    }

    vec3 corrected{ original };
    if ( !pipe )
    {
        projectile_setup_t reference{ };
        if ( !this->gather( g_cl.m_local, g_cl.m_weapon, original, reference ) )
        {
            return;
        }
        const vec3 desired{ reference.m_angles };
        for ( i32 iteration{ }; iteration < 4; ++iteration )
        {
            if ( iteration > 0 &&
                 !this->gather( g_cl.m_local, g_cl.m_weapon, corrected, launch, command ) )
            {
                return;
            }
            const vec3 error{ math::normalize_yaw( launch.m_angles.x - desired.x ),
                              math::normalize_yaw( launch.m_angles.y - desired.y ), 0.0f };
            if ( error.length_sqr( ) < 0.000001f )
            {
                if ( !bow && id != TF_WEAPON_SYRINGEGUN_MEDIC &&
                     ( launch.m_position - reference.m_position ).length_sqr( ) > 0.0001f )
                {
                    return;
                }
                g_cl.m_cmd->viewangles = corrected;
                return;
            }
            corrected -= error;
            if ( corrected.x < -89.0f || corrected.x > 89.0f )
            {
                return;
            }
            corrected.y = math::normalize_yaw( corrected.y );
        }
        return;
    }

    vec3 forward{ }, up{ };
    math::angle_vectors( original, &forward, nullptr, &up );
    const vec3 direction{ forward * launch.m_speed + up * launch.m_up_kick };
    const vec3 angular_offset{ launch.m_angles - original };
    if ( !math::launch_angles( direction, launch.m_speed, launch.m_jitter_right,
                               launch.m_up_kick + launch.m_jitter_up, corrected ) )
    {
        return;
    }
    corrected -= angular_offset;
    if ( corrected.x < -89.0f || corrected.x > 89.0f )
    {
        return;
    }
    corrected.y = math::normalize_yaw( corrected.y );
    if ( this->gather( g_cl.m_local, g_cl.m_weapon, corrected, launch, command ) && !launch.m_blocked )
    {
        g_cl.m_cmd->viewangles = corrected;
    }
}

void projectile_sim::capture_shot( )
{
    this->m_shot_setup.m_valid = false;

    if ( !g_cfg.m_projectile_path.m_enabled || !g_cl.m_will_fire || g_cl.m_cmd->weaponselect != 0 )
    {
        return;
    }

    // capture before ItemPostFrame consumes the charge and prediction restores the shoot position.
    const vec3 *held{ g_holdaim_mgr.held( ) };
    this->m_shot_angles  = held ? *held : g_cl.m_cmd->viewangles;
    this->m_shot_command = g_cl.m_cmd->command_number;
    this->gather( g_cl.m_local, g_cl.m_weapon, this->m_shot_angles, this->m_shot_setup, this->m_shot_command );
}

// did we hit something beside the world itself?
static void end_at_contact( vec3 *path, i32 &count, const vec3 &contact, impact_t &impact )
{
    constexpr f32 TOLERANCE{ 4.0f };
    for ( i32 at{ 1 }; at < count; ++at )
    {
        const vec3 from{ path[ at - 1 ] };
        const vec3 along{ path[ at ] - from };
        const f32  length_sqr{ along.length_sqr( ) };
        if ( length_sqr <= 0.0f )
        {
            continue;
        }

        const vec3 nearest{ from + along * math::clamp( ( contact - from ).dot( along ) / length_sqr, 0.0f, 1.0f ) };
        if ( ( contact - nearest ).length( ) > TOLERANCE )
        {
            continue;
        }

        if ( !impact.m_hit || at != count - 1 )
        {
            impact.m_normal = -along.normalized( );
        }

        path[ at ]        = nearest;
        count             = at + 1;
        impact.m_hit      = true;
        impact.m_position = nearest;
        return;
    }
}

void projectile_sim::on_create_move( )
{
    if ( !g_cfg.m_projectile_path.m_enabled )
    {
        this->clear_path( );
        return;
    }

    if ( !this->m_shot_setup.m_valid )
    {
        return;
    }

    projectile_setup_t launch{ this->m_shot_setup };
    this->m_shot_setup.m_valid = false;

    const i32 frame{ g_tickbase.send_frame( ) };
    if ( !g_cl.m_will_fire || frame < 0 )
    {
        return;
    }

    const vec3 delta{ this->m_shot_angles - g_cl.m_cmd->viewangles };
    for ( i32 axis{ 0 }; axis < 3; ++axis )
    {
        if ( math::abs( math::normalize_yaw( delta[ axis ] ) ) > 0.001f )
        {
            return;
        }
    }

    if ( this->m_shot_command != g_cl.m_cmd->command_number )
    {
        if ( launch.m_movetype != projectile_movetype::vphysics ||
             g_cl.attrib_hook( g_cl.m_weapon, 0.0f, ATLAS_XORSTR( "projectile_spread_angle" ).get( ) ) != 0.0f )
        {
            return;
        }
        this->apply_launch_randomness( launch, g_cl.m_weapon, g_cl.m_cmd->command_number );
    }

    impact_t  impact{ };
    const i32 ticks{ math::clamp( TIME_TO_TICKS( launch.m_lifetime ), 1, MAX_PATH_POINTS - 1 ) };
    const i32 interval{ math::clamp( static_cast< i32 >( math::sqrt( static_cast< f32 >( ticks ) ) ), 1, 12 ) };

    this->m_shot_path[ 0 ] = launch.m_position;
    this->m_shot_count     = 1 + this->simulate( launch, &this->m_shot_path[ 1 ], ticks, interval, impact );
    this->m_shot_time      = g_tf2.m_engine->Plat_FloatTime( );

    const i32 spawn{ g_cl.is_delayed_throw_weapon( g_cl.m_weapon ) ? g_cl.throw_spawn_tick( )
                                                                   : frame + g_networking.send_delay( ).ticks( ) };
    this->m_shot_tick = launch.m_movetype == projectile_movetype::vphysics ? spawn - 1 : spawn;

    vec3 contact{ };
    if ( g_ragebot.aimed_projectile_this_command( ) && g_ragebot.visual_aim( contact ) )
    {
        end_at_contact( this->m_shot_path, this->m_shot_count, contact, impact );
    }

    this->m_shot_hit = impact.m_hit;

    if ( !impact.m_hit )
    {
        return;
    }

    math::vector_angles( impact.m_normal, this->m_impact_angles );
}

void projectile_sim::render( ) const
{
    const auto &settings{ g_cfg.m_projectile_path };
    if ( !settings.m_enabled || this->m_shot_count < 2 || !g_tf2.m_engine->IsInGame( ) || g_gui.open( ) )
    {
        return;
    }

    const f32  duration{ math::bigger( settings.m_duration, 0.1f ) };
    const f32  age{ static_cast< f32 >( g_tf2.m_engine->Plat_FloatTime( ) - this->m_shot_time ) };
    const bool animate{ settings.m_animate };
    const f32  shown{ math::smaller( g_tf2.m_globals->curtime - TICK_INTERVAL,
                                     TICKS_TO_TIME( g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick ) ) };
    const f32  traveled{ shown - TICKS_TO_TIME( this->m_shot_tick ) };
    const f32  travel_duration{ static_cast< f32 >( this->m_shot_count - 1 + ragebot::PATH_TRAVEL_FADE_TICKS ) * TICK_INTERVAL };
    if ( animate ? traveled >= travel_duration : age >= duration )
    {
        return;
    }

    const f32 alpha{ animate ? 1.0f : math::clamp( 1.0f - age / duration, 0.0f, 1.0f ) };

    color_t value{ settings.m_color };
    value.a = static_cast< u8 >( static_cast< f32 >( value.a ) * alpha );
    const ragebot::path_pen_t pen{ ragebot::path_pen( settings.m_style, settings.m_outline ) };

    for ( i32 at{ 1 }; at < this->m_shot_count; ++at )
    {
        const f32 travel_alpha{ animate ? ragebot::path_travel_alpha( at, traveled ) : 1.0f };
        if ( travel_alpha <= 0.0f )
        {
            continue;
        }

        color_t segment_color{ value };
        segment_color.a = static_cast< u8 >( static_cast< f32 >( value.a ) * travel_alpha );
        ragebot::draw_path_step( pen, this->m_shot_path, this->m_shot_count, at, segment_color );
    }

    if ( !settings.m_impact_box || !this->m_shot_hit ||
         ( animate && traveled < static_cast< f32 >( this->m_shot_count - 1 ) * TICK_INTERVAL ) )
    {
        return;
    }

    color_t   box{ settings.m_impact_color };
    const f32 impact_alpha{ animate ? ragebot::path_travel_alpha( this->m_shot_count - 1, traveled ) : alpha };
    box.a = static_cast< u8 >( static_cast< f32 >( box.a ) * impact_alpha );

    const vec3 low{ -2.0f, -2.0f, -2.0f };
    const vec3 high{ 2.0f, 2.0f, 2.0f };
    g_cl.draw_3d_box( this->m_shot_path[ this->m_shot_count - 1 ], low, high, box, this->m_impact_angles );
}

void projectile_sim::clear_path( )
{
    this->m_shot_setup.m_valid = false;
    this->m_shot_count         = 0;
}
