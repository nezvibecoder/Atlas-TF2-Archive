#include "chams.hpp"
#include "entity_cache.hpp"

#include "anti_aim.hpp"
#include "lagcomp.hpp"
#include "model_render.hpp"

static color_t faded( const color_t &color, float fade )
{
    color_t out{ color };
    out.a = static_cast< u8 >( static_cast< float >( color.a ) *
                               math::clamp( fade, 0.0f, 1.0f ) );
    return out;
}

ATLAS_FORCE_INLINE bool is_d3d_mode( usize mode )
{
    switch ( mode )
    {
        case chams::CHAM_OFF:
        case chams::CHAM_FLAT:
        case chams::CHAM_SHADED:
        case chams::CHAM_FRESNEL:
        case chams::CHAM_WIREFRAME:
        case chams::CHAM_LAVA:
        case chams::CHAM_PEARL:
        case chams::CHAM_BUBBLE:
        case chams::CHAM_LSD:
        case chams::CHAM_NETWORK:
        case chams::CHAM_SOAP:
        case chams::CHAM_AURORA:
        case chams::CHAM_AQUARIUM:
        case chams::CHAM_SKY: return true;
        default: return false;
    }
}

ATLAS_FORCE_INLINE model_render::shade_t shade_of( usize mode )
{
    switch ( mode )
    {
        case chams::CHAM_SHADED: return model_render::SHADE_SHADED;
        case chams::CHAM_FRESNEL: return model_render::SHADE_FRESNEL;
        case chams::CHAM_LAVA: return model_render::SHADE_LAVA;
        case chams::CHAM_PEARL: return model_render::SHADE_PEARL;
        case chams::CHAM_BUBBLE: return model_render::SHADE_BUBBLE;
        case chams::CHAM_LSD: return model_render::SHADE_LSD;
        case chams::CHAM_NETWORK: return model_render::SHADE_NETWORK;
        case chams::CHAM_SOAP: return model_render::SHADE_SOAP;
        case chams::CHAM_AURORA: return model_render::SHADE_AURORA;
        case chams::CHAM_AQUARIUM: return model_render::SHADE_AQUARIUM;
        case chams::CHAM_SKY: return model_render::SHADE_SKY;
        default: return model_render::SHADE_FLAT;
    }
}

ATLAS_FORCE_INLINE bool is_wireframe( usize mode )
{
    return mode == chams::CHAM_WIREFRAME || mode == chams::CHAM_NETWORK;
}

ATLAS_FORCE_INLINE bool is_d3d_style( const config::cham_profile_t &style )
{
    return is_d3d_mode( style.m_visible ) && is_d3d_mode( style.m_visible_overlay ) &&
           is_d3d_mode( style.m_invisible ) && is_d3d_mode( style.m_invisible_overlay );
}

ATLAS_FORCE_INLINE bool is_engine_base_style( const config::cham_profile_t &style )
{
    return ( style.m_visible == chams::CHAM_OFF || style.m_visible == chams::CHAM_TEXTURED ) &&
           is_d3d_mode( style.m_visible_overlay ) && is_d3d_mode( style.m_invisible ) && is_d3d_mode( style.m_invisible_overlay ) &&
           ( style.m_visible_overlay != chams::CHAM_OFF || style.m_invisible != chams::CHAM_OFF || style.m_invisible_overlay != chams::CHAM_OFF );
}

ATLAS_NO_DESTROY chams g_chams{ };

static KeyValues *build_flat( )
{
    KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "UnlitGeneric" ).get( ) ) };
    kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "vgui/white_additive" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$nodecal" ).get( ), 1 );
    return kv;
}

static KeyValues *build_shaded( )
{
    KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "VertexLitGeneric" ).get( ) ) };
    kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "vgui/white_additive" ).get( ) );
    kv->SetString( ATLAS_XORSTR( "$bumpmap" ).get( ), ATLAS_XORSTR( "vgui/white_additive" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$selfillum" ).get( ), 1 );
    kv->SetInt( ATLAS_XORSTR( "$selfillumfresnel" ).get( ), 1 );
    kv->SetString( ATLAS_XORSTR( "$selfillumfresnelminmaxexp" ).get( ), ATLAS_XORSTR( "[-0.25 1 1]" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$nodecal" ).get( ), 1 );
    return kv;
}

static KeyValues *build_fresnel( )
{
    KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "VertexLitGeneric" ).get( ) ) };
    kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "vgui/white_additive" ).get( ) );
    kv->SetString( ATLAS_XORSTR( "$bumpmap" ).get( ), ATLAS_XORSTR( "vgui/white_additive" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$additive" ).get( ), 1 );
    kv->SetString( ATLAS_XORSTR( "$color2" ).get( ), ATLAS_XORSTR( "[5.5 5.5 5.5]" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$selfillum" ).get( ), 1 );
    kv->SetInt( ATLAS_XORSTR( "$selfillumfresnel" ).get( ), 1 );
    kv->SetInt( ATLAS_XORSTR( "$nodecal" ).get( ), 1 );
    kv->SetString( ATLAS_XORSTR( "$selfillumfresnelminmaxexp" ).get( ), ATLAS_XORSTR( "[0 0 0.75]" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$nodecal" ).get( ), 1 );
    return kv;
}

static KeyValues *build_lava( )
{
    KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "VertexLitGeneric" ).get( ) ) };
    kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "vgui/white_additive" ).get( ) );
    kv->SetString( ATLAS_XORSTR( "$detail" ).get( ), ATLAS_XORSTR( "effects/tiledfire/fireLayeredSlowTiled512" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$detailblendmode" ).get( ), 0 );
    kv->SetString( ATLAS_XORSTR( "$detailblendfactor" ).get( ), ATLAS_XORSTR( "0.8" ).get( ) );
    kv->SetString( ATLAS_XORSTR( "$detailscale" ).get( ), ATLAS_XORSTR( "0.75" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$detailframe" ).get( ), 0 );
    kv->SetInt( ATLAS_XORSTR( "$selfillum" ).get( ), 1 );
    kv->SetInt( ATLAS_XORSTR( "$selfillumfresnel" ).get( ), 1 );
    kv->SetString( ATLAS_XORSTR( "$selfillumfresnelminmaxexp" ).get( ), ATLAS_XORSTR( "[-0.25 1 1]" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$nodecal" ).get( ), 1 );
    return kv;
}

static KeyValues *build_macintosh_plus( )
{
    KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "UnlitGeneric" ).get( ) ) };
    kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "effects/rainbow_curl" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$model" ).get( ), 1 );
    kv->SetString( ATLAS_XORSTR( "$envmap" ).get( ), ATLAS_XORSTR( "cubemaps/cubemap_sheen001" ).get( ) );
    kv->SetString( ATLAS_XORSTR( "$envmaptint" ).get( ), ATLAS_XORSTR( "[0.6 0.6 0.6]" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$nofog" ).get( ), 1 );
    kv->SetInt( ATLAS_XORSTR( "$nodecal" ).get( ), 1 );
    return kv;
}

static KeyValues *build_bubble( )
{
    KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "VertexLitGeneric" ).get( ) ) };
    kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "vgui/white_additive" ).get( ) );
    kv->SetString( ATLAS_XORSTR( "$lightwarptexture" ).get( ), ATLAS_XORSTR( "atlas_cham_ramp" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$nodecal" ).get( ), 1 );
    return kv;
}

static KeyValues *build_lsd( )
{
    KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "VertexLitGeneric" ).get( ) ) };
    kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "vgui/white_additive" ).get( ) );
    kv->SetString( ATLAS_XORSTR( "$lightwarptexture" ).get( ), ATLAS_XORSTR( "atlas_cham_ramp" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$additive" ).get( ), 1 );
    kv->SetInt( ATLAS_XORSTR( "$phong" ).get( ), 1 );
    kv->SetString( ATLAS_XORSTR( "$phongexponent" ).get( ), ATLAS_XORSTR( "8" ).get( ) );
    kv->SetString( ATLAS_XORSTR( "$phongboost" ).get( ), ATLAS_XORSTR( "1" ).get( ) );
    kv->SetString( ATLAS_XORSTR( "$phongfresnelranges" ).get( ), ATLAS_XORSTR( "[0 0.5 1]" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$selfillum" ).get( ), 1 );
    kv->SetInt( ATLAS_XORSTR( "$selfillumfresnel" ).get( ), 1 );
    kv->SetString( ATLAS_XORSTR( "$selfillumfresnelminmaxexp" ).get( ), ATLAS_XORSTR( "[0 1 0.2]" ).get( ) );
    kv->SetString( ATLAS_XORSTR( "$selfillumtint" ).get( ), ATLAS_XORSTR( "[0 0 0]" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$nodecal" ).get( ), 1 );
    return kv;
}

static KeyValues *build_wireframe( )
{
    KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "UnlitGeneric" ).get( ) ) };
    kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "vgui/white_additive" ).get( ) );
    kv->SetInt( ATLAS_XORSTR( "$wireframe" ).get( ), 1 );
    kv->SetInt( ATLAS_XORSTR( "$nodecal" ).get( ), 1 );
    return kv;
}

void chams::render_main( )
{
    if ( !this->m_enabled || !g_cl.m_local || !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    if ( !this->m_flat )
    {
        return;
    }

    this->paint_ramp( );
    this->collect( );
    this->advance_backtrack_fade( );

    const auto &backtrack{ g_cfg.m_chams.m_backtrack };
    const auto &backtrack_glow{ g_cfg.m_glow.m_backtrack };
    const bool  backtrack_active{ ( !backtrack.off( ) || !backtrack_glow.off( ) ) && this->m_backtrack_mix > 0.0f };

    const usize                   fake{ g_cfg.m_anti_hack.m_fake_chams };
    const config::glow_profile_t &fake_glow{ g_cfg.m_anti_hack.m_fake_glow };
    i32                           fake_bone_count{ };
    const bool                    fake_active{ ( fake != CHAM_OFF || !fake_glow.off( ) ) && g_tf2.m_input->CAM_IsThirdPerson( ) &&
                                               g_anti_aim.build_shown_bones( this->m_fake_bones, fake_bone_count ) };
    if ( !this->m_draw_count && !backtrack_active && !fake_active )
    {
        return;
    }

    this->animate_lava( );

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };

    const bool d3d{ render_context && g_model_render.active( ) };

    for ( bool &queued : this->m_records_queued )
    {
        queued = false;
    }

    bool records_left{ backtrack_active };
    bool fake_left{ fake_active && fake != CHAM_OFF };

    this->m_queued_occluded = false;

    if ( d3d )
    {
        this->queue_draws( );

        const bool d3d_records{ is_d3d_style( backtrack ) };

        if ( backtrack_active && ( d3d_records || !backtrack_glow.off( ) ) )
        {
            const bool queued{ this->queue_backtrack( d3d_records ) };
            records_left = !d3d_records || !queued;
        }

        const bool d3d_fake{ fake != CHAM_OFF && is_d3d_mode( fake ) };

        u32 instance{ };
        if ( fake_active && ( d3d_fake || !fake_glow.off( ) ) &&
             g_model_render.add_instance( g_cl.m_local, this->m_fake_bones, fake_bone_count, instance ) )
        {
            if ( d3d_fake )
            {
                g_model_render.add_layer( model_render::LAYER_VISIBLE_BASE, instance, g_cfg.m_anti_hack.m_fake_chams_color, shade_of( fake ),
                                          is_wireframe( fake ) );
                fake_left = false;
            }

            if ( !fake_glow.off( ) )
            {
                g_model_render.add_glow( instance, fake_glow, fake_glow.m_color );
            }
        }
    }

    const float old_blend{ g_tf2.m_render_view->GetBlend( ) };
    float       old_color[ 3 ]{ };
    g_tf2.m_render_view->GetColorModulation( old_color );

    this->m_driving = true;

    const bool occluded_pass{ this->m_xqz && render_context &&
                              ( this->m_has_occluded || this->m_queued_occluded ||
                                ( records_left && ( backtrack.m_invisible || backtrack.m_invisible_overlay ) ) ) };

    if ( occluded_pass )
    {
        render_context->ClearBuffers( false, false, true );

        render_context->SetStencilEnable( true );
        render_context->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_ALWAYS );
        render_context->SetStencilPassOperation( STENCILOPERATION_REPLACE );
        render_context->SetStencilFailOperation( STENCILOPERATION_KEEP );
        render_context->SetStencilZFailOperation( STENCILOPERATION_KEEP );
        render_context->SetStencilReferenceValue( 1 );
        render_context->SetStencilWriteMask( 0xFF );
        render_context->SetStencilTestMask( 0x00 );
    }

    this->run_sweep( false );
    if ( records_left )
    {
        this->sweep_backtrack( false );
    }

    if ( fake_left )
    {
        this->draw_fake( );
    }

    if ( d3d )
    {
        this->queue_engine_bases( );
        g_model_render.submit( render_context, occluded_pass );
    }

    if ( occluded_pass )
    {
        render_context->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
        render_context->SetStencilPassOperation( STENCILOPERATION_KEEP );
        render_context->SetStencilReferenceValue( 0 );
        render_context->SetStencilWriteMask( 0x00 );
        render_context->SetStencilTestMask( 0xFF );

        render_context->DepthRange( 0.0f, 0.2f );
        this->run_sweep( true );
        if ( records_left )
        {
            this->sweep_backtrack( true );
        }
        render_context->DepthRange( 0.0f, 1.0f );

        render_context->SetStencilEnable( false );
    }

    g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render, static_cast< IMaterial * >( nullptr ), OVERRIDE_NORMAL );

    this->m_driving    = false;
    this->m_draw_count = 0;

    render_context->Release( );

    g_tf2.m_render_view->SetBlend( old_blend );
    g_tf2.m_render_view->SetColorModulation( old_color );
}

void chams::collect( )
{
    this->m_draw_count   = 0;
    this->m_has_occluded = false;
    for ( u64 &word : this->m_drawn )
    {
        word = 0;
    }

    for ( player_style_t &player : this->m_player_styles )
    {
        player.m_checked = false;
    }

    bool active{ !g_cfg.m_chams.m_buildings.off( ) || !g_cfg.m_chams.m_projectiles.off( ) ||
                 !g_cfg.m_chams.m_pickups.off( ) || !g_cfg.m_chams.m_intel.off( ) ||
                 !g_cfg.m_chams.m_npcs.off( ) };

    for ( const auto &profile : g_cfg.m_esp.m_profiles )
    {
        active |= !profile.m_chams.off( );
    }

    if ( !active )
    {
        return;
    }

    const auto entities{ g_entity_cache.networked( ) };

    for ( const auto *cached : entities )
    {
        C_BaseEntity *entity{ cached->m_entity };
        if ( !cached->m_client_class || entity->IsDormant( ) )
        {
            continue;
        }

        config::cham_profile_t style{ };
        if ( !this->decide( entity, cached->m_class_id, style ) )
        {
            continue;
        }

        if ( !g_cl.is_on_screen( entity ) )
        {
            continue;
        }

        this->m_draws[ this->m_draw_count++ ] = draw_t{ cached->m_handle, style };
        this->m_has_occluded |= style.m_invisible != CHAM_OFF || style.m_invisible_overlay != CHAM_OFF;
    }
}

void chams::run_sweep( bool occluded )
{
    for ( usize at{ }; at < this->m_draw_count; ++at )
    {
        const draw_t &draw{ this->m_draws[ at ] };
        if ( draw.m_d3d || ( occluded && draw.m_engine_base ) )
        {
            continue;
        }

        const auto &style{ draw.m_style };
        usize       base{ occluded ? style.m_invisible : style.m_visible };
        const usize overlay{ draw.m_engine_base ? CHAM_OFF : occluded ? style.m_invisible_overlay
                                                                      : style.m_visible_overlay };
        if ( !occluded && base == CHAM_OFF )
        {
            base = CHAM_ORIGINAL;
        }

        if ( base == CHAM_OFF && overlay == CHAM_OFF )
        {
            continue;
        }

        C_BaseEntity *entity{ g_entity_cache.resolve( draw.m_handle ) };
        if ( !entity || !entity->ShouldDraw( ) )
        {
            continue;
        }

        this->draw_layer( entity, draw.m_handle, base, occluded ? style.m_occluded_color : style.m_visible_color );
        if ( overlay != CHAM_OFF && g_entity_cache.resolve( draw.m_handle ) == entity )
        {
            this->draw_layer( entity, draw.m_handle, overlay, occluded ? style.m_occluded_overlay_color : style.m_visible_overlay_color );
        }
    }
}

void chams::advance_backtrack_fade( )
{
    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const double previous{ this->m_backtrack_time };

    this->m_backtrack_time = now;

    const float target{ this->backtrack_applies( ) ? 1.0f : 0.0f };

    if ( previous <= 0.0 )
    {
        this->m_backtrack_mix = target;
        return;
    }

    constexpr float FADE_SECONDS{ 0.18f };

    const float delta{ math::smaller( static_cast< float >( now - previous ), 0.1f ) };
    const float step{ delta / FADE_SECONDS };

    this->m_backtrack_mix = math::clamp(
        this->m_backtrack_mix + ( target > 0.0f ? step : -step ), 0.0f, 1.0f );
}

bool chams::backtrack_applies( ) const
{
    C_TFWeaponBase *weapon{ g_cl.m_weapon };

    if ( !weapon )
    {
        return false;
    }

    if ( weapon->GetSlot( ) == 2 )
    {
        return true;
    }

    if ( weapon->GetWeaponID( ) == TF_WEAPON_FLAMETHROWER )
    {
        return weapon->m_iItemDefinitionIndex( ) != Pyro_m_ThePhlogistinator;
    }

    CTFWeaponInfo *info{ weapon->GetWeaponInfo( ) };

    if ( !info )
    {
        return false;
    }

    return info->GetWeaponData( weapon->m_iWeaponMode( ) ).m_iProjectile == TF_PROJECTILE_BULLET;
}

template< typename F >
void chams::for_each_backtrack_player( F &&visit )
{
    const i32 local_index{ g_cl.m_local->entindex( ) };
    const i32 local_team{ g_cl.m_local->m_iTeamNum( ) };

    for ( const auto *cached : g_entity_cache.players( ) )
    {
        const i32 index{ cached->m_index };
        if ( index < 1 || index >= MAX_TRACKED_PLAYERS || index == local_index || g_lagcomp.count( index ) <= 0 )
        {
            continue;
        }

        C_BaseEntity *entity{ cached->m_entity };
        if ( entity->IsDormant( ) )
        {
            continue;
        }

        auto *player{ entity->As< C_TFPlayer * >( ) };

        if ( !player->IsAlive( ) || player->InCond( TF_COND_STEALTHED ) ||
             player->m_iTeamNum( ) == local_team || !player->ShouldDraw( ) )
        {
            continue;
        }

        visit( player, index );
    }
}

template< typename F >
void chams::for_each_record( C_TFPlayer *player, i32 index, F &&visit )
{
    const config::chams_t &cfg{ g_cfg.m_chams };
    const i32              count{ g_lagcomp.count( index ) };
    const CStudioHdr      *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( player ) };

    placed_t placed[ 72 ]{ };
    usize    placed_count{ 1 };

    placed[ 0 ].m_origin = player->m_vecOrigin( );
    placed[ 0 ].m_speed  = player->m_vecVelocity( ).length( );

    for ( i32 age{ count - 1 }; age >= 0; --age )
    {
        const lag_record_t *entry{ g_lagcomp.record( index, age ) };
        if ( !entry || !studio || entry->m_header != studio->m_pStudioHdr || !g_lagcomp.reachable( index, *entry ) )
        {
            continue;
        }

        const float speed{ entry->m_velocity.length( ) };

        float spread{ 200.f };

        for ( usize slot{ }; slot < placed_count; ++slot )
        {
            spread = math::smaller( spread,
                                    speed + placed[ slot ].m_speed +
                                        placed[ slot ].m_origin.dist_to( entry->m_pose.m_origin ) );
        }

        if ( spread <= 1.0f )
        {
            continue;
        }

        if ( entry->m_bone_count <= 0 )
        {
            continue;
        }

        if ( !visit( *entry, ( spread / 200.f ) * this->m_backtrack_mix ) )
        {
            break;
        }

        if ( cfg.m_backtrack_records == config::BACKTRACK_OLDEST )
        {
            break;
        }

        if ( placed_count < 72 )
        {
            placed[ placed_count ].m_origin = entry->m_pose.m_origin;
            placed[ placed_count ].m_speed  = speed;
            ++placed_count;
        }
    }
}

void chams::sweep_backtrack( bool occluded )
{
    const config::chams_t &cfg{ g_cfg.m_chams };

    if ( cfg.m_backtrack.off( ) || !g_cl.m_local || this->m_backtrack_mix <= 0.0f )
    {
        return;
    }

    const usize base{ occluded ? cfg.m_backtrack.m_invisible : cfg.m_backtrack.m_visible };
    const usize overlay{ occluded ? cfg.m_backtrack.m_invisible_overlay
                                  : cfg.m_backtrack.m_visible_overlay };

    if ( base == CHAM_OFF && overlay == CHAM_OFF )
    {
        return;
    }

    this->for_each_backtrack_player( [ & ]( C_TFPlayer *player, i32 index )
    {
        if ( !this->m_records_queued[ index ] )
        {
            this->draw_records( player, index, base, overlay );
        }
    } );
}

void chams::draw_records( C_TFPlayer *player, i32 index, usize base, usize overlay )
{
    const bool saved_drawn{ this->drawn( index ) };
    const u32  handle{ static_cast< u32 >( player->GetRefEHandle( ).GetIndex( ) ) };

    const config::chams_t &cfg{ g_cfg.m_chams };

    this->for_each_record( player, index, [ & ]( const lag_record_t &entry, float fade )
    {
        if ( g_entity_cache.resolve( handle ) != player )
        {
            return false;
        }

        atl::memcpy( this->m_record_scratch, entry.m_bones,
                     sizeof( matrix3x4 ) * static_cast< usize >( entry.m_bone_count ) );

        this->m_record_bones = this->m_record_scratch;

        this->draw_layer( player, handle, base, faded( cfg.m_backtrack_color, fade ) );
        if ( overlay != CHAM_OFF && g_entity_cache.resolve( handle ) == player )
        {
            this->draw_layer( player, handle, overlay, faded( cfg.m_backtrack_overlay_color, fade ) );
        }

        return true;
    } );

    this->m_record_bones = nullptr;
    this->mark_drawn( index, saved_drawn && g_entity_cache.resolve( handle ) == player );
}

void chams::draw_fake( )
{
    C_TFPlayer *local{ g_cl.m_local };
    const i32   index{ local->entindex( ) };
    const bool  saved_drawn{ this->drawn( index ) };

    this->m_record_bones = this->m_fake_bones;
    this->draw_layer( local, static_cast< u32 >( local->GetRefEHandle( ).GetIndex( ) ), g_cfg.m_anti_hack.m_fake_chams,
                      g_cfg.m_anti_hack.m_fake_chams_color );
    this->m_record_bones = nullptr;

    // the fake pass marks us drawn, which would stop the engine drawing the real pose next frame
    this->mark_drawn( index, saved_drawn );
}

void chams::queue_draws( )
{
    this->m_has_occluded = false;

    for ( usize at{ }; at < this->m_draw_count; ++at )
    {
        draw_t                       &draw{ this->m_draws[ at ] };
        const config::cham_profile_t &style{ draw.m_style };
        C_BaseEntity                 *entity{ g_entity_cache.resolve( draw.m_handle ) };

        draw.m_d3d = style.m_visible != CHAM_OFF && is_d3d_style( style ) && entity && entity->ShouldDraw( );

        if ( draw.m_d3d )
        {
            u32        instance{ g_model_render.find_instance( entity->entindex( ) ) };
            const bool in_place{ instance != model_render::INVALID_INSTANCE };

            draw.m_d3d = in_place || g_model_render.add_instance( entity, nullptr, 0, instance );

            if ( draw.m_d3d )
            {
                this->queue_layers( instance, style, false );
                continue;
            }
        }

        draw.m_engine_base = is_engine_base_style( style ) && entity && entity->ShouldDraw( );

        if ( draw.m_engine_base )
        {
            this->m_queued_occluded |= this->m_xqz && ( style.m_invisible != CHAM_OFF || style.m_invisible_overlay != CHAM_OFF );
            continue;
        }

        this->m_has_occluded |= style.m_invisible != CHAM_OFF || style.m_invisible_overlay != CHAM_OFF;
    }
}

void chams::queue_engine_bases( )
{
    for ( usize at{ }; at < this->m_draw_count; ++at )
    {
        const draw_t &draw{ this->m_draws[ at ] };
        if ( !draw.m_engine_base || !this->drawn( static_cast< i32 >( draw.m_handle & ENT_ENTRY_MASK ) ) )
        {
            continue;
        }

        C_BaseEntity *entity{ g_entity_cache.resolve( draw.m_handle ) };

        u32 instance{ };
        if ( !entity || !g_model_render.add_instance( entity, nullptr, 0, instance ) )
        {
            continue;
        }

        config::cham_profile_t layers{ draw.m_style };
        layers.m_visible = CHAM_OFF;

        this->queue_layers( instance, layers, true );
    }
}

bool chams::queue_backtrack( bool layers )
{
    const config::chams_t        &cfg{ g_cfg.m_chams };
    const config::glow_profile_t &glow{ g_cfg.m_glow.m_backtrack };
    bool                          all_queued{ true };

    this->for_each_backtrack_player( [ & ]( C_TFPlayer *player, i32 index )
    {
        bool queued{ true };

        this->for_each_record( player, index, [ & ]( const lag_record_t &entry, float fade )
        {
            u32 instance{ };
            queued = g_model_render.add_instance( player, entry.m_bones, entry.m_bone_count, instance );

            if ( !queued )
            {
                return false;
            }

            if ( layers )
            {
                config::cham_profile_t record{ cfg.m_backtrack };
                record.m_visible_color          = faded( cfg.m_backtrack_color, fade );
                record.m_occluded_color         = record.m_visible_color;
                record.m_visible_overlay_color  = faded( cfg.m_backtrack_overlay_color, fade );
                record.m_occluded_overlay_color = record.m_visible_overlay_color;

                this->queue_layers( instance, record, true );
            }

            if ( !glow.off( ) )
            {
                g_model_render.add_glow( instance, glow, faded( glow.m_color, fade ) );
            }

            return true;
        } );

        this->m_records_queued[ index ] = layers && queued;
        all_queued &= queued;
    } );

    return all_queued;
}

void chams::queue_layers( u32 instance, const config::cham_profile_t &style, bool visible )
{
    if ( visible && style.m_visible != CHAM_OFF )
    {
        g_model_render.add_layer( model_render::LAYER_VISIBLE_BASE, instance, style.m_visible_color, shade_of( style.m_visible ),
                                  is_wireframe( style.m_visible ) );
    }

    if ( visible && style.m_visible_overlay != CHAM_OFF )
    {
        g_model_render.add_layer( model_render::LAYER_VISIBLE_OVERLAY, instance, style.m_visible_overlay_color,
                                  shade_of( style.m_visible_overlay ), is_wireframe( style.m_visible_overlay ) );
    }

    if ( !this->m_xqz )
    {
        return;
    }

    if ( style.m_invisible != CHAM_OFF )
    {
        g_model_render.add_layer( model_render::LAYER_OCCLUDED_BASE, instance, style.m_occluded_color, shade_of( style.m_invisible ),
                                  is_wireframe( style.m_invisible ) );
    }

    if ( style.m_invisible_overlay != CHAM_OFF )
    {
        g_model_render.add_layer( model_render::LAYER_OCCLUDED_OVERLAY, instance, style.m_occluded_overlay_color,
                                  shade_of( style.m_invisible_overlay ), is_wireframe( style.m_invisible_overlay ) );
    }

    this->m_queued_occluded |= style.m_invisible != CHAM_OFF || style.m_invisible_overlay != CHAM_OFF;
}

void chams::draw_layer( C_BaseEntity *entity, u32 handle, usize mode, const color_t &color )
{
    if ( mode == CHAM_OFF )
    {
        return;
    }

    if ( mode == CHAM_TEXTURED || mode == CHAM_ORIGINAL )
    {
        this->setup_pass( nullptr, mode == CHAM_TEXTURED ? color : color_t{ 255, 255, 255 } );
        this->draw_entity( entity, handle );
        return;
    }

    IMaterial *material{ this->resolve_material( mode ) };
    if ( !material )
    {
        return;
    }

    this->setup_pass( material, color );
    this->draw_entity( entity, handle );
}

void chams::draw_entity( C_BaseEntity *entity, u32 handle )
{
    if ( entity->DrawModel( STUDIO_RENDER ) != 0 && g_entity_cache.resolve( handle ) == entity )
    {
        this->mark_drawn( static_cast< i32 >( handle & ENT_ENTRY_MASK ), true );
    }
}

bool chams::on_draw_model( const DrawModelResults_t *results, const DrawModelInfo_t *info, const matrix3x4 *bone_to_world, i32 &flags )
{
    if ( !this->m_enabled )
    {
        return true;
    }

    // our own driven pass already carries the override, so let it through
    if ( this->m_driving )
    {
        return true;
    }

    if ( ( flags & ( STUDIORENDER_SHADOWDEPTHTEXTURE | STUDIORENDER_SSAODEPTHTEXTURE ) ) || !results )
    {
        return true;
    }

    if ( !info || !info->m_pClientEntity )
    {
        return true;
    }

    auto           *renderable{ static_cast< IClientRenderable * >( info->m_pClientEntity ) };
    IClientUnknown *unknown{ renderable->GetIClientUnknown( ) };
    if ( !unknown )
    {
        return true;
    }

    C_BaseEntity *entity{ unknown->GetBaseEntity( ) };
    if ( !entity )
    {
        return true;
    }

    if ( this->begin_local_model( entity, *info, bone_to_world, flags ) )
    {
        return true;
    }

    const i32 index{ entity->entindex( ) };

    if ( ( flags & STUDIORENDER_DRAW_GROUP_MASK ) == STUDIORENDER_DRAW_TRANSLUCENT_ONLY )
    {
        if ( index > 0 && index < MAX_EDICTS && g_model_render.find_instance( index ) != model_render::INVALID_INSTANCE )
        {
            return false;
        }
    } else if ( this->route_in_place( entity, *info, bone_to_world, flags ) )
    {
        return true;
    }

    return !this->drawn( index );
}

bool chams::route_in_place( C_BaseEntity *entity, const DrawModelInfo_t &info, const matrix3x4 *bone_to_world, i32 &flags )
{
    if ( !g_model_render.active( ) )
    {
        return false;
    }

    const auto *cached{ g_entity_cache.find( entity ) };
    if ( !cached )
    {
        return false;
    }

    config::cham_profile_t style{ };
    if ( !this->decide( entity, cached->m_class_id, style ) || style.m_visible == CHAM_OFF || !is_d3d_style( style ) )
    {
        return false;
    }

    u32 instance{ };
    if ( !g_model_render.take_over( entity, info, bone_to_world, model_render::PLACE_WORLD, false, flags, instance ) )
    {
        return false;
    }

    g_model_render.add_in_place( instance, style.m_visible_color, shade_of( style.m_visible ), is_wireframe( style.m_visible ) );

    if ( style.m_visible_overlay != CHAM_OFF )
    {
        g_model_render.add_in_place( instance, style.m_visible_overlay_color, shade_of( style.m_visible_overlay ),
                                     is_wireframe( style.m_visible_overlay ) );
    }

    return true;
}

void chams::on_level_init( )
{
    this->destroy_materials( );
    this->create_materials( );
}

void chams::on_level_shutdown( )
{
    this->destroy_materials( );
    this->m_draw_count = 0;

    for ( u64 &word : this->m_drawn )
    {
        word = 0;
    }
}

void chams::on_entity_deleted( u32 handle )
{
    this->mark_drawn( static_cast< i32 >( handle & ENT_ENTRY_MASK ), false );
}

void chams::on_device_lost( )
{
    this->m_ramp_dirty = true;
}

bool chams::drawn( i32 index ) const
{
    if ( index <= 0 || index >= MAX_EDICTS )
    {
        return false;
    }

    return ( this->m_drawn[ index / 64 ] & ( 1ull << ( index % 64 ) ) ) != 0;
}

void chams::mark_drawn( i32 index, bool value )
{
    if ( index <= 0 || index >= MAX_EDICTS )
    {
        return;
    }

    if ( value )
    {
        this->m_drawn[ index / 64 ] |= ( 1ull << ( index % 64 ) );
    } else
    {
        this->m_drawn[ index / 64 ] &= ~( 1ull << ( index % 64 ) );
    }
}

usize chams::local_model_kind( C_BaseEntity *entity )
{
    C_TFPlayer *local{ g_cl.m_local };

    if ( !local || !entity )
    {
        return config::LOCAL_MODEL_COUNT;
    }

    ClientClass *client_class{ entity->GetClientClass( ) };

    if ( !client_class )
    {
        return config::LOCAL_MODEL_COUNT;
    }

    if ( client_class->GetClassID( ) == ETFClassID::CTFViewModel )
    {
        return config::LOCAL_MODEL_VIEWMODEL;
    }

    if ( client_class->GetClassID( ) == ETFClassID::CBaseAnimating )
    {
        C_BaseEntity *owner{ entity->m_hOwnerEntity( ).Get( ) };
        if ( owner && owner == local->m_hActiveWeapon( ).Get( ) )
        {
            return config::LOCAL_MODEL_WEAPON;
        }
    }

    return config::LOCAL_MODEL_COUNT;
}

bool chams::begin_local_model( C_BaseEntity *entity, const DrawModelInfo_t &info, const matrix3x4 *bone_to_world, i32 &flags )
{
    this->m_local_override = false;
    this->m_local_overlay  = CHAM_OFF;

    C_TFPlayer *local{ g_cl.m_local };

    if ( !this->m_enabled || !local || !entity )
    {
        return false;
    }

    const usize kind{ local_model_kind( entity ) };

    usize base{ CHAM_OFF };

    if ( kind == config::LOCAL_MODEL_VIEWMODEL )
    {
        base                        = g_cfg.m_chams.m_viewmodel;
        this->m_local_overlay       = g_cfg.m_chams.m_viewmodel_overlay;
        this->m_local_color         = g_cfg.m_chams.m_viewmodel_color;
        this->m_local_overlay_color = g_cfg.m_chams.m_viewmodel_overlay_color;
    } else if ( kind == config::LOCAL_MODEL_WEAPON )
    {
        base                        = g_cfg.m_chams.m_weapon;
        this->m_local_overlay       = g_cfg.m_chams.m_weapon_overlay;
        this->m_local_color         = g_cfg.m_chams.m_weapon_color;
        this->m_local_overlay_color = g_cfg.m_chams.m_weapon_overlay_color;
    } else
    {
        return false;
    }

    if ( base == CHAM_OFF && this->m_local_overlay == CHAM_OFF )
    {
        return false;
    }

    if ( route_layers( entity, info, bone_to_world, model_render::PLACE_VIEWMODEL, base, this->m_local_color, this->m_local_overlay,
                       this->m_local_overlay_color, flags ) )
    {
        this->m_local_overlay = CHAM_OFF;

        if ( base != CHAM_TEXTURED )
        {
            return true;
        }
    }

    // we are inside the engine's own draw loop rather than our sweep, so whatever modulation
    // the engine had set has to come back exactly, not be reset to white
    g_tf2.m_render_view->GetColorModulation( this->m_local_modulation );
    this->m_local_blend = g_tf2.m_render_view->GetBlend( );

    this->m_local_override = true;

    if ( base == CHAM_OFF )
    {
        return false; // nothing to replace the base pass with, so the engine draws its own
    }

    IMaterial *material{ base == CHAM_TEXTURED ? nullptr : this->resolve_material( base ) };

    if ( base != CHAM_TEXTURED && !material )
    {
        return false;
    }

    this->setup_local_pass( material, this->m_local_color );

    return true;
}

bool chams::route_layers( C_BaseEntity             *entity,
                          const DrawModelInfo_t    &info,
                          const matrix3x4          *bone_to_world,
                          model_render::placement_t placement,
                          usize                     base,
                          const color_t            &base_color,
                          usize                     overlay,
                          const color_t            &overlay_color,
                          i32                      &flags )
{
    const bool engine_base{ base == CHAM_OFF || !is_d3d_mode( base ) };

    u32 instance{ };
    if ( !is_d3d_mode( overlay ) || ( engine_base && overlay == CHAM_OFF ) || !g_model_render.active( ) ||
         !g_model_render.take_over( entity, info, bone_to_world, placement, engine_base, flags, instance ) )
    {
        return false;
    }

    if ( !engine_base )
    {
        g_model_render.add_in_place( instance, base_color, shade_of( base ), is_wireframe( base ) );
    }

    if ( overlay != CHAM_OFF )
    {
        g_model_render.add_in_place( instance, overlay_color, shade_of( overlay ), is_wireframe( overlay ) );
    }

    return true;
}

void chams::end_local_model(
    CStudioRender      *thisptr,
    DrawModelResults_t *results,
    DrawModelInfo_t    *info,
    matrix3x4          *bone_to_world,
    float              *flex_weights,
    float              *flex_delayed_weights,
    const vec3         *model_origin,
    i32                 flags )
{
    if ( !this->m_local_override )
    {
        return;
    }

    this->m_local_override = false;

    const usize overlay{ this->m_local_overlay };
    this->m_local_overlay = CHAM_OFF;

    if ( overlay != CHAM_OFF && overlay < CHAM_COUNT )
    {
        IMaterial *material{ overlay == CHAM_TEXTURED ? nullptr
                                                      : this->resolve_material( overlay ) };

        if ( overlay == CHAM_TEXTURED || material )
        {
            this->setup_local_pass( material, this->m_local_overlay_color );

            g_hooks.m_draw_model.thiscall< void >( thisptr, results, info, bone_to_world,
                                                   flex_weights, flex_delayed_weights,
                                                   model_origin, flags );
        }
    }

    g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render,
                                                         static_cast< IMaterial * >( nullptr ),
                                                         OVERRIDE_NORMAL );
    g_tf2.m_studio_render->SetColorModulation( this->m_local_modulation );
    g_tf2.m_studio_render->SetAlphaModulation( this->m_local_blend );

    g_tf2.m_render_view->SetColorModulation( this->m_local_modulation );
    g_tf2.m_render_view->SetBlend( this->m_local_blend );
}

bool chams::player_style( C_TFPlayer *player, config::cham_profile_t &style ) const
{
    if ( !player->IsAlive( ) || player->InCond( TF_COND_STEALTHED ) )
    {
        return false;
    }

    C_TFPlayer          *local{ g_cl.m_local };
    bool                 teammate{ local->m_iTeamNum( ) == player->m_iTeamNum( ) };
    config::esp_target_t target{ config::esp_target_t::enemy };
    if ( local == player )
    {
        target = config::esp_target_t::local;
    } else
    {
        bool atlas{ g_cl.is_atlas_user( player ) };

        if ( g_cl.is_friend( player ) )
        {
            target = config::esp_target_t::friends;
        } else if ( atlas )
        {
            target = config::esp_target_t::atlas;
        } else
        {
            if ( teammate )
            {
                if ( !g_cl.should_draw_teammate( player ) )
                {
                    return false;
                }

                target = config::esp_target_t::team;
            }
        }
    }

    const auto &profile{ g_cfg.m_esp.m_profiles[ static_cast< usize >( target ) ].m_chams };
    if ( profile.off( ) )
    {
        return false;
    }

    if ( player->InCond( TF_COND_INVULNERABLE ) )
    {
        style.m_visible   = CHAM_ORIGINAL;
        style.m_invisible = profile.m_invisible || profile.m_invisible_overlay ? CHAM_ORIGINAL : CHAM_OFF;
    } else
    {
        style = profile;
    }
    return true;
}

bool chams::decide( C_BaseEntity *entity, ETFClassID class_id, config::cham_profile_t &style )
{
    C_TFPlayer *local{ g_cl.m_local };
    if ( !local )
    {
        return false;
    }

    // a pickup with its profile off still falls through, so a carried intel keeps its
    // carrier's chams like any other attachment
    const usize pickup{ g_cl.pickup_kind( entity, class_id ) };
    if ( pickup != config::PICKUP_COUNT )
    {
        const config::cham_profile_t &profile{ pickup == config::PICKUP_INTEL ? g_cfg.m_chams.m_intel
                                                                              : g_cfg.m_chams.m_pickups };
        if ( !profile.off( ) )
        {
            style = profile;
            return true;
        }
    }

    switch ( class_id )
    {
        case ETFClassID::CTFPlayer:
        {
            auto       *player{ entity->As< C_TFPlayer * >( ) };
            const auto *identity{ g_entity_cache.find( entity ) };
            if ( !identity || identity->m_index < 1 || identity->m_index >= MAX_TRACKED_PLAYERS )
            {
                return this->player_style( player, style );
            }

            player_style_t &saved{ this->m_player_styles[ identity->m_index ] };
            if ( !saved.m_checked || saved.m_handle != identity->m_handle )
            {
                saved           = player_style_t{ };
                saved.m_handle  = identity->m_handle;
                saved.m_allowed = this->player_style( player, saved.m_style );
                saved.m_checked = true;
            }
            style = saved.m_style;
            return saved.m_allowed;
        }

        case ETFClassID::CObjectSentrygun:
        case ETFClassID::CObjectDispenser:
        case ETFClassID::CObjectTeleporter:
        {
            if ( g_cfg.m_chams.m_buildings.off( ) )
            {
                return false;
            }

            auto *object{ entity->As< C_BaseObject * >( ) };
            if ( object->m_iHealth( ) <= 0 || object->m_bCarried( ) )
            {
                return false;
            }

            // buildings have no profile of their own, so they stay enemy only, like their box
            if ( entity->m_iTeamNum( ) == local->m_iTeamNum( ) )
            {
                return false;
            }

            style = g_cfg.m_chams.m_buildings;
            break;
        }

        case ETFClassID::CTFProjectile_Arrow:
        case ETFClassID::CTFProjectile_HealingBolt:
        case ETFClassID::CTFProjectile_Rocket:
        case ETFClassID::CTFProjectile_SentryRocket:
        case ETFClassID::CTFGrenadePipebombProjectile:
        case ETFClassID::CTFProjectile_Cleaver:
        case ETFClassID::CTFProjectile_Flare:
        {
            if ( g_cfg.m_chams.m_projectiles.off( ) || !g_cfg.m_esp.shows_projectile( entity->m_iTeamNum( ) == local->m_iTeamNum( ) ) )
            {
                return false;
            }

            style = g_cfg.m_chams.m_projectiles;
            break;
        }

        case ETFClassID::CEyeballBoss:
        case ETFClassID::CHeadlessHatman:
        case ETFClassID::CMerasmus:
        case ETFClassID::CZombie:
        {
            if ( g_cfg.m_chams.m_npcs.off( ) )
            {
                return false;
            }

            if ( entity->m_iTeamNum( ) == local->m_iTeamNum( ) )
            {
                return false;
            }

            style = g_cfg.m_chams.m_npcs;
            break;
        }

        default:
        {
            for ( i32 depth{ 0 }; depth < 8; ++depth )
            {
                C_BaseEntity *owner{ entity->m_hMoveParent( ).Get( ) };

                if ( !owner || owner == entity )
                {
                    return false;
                }

                const auto *owner_entry{ g_entity_cache.find( owner ) };
                if ( owner_entry && owner_entry->m_class_id == ETFClassID::CTFPlayer )
                {
                    return this->decide( owner, ETFClassID::CTFPlayer, style );
                }

                entity = owner;
            }

            return false;
        }
    }

    return true;
}

IMaterial *chams::resolve_material( usize mode ) const
{
    switch ( mode )
    {
        case CHAM_FLAT: return this->m_flat;
        case CHAM_SHADED: return this->m_shaded;
        case CHAM_FRESNEL: return this->m_fresnel;
        case CHAM_WIREFRAME: return this->m_wireframe;
        case CHAM_LAVA: return this->m_lava;
        case CHAM_PEARL: return this->m_pearl;
        case CHAM_BUBBLE: return this->m_bubble;
        case CHAM_LSD: return this->m_lsd;
        case CHAM_NETWORK: return this->m_wireframe;
        case CHAM_SOAP:
        case CHAM_AURORA:
        case CHAM_AQUARIUM:
        case CHAM_SKY: return this->m_fresnel;
        default: return nullptr;
    }
}

IMaterial *chams::material_for( usize mode )
{
    // the menu preview runs outside a level, where nothing has created the set yet
    if ( !this->m_flat )
    {
        this->create_materials( );
    }

    this->animate_lava( );
    this->paint_ramp( );

    return this->resolve_material( mode );
}

static void *material_shader( IMaterial *material )
{
    void *realtime{ *reinterpret_cast< void ** >( reinterpret_cast< u8 * >( material ) + 8 ) };
    return *reinterpret_cast< void ** >( static_cast< u8 * >( realtime ) + 24 );
}

void chams::animate_lava( )
{
    if ( !this->m_lava )
    {
        return;
    }

    void *shader{ material_shader( this->m_lava ) };
    if ( !shader )
    {
        return;
    }

    if ( shader != this->m_lava_shader )
    {
        this->resolve_lava_vars( );
    }

    if ( !this->m_lava_frame || !this->m_lava_blend || !this->m_lava_scale )
    {
        return;
    }

    const float now{ g_tf2.m_globals->realtime };

    // effects/tiledfire/firelayeredslowtiled512.vtf
    this->m_lava_frame->SetFloatValue(
        static_cast< float >( static_cast< i32 >( now * 12.0f ) % 121 ) );
    this->m_lava_blend->SetFloatValue( 0.78f + 0.17f * math::sin( now * ( PI * 2.0f / 6.1f ) ) +
                                       0.05f * math::sin( now * ( PI * 2.0f / 2.3f ) ) );
    this->m_lava_scale->SetFloatValue( 0.75f + 0.22f * math::sin( now * ( PI * 2.0f / 12.7f ) ) );
}

void chams::resolve_lava_vars( )
{
    this->m_lava_frame  = this->m_lava->FindVar( ATLAS_XORSTR( "$detailframe" ).get( ), nullptr, false );
    this->m_lava_blend  = this->m_lava->FindVar( ATLAS_XORSTR( "$detailblendfactor" ).get( ), nullptr, false );
    this->m_lava_scale  = this->m_lava->FindVar( ATLAS_XORSTR( "$detailscale" ).get( ), nullptr, false );
    this->m_lava_shader = material_shader( this->m_lava );
}

void chams::paint_ramp( )
{
    if ( !this->m_ramp_dirty )
    {
        return;
    }

    static constexpr color_t RAMP[ 4 ]{
        color_t{ 0, 0, 0 },
        color_t{ 0, 230, 255 },
        color_t{ 255, 230, 0 },
        color_t{ 255, 60, 230 },
    };

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };

    render_context->PushRenderTargetAndViewport( );
    render_context->SetRenderTarget( this->m_ramp );

    for ( i32 texel{ }; texel < 4; ++texel )
    {
        render_context->Viewport( texel, 0, 1, 1 );
        render_context->ClearColor4ub( RAMP[ texel ].r, RAMP[ texel ].g, RAMP[ texel ].b, 255 );
        render_context->ClearBuffers( true, false, false );
    }

    render_context->PopRenderTargetAndViewport( );
    render_context->Release( );

    this->m_ramp_dirty = false;
}

void chams::create_materials( )
{
    if ( !this->m_ramp )
    {
        this->m_ramp = g_tf2.m_material_system->CreateNamedRenderTargetTextureEx(
            ATLAS_XORSTR( "atlas_cham_ramp" ).get( ), 4, 1, RT_SIZE_LITERAL, IMAGE_FORMAT_RGBA8888,
            MATERIAL_RT_DEPTH_SEPARATE, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT, 0 );
        if ( !this->m_ramp )
        {
            return;
        }
        this->m_ramp->IncrementReferenceCount( );
    }
    this->m_ramp_dirty = true;

    if ( !this->m_base_texture )
    {
        this->m_base_texture = g_tf2.m_material_system->FindMaterial( ATLAS_XORSTR( "vgui/white_additive" ).get( ), ATLAS_XORSTR( "Other textures" ).get( ) );
        if ( this->m_base_texture )
        {
            this->m_base_texture->IncrementReferenceCount( );
        }
    }

    this->m_flat      = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_cham_flat" ).get( ), build_flat( ) );
    this->m_shaded    = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_cham_shaded" ).get( ), build_shaded( ) );
    this->m_fresnel   = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_cham_fresnel" ).get( ), build_fresnel( ) );
    this->m_wireframe = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_cham_wireframe" ).get( ), build_wireframe( ) );
    this->m_lava      = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_cham_lava" ).get( ), build_lava( ) );
    this->m_pearl     = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_cham_pearl" ).get( ), build_macintosh_plus( ) );
    this->m_bubble    = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_cham_bubble" ).get( ), build_bubble( ) );
    this->m_lsd       = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_cham_lsd" ).get( ), build_lsd( ) );

    if ( this->m_lava )
    {
        this->resolve_lava_vars( );
    }

    if ( !this->m_flat || !this->m_shaded || !this->m_fresnel || !this->m_wireframe ||
         !this->m_lava || !this->m_pearl || !this->m_bubble || !this->m_lsd )
    {
        this->destroy_materials( );
    }
}

void chams::destroy_materials( )
{
    IMaterial *materials[]{
        this->m_flat, this->m_shaded, this->m_fresnel, this->m_wireframe, this->m_lava,
        this->m_pearl, this->m_bubble, this->m_lsd
    };

    this->m_lava_frame  = nullptr;
    this->m_lava_blend  = nullptr;
    this->m_lava_scale  = nullptr;
    this->m_lava_shader = nullptr;

    this->m_flat      = nullptr;
    this->m_shaded    = nullptr;
    this->m_fresnel   = nullptr;
    this->m_wireframe = nullptr;
    this->m_lava      = nullptr;
    this->m_pearl     = nullptr;
    this->m_bubble    = nullptr;
    this->m_lsd       = nullptr;

    for ( IMaterial *material : materials )
    {
        if ( material )
        {
            material->DecrementReferenceCount( );
            material->DeleteIfUnreferenced( );
        }
    }
}

void chams::setup_local_pass( IMaterial *material, const color_t &color ) const
{
    const float modulation[ 3 ]{
        static_cast< float >( color.r ) / 255.0f,
        static_cast< float >( color.g ) / 255.0f,
        static_cast< float >( color.b ) / 255.0f,
    };

    g_tf2.m_studio_render->SetColorModulation( modulation );
    g_tf2.m_studio_render->SetAlphaModulation( static_cast< float >( color.a ) / 255.0f );
    g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render, material, OVERRIDE_NORMAL );
}

void chams::setup_pass( IMaterial *material, const color_t &color ) const
{
    const float modulation[ 3 ]{
        static_cast< float >( color.r ) / 255.0f,
        static_cast< float >( color.g ) / 255.0f,
        static_cast< float >( color.b ) / 255.0f,
    };

    g_tf2.m_render_view->SetColorModulation( modulation );
    g_tf2.m_render_view->SetBlend( static_cast< float >( color.a ) / 255.0f );
    g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render, material, OVERRIDE_NORMAL );
}
