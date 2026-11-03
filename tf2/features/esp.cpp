#include "esp.hpp"
#include "entity_cache.hpp"
#include "lagcomp.hpp"

#include "player_cloud.hpp"
#include "player_data.hpp"
#include "ragebot.hpp"
#include "render/icons.hpp"

#include "streamer_mode.hpp"

ATLAS_NO_DESTROY esp g_esp{ };

inline constexpr float ESP_ALPHA_RESPONSE{ 140.0f };
inline constexpr float DEATH_FADE_RESPONSE{ 250.0f };
inline constexpr float HEALTH_FILL_RESPONSE{ 90.0f };
inline constexpr float FLAG_IN_RESPONSE{ 110.0f };
inline constexpr float AMMO_SLIDE_OUT_RESPONSE{ 300.0f };

inline constexpr float FLAG_STEP{ 9.0f };
inline constexpr usize FLAG_PARTY{ 2 };
inline constexpr usize FLAG_BUFF_FIRST{ 20 };
inline constexpr usize FLAG_RESIST_FIRST{ 24 };
inline constexpr float RESIST_STEP{ static_cast< float >( render::icons::SIZE_VACC ) + 1.0f };
inline constexpr float AMMO_SLIDE{ 6.0f };

inline constexpr u8 PLAYER_TAG_ATLAS{ config::PLAYER_TAG_CHEATER + 1 };

// stacking order on screen; index matches the bit laid down in flag_mask

// hmmm bits
static u32 flag_mask( const esp_entity_t &entry )
{
    u32 mask{ };

    // who the player is does not come from the entity, so it survives dormancy
    if ( entry.m_atlas )
    {
        mask |= 1u << 1;
    }

    if ( entry.m_party )
    {
        mask |= 1u << FLAG_PARTY;
    }

    // conds on a dormant entity are frozen at whatever was networked last, so only the
    // dormancy marker itself is trustworthy there
    if ( entry.m_dormant )
    {
        return mask | 1u;
    }

    auto *player{ entry.m_ent->As< C_TFPlayer * >( ) };

    if ( player->InCond( TF_COND_INVULNERABLE ) &&
         !player->InCond( TF_COND_INVULNERABLE_WEARINGOFF ) )
    {
        mask |= 1u << 3;
    }

    if ( player->InCond( TF_COND_CRITBOOSTED ) )
    {
        mask |= 1u << 4;
    }

    if ( player->InCond( TF_COND_STEALTHED ) || player->InCond( TF_COND_STEALTHED_BLINK ) )
    {
        mask |= 1u << 5;
    }

    if ( player->InCond( TF_COND_PHASE ) )
    {
        mask |= 1u << 6;
    }

    if ( player->InCond( TF_COND_ZOOMED ) )
    {
        mask |= 1u << 7;
    }

    if ( player->InCond( TF_COND_BURNING ) )
    {
        mask |= 1u << 8;
    }

    if ( player->InCond( TF_COND_URINE ) )
    {
        mask |= 1u << 9;
    }

    if ( player->InCond( TF_COND_MAD_MILK ) )
    {
        mask |= 1u << 10;
    }

    if ( player->InCond( TF_COND_BLEEDING ) )
    {
        mask |= 1u << 11;
    }

    if ( player->InCond( TF_COND_MARKEDFORDEATH ) || player->InCond( TF_COND_MARKEDFORDEATH_SILENT ) )
    {
        mask |= 1u << 12;
    }

    // aiming covers the heavy's spun minigun; snipers already surface through ZOOM
    if ( player->InCond( TF_COND_AIMING ) && player->m_iClass( ) == TF_CLASS_HEAVYWEAPONS )
    {
        mask |= 1u << 13;
    }

    if ( player->InCond( TF_COND_STUNNED ) )
    {
        mask |= 1u << 14;
    }

    if ( player->InCond( TF_COND_TAUNTING ) )
    {
        mask |= 1u << 15;
    }

    if ( player->InCond( TF_COND_DISGUISED ) )
    {
        mask |= 1u << 16;
    }

    if ( player->InCond( TF_COND_SHIELD_CHARGE ) )
    {
        mask |= 1u << 17;
    }

    if ( player->InCond( TF_COND_BLASTJUMPING ) )
    {
        mask |= 1u << 18;
    }

    if ( g_lagcomp.broken( player->entindex( ) ) )
    {
        mask |= 1u << 19;
    }

    if ( player->InCond( TF_COND_OFFENSEBUFF ) )
    {
        mask |= 1u << 20;
    }

    if ( player->InCond( TF_COND_DEFENSEBUFF ) )
    {
        mask |= 1u << 21;
    }

    if ( player->InCond( TF_COND_REGENONDAMAGEBUFF ) )
    {
        mask |= 1u << 22;
    }

    if ( player->InCond( TF_COND_CRITBOOSTED_RAGE_BUFF ) )
    {
        mask |= 1u << 23;
    }

    if ( player->InCond( TF_COND_MEDIGUN_SMALL_BULLET_RESIST ) ||
         player->InCond( TF_COND_MEDIGUN_UBER_BULLET_RESIST ) ||
         player->InCond( TF_COND_BULLET_IMMUNE ) )
    {
        mask |= 1u << 24;
    }

    if ( player->InCond( TF_COND_MEDIGUN_SMALL_BLAST_RESIST ) ||
         player->InCond( TF_COND_MEDIGUN_UBER_BLAST_RESIST ) ||
         player->InCond( TF_COND_BLAST_IMMUNE ) )
    {
        mask |= 1u << 25;
    }

    if ( player->InCond( TF_COND_MEDIGUN_SMALL_FIRE_RESIST ) ||
         player->InCond( TF_COND_MEDIGUN_UBER_FIRE_RESIST ) ||
         player->InCond( TF_COND_FIRE_IMMUNE ) )
    {
        mask |= 1u << 26;
    }

    if ( player->IsFeigned( ) )
    {
        mask |= 1 << 27;
    }

    return mask;
}

rect_t esp_layout_t::claim_bar( config::esp_anchor_t anchor )
{
    const usize side{ static_cast< usize >( anchor ) };
    const i32   cursor{ this->m_bar[ side ] };

    this->m_bar[ side ] += ESP_BAR_STEP;

    switch ( anchor )
    {
        case config::esp_anchor_t::top:
            return rect_t{ this->m_box.left, this->m_box.top - cursor - ESP_BAR_WIDTH,
                           this->m_box.right, this->m_box.top - cursor };
        case config::esp_anchor_t::bottom:
            return rect_t{ this->m_box.left, this->m_box.bottom + cursor, this->m_box.right,
                           this->m_box.bottom + cursor + ESP_BAR_WIDTH };
        case config::esp_anchor_t::right:
            return rect_t{ this->m_box.right + cursor, this->m_box.top,
                           this->m_box.right + cursor + ESP_BAR_WIDTH, this->m_box.bottom };
        default:
            return rect_t{ this->m_box.left - cursor - ESP_BAR_WIDTH, this->m_box.top,
                           this->m_box.left - cursor, this->m_box.bottom };
    }
}

point_t esp_layout_t::claim_block( config::esp_anchor_t anchor, config::esp_align_t align,
                                   i32 height )
{
    const usize side{ static_cast< usize >( anchor ) };
    if ( anchor == config::esp_anchor_t::top || anchor == config::esp_anchor_t::bottom )
    {
        align = config::esp_align_t::center;
    }

    i32 &stack{ this->m_stack[ side ][ static_cast< usize >( align ) ] };

    if ( anchor == config::esp_anchor_t::top || anchor == config::esp_anchor_t::bottom )
    {
        if ( !stack )
        {
            stack = this->m_bar[ side ];
        }

        const i32 center_x{ this->m_box.left + ( this->m_box.right - this->m_box.left ) / 2 };
        const i32 y{ anchor == config::esp_anchor_t::top ? this->m_box.top - stack - height
                                                         : this->m_box.bottom + stack };

        stack += height + ESP_BLOCK_GAP;
        return point_t{ center_x, y };
    }

    const i32 span{ this->m_box.bottom - this->m_box.top - height };
    const i32 y{ this->m_box.top + ( align == config::esp_align_t::start ? 0 : align == config::esp_align_t::center ? span / 2
                                                                                                                    : span ) +
                 stack };

    stack += height + ESP_BLOCK_GAP;

    return point_t{ anchor == config::esp_anchor_t::left
                        ? this->m_box.left - this->m_bar[ static_cast< usize >( anchor ) ]
                        : this->m_box.right + this->m_bar[ static_cast< usize >( anchor ) ],
                    y };
}

render::text_align esp_layout_t::align_for( config::esp_anchor_t anchor )
{
    switch ( anchor )
    {
        case config::esp_anchor_t::left:
            return render::text_align::right;
        case config::esp_anchor_t::right:
            return render::text_align::left;
        default:
            return render::text_align::center;
    }
}

static vec3 bone_origin( const matrix3x4 &bone )
{
    return vec3{ bone[ 0 ][ 3 ], bone[ 1 ][ 3 ], bone[ 2 ][ 3 ] };
}

static const char *weapon_label( C_TFPlayer *player, char ( &out )[ 32 ] )
{
    C_BaseEntity *weapon{ player->m_hActiveWeapon( ).Get( ) };
    if ( !weapon )
    {
        return nullptr;
    }

    if ( const char *name{ weapon->As< C_TFWeaponBase * >( )->GetWeaponName( ) } )
    {
        return name;
    }

    return nullptr;
}

template< usize count >
static bool any_element( const bool ( &elements )[ count ] )
{
    for ( const bool enabled : elements )
    {
        if ( enabled )
        {
            return true;
        }
    }

    return false;
}

static bool element_on( const config::esp_profile_t &profile, config::esp_element_t element )
{
    return profile.m_elements[ static_cast< usize >( element ) ];
}

static bool profile_active( const config::esp_profile_t &profile )
{
    for ( const bool enabled : profile.m_elements )
    {
        if ( enabled )
        {
            return true;
        }
    }

    return false;
}

static color_t modulate( color_t value, u8 opacity )
{
    value.a = static_cast< u8 >( ( static_cast< u32 >( value.a ) * opacity + 127u ) / 255u );
    return value;
}

static color_t health_color( float fill, u8 opacity )
{
    const float clamped{ math::clamp( fill, 0.0f, 1.0f ) };
    const u8    red{ static_cast< u8 >( math::smaller( 510.0f * ( 1.0f - clamped ), 200.0f ) ) };
    const u8    green{ static_cast< u8 >( math::smaller( 510.0f * clamped, 200.0f ) ) };

    return color_t{ red, green, 32, opacity };
}

static i32 scale_to_height( i32 height, float fraction )
{
    const float scaled{ static_cast< float >( height ) * math::clamp( fraction, 0.0f, 1.0f ) };
    return static_cast< i32 >( scaled + 0.5f );
}

static rect_t bar_fill( const rect_t &bar, bool vertical, i32 length, i32 from )
{
    return vertical ? rect_t{ bar.left, bar.bottom - length, bar.right, bar.bottom - from }
                    : rect_t{ bar.left + from, bar.top, bar.left + length, bar.bottom };
}

static void draw_bar_divisions( const rect_t &bar, bool vertical, i32 divisions, u8 opacity )
{
    if ( divisions < 2 )
    {
        return;
    }

    const i32     span{ vertical ? bar.bottom - bar.top : bar.right - bar.left };
    const color_t separator{ modulate( ESP_BAR_BACKGROUND, opacity ) };

    for ( i32 index{ 1 }; index < divisions; ++index )
    {
        const i32 offset{ ( span * index + divisions / 2 ) / divisions };

        if ( vertical )
        {
            const i32 y{ bar.top + offset };
            g_draw.box( rect_t{ bar.left, y, bar.right, y + 1 }, separator, true );
        } else
        {
            const i32 x{ bar.left + offset };
            g_draw.box( rect_t{ x, bar.top, x + 1, bar.bottom }, separator, true );
        }
    }
}

static rect_t bar_slide( const rect_t &bar, config::esp_anchor_t anchor, i32 slide )
{
    switch ( anchor )
    {
        case config::esp_anchor_t::top:
            return rect_t{ bar.left, bar.top + slide, bar.right, bar.bottom + slide };
        case config::esp_anchor_t::bottom:
            return rect_t{ bar.left, bar.top - slide, bar.right, bar.bottom - slide };
        case config::esp_anchor_t::left:
            return rect_t{ bar.left + slide, bar.top, bar.right + slide, bar.bottom };
        default:
            return rect_t{ bar.left - slide, bar.top, bar.right - slide, bar.bottom };
    }
}

static bool calc_bbox( const esp_entity_t &entry, rect_t &bounds )
{
    C_BaseEntity *entity{ entry.m_ent };
    vec3          mins{ entity->m_vecMins( ) };
    vec3          maxs{ entity->m_vecMaxs( ) };

    // rockets and flares spawn with a zero-size collision hull and pipes are only four
    // units wide, so projectiles take the model's render bounds instead
    switch ( entry.m_class_id )
    {
        case ETFClassID::CTFPlayer:
        case ETFClassID::CObjectSentrygun:
        case ETFClassID::CObjectDispenser:
        case ETFClassID::CObjectTeleporter: break;
        default: entity->GetRenderBounds( mins, maxs ); break;
    }

    // a dormant entity's render transform is frozen at the last frame it was
    // networked, so the cached origin drives an axis aligned box instead
    matrix3x4 cached{
        { 1.0f, 0.0f, 0.0f, entry.m_origin.x },
        { 0.0f, 1.0f, 0.0f, entry.m_origin.y },
        { 0.0f, 0.0f, 1.0f, entry.m_origin.z },
    };

    const float ( *transform )[ 4 ]{ entry.m_dormant ? cached : entity->RenderableToWorldTransform( ) };

    const vec3 corners[ 8 ]{
        { mins.x, mins.y, mins.z },
        { mins.x, mins.y, maxs.z },
        { mins.x, maxs.y, mins.z },
        { mins.x, maxs.y, maxs.z },
        { maxs.x, mins.y, mins.z },
        { maxs.x, mins.y, maxs.z },
        { maxs.x, maxs.y, mins.z },
        { maxs.x, maxs.y, maxs.z },
    };

    float min_x{ };
    float max_x{ };
    float min_y{ };
    float max_y{ };
    bool  seeded{ };

    for ( const vec3 &corner : corners )
    {
        const vec3 world{
            corner.x * transform[ 0 ][ 0 ] + corner.y * transform[ 0 ][ 1 ] +
                corner.z * transform[ 0 ][ 2 ] + transform[ 0 ][ 3 ],
            corner.x * transform[ 1 ][ 0 ] + corner.y * transform[ 1 ][ 1 ] +
                corner.z * transform[ 1 ][ 2 ] + transform[ 1 ][ 3 ],
            corner.x * transform[ 2 ][ 0 ] + corner.y * transform[ 2 ][ 1 ] +
                corner.z * transform[ 2 ][ 2 ] + transform[ 2 ][ 3 ],
        };

        vec2 screen{ };
        if ( !g_cl.world_to_screen( world, screen ) )
        {
            return false;
        }

        if ( !seeded )
        {
            min_x  = screen.x;
            max_x  = screen.x;
            min_y  = screen.y;
            max_y  = screen.y;
            seeded = true;
            continue;
        }

        if ( screen.x < min_x )
            min_x = screen.x;
        if ( screen.x > max_x )
            max_x = screen.x;
        if ( screen.y < min_y )
            min_y = screen.y;
        if ( screen.y > max_y )
            max_y = screen.y;
    }

    bounds.left   = static_cast< i32 >( min_x );
    bounds.top    = static_cast< i32 >( min_y );
    bounds.right  = static_cast< i32 >( max_x );
    bounds.bottom = static_cast< i32 >( max_y );
    return true;
}

static u8 entry_alpha( const esp_entity_t &entry )
{
    if ( !entry.m_dormant )
    {
        return 255;
    }

    const float ramp{ player_data::window( ) * 0.84f };
    if ( ramp <= 0.0f || entry.m_fade_time >= ramp )
    {
        return 128;
    }

    return static_cast< u8 >( 128.0f * math::clamp( entry.m_fade_time / ramp, 0.0f, 1.0f ) );
}

static void draw_chevron( vec2 at, vec2 radial, float along, float half_height, float width, float band,
                          color_t tail, color_t tip, color_t shadow )
{
    const vec2  across{ -radial.y, radial.x };
    const float inner_tail{ half_height * ( 1.0f - band / width ) };
    const auto  place{ [ & ]( float x, float y )
    {
        return at + radial * ( along + x ) + across * y;
    } };

    const vec2 points[ 6 ]{
        place( -width * 0.5f, -half_height ),
        place( width * 0.5f, 0.0f ),
        place( -width * 0.5f, half_height ),
        place( -width * 0.5f, inner_tail ),
        place( width * 0.5f - band, 0.0f ),
        place( -width * 0.5f, -inner_tail ),
    };

    const color_t colors[ 6 ]{ tail, tip, tail, tail, tip, tail };

    vec2    dropped[ 6 ]{ };
    color_t shadows[ 6 ]{ };

    for ( usize index{ }; index < 6; ++index )
    {
        dropped[ index ] = points[ index ] + vec2{ 1.0f, 1.0f };
        shadows[ index ] = shadow;
    }

    g_draw.polygon( dropped, shadows, 6 );
    g_draw.polygon( points, colors, 6 );
}

ATLAS_FORCE_INLINE void draw_target_corners( const vec3 &origin, const vec3 &mins,
                                             const vec3 &maxs, float padding, color_t color )
{
    vec3 low{ origin + mins - vec3{ padding, padding, 0.0f } };
    vec3 high{ origin + maxs + vec3{ padding, padding, 1.0f } };
    low.z += 1.0f;

    const float reach{ math::smaller( high.x - low.x, high.y - low.y ) * 0.24f };
    for ( i32 x{ }; x < 2; ++x )
    {
        for ( i32 y{ }; y < 2; ++y )
        {
            for ( i32 z{ }; z < 2; ++z )
            {
                const vec3 corner{ x ? high.x : low.x, y ? high.y : low.y,
                                   z ? high.z : low.z };
                const vec3 along_x{ corner.x + ( x ? -reach : reach ), corner.y, corner.z };
                const vec3 along_y{ corner.x, corner.y + ( y ? -reach : reach ), corner.z };
                const vec3 along_z{ corner.x, corner.y, corner.z + ( z ? -reach : reach ) };
                g_cl.draw_3d_line( corner, along_x, color, false );
                g_cl.draw_3d_line( corner, along_y, color, false );
                g_cl.draw_3d_line( corner, along_z, color, false );
            }

            color_t rail{ color };
            rail.a = static_cast< u8 >( rail.a / 4 );
            const vec3 bottom{ x ? high.x : low.x, y ? high.y : low.y, low.z + reach };
            const vec3 top{ bottom.x, bottom.y, high.z - reach };
            g_cl.draw_3d_line( bottom, top, rail, false );
        }
    }
}

ATLAS_FORCE_INLINE void draw_target_orbit( const vec3 &center, float radius, float phase,
                                           color_t color )
{
    constexpr i32 SEGMENTS{ 48 };
    for ( i32 segment{ }; segment < SEGMENTS; ++segment )
    {
        if ( segment % 12 >= 7 )
        {
            continue;
        }

        const float from_angle{ phase + static_cast< float >( segment ) * ( 2.0f * PI / SEGMENTS ) };
        const float to_angle{ from_angle + 2.0f * PI / SEGMENTS };
        const vec3  from{ center.x + math::cos( from_angle ) * radius,
                          center.y + math::sin( from_angle ) * radius, center.z };
        const vec3  to{ center.x + math::cos( to_angle ) * radius,
                        center.y + math::sin( to_angle ) * radius, center.z };
        g_cl.draw_3d_line( from, to, color, false );
    }

    for ( i32 tick{ }; tick < 4; ++tick )
    {
        const float angle{ phase + static_cast< float >( tick ) * ( PI * 0.5f ) };
        const vec3  from{ center.x + math::cos( angle ) * radius,
                          center.y + math::sin( angle ) * radius, center.z - 3.0f };
        const vec3  to{ from.x, from.y, from.z + 6.0f };
        g_cl.draw_3d_line( from, to, color, false );
    }
}

ATLAS_FORCE_INLINE bool could_touch( C_BaseEntity *pack, C_TFPlayer *player, const vec3 &origin, float slack )
{
    const vec3 pack_mins{ pack->m_vecOrigin( ) + pack->m_vecMins( ) };
    const vec3 pack_maxs{ pack->m_vecOrigin( ) + pack->m_vecMaxs( ) };
    const vec3 hull_mins{ origin + player->m_vecMins( ) };
    const vec3 hull_maxs{ origin + player->m_vecMaxs( ) };

    return hull_mins.x - slack <= pack_maxs.x + ITEM_PICKUP_BOX_BLOAT &&
           hull_maxs.x + slack >= pack_mins.x - ITEM_PICKUP_BOX_BLOAT &&
           hull_mins.y - slack <= pack_maxs.y + ITEM_PICKUP_BOX_BLOAT &&
           hull_maxs.y + slack >= pack_mins.y - ITEM_PICKUP_BOX_BLOAT &&
           hull_mins.z - slack <= pack_maxs.z + ITEM_PICKUP_BOX_BLOAT * 0.5f &&
           hull_maxs.z + slack >= pack_mins.z;
}

void esp::think( )
{
    this->m_entities.clear( );

    if ( !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        this->m_pickup_events.clear( );
        return;
    }

    this->gather_entities( );

    for ( const pickup_event_t &event : this->m_pickup_events )
    {
        this->resolve_pickup( event );
    }

    this->m_pickup_events.clear( );

    atl::sort( m_entities.begin( ), m_entities.end( ), []( esp_entity_t &first, esp_entity_t &second )
    {
        return first.m_distance > second.m_distance;
    } );
}

void esp::gather_entities( )
{
    const u32 visual_target{ g_cfg.m_esp.m_target_chevron
                                 ? g_ragebot.visual_target_handle( )
                                 : INVALID_EHANDLE_INDEX };

    for ( const auto *cached : g_entity_cache.visuals( ) )
    {
        if ( !cached->networked( ) )
        {
            continue;
        }

        const i32        i{ cached->m_index };
        C_BaseEntity    *base_entity{ cached->m_entity };
        const ETFClassID class_id{ cached->m_class_id };

        // TODO: do a team check
        if ( class_id == ETFClassID::CCaptureFlag )
        {
            auto *flag{ base_entity->As< C_CaptureFlag * >( ) };
            if ( !any_element( g_cfg.m_esp.m_intel_elements ) ||
                 base_entity->IsDormant( ) || flag->m_nFlagStatus( ) == TF_FLAGINFO_STOLEN )
            {
                continue;
            }

            bool is_enemy{ flag->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) };

            const vec3 flag_delta{ base_entity->m_vecOrigin( ) - g_cl.m_local->m_vecOrigin( ) };
            this->m_entities.emplace_back( base_entity, flag_delta.length2d( ), class_id,
                                           base_entity->m_vecOrigin( ), false, 0.0f, false,
                                           config::esp_target_t{ }, cached->m_handle );
            continue;
        }

        if ( class_id == ETFClassID::CBaseAnimating ||
             class_id == ETFClassID::CTFAmmoPack )
        {
            if ( !any_element( g_cfg.m_esp.m_pickup_elements ) )
            {
                continue;
            }

            const pickup_record_t *record{ this->track_pickup( cached->m_handle, base_entity, class_id ) };
            if ( !record )
            {
                continue;
            }

            const vec3 pack_origin{ base_entity->m_vecOrigin( ) };
            const vec3 pack_delta{ pack_origin - g_cl.m_local->m_vecOrigin( ) };
            this->m_entities.emplace_back( base_entity, pack_delta.length2d( ), class_id, pack_origin,
                                           record->m_dormant, 0.0f, false, config::esp_target_t{ }, cached->m_handle );
            continue;
        }

        const bool is_player{ class_id == ETFClassID::CTFPlayer };
        const bool dormant{ base_entity->IsDormant( ) };
        const i32  team{ is_player && dormant ? g_player_data.team( i ) : base_entity->m_iTeamNum( ) };

        config::esp_target_t target{ config::esp_target_t::enemy };
        u8                   tag{ config::PLAYER_TAG_NONE };
        bool                 atlas{ };
        u8                   party{ };
        u16                  groups{ };
        const bool           teammate{ team == g_cl.m_local->m_iTeamNum( ) };

        if ( is_player )
        {
            player_info_t info{ };
            if ( g_tf2.m_engine->GetPlayerInfo( i, &info ) )
            {
                tag = g_cfg.player_tag( info.friendsID );
                char cloud_alias[ 129 ]{ };
                if ( g_player_cloud.known( info.friendsID, cloud_alias ) )
                {
                    tag = config::PLAYER_TAG_CHEATER;
                }
            }

            if ( i == g_tf2.m_engine->GetLocalPlayer( ) )
            {
                target = config::esp_target_t::local;
                tag    = config::PLAYER_TAG_NONE;
            } else
            {
                atlas  = g_cl.is_atlas_user( info.friendsID );
                party  = g_cl.party_index( info.friendsID );
                groups = g_cfg.player_groups( info.friendsID );

                if ( g_cl.is_friend( info.friendsID ) )
                {
                    target = config::esp_target_t::friends;
                    tag    = config::PLAYER_TAG_FRIEND;
                } else if ( atlas )
                {
                    target = config::esp_target_t::atlas;
                    tag    = PLAYER_TAG_ATLAS;
                } else
                {
                    if ( teammate )
                    {
                        if ( !g_cl.should_draw_teammate( base_entity->As< C_TFPlayer * >( ) ) )
                        {
                            continue;
                        }
                    }

                    target = teammate ? config::esp_target_t::team : config::esp_target_t::enemy;
                }
            }

            const bool arrowed{ target == config::esp_target_t::enemy && g_cfg.m_arrows.m_enabled };
            if ( !arrowed && cached->m_handle != visual_target &&
                 !profile_active( g_cfg.m_esp.m_profiles[ static_cast< usize >( target ) ] ) )
            {
                continue;
            }
        }

        if ( target == config::esp_target_t::local && !g_tf2.m_input->CAM_IsThirdPerson( ) )
        {
            continue;
        }

        vec3  origin{ base_entity->m_vecOrigin( ) };
        float fade_time{ };

        if ( dormant )
        {
            if ( is_player && !g_player_data.connected( i ) )
            {
                continue;
            }

            if ( !g_player_data.dormant_origin( base_entity, origin, fade_time ) )
            {
                continue;
            }
        }

        const vec3 delta{ origin - g_cl.m_local->m_vecOrigin( ) };

        switch ( class_id )
        {
            case ETFClassID::CTFPlayer:
            {
                const auto tf_player{ base_entity->As< C_TFPlayer * >( ) };
                if ( !tf_player )
                {
                    continue;
                }

                const bool dead{ dormant ? !g_player_data.alive( i ) : !tf_player->IsAlive( ) };
                if ( dead && dormant )
                {
                    continue;
                }

                this->m_entities.emplace_back( base_entity, delta.length2d( ), class_id, origin, dormant, fade_time, dead, target, cached->m_handle, tag, atlas, party, groups );
                break;
            }

            case ETFClassID::CObjectSentrygun:
            case ETFClassID::CObjectDispenser:
            case ETFClassID::CObjectTeleporter:
            {
                if ( !any_element( g_cfg.m_esp.m_building_elements ) || teammate )
                {
                    continue;
                }

                auto *object{ base_entity->As< C_BaseObject * >( ) };

                // a hauled building rides inside the engineer, drawing it is just noise
                if ( object->m_bCarried( ) )
                {
                    continue;
                }

                const bool dead{ object->m_iHealth( ) <= 0 };
                this->m_entities.emplace_back( base_entity, delta.length2d( ), class_id, origin, dormant, fade_time, dead, config::esp_target_t{ }, cached->m_handle );
                break;
            }

            case ETFClassID::CTFProjectile_Rocket:
            case ETFClassID::CTFProjectile_SentryRocket:
            case ETFClassID::CTFProjectile_Arrow:
            case ETFClassID::CTFProjectile_Flare:
            case ETFClassID::CTFProjectile_HealingBolt:
            case ETFClassID::CTFProjectile_Cleaver:
            case ETFClassID::CTFGrenadePipebombProjectile:
            {
                if ( !any_element( g_cfg.m_esp.m_projectile_elements ) || !g_cfg.m_esp.shows_projectile( teammate ) )
                {
                    continue;
                }

                this->m_entities.emplace_back( base_entity, delta.length2d( ), class_id, origin, dormant, fade_time, false, config::esp_target_t{ }, cached->m_handle );
                break;
            }

            case ETFClassID::CEyeballBoss:
            case ETFClassID::CHeadlessHatman:
            case ETFClassID::CMerasmus:
            case ETFClassID::CZombie:
            {
                if ( !any_element( g_cfg.m_esp.m_npc_elements ) || teammate )
                {
                    continue;
                }

                this->m_entities.emplace_back( base_entity, delta.length2d( ), class_id, origin, dormant, fade_time, false, config::esp_target_t{ }, cached->m_handle );
                break;
            }

            default: break;
        }
    }
}

void esp::paint( )
{
    if ( !g_cl.m_local || !g_cl.m_resource || !g_tf2.m_engine->IsInGame( ) )
        return;

    const u32 delta{ this->frame_delta( ) };

    for ( const esp_entity_t &entry : this->m_entities )
    {
        if ( !entry.m_ent || g_entity_cache.resolve( entry.m_handle ) != entry.m_ent )
        {
            continue;
        }

        rect_t     bounds{ };
        const bool on_screen{ calc_bbox( entry, bounds ) && g_cl.is_on_screen( bounds ) };

        if ( g_cfg.m_arrows.m_enabled && entry.m_class_id == ETFClassID::CTFPlayer &&
             entry.m_target == config::esp_target_t::enemy )
        {
            this->draw_arrow( entry, on_screen, delta );
        }

        if ( !on_screen )
        {
            continue;
        }

        switch ( entry.m_class_id )
        {
            case ETFClassID::CTFPlayer:
            {
                this->draw_player( entry, bounds, delta );
                break;
            }

            case ETFClassID::CObjectSentrygun:
            case ETFClassID::CObjectDispenser:
            case ETFClassID::CObjectTeleporter:
            {
                this->draw_building( entry, bounds, delta );
                break;
            }

            case ETFClassID::CCaptureFlag:
            {
                this->draw_objective( entry, bounds, delta );
                break;
            }

            case ETFClassID::CTFAmmoPack:
            case ETFClassID::CBaseAnimating:
            {
                this->draw_pack( entry, bounds, delta );
                break;
            }

            case ETFClassID::CEyeballBoss:
            case ETFClassID::CHeadlessHatman:
            case ETFClassID::CMerasmus:
            case ETFClassID::CZombie:
            {
                this->draw_npc( entry, bounds, delta );
                break;
            }

            default:
            {
                this->draw_projectile( entry, bounds, delta );
                break;
            }
        }
    }

#if defined( ATLAS_DEV )
    this->draw_debug( );
#endif // ATLAS_DEV
}

void esp::render_target( )
{
    const usize style{ g_cfg.m_esp.m_target_style };
    if ( style == config::TARGET_VISUAL_OFF || !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    const u32 handle{ g_ragebot.visual_target_handle( ) };
    if ( handle == INVALID_EHANDLE_INDEX )
    {
        return;
    }

    C_BaseEntity *entity{ g_entity_cache.resolve( handle ) };
    if ( !entity )
    {
        return;
    }

    auto *player{ entity->As< C_TFPlayer * >( ) };
    if ( player->IsDormant( ) || !player->IsAlive( ) )
    {
        return;
    }

    const float now{ static_cast< float >( g_tf2.m_engine->Plat_FloatTime( ) ) };
    const float elapsed{ now - static_cast< float >( g_ragebot.visual_target_started( ) ) };
    const float appear{ g_cfg.m_esp.m_target_animate
                            ? math::smooth_step( math::clamp( elapsed / 0.18f, 0.0f, 1.0f ) )
                            : 1.0f };
    color_t     color{ g_cfg.m_esp.m_target_color };
    color.a = static_cast< u8 >( static_cast< float >( color.a ) * appear );
    if ( !color.a )
    {
        return;
    }

    const vec3  origin{ player->m_vecOrigin( ) };
    const vec3  mins{ player->m_vecMins( ) };
    const vec3  maxs{ player->m_vecMaxs( ) };
    const vec3  center{ origin + ( mins + maxs ) * 0.5f };
    const float radius{ math::bigger( maxs.x - mins.x, maxs.y - mins.y ) * 0.5f + 9.0f };
    const float phase{ g_cfg.m_esp.m_target_animate ? now * 0.65f : 0.0f };
    const float pulse{ g_cfg.m_esp.m_target_animate ? math::sin( now * 2.4f ) * 1.5f : 0.0f };

    if ( style == config::TARGET_VISUAL_BRACKETS )
    {
        draw_target_corners( origin, mins, maxs, 5.0f + ( 1.0f - appear ) * 12.0f + pulse, color );
    }

    if ( style == config::TARGET_VISUAL_ORBIT )
    {
        draw_target_orbit( vec3{ center.x, center.y, origin.z + mins.z + 4.0f },
                           radius + pulse, phase, color );
        draw_target_orbit( vec3{ center.x, center.y, origin.z + ( mins.z + maxs.z ) * 0.5f },
                           radius - 3.0f + pulse, -phase, color );
    }
}

#if defined( ATLAS_DEV )
void esp::draw_debug( )
{
    if ( !g_input.is_held( VK_HOME ) )
    {
        return;
    }

    for ( i32 i{ 1 }; i <= g_tf2.m_entity_list->GetHighestEntityIndex( ); ++i )
    {
        IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( i ) };
        C_BaseEntity  *base_entity{ client_entity ? client_entity->GetBaseEntity( ) : nullptr };
        ClientClass   *client_class{ base_entity ? base_entity->GetClientClass( ) : nullptr };

        if ( !client_class )
        {
            continue;
        }

        vec2 pos{ };
        if ( !g_cl.world_to_screen( base_entity->WorldSpaceCenter( ), pos ) )
        {
            continue;
        }

        char buf[ 512 ]{ };
        atl::format( buf, sizeof( buf ), "entindex: %i\nm_ClassID: %i\nm_pNetworkName: %s\n", i, client_class->m_ClassID, client_class->m_pNetworkName );

        g_draw.text( "Debug Overlay", point_t{ ( i32 )pos.x, ( i32 )pos.y }, color_t{ 255, 255, 255 }, buf, true );
    }
}
#endif // ATLAS_DEV

void esp::on_entity_deleted( C_BaseEntity *entity, u32 handle )
{
    for ( usize at{ }; at < this->m_entities.size( ); )
    {
        const auto &entry{ this->m_entities[ at ] };
        if ( entry.m_ent == entity && entry.m_handle == handle )
        {
            this->m_entities.erase( at );
        } else
        {
            ++at;
        }
    }

    for ( usize at{ }; at < this->m_pickups.size( ); ++at )
    {
        if ( this->m_pickups[ at ].m_handle == handle )
        {
            this->m_pickups.swap_erase( at );
            break;
        }
    }
}

pickup_record_t *esp::find_pickup( u32 handle )
{
    for ( pickup_record_t &record : this->m_pickups )
    {
        if ( record.m_handle == handle )
        {
            return &record;
        }
    }

    return nullptr;
}

pickup_record_t *esp::track_pickup( u32 handle, C_BaseEntity *entity, ETFClassID class_id )
{
    const bool       dormant{ entity->IsDormant( ) };
    pickup_record_t *record{ this->find_pickup( handle ) };

    if ( !record )
    {
        const usize kind{ g_cl.pickup_kind( entity, class_id ) };
        if ( kind == config::PICKUP_COUNT )
        {
            return nullptr;
        }

        const bool respawns{ class_id == ETFClassID::CBaseAnimating && !entity->m_hOwnerEntity( ).IsValid( ) };
        return this->m_pickups.emplace_back( handle, kind, respawns, dormant );
    }

    if ( !dormant )
    {
        record->m_vanished = 0.0f;
        record->m_respawn  = 0.0f;
    } else if ( !record->m_dormant )
    {
        record->m_vanished = g_tf2.m_globals->curtime;
    }

    record->m_dormant = dormant;
    return record;
}

u32 esp::frame_delta( )
{
    const float now{ g_tf2.m_globals->realtime };
    float       elapsed{ now - this->m_last_paint };

    if ( this->m_last_paint <= 0.0f || elapsed < 0.0f )
    {
        elapsed = 0.0f;
    }

    this->m_last_paint = now;

    // a hitch would otherwise teleport every animation to its target in one frame
    const u32 milliseconds{ static_cast< u32 >( elapsed * 1000.0f ) };
    return milliseconds > 100u ? 100u : milliseconds;
}

visual_record_t *esp::visual_for( C_BaseEntity *entity )
{
    const i32 index{ entity->entindex( ) };
    const i32 serial{ entity->GetRefEHandle( ).GetSerialNumber( ) };

    visual_record_t *visual{ this->m_visuals.get( index ) };

    // a reused player slot has to start over instead of inheriting the previous animation
    if ( !visual || visual->m_serial != serial )
    {
        visual_record_t fresh{ };
        fresh.m_serial = serial;
        visual         = this->m_visuals.assign( index, fresh );
    }

    return visual;
}

void esp::draw_player( const esp_entity_t &entry, const rect_t &bounds, u32 delta )
{
    const config::esp_profile_t &profile{
        g_cfg.m_esp.m_profiles[ static_cast< usize >( entry.m_target ) ]
    };

    visual_record_t *visual{ this->visual_for( entry.m_ent ) };

    const u8 faded{ entry.m_dead ? static_cast< u8 >( 0 ) : entry_alpha( entry ) };
    u8       opacity{ faded };

    // the map only fails under allocation pressure; fall back to the un-animated opacity
    if ( visual )
    {
        // death dissolves slower than the regular ease so the overlay visibly fades
        // out at the corpse instead of blinking away
        const float response{ entry.m_dead ? DEATH_FADE_RESPONSE : ESP_ALPHA_RESPONSE };

        visual->m_alpha = math::animate( visual->m_alpha, static_cast< float >( faded ) / 255.0f, delta, response );
        opacity         = static_cast< u8 >( math::clamp( visual->m_alpha, 0.0f, 1.0f ) * 255.0f );
    }

    if ( !opacity )
    {
        return;
    }

    if ( element_on( profile, config::esp_element_t::box ) )
    {
        const color_t shadow{ modulate( { 0, 0, 0, profile.m_box_color.a }, opacity ) };
        g_draw.box( rect_t{ bounds.left - 1, bounds.top - 1, bounds.right + 1, bounds.bottom + 1 }, shadow );
        g_draw.box( bounds, modulate( profile.m_box_color, opacity ) );
        g_draw.box( rect_t{ bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.bottom - 1 }, shadow );
    }

    auto *player{ entry.m_ent->As< C_TFPlayer * >( ) };

    if ( element_on( profile, config::esp_element_t::skeleton ) && !entry.m_dormant && !entry.m_dead )
    {
        this->draw_bones( player, profile.m_skeleton_color, opacity );
    }

    esp_layout_t layout{ bounds };

    if ( visual && element_on( profile, config::esp_element_t::health ) )
    {
        const config::esp_anchor_t anchor{
            profile.m_anchors[ static_cast< usize >( config::esp_element_t::health ) ]
        };

        i32 health{ };
        i32 max_health{ };

        if ( this->query_health( entry.m_ent, health, max_health ) )
        {
            this->draw_health_bar( layout, anchor, *visual, health, max_health,
                                   profile.m_health_style, profile.m_health_color,
                                   profile.m_health_low_color, profile.m_overheal_color,
                                   profile.m_health_divisions, delta, opacity );
        }

        C_WeaponMedigun *medigun{ };
        if ( auto *weapon{ entry.m_dormant ? nullptr : player->m_hActiveWeapon( ).Get( ) } )
        {
            ClientClass *weapon_class{ weapon->GetClientClass( ) };
            if ( weapon_class && weapon_class->GetClassID( ) == ETFClassID::CWeaponMedigun )
            {
                medigun = weapon->As< C_WeaponMedigun * >( );
            }
        }

        this->draw_uber_bar( layout, anchor, medigun, *visual, delta, opacity );
    }

    for ( usize position{ }; position < config::ESP_ELEMENT_COUNT; ++position )
    {
        const auto element{ static_cast< config::esp_element_t >( profile.m_order[ position ] ) };

        if ( !element_on( profile, element ) )
        {
            continue;
        }

        const usize                slot{ static_cast< usize >( element ) };
        const config::esp_anchor_t anchor{ profile.m_anchors[ slot ] };
        const config::esp_align_t  align{ profile.m_aligns[ slot ] };

        switch ( element )
        {
            case config::esp_element_t::name:
            {
                player_info_t info{ };
                i32           idx{ entry.m_ent->entindex( ) };

                if ( g_tf2.m_engine->GetPlayerInfo( idx, &info ) )
                {
                    const point_t at{ layout.claim_block( anchor, align, ESP_NAME_HEIGHT ) };
                    const usize   shape{ g_cfg.m_esp.m_avatars };

                    g_draw.text( ATLAS_XORSTR( "ESP Name" ).get( ), at,
                                 modulate( profile.m_name_color, opacity ),
                                 g_cl.get_player_name( idx ), true,
                                 esp_layout_t::align_for( anchor ), 0.0f,
                                 shape != render::AVATAR_NONE ? info.friendsID : 0,
                                 static_cast< u32 >( shape ) );
                }

                break;
            }

            case config::esp_element_t::tag:
            {
                const char *label{ };
                color_t     color{ };
                switch ( entry.m_tag )
                {
                    case config::PLAYER_TAG_FRIEND:
                    {
                        label = ATLAS_XORSTR_STATIC( "Friend" );
                        color = color_t{ 100, 215, 110 };
                        break;
                    }

                    case config::PLAYER_TAG_IGNORED:
                    {
                        label = ATLAS_XORSTR_STATIC( "Ignored" );
                        color = color_t{ 170, 170, 175 };
                        break;
                    }

                    case config::PLAYER_TAG_CHEATER:
                    {
                        label = ATLAS_XORSTR_STATIC( "Cheater" );
                        color = color_t{ 255, 100, 100 };
                        break;
                    }

                    case PLAYER_TAG_ATLAS:
                    {
                        label = ATLAS_XORSTR_STATIC( "Atlas User" );
                        color = color_t{ 75, 105, 215 };
                        break;
                    }

                    default: break;
                }

                if ( label )
                {
                    const point_t at{ layout.claim_block( anchor, align, ESP_NAME_HEIGHT ) };
                    g_draw.text( ATLAS_XORSTR( "ESP Name" ).get( ), at,
                                 modulate( color, opacity ), label, true,
                                 esp_layout_t::align_for( anchor ) );
                }

                for ( usize index{ }; index < config::PLAYER_GROUP_COUNT; ++index )
                {
                    if ( !( entry.m_groups & ( 1u << index ) ) )
                    {
                        continue;
                    }

                    const config::player_group_t &group{ g_cfg.m_player_groups[ index ] };
                    const point_t                 at{ layout.claim_block( anchor, align, ESP_NAME_HEIGHT ) };
                    g_draw.text( ATLAS_XORSTR( "ESP Name" ).get( ), at,
                                 modulate( group.m_color, opacity ),
                                 reinterpret_cast< const char * >( group.m_name ), true,
                                 esp_layout_t::align_for( anchor ) );
                }
                break;
            }

            case config::esp_element_t::distance:
            {
                // 0.01905 is the hammer-unit to meter factor the reference client uses
                char text[ 12 ]{ };
                str::format_to( text, sizeof( text ), ATLAS_XORSTR( "[{}M]" ).get( ),
                                static_cast< i32 >( entry.m_distance * 0.01905f + 0.5f ) );

                const point_t at{ layout.claim_block( anchor, align, ESP_SMALL_HEIGHT ) };

                g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), at,
                             modulate( profile.m_distance_color, opacity ), text, true,
                             esp_layout_t::align_for( anchor ) );
                break;
            }

            case config::esp_element_t::weapon:
            {
                char        label[ 32 ]{ };
                const char *name{ entry.m_dormant ? nullptr : weapon_label( player, label ) };

                if ( name )
                {
                    const point_t at{ layout.claim_block( anchor, align, ESP_SMALL_HEIGHT ) };

                    g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), at,
                                 modulate( profile.m_weapon_color, opacity ), name, true,
                                 esp_layout_t::align_for( anchor ) );
                }
                break;
            }

            case config::esp_element_t::flags:
            {
                if ( visual )
                {
                    this->draw_flags( entry, layout, anchor, align, *visual, delta, opacity );
                }

                break;
            }

            default:
                break; // the box, the skeleton and the bars are not stacked blocks
        }
    }

    if ( !g_cfg.m_esp.m_target_chevron || entry.m_dormant || entry.m_dead ||
         entry.m_handle != g_ragebot.visual_target_handle( ) )
    {
        return;
    }

    const float now{ static_cast< float >( g_tf2.m_engine->Plat_FloatTime( ) ) };
    const float elapsed{ now - static_cast< float >( g_ragebot.visual_target_started( ) ) };
    const float appear{ g_cfg.m_esp.m_target_animate
                            ? math::smooth_step( math::clamp( elapsed / 0.18f, 0.0f, 1.0f ) )
                            : 1.0f };
    const float bob{ g_cfg.m_esp.m_target_animate ? math::sin( now * 3.0f ) * 2.0f : 0.0f };
    const usize top{ static_cast< usize >( config::esp_anchor_t::top ) };
    const usize center{ static_cast< usize >( config::esp_align_t::center ) };
    const i32   stack{ layout.m_stack[ top ][ center ] };
    const float x{ static_cast< float >( bounds.left + ( bounds.right - bounds.left ) / 2 - 2 ) };
    const float y{ static_cast< float >( bounds.top - ( stack ? stack : layout.m_bar[ top ] ) - 11 ) +
                   bob - ( 1.0f - appear ) * 5.0f };
    color_t     color{ modulate( g_cfg.m_esp.m_target_color, opacity ) };
    color.a = static_cast< u8 >( static_cast< float >( color.a ) * appear );
    const color_t shadow{ 0, 0, 0, color.a };
    const vec2    outline[ 6 ]{
        { x - 10.0f, y - 5.0f },
        { x, y + 1.0f },
        { x + 10.0f, y - 5.0f },
        { x + 10.0f, y },
        { x, y + 8.0f },
        { x - 10.0f, y },
    };
    const vec2 chevron[ 6 ]{
        { x - 9.0f, y - 4.0f },
        { x, y + 3.0f },
        { x + 9.0f, y - 4.0f },
        { x + 9.0f, y - 1.0f },
        { x, y + 6.0f },
        { x - 9.0f, y - 1.0f },
    };
    const color_t outline_colors[ 6 ]{ shadow, shadow, shadow, shadow, shadow, shadow };
    const color_t chevron_colors[ 6 ]{ color, color, color, color, color, color };
    g_draw.polygon( outline, outline_colors, 6 );
    g_draw.polygon( chevron, chevron_colors, 6 );
}

void esp::draw_uber_bar( esp_layout_t &layout, config::esp_anchor_t anchor,
                         C_WeaponMedigun *medigun, visual_record_t &visual, u32 delta, u8 opacity )
{
    visual.m_uber_pop = math::animate( visual.m_uber_pop, medigun ? 1.0f : 0.0f, delta,
                                       medigun ? FLAG_IN_RESPONSE : AMMO_SLIDE_OUT_RESPONSE );

    float charge{ };
    bool  vaccinator{ };

    if ( medigun )
    {
        charge        = math::clamp( medigun->m_flChargeLevel( ), 0.0f, 1.0f );
        visual.m_uber = math::animate( visual.m_uber, charge, delta, HEALTH_FILL_RESPONSE );
        vaccinator    = medigun->m_iItemDefinitionIndex( ) == Medic_s_TheVaccinator;
    }

    if ( visual.m_uber_pop <= 0.02f )
    {
        return;
    }

    const float eased{ math::smooth_step( visual.m_uber_pop ) };
    const u8    bar_alpha{ static_cast< u8 >( eased * static_cast< float >( opacity ) ) };

    // slide it back into the box when the medigun is holstered
    const i32    slide{ static_cast< i32 >( ( 1.0f - eased ) * AMMO_SLIDE + 0.5f ) };
    const rect_t bar{ bar_slide( layout.claim_bar( anchor ), anchor, slide ) };
    const bool   vertical{ anchor == config::esp_anchor_t::left ||
                           anchor == config::esp_anchor_t::right };
    const i32    span{ vertical ? bar.bottom - bar.top : bar.right - bar.left };

    g_draw.box( rect_t{ bar.left - 1, bar.top - 1, bar.right + 1, bar.bottom + 1 },
                modulate( ESP_BAR_BACKGROUND, bar_alpha ), true );

    const i32 fill{ scale_to_height( span, visual.m_uber ) };
    if ( fill > 0 )
    {
        g_draw.box( bar_fill( bar, vertical, fill, 0 ),
                    modulate( color_t{ 120, 170, 255 }, bar_alpha ), true );
    }

    if ( !medigun )
    {
        return;
    }

    // the vaccinator meter is four discrete pops, so it gets dividers
    if ( vaccinator )
    {
        for ( i32 divider{ 1 }; divider < 4; ++divider )
        {
            const i32 at{ ( span * divider ) / 4 };

            g_draw.box( vertical ? rect_t{ bar.left, bar.bottom - at, bar.right, bar.bottom - at + 1 }
                                 : rect_t{ bar.left + at, bar.top, bar.left + at + 1, bar.bottom },
                        modulate( color_t{ 0, 0, 0 }, bar_alpha ), true );
        }
    }

    char text[ 8 ]{ };
    if ( vaccinator )
    {
        str::format_to( text, sizeof( text ), ATLAS_XORSTR( "{}/4" ).get( ),
                        static_cast< i32 >( charge * 4.0f ) );
    } else
    {
        str::format_to( text, sizeof( text ), ATLAS_XORSTR( "{}%" ).get( ),
                        static_cast< i32 >( visual.m_uber * 100.0f + 0.5f ) );
    }

    // the number rides the head of the fill on a column, and sits at the foot of a row
    if ( !vertical )
    {
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ),
                     point_t{ bar.left - 3, bar.top - ESP_SMALL_HEIGHT / 2 },
                     color_t{ 255, 255, 255, bar_alpha }, text, true, render::text_align::right );
        return;
    }

    const i32 head{ bar.bottom - fill };
    const i32 y{ math::clamp( head - 4, bar.top - 1, bar.bottom - 9 ) };

    if ( anchor == config::esp_anchor_t::right )
    {
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ bar.right + 3, y },
                     color_t{ 255, 255, 255, bar_alpha }, text, true, render::text_align::left );
        return;
    }

    g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ bar.left - 3, y },
                 color_t{ 255, 255, 255, bar_alpha }, text, true, render::text_align::right );
}

void esp::draw_bones( C_TFPlayer *player, color_t value, u8 opacity )
{
    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( player ) };

    if ( !studio || !studio->m_pStudioHdr )
    {
        return;
    }

    matrix3x4 bones[ MAX_STUDIO_BONES ]{ };
    if ( !player->SetupBones( bones, MAX_STUDIO_BONES, BONE_USED_BY_HITBOX, player->m_flSimulationTime( ) ) )
    {
        return;
    }

    const studiohdr_t *header{ studio->m_pStudioHdr };
    const color_t      bone_color{ modulate( value, opacity ) };

    for ( i32 index{ }; index < header->numbones; ++index )
    {
        const mstudiobone_t *bone{ header->pBone( index ) };

        if ( bone->parent < 0 || !( bone->flags & BONE_USED_BY_HITBOX ) ||
             !( header->pBone( bone->parent )->flags & BONE_USED_BY_HITBOX ) )
        {
            continue;
        }

        g_cl.draw_3d_line( bone_origin( bones[ index ] ), bone_origin( bones[ bone->parent ] ), bone_color, false );
    }
}

void esp::draw_flags( const esp_entity_t &entry, esp_layout_t &layout, config::esp_anchor_t anchor,
                      config::esp_align_t align, visual_record_t &visual, u32 delta, u8 opacity )
{
    static const flag_def_t FLAG_DEFS[ ESP_FLAG_COUNT ]{
        { ATLAS_XORSTR_STATIC( "DORMANT" ), { 220, 220, 220 } },
        { ATLAS_XORSTR_STATIC( "ATLAS" ), { 150, 120, 255 } },
        { ATLAS_XORSTR_STATIC( "PARTY" ), { 90, 220, 150 } },
        { ATLAS_XORSTR_STATIC( "UBER" ), { 255, 70, 70 } },
        { ATLAS_XORSTR_STATIC( "KRITZ" ), { 120, 200, 255 } },
        { ATLAS_XORSTR_STATIC( "CLOAK" ), { 0, 200, 200 } },
        { ATLAS_XORSTR_STATIC( "BONK" ), { 255, 220, 60 } },
        { ATLAS_XORSTR_STATIC( "ZOOM" ), { 60, 215, 235 } },
        { ATLAS_XORSTR_STATIC( "BURN" ), { 255, 140, 40 } },
        { ATLAS_XORSTR_STATIC( "JARATE" ), { 255, 255, 77 } },
        { ATLAS_XORSTR_STATIC( "MILK" ), { 230, 230, 230 } },
        { ATLAS_XORSTR_STATIC( "BLEED" ), { 180, 40, 40 } },
        { ATLAS_XORSTR_STATIC( "MARKED" ), { 200, 50, 200 } },
        { ATLAS_XORSTR_STATIC( "REVVED" ), { 170, 170, 170 } },
        { ATLAS_XORSTR_STATIC( "SLOWED" ), { 143, 143, 143 } },
        { ATLAS_XORSTR_STATIC( "TAUNT" ), { 156, 89, 255 } },
        { ATLAS_XORSTR_STATIC( "DISGUISED" ), { 227, 227, 227 } },
        { ATLAS_XORSTR_STATIC( "CHARGE" ), { 204, 255, 130 } },
        { ATLAS_XORSTR_STATIC( "BLASTJUMP" ), { 255, 160, 60 } },
        { ATLAS_XORSTR_STATIC( "LC" ), { 255, 70, 70 } },
        { ATLAS_XORSTR_STATIC( "BANNER" ), { 240, 190, 90 } },
        { ATLAS_XORSTR_STATIC( "BATTALION" ), { 90, 150, 255 } },
        { ATLAS_XORSTR_STATIC( "CONCHEROR" ), { 120, 230, 120 } },
        { ATLAS_XORSTR_STATIC( "MMMPH" ), { 255, 110, 160 } },
        { ATLAS_XORSTR_STATIC( "BULLET" ), { 220, 220, 230 } },
        { ATLAS_XORSTR_STATIC( "BLAST" ), { 220, 220, 230 } },
        { ATLAS_XORSTR_STATIC( "FIRE" ), { 220, 220, 230 } },
        { ATLAS_XORSTR_STATIC( "FEIGNED" ), { 0, 200, 200 } },
    };

    const u32 mask{ flag_mask( entry ) };

    float height{ };

    // the stack has to be measured before the first label is placed, otherwise the block
    // cannot be aligned against the box
    for ( usize index{ }; index < ESP_FLAG_COUNT; ++index )
    {
        const bool active{ ( ( mask >> index ) & 1u ) != 0u };
        float     &pop{ visual.m_flag_pop[ index ] };

        pop = active ? math::animate( pop, 1.0f, delta, FLAG_IN_RESPONSE ) : 0.0f;

        if ( pop >= 0.02f )
        {
            height += ( index >= FLAG_RESIST_FIRST ? RESIST_STEP : FLAG_STEP ) * pop;
        }
    }

    if ( height < 1.0f )
    {
        return;
    }

    const point_t            block{ layout.claim_block( anchor, align, static_cast< i32 >( height ) ) };
    const render::text_align text_align{ esp_layout_t::align_for( anchor ) };
    const bool               leftwards{ anchor == config::esp_anchor_t::left };

    float y_cursor{ static_cast< float >( block.y ) };

    for ( usize index{ }; index < ESP_FLAG_COUNT; ++index )
    {
        const float pop{ visual.m_flag_pop[ index ] };

        if ( pop < 0.02f )
        {
            continue;
        }

        const float eased{ math::smooth_step( pop ) };
        const u8    alpha{ static_cast< u8 >( eased * static_cast< float >( opacity ) ) };

        // a collapsing flag slides back towards the box instead of blinking out
        const float slide{ ( 1.0f - eased ) * 4.0f };
        const i32   x{ static_cast< i32 >( static_cast< float >( block.x ) +
                                           ( leftwards ? slide : -slide ) + 0.5f ) };
        const i32   y{ static_cast< i32 >( y_cursor + 0.5f ) };

        if ( index >= FLAG_RESIST_FIRST )
        {
            const usize resist{ index - FLAG_RESIST_FIRST };
            auto       *player{ entry.m_ent->As< C_TFPlayer * >( ) };

            bool popped{ };
            if ( !entry.m_dormant )
            {
                popped = resist == 0   ? player->InCond( TF_COND_MEDIGUN_UBER_BULLET_RESIST )
                         : resist == 1 ? player->InCond( TF_COND_MEDIGUN_UBER_BLAST_RESIST )
                                       : player->InCond( TF_COND_MEDIGUN_UBER_FIRE_RESIST );
            }

            const i32 slot{ static_cast< i32 >( render::icons::SIZE_VACC ) };
            const i32 icon_x{ text_align == render::text_align::right    ? x - slot
                              : text_align == render::text_align::center ? x - slot / 2
                                                                         : x };

            if ( popped )
            {
                const float duration{ g_tf2.weapon_vaccinator_resist_duration->GetFloat( ) };
                const float remaining{ g_player_data.vacc_remaining( player, static_cast< i32 >( resist ) ) };
                const float drain{ duration > 0.0f ? math::clamp( 1.0f - remaining / duration, 0.0f, 1.0f ) : 1.0f };

                g_draw.icon( static_cast< u32 >( resist ), render::icons::KIND_VACC,
                             point_t{ icon_x, y }, modulate( color_t{ 90, 90, 100 }, alpha ),
                             0.0f, true );
                g_draw.icon( static_cast< u32 >( resist ), render::icons::KIND_VACC,
                             point_t{ icon_x, y }, modulate( color_t{ 80, 160, 255 }, alpha ),
                             drain );

                if ( remaining > 0.0f )
                {
                    char timer[ 8 ]{ };
                    str::format_to( timer, sizeof( timer ), ATLAS_XORSTR( "{:.1f}s" ).get( ), remaining );

                    const i32 timer_x{ x + ( text_align == render::text_align::right    ? -slot
                                             : text_align == render::text_align::center ? slot / 2
                                                                                        : slot ) };

                    g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ timer_x, y + slot / 3 },
                                 color_t{ 255, 255, 255, alpha }, timer, true, text_align );
                }
            } else
            {
                g_draw.icon( static_cast< u32 >( resist ), render::icons::KIND_VACC,
                             point_t{ icon_x, y },
                             modulate( FLAG_DEFS[ index ].m_color, alpha ), 0.0f, true );
            }

            y_cursor += RESIST_STEP * pop;
            continue;
        }

        const char *label{ FLAG_DEFS[ index ].m_label };
        color_t     color{ FLAG_DEFS[ index ].m_color };
        char        timed[ 24 ]{ };

        if ( index == FLAG_PARTY )
        {
            color = client::party_color( entry.m_party );
            if ( entry.m_party > 1 )
            {
                str::format_to( timed, sizeof( timed ), ATLAS_XORSTR( "{} {}" ).get( ), label, entry.m_party - 1 );
                label = timed;
            }
        }

        if ( index >= FLAG_BUFF_FIRST )
        {
            const float remaining{ g_player_data.buff_remaining( entry.m_ent->entindex( ) ) };

            if ( remaining > 0.0f )
            {
                str::format_to( timed, sizeof( timed ), ATLAS_XORSTR( "{} {:.1f}s" ).get( ), label, remaining );
                label = timed;
            }
        }

        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ x, y }, modulate( color, alpha ),
                     label, true, text_align );

        // a collapsing flag releases its slot gradually, so the stack slides instead of jumping
        y_cursor += FLAG_STEP * pop;
    }
}

void esp::draw_building( const esp_entity_t &entry, const rect_t &bounds, u32 delta )
{
    auto *object{ entry.m_ent->As< C_BaseObject * >( ) };

    visual_record_t *visual{ this->visual_for( entry.m_ent ) };

    const u8 target{ entry.m_dead ? static_cast< u8 >( 0 ) : entry_alpha( entry ) };
    u8       opacity{ target };

    if ( visual )
    {
        const float response{ entry.m_dead ? DEATH_FADE_RESPONSE : ESP_ALPHA_RESPONSE };

        visual->m_alpha = math::animate( visual->m_alpha, static_cast< float >( target ) / 255.0f,
                                         delta, response );
        opacity         = static_cast< u8 >( math::clamp( visual->m_alpha, 0.0f, 1.0f ) * 255.0f );
    }

    if ( !opacity )
    {
        return;
    }

    // box, name, distance, health bar, flags
    const bool *elements{ g_cfg.m_esp.m_building_elements };

    if ( elements[ 0 ] )
    {
        const color_t shadow{ modulate( color_t{ 0, 0, 0 }, opacity ) };
        g_draw.box( rect_t{ bounds.left - 1, bounds.top - 1, bounds.right + 1, bounds.bottom + 1 }, shadow );
        g_draw.box( bounds, modulate( color_t{ 255, 255, 255 }, opacity ) );
        g_draw.box( rect_t{ bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.bottom - 1 }, shadow );
    }

    if ( visual && elements[ 3 ] )
    {
        const i32 health{ object->m_iHealth( ) };
        const i32 max_health{ object->m_iMaxHealth( ) };

        if ( max_health >= 1 && max_health <= 5000 && health >= 0 )
        {
            // buildings have no profile to drag, so their bar stays on the left edge
            esp_layout_t layout{ bounds };
            const float  ratio{ math::clamp( static_cast< float >( health ) /
                                                 static_cast< float >( max_health ),
                                             0.0f, 1.0f ) };
            this->draw_health_bar( layout, config::esp_anchor_t::left, *visual, health, max_health,
                                   config::HEALTH_STYLE_SOLID, health_color( ratio, 255 ), { }, { },
                                   0, delta, opacity );
        }
    }

    const i32 center_x{ bounds.left + ( bounds.right - bounds.left ) / 2 };

    // name with the level baked in
    if ( elements[ 1 ] )
    {
        const char *base{ };
        switch ( entry.m_class_id )
        {
            case ETFClassID::CObjectSentrygun:
            {
                base = object->m_bMiniBuilding( ) ? ATLAS_XORSTR_STATIC( "MINI-SENTRY" ) : ATLAS_XORSTR_STATIC( "SENTRY" );
                break;
            }

            case ETFClassID::CObjectDispenser:
            {
                base = ATLAS_XORSTR_STATIC( "DISPENSER" );
                break;
            }

            default:
            {
                base = object->m_iObjectMode( ) ? ATLAS_XORSTR_STATIC( "TELE OUT" ) : ATLAS_XORSTR_STATIC( "TELE IN" );
                break;
            }
        }

        char name[ 24 ]{ };
        if ( object->m_bMiniBuilding( ) )
        {
            str::format_to( name, sizeof( name ), ATLAS_XORSTR( "{}" ).get( ), base );
        } else
        {
            str::format_to( name, sizeof( name ), ATLAS_XORSTR( "{} (LEVEL {})" ).get( ), base, object->m_iUpgradeLevel( ) );
        }

        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, bounds.top - 12 },
                     color_t{ 255, 255, 255, opacity }, name, true, render::text_align::center );
    }

    i32  flag_x{ bounds.right + 4 };
    bool out_of_shells{ };
    bool out_of_rockets{ };

    if ( entry.m_class_id == ETFClassID::CObjectSentrygun && visual && elements[ 4 ] )
    {
        auto *sentry{ entry.m_ent->As< C_ObjectSentrygun * >( ) };

        const i32 level{ object->m_iUpgradeLevel( ) };
        const i32 max_shells{ level >= 2 ? SENTRYGUN_MAX_SHELLS_2 : SENTRYGUN_MAX_SHELLS_1 };
        const i32 shells{ sentry->m_iAmmoShells( ) };
        const i32 rockets{ sentry->m_iAmmoRockets( ) };

        // free running phase for the out-of-ammo blink, one cycle every 900ms
        visual->m_blink += static_cast< float >( delta ) / 900.0f;
        if ( visual->m_blink >= 1.0f )
        {
            visual->m_blink -= 1.0f;
        }

        out_of_shells  = shells <= 0;
        out_of_rockets = level == 3 && rockets <= 0;

        float cursor{ static_cast< float >( bounds.right + 4 ) };
        auto  ammo_column{ [ & ]( float &presence, float &fill, i32 value, i32 maximum, color_t color )
        {
            presence = math::animate( presence, value > 0 ? 1.0f : 0.0f, delta,
                                      presence > 0.5f ? AMMO_SLIDE_OUT_RESPONSE : FLAG_IN_RESPONSE );

            if ( presence > 0.02f )
            {
                const float ratio{ math::clamp( static_cast< float >( value ) /
                                                    static_cast< float >( maximum ),
                                                0.0f, 1.0f ) };
                fill = math::animate( fill, ratio, delta, HEALTH_FILL_RESPONSE );

                const float eased{ math::smooth_step( presence ) };
                const u8    alpha{ static_cast< u8 >( eased * static_cast< float >( opacity ) ) };

                const i32 left{ static_cast< i32 >( cursor - ( 1.0f - eased ) * AMMO_SLIDE + 0.5f ) };
                const i32 right{ left + 2 };

                g_draw.box( rect_t{ left - 1, bounds.top - 1, right + 1, bounds.bottom + 1 },
                            modulate( color_t{ 0, 0, 0 }, alpha ), true );

                const i32 height{ scale_to_height( bounds.bottom - bounds.top, fill ) };
                if ( height > 0 )
                {
                    g_draw.box( rect_t{ left, bounds.bottom - height, right, bounds.bottom },
                                modulate( color, alpha ), true );
                }
            }

            // bar width plus the split, released in proportion so columns reflow
            cursor += 5.0f * presence;
        } };

        ammo_column( visual->m_ammo_pop, visual->m_ammo, shells, max_shells, color_t{ 80, 140, 200 } );

        // only a level three stocks rockets at all
        ammo_column( visual->m_rocket_pop, visual->m_rockets, level == 3 ? rockets : 0,
                     SENTRYGUN_MAX_ROCKETS, color_t{ 210, 140, 90 } );

        flag_x = static_cast< i32 >( cursor + 0.5f ) + 4;
    }

    i32      flag_y{ bounds.top - 1 };
    const u8 flag_alpha{ static_cast< u8 >( ( 220u * opacity ) / 255u ) };

    auto flag{ [ & ]( const char *text, color_t color )
    {
        color.a = flag_alpha;
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ flag_x, flag_y }, color, text, true );
        flag_y += static_cast< i32 >( FLAG_STEP );
    } };

    if ( entry.m_class_id == ETFClassID::CObjectTeleporter && elements[ 4 ] )
    {
        auto *teleporter{ entry.m_ent->As< C_ObjectTeleporter * >( ) };

        switch ( teleporter->m_iState( ) )
        {
            case TELEPORTER_STATE_IDLE:
            {
                flag( ATLAS_XORSTR( "IDLE" ).get( ), { 192, 192, 192 } );
                break;
            }

            case TELEPORTER_STATE_READY:
            {
                flag( ATLAS_XORSTR( "ARMED" ).get( ), { 25, 225, 85 } );
                break;
            }

            case TELEPORTER_STATE_SENDING:
            case TELEPORTER_STATE_RECEIVING:
            case TELEPORTER_STATE_RECEIVING_RELEASE:
            {
                flag( ATLAS_XORSTR( "TELEPORTING" ).get( ), { 250, 28, 12 } );
                break;
            }

            case TELEPORTER_STATE_RECHARGING:
            {
                // m_flRechargeTime is the curtime stamp the cooldown ends at
                const float remaining{ math::clamp( teleporter->m_flRechargeTime( ) -
                                                        g_tf2.m_globals->curtime,
                                                    0.0f, 99.9f ) };

                char text[ 20 ]{ };
                str::format_to( text, sizeof( text ), ATLAS_XORSTR( "RECHARGE {:.1f}S" ).get( ), remaining );

                flag( text, { 250, 140, 15 } );
                break;
            }

            case TELEPORTER_STATE_UPGRADING:
            {
                flag( ATLAS_XORSTR( "UPGRADING" ).get( ), { 237, 135, 20 } );
                break;
            }

            default: break;
        }
    }

    if ( out_of_shells )
    {
        const float wave{ math::cos( ( visual ? visual->m_blink : 0.0f ) * ( PI * 2.0f ) ) * 0.5f + 0.5f };

        auto tone{ [ wave ]( u8 weak, u8 strong )
        {
            return static_cast< u8 >( static_cast< float >( weak ) +
                                      ( static_cast< float >( strong ) - static_cast< float >( weak ) ) * wave );
        } };

        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ flag_x, flag_y },
                     color_t{ tone( 150, 255 ), tone( 120, 235 ), tone( 30, 120 ), flag_alpha },
                     ATLAS_XORSTR( "NO AMMO" ).get( ), true );
        flag_y += static_cast< i32 >( FLAG_STEP );
    }

    if ( out_of_rockets )
    {
        flag( ATLAS_XORSTR( "NO ROCKETS" ).get( ), { 210, 140, 90 } );
    }

    if ( object->m_bHasSapper( ) && elements[ 4 ] )
    {
        flag( ATLAS_XORSTR( "SAPPED" ).get( ), { 110, 200, 230 } );
    }

    if ( elements[ 2 ] )
    {
        // 0.01905 is the hammer-unit to meter factor the reference client uses
        char distance[ 12 ]{ };
        str::format_to( distance, sizeof( distance ), ATLAS_XORSTR( "[{}M]" ).get( ),
                        static_cast< i32 >( entry.m_distance * 0.01905f + 0.5f ) );
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, bounds.bottom + 3 },
                     color_t{ 255, 255, 255, opacity }, distance, true, render::text_align::center );
    }
}

void esp::draw_projectile( const esp_entity_t &entry, const rect_t &bounds, u32 delta )
{
    visual_record_t *visual{ this->visual_for( entry.m_ent ) };

    u8 opacity{ 255 };

    if ( visual )
    {
        visual->m_alpha = math::animate( visual->m_alpha, 1.0f, delta, ESP_ALPHA_RESPONSE );
        opacity         = static_cast< u8 >( math::clamp( visual->m_alpha, 0.0f, 1.0f ) * 255.0f );
    }

    if ( !opacity )
    {
        return;
    }

    // projectile collision hulls are a few units wide, keep the box readable
    rect_t        box{ bounds };
    constexpr i32 MIN_SIZE{ 8 };

    if ( box.right - box.left < MIN_SIZE )
    {
        const i32 center{ ( box.left + box.right ) / 2 };
        box.left  = center - MIN_SIZE / 2;
        box.right = center + MIN_SIZE / 2;
    }

    if ( box.bottom - box.top < MIN_SIZE )
    {
        const i32 center{ ( box.top + box.bottom ) / 2 };
        box.top    = center - MIN_SIZE / 2;
        box.bottom = center + MIN_SIZE / 2;
    }

    // box, name, distance, flags
    const bool *elements{ g_cfg.m_esp.m_projectile_elements };

    if ( elements[ 0 ] )
    {
        const color_t shadow{ modulate( color_t{ 0, 0, 0 }, opacity ) };
        g_draw.box( rect_t{ box.left - 1, box.top - 1, box.right + 1, box.bottom + 1 }, shadow );
        g_draw.box( box, modulate( color_t{ 255, 255, 255 }, opacity ) );
        g_draw.box( rect_t{ box.left + 1, box.top + 1, box.right - 1, box.bottom - 1 }, shadow );
    }

    const char *name{ ATLAS_XORSTR_STATIC( "PROJECTILE" ) };
    switch ( entry.m_class_id )
    {
        case ETFClassID::CTFProjectile_Rocket:
        {
            name = ATLAS_XORSTR_STATIC( "ROCKET" );
            break;
        }

        case ETFClassID::CTFProjectile_SentryRocket:
        {
            name = ATLAS_XORSTR_STATIC( "SENTRY ROCKET" );
            break;
        }

        case ETFClassID::CTFProjectile_Arrow:
        {
            name = ATLAS_XORSTR_STATIC( "ARROW" );
            break;
        }

        case ETFClassID::CTFProjectile_Flare:
        {
            name = ATLAS_XORSTR_STATIC( "FLARE" );
            break;
        }

        case ETFClassID::CTFProjectile_HealingBolt:
        {
            name = ATLAS_XORSTR_STATIC( "HEALING BOLT" );
            break;
        }

        case ETFClassID::CTFProjectile_Cleaver:
        {
            name = ATLAS_XORSTR_STATIC( "CLEAVER" );
            break;
        }

        case ETFClassID::CTFGrenadePipebombProjectile:
        {
            auto      *pipe{ entry.m_ent->As< C_TFGrenadePipebombProjectile * >( ) };
            const bool sticky{ pipe->m_iType( ) == TF_GL_MODE_REMOTE_DETONATE };
            name = sticky ? ATLAS_XORSTR_STATIC( "STICKY" ) : ATLAS_XORSTR_STATIC( "PIPE" );
            break;
        }

        default:
            break;
    }

    const i32 center_x{ ( box.left + box.right ) / 2 };

    if ( elements[ 1 ] )
    {
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, box.top - 12 },
                     color_t{ 255, 255, 255, opacity }, name, true, render::text_align::center );
    }

    bool critical{ };

    switch ( entry.m_class_id )
    {
        case ETFClassID::CTFProjectile_Rocket:
        case ETFClassID::CTFProjectile_SentryRocket:
            critical = entry.m_ent->As< C_TFProjectile_Rocket * >( )->m_bCritical( );
            break;

        case ETFClassID::CTFProjectile_Arrow:
        case ETFClassID::CTFProjectile_HealingBolt:
            critical = entry.m_ent->As< C_TFProjectile_Arrow * >( )->m_bCritical( );
            break;

        case ETFClassID::CTFProjectile_Flare:
            critical = entry.m_ent->As< C_TFProjectile_Flare * >( )->m_bCritical( );
            break;

        case ETFClassID::CTFGrenadePipebombProjectile:
            critical = entry.m_ent->As< C_TFGrenadePipebombProjectile * >( )->m_bCritical( );
            break;

        default:
            break;
    }

    if ( critical && elements[ 3 ] )
    {
        const u8 flag_alpha{ static_cast< u8 >( ( 220u * opacity ) / 255u ) };
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ box.right + 4, box.top - 1 },
                     color_t{ 255, 70, 70, flag_alpha }, ATLAS_XORSTR( "CRIT" ).get( ), true );
    }

    if ( elements[ 2 ] )
    {
        // 0.01905 is the hammer-unit to meter factor the reference client uses
        char distance[ 12 ]{ };
        str::format_to( distance, sizeof( distance ), ATLAS_XORSTR( "[{}M]" ).get( ),
                        static_cast< i32 >( entry.m_distance * 0.01905f + 0.5f ) );
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, box.bottom + 3 },
                     color_t{ 255, 255, 255, opacity }, distance, true, render::text_align::center );
    }
}

void esp::draw_pack( const esp_entity_t &entry, const rect_t &bounds, u32 delta )
{
    const pickup_record_t *record{ this->find_pickup( entry.m_handle ) };
    Assert( record != nullptr );

    const bool    health{ record->m_kind == config::PICKUP_HEALTH };
    const color_t body{ health ? color_t{ 120, 220, 120 } : color_t{ 200, 200, 130 } };
    const bool   *elements{ g_cfg.m_esp.m_pickup_elements };

    visual_record_t *visual{ this->visual_for( entry.m_ent ) };
    if ( !visual )
    {
        return;
    }

    const float remaining{ record->m_respawn - g_tf2.m_globals->curtime };
    const bool  taken{ remaining > 0.0f };
    const float target{ taken ? 0.0f : 1.0f };

    visual->m_alpha = math::animate( visual->m_alpha, target, delta, ESP_ALPHA_RESPONSE );
    const u8 opacity{ static_cast< u8 >( math::clamp( visual->m_alpha, 0.0f, 1.0f ) * 255.0f ) };

    if ( opacity > 2 )
    {
        if ( elements[ 0 ] )
        {
            const color_t shadow{ modulate( color_t{ 0, 0, 0 }, opacity ) };
            g_draw.box( rect_t{ bounds.left - 1, bounds.top - 1, bounds.right + 1, bounds.bottom + 1 }, shadow );
            g_draw.box( bounds, modulate( body, opacity ) );
            g_draw.box( rect_t{ bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.bottom - 1 }, shadow );
        }

        const i32 center_x{ bounds.left + ( bounds.right - bounds.left ) / 2 };

        if ( elements[ 1 ] )
        {
            g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, bounds.top - 12 },
                         modulate( body, opacity ), health ? ATLAS_XORSTR_STATIC( "HEALTH" ) : ATLAS_XORSTR_STATIC( "AMMO" ),
                         true, render::text_align::center );
        }

        if ( elements[ 2 ] )
        {
            // 0.01905 is the hammer-unit to meter factor the reference client uses
            char distance[ 12 ]{ };
            str::format_to( distance, sizeof( distance ), ATLAS_XORSTR( "[{}M]" ).get( ),
                            static_cast< i32 >( entry.m_distance * 0.01905f + 0.5f ) );
            g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, bounds.bottom + 3 },
                         color_t{ 255, 255, 255, opacity }, distance, true, render::text_align::center );
        }
    }

    if ( taken && elements[ 3 ] )
    {
        const u8 readout{ static_cast< u8 >( ( 1.0f - math::clamp( visual->m_alpha, 0.0f, 1.0f ) ) * 255.0f ) };
        if ( readout <= 2 )
        {
            return;
        }

        const float progress{ math::clamp( 1.0f - remaining / TF_ITEM_RESPAWN_TIME, 0.0f, 1.0f ) };

        const i32 center_x{ bounds.left + ( bounds.right - bounds.left ) / 2 };
        const i32 center_y{ bounds.top + ( bounds.bottom - bounds.top ) / 2 };

        char text[ 12 ]{ };
        str::format_to( text, sizeof( text ), ATLAS_XORSTR( "{:.1f}s" ).get( ), remaining );
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, center_y - 10 },
                     color_t{ 220, 220, 220, readout }, text, true, render::text_align::center );

        const i32 bar_left{ center_x - 26 / 2 };
        const i32 bar_right{ bar_left + 26 };

        g_draw.box( rect_t{ bar_left - 1, center_y - 1, bar_right + 1, center_y + 3 },
                    modulate( color_t{ 0, 0, 0 }, readout ), true );

        const i32 fill{ static_cast< i32 >( static_cast< float >( 26 ) * progress + 0.5f ) };
        if ( fill > 0 )
        {
            g_draw.box( rect_t{ bar_left, center_y, bar_left + fill, center_y + 2 },
                        modulate( body, readout ), true );
        }
    }
}

void esp::draw_objective( const esp_entity_t &entry, const rect_t &bounds, u32 delta )
{
    visual_record_t *visual{ this->visual_for( entry.m_ent ) };

    u8 opacity{ 255 };

    if ( visual )
    {
        visual->m_alpha = math::animate( visual->m_alpha, 1.0f, delta, ESP_ALPHA_RESPONSE );
        opacity         = static_cast< u8 >( math::clamp( visual->m_alpha, 0.0f, 1.0f ) * 255.0f );
    }

    if ( !opacity )
    {
        return;
    }

    color_t   body{ color_t{ 255, 255, 255 } };
    const i32 team{ entry.m_ent->m_iTeamNum( ) };

    if ( team == TF_TEAM_RED )
    {
        body = color_t{ 255, 90, 90 };
    } else if ( team == TF_TEAM_BLUE )
    {
        body = color_t{ 90, 140, 255 };
    }

    const bool *elements{ g_cfg.m_esp.m_intel_elements };

    if ( elements[ 0 ] )
    {
        const color_t shadow{ modulate( color_t{ 0, 0, 0 }, opacity ) };
        g_draw.box( rect_t{ bounds.left - 1, bounds.top - 1, bounds.right + 1, bounds.bottom + 1 }, shadow );
        g_draw.box( bounds, modulate( body, opacity ) );
        g_draw.box( rect_t{ bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.bottom - 1 }, shadow );
    }

    const i32 center_x{ bounds.left + ( bounds.right - bounds.left ) / 2 };

    if ( elements[ 1 ] )
    {
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, bounds.top - 12 },
                     color_t{ 255, 255, 255, opacity }, ATLAS_XORSTR( "INTEL" ).get( ), true, render::text_align::center );
    }

    auto *flag{ entry.m_ent->As< C_CaptureFlag * >( ) };
    if ( elements[ 3 ] && flag->m_nFlagStatus( ) == TF_FLAGINFO_DROPPED )
    {
        const u8 flag_alpha{ static_cast< u8 >( ( 220u * opacity ) / 255u ) };
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ bounds.right + 4, bounds.top - 1 },
                     color_t{ 255, 220, 60, flag_alpha }, ATLAS_XORSTR( "DROPPED" ).get( ), true );
    }

    if ( elements[ 2 ] )
    {
        // 0.01905 is the hammer-unit to meter factor the reference client uses
        char distance[ 12 ]{ };
        str::format_to( distance, sizeof( distance ), ATLAS_XORSTR( "[{}M]" ).get( ),
                        static_cast< i32 >( entry.m_distance * 0.01905f + 0.5f ) );
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, bounds.bottom + 3 },
                     color_t{ 255, 255, 255, opacity }, distance, true, render::text_align::center );
    }
}

void esp::draw_npc( const esp_entity_t &entry, const rect_t &bounds, u32 delta )
{
    visual_record_t *visual{ this->visual_for( entry.m_ent ) };

    const u8 target{ entry_alpha( entry ) };
    u8       opacity{ target };

    if ( visual )
    {
        visual->m_alpha = math::animate( visual->m_alpha, static_cast< float >( target ) / 255.0f,
                                         delta, ESP_ALPHA_RESPONSE );
        opacity         = static_cast< u8 >( math::clamp( visual->m_alpha, 0.0f, 1.0f ) * 255.0f );
    }

    if ( !opacity )
    {
        return;
    }

    // box, name, distance
    const bool *elements{ g_cfg.m_esp.m_npc_elements };

    if ( elements[ 0 ] )
    {
        const color_t shadow{ modulate( color_t{ 0, 0, 0 }, opacity ) };
        g_draw.box( rect_t{ bounds.left - 1, bounds.top - 1, bounds.right + 1, bounds.bottom + 1 }, shadow );
        g_draw.box( bounds, modulate( color_t{ 255, 255, 255 }, opacity ) );
        g_draw.box( rect_t{ bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.bottom - 1 }, shadow );
    }

    const i32 center_x{ bounds.left + ( bounds.right - bounds.left ) / 2 };

    if ( elements[ 1 ] )
    {
        const char *name{ };
        switch ( entry.m_class_id )
        {
            case ETFClassID::CEyeballBoss: name = ATLAS_XORSTR_STATIC( "MONOCULUS" ); break;
            case ETFClassID::CHeadlessHatman: name = ATLAS_XORSTR_STATIC( "HORSEMANN" ); break;
            case ETFClassID::CMerasmus: name = ATLAS_XORSTR_STATIC( "MERASMUS" ); break;
            default: name = ATLAS_XORSTR_STATIC( "SKELETON" ); break;
        }

        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, bounds.top - 12 },
                     color_t{ 255, 255, 255, opacity }, name, true, render::text_align::center );
    }

    if ( elements[ 2 ] )
    {
        // 0.01905 is the hammer-unit to meter factor the reference client uses
        char distance[ 12 ]{ };
        str::format_to( distance, sizeof( distance ), ATLAS_XORSTR( "[{}M]" ).get( ),
                        static_cast< i32 >( entry.m_distance * 0.01905f + 0.5f ) );
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ center_x, bounds.bottom + 3 },
                     color_t{ 255, 255, 255, opacity }, distance, true, render::text_align::center );
    }
}

// doing this guy an exclusive func because it holds a lot of logic that we can apply to more than one type of entity.
void esp::draw_arrow( const esp_entity_t &entry, bool on_screen, u32 delta )
{
    visual_record_t *visual{ this->visual_for( entry.m_ent ) };

    if ( !visual )
    {
        return;
    }

    const config::arrows_t &cfg{ g_cfg.m_arrows };

    const auto &m{ g_cl.m_w2s };
    vec2        forward{ m[ 3 ][ 0 ], m[ 3 ][ 1 ] };
    forward.normalize( );

    const vec2  right{ forward.y, -forward.x };
    const vec3  offset{ entry.m_origin - g_cl.m_local->m_vecOrigin( ) };
    const float bearing{ math::atan2( offset.x * right.x + offset.y * right.y,
                                      offset.x * forward.x + offset.y * forward.y ) };

    const bool  fresh{ visual->m_arrow <= 0.0f };
    const float shown{ on_screen || entry.m_dead ? 0.0f : static_cast< float >( entry_alpha( entry ) ) / 255.0f };
    const float response{ shown > visual->m_arrow ? 90.0f : 160.0f };

    visual->m_arrow = math::animate( visual->m_arrow, shown, delta, response );

    if ( visual->m_arrow <= 0.0f )
    {
        return;
    }

    if ( fresh )
    {
        visual->m_arrow_angle = bearing;
    } else
    {
        const float turn{ math::wrap_turn( bearing - visual->m_arrow_angle ) };
        visual->m_arrow_angle = math::wrap_turn( visual->m_arrow_angle + math::animate( 0.0f, turn, delta, 70.0f ) );
    }

    const float presence{ visual->m_arrow };
    const float landed{ math::smaller( presence * 2.0f, 1.0f ) };

    const i32   screen_width{ g_cl.m_screen_width };
    const i32   screen_height{ g_cl.m_screen_height };
    const vec2  radial{ math::sin( visual->m_arrow_angle ), -math::cos( visual->m_arrow_angle ) };
    const float ring{ static_cast< float >( cfg.m_radius ) + ( 1.0f - landed ) * 28.0f };
    const vec2  at{ static_cast< float >( screen_width ) * 0.5f + radial.x * ring,
                    static_cast< float >( screen_height ) * 0.5f + radial.y * ring };

    const float half_height{ static_cast< float >( cfg.m_size ) * ( 0.8f + 0.2f * landed ) };
    const float width{ half_height * 1.5f };
    const float band{ width * 0.55f };
    const float spread{ width * 0.7f };

    const u8      opacity{ static_cast< u8 >( presence * 255.0f + 0.5f ) };
    const color_t base{ modulate( cfg.m_color, opacity ) };
    const color_t shadow{ modulate( { 0, 0, 0, 120 }, opacity ) };
    const color_t white{ 255, 255, 255, base.a };
    const color_t black{ 0, 0, 0, base.a };
    const color_t back{ base.blend( black, 0.4f ) };

    draw_chevron( at, radial, -spread * 0.5f, half_height, width, band, back, back.blend( white, 0.1f ), shadow );
    draw_chevron( at, radial, spread * 0.5f, half_height, width, band, base, base.blend( white, 0.15f ), shadow );

    if ( !cfg.m_distance )
    {
        return;
    }

    char text[ 12 ]{ };
    str::format_to( text, sizeof( text ), ATLAS_XORSTR( "{}M" ).get( ),
                    static_cast< i32 >( entry.m_distance * 0.01905f + 0.5f ) );

    const vec2 label{ at - radial * ( spread * 0.5f + width * 0.5f + 8.0f ) };

    g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ),
                 point_t{ static_cast< i32 >( label.x ), static_cast< i32 >( label.y ) - ESP_SMALL_HEIGHT / 2 },
                 modulate( color_t{ 255, 255, 255 }, opacity ), text, true, render::text_align::center );
}

void esp::draw_health_bar( esp_layout_t &layout, config::esp_anchor_t anchor,
                           visual_record_t &visual, i32 health, i32 max_health, usize style,
                           color_t high, color_t low, color_t overheal_value, i32 divisions,
                           u32 delta, u8 opacity )
{
    const bool vertical{ anchor == config::esp_anchor_t::left ||
                         anchor == config::esp_anchor_t::right };
    rect_t     bar{ layout.claim_bar( anchor ) };

    if ( vertical )
    {
        ++bar.bottom;
    }

    const i32 span{ vertical ? bar.bottom - bar.top : bar.right - bar.left };

    if ( span < 4 )
    {
        return;
    }

    // overheal in tf2 caps at 150% of max health, so the raw ratio runs 0..1.5..
    const f32 raw{ math::clamp( static_cast< f32 >( health ) / static_cast< f32 >( max_health ), 0.0f, 1.5f ) };
    const f32 ratio{ math::clamp( raw, 0.0f, 1.0f ) };
    const f32 overheal{ math::clamp( ( raw - 1.0f ) / 0.5f, 0.0f, 1.0f ) };

    // the first frame snaps so a player entering view does not sweep the bar up from empty
    if ( !visual.m_seeded )
    {
        visual.m_fill     = ratio;
        visual.m_overheal = overheal;
        visual.m_lag      = ratio;
        visual.m_ratio    = ratio;
        visual.m_seeded   = true;
    }

    // a drop arms the trail so the pre-damage value hangs behind the fill for a moment
    if ( ratio < visual.m_ratio )
    {
        visual.m_lag_hold = 0.2f;
    }

    visual.m_ratio    = ratio;
    visual.m_fill     = math::animate( visual.m_fill, ratio, delta, HEALTH_FILL_RESPONSE );
    visual.m_overheal = math::animate( visual.m_overheal, overheal, delta, HEALTH_FILL_RESPONSE );

    if ( visual.m_lag <= visual.m_fill )
    {
        // healing caught up with the trail, there is nothing left to drain
        visual.m_lag      = visual.m_fill;
        visual.m_lag_hold = 0.0f;
    } else if ( visual.m_lag_hold > 0.0f )
    {
        visual.m_lag_hold -= static_cast< float >( delta ) / 1000.0f;
    } else
    {
        visual.m_lag = math::animate( visual.m_lag, visual.m_fill, delta, 210.0f );
    }

    g_draw.box( rect_t{ bar.left - 1, bar.top - 1, bar.right + 1, bar.bottom + 1 },
                modulate( ESP_BAR_BACKGROUND, opacity ), true );

    const i32 fill{ scale_to_height( span, visual.m_fill ) };
    const i32 lag{ scale_to_height( span, visual.m_lag ) };
    const i32 overheal_length{ scale_to_height( span, visual.m_overheal ) };

    // kept well below the fill's channel ceiling of 200 so the trail never competes with it
    if ( lag > fill )
    {
        g_draw.box( bar_fill( bar, vertical, lag, fill ),
                    modulate( color_t{ 85, 22, 22 }, opacity ), true );
    }

    const color_t head_color{ style == config::HEALTH_STYLE_HEALTH ? low.blend( high, visual.m_fill ) : high };
    const color_t base_color{ style == config::HEALTH_STYLE_GRADIENT ? low : head_color };

    if ( fill > 0 )
    {
        g_draw.gradient( bar_fill( bar, vertical, fill, 0 ),
                         modulate( vertical ? head_color : base_color, opacity ),
                         modulate( vertical ? base_color : head_color, opacity ), vertical );
    }

    if ( overheal_length > 0 )
    {
        g_draw.box( bar_fill( bar, vertical, overheal_length, 0 ),
                    modulate( overheal_value, opacity ), true );
    }

    draw_bar_divisions( bar, vertical, divisions, opacity );

    visual.m_hp_pop = math::animate( visual.m_hp_pop, health != max_health ? 1.0f : 0.0f,
                                     delta, 170.0f );

    if ( visual.m_hp_pop <= 0.02f )
    {
        return;
    }

    char text[ 12 ]{ };
    str::format_to( text, sizeof( text ), health > max_health ? ATLAS_XORSTR( "+{}HP" ).get( ) : ATLAS_XORSTR( "{}HP" ).get( ), health );

    const float eased{ math::smooth_step( visual.m_hp_pop ) };
    const u8    alpha{ static_cast< u8 >( eased * static_cast< float >( opacity ) ) };

    const color_t text_color{ health > max_health ? modulate( overheal_value, alpha )
                                                  : modulate( head_color, alpha ) };

    const i32 slide{ static_cast< i32 >( ( 1.0f - eased ) * 5.0f + 0.5f ) };

    // the number rides the head of the fill on a column, and sits at the foot of a row
    if ( !vertical )
    {
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ),
                     point_t{ bar.left - 2 - slide, bar.top - ESP_SMALL_HEIGHT / 2 }, text_color,
                     text, true, render::text_align::right );
        return;
    }

    const i32 head{ bar.bottom - math::bigger( fill, overheal_length ) };
    const i32 y{ math::clamp( head - 4, bar.top - 1, bar.bottom - 9 ) };

    if ( anchor == config::esp_anchor_t::right )
    {
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ bar.right + 2 + slide, y },
                     text_color, text, true, render::text_align::left );
        return;
    }

    g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ bar.left - 2 - slide, y }, text_color,
                 text, true, render::text_align::right );
}

bool esp::query_health( C_BaseEntity *entity, i32 &health, i32 &max_health ) const
{
    if ( !g_cl.m_resource )
    {
        return false;
    }

    const i32 index{ entity->entindex( ) };
    if ( index < 1 || index > g_tf2.m_engine->GetMaxClients( ) )
    {
        return false;
    }

    health     = g_cl.m_resource->m_iHealth( )[ index ];
    max_health = g_cl.m_resource->m_iMaxHealth( )[ index ];

    if ( max_health < 1 || health < 0 )
    {
        return false;
    }

    return true;
}

void esp::on_item_pickup( i32 player, usize kind )
{
    this->m_pickup_events.emplace_back( player, kind );
}

void esp::resolve_pickup( const pickup_event_t &event )
{
    if ( event.m_player < 1 || event.m_player > g_tf2.m_globals->maxClients )
    {
        return;
    }

    IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( event.m_player ) };
    C_BaseEntity  *base_entity{ client_entity ? client_entity->GetBaseEntity( ) : nullptr };
    CNetChannel   *channel{ g_tf2.m_engine->GetNetChannelInfo( ) };

    if ( !base_entity || !channel )
    {
        return;
    }

    auto       *player{ base_entity->As< C_TFPlayer * >( ) };
    const float now{ g_tf2.m_globals->curtime };
    const float round_trip{ channel->GetLatency( FLOW_OUTGOING ) + channel->GetLatency( FLOW_INCOMING ) };
    const float batch{ math::clamp( g_player_data.simulation_time( event.m_player ) -
                                        g_player_data.previous_simulation_time( event.m_player ),
                                    0.0f, TICKS_TO_TIME( g_tf2.sv_maxusrcmdprocessticks->GetInt( ) ) ) };
    vec3        origin{ player->m_vecOrigin( ) };
    float       elapsed{ round_trip + batch };

    if ( player->IsDormant( ) )
    {
        float remaining{ };
        if ( !g_player_data.dormant_origin( player, origin, remaining ) )
        {
            return;
        }

        elapsed = round_trip + player_data::window( ) - remaining;
    }

    const float      slack{ player->m_flMaxspeed( ) * elapsed };
    pickup_record_t *best{ };
    bool             best_vanished{ };
    float            best_distance{ };

    for ( pickup_record_t &record : this->m_pickups )
    {
        if ( record.m_kind != event.m_kind ||
             !record.m_respawns ||
             !record.m_dormant ||
             record.m_respawn > now )
        {
            continue;
        }

        C_BaseEntity *pack{ g_entity_cache.resolve( record.m_handle ) };
        if ( !pack || !could_touch( pack, player, origin, slack ) )
        {
            continue;
        }

        const bool  vanished{ record.m_vanished > 0.0f && now - record.m_vanished <= round_trip };
        const float distance{ ( pack->m_vecOrigin( ) - origin ).length( ) };

        if ( best && ( best_vanished > vanished || ( best_vanished == vanished && best_distance <= distance ) ) )
        {
            continue;
        }

        best          = &record;
        best_vanished = vanished;
        best_distance = distance;
    }

    if ( !best )
    {
        return;
    }

    best->m_respawn  = ( best_vanished ? best->m_vanished : now ) + TF_ITEM_RESPAWN_TIME;
    best->m_vanished = 0.0f;
}

void esp::on_level_shutdown( )
{
    this->m_entities.clear( );
    this->m_pickups.clear( );
    this->m_pickup_events.clear( );
    this->m_visuals.reset( );
    this->m_last_paint = 0.0f;
}
