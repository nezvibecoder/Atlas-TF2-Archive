#include "glow.hpp"
#include "chams.hpp"
#include "entity_cache.hpp"
#include "model_render.hpp"

ATLAS_NO_DESTROY glow g_glow{ };

// backpointer at +8u & shader at +24u; should fix breaking glow materials / black chams
// IF my IDA is correct of course.
static void *material_shader( IMaterial *material )
{
    void *realtime{ *reinterpret_cast< void ** >( reinterpret_cast< u8 * >( material ) + 8 ) };
    return *reinterpret_cast< void ** >( static_cast< u8 * >( realtime ) + 24 );
}

void glow::render_main( )
{
    if ( !this->m_enabled || !g_cl.m_local || !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    // a mid-map inject never saw level init, so build the resources on first use
    if ( !this->m_halo )
    {
        this->create_resources( );

        if ( !this->m_halo )
        {
            return;
        }
    }

    this->collect( );

    if ( g_model_render.active( ) )
    {
        this->queue_d3d( );
        return;
    }

    usize glow_entities{ };
    for ( const pass_t &pass : this->m_passes )
    {
        glow_entities += pass.m_entities.size( );
    }

    if ( !glow_entities )
    {
        return;
    }

    const i32 width{ g_cl.m_screen_width };
    const i32 height{ g_cl.m_screen_height };
    if ( width < 1 || height < 1 )
    {
        return;
    }

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };

    const float old_blend{ g_tf2.m_render_view->GetBlend( ) };
    float       old_color[ 3 ]{ };
    g_tf2.m_render_view->GetColorModulation( old_color );

    this->m_driving = true;
    g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render, this->m_glow_color, OVERRIDE_NORMAL );

    {
        this->mask_pass( render_context );
    }

    for ( const pass_t &pass : this->m_passes )
    {
        if ( !pass.m_entities.empty( ) )
        {
            this->color_pass( render_context, pass, width, height );
        }
    }
    g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render, static_cast< IMaterial * >( nullptr ), OVERRIDE_NORMAL );
    this->m_driving = false;

    render_context->Release( );

    g_tf2.m_render_view->SetBlend( old_blend );
    g_tf2.m_render_view->SetColorModulation( old_color );
}

void glow::queue_d3d( )
{
    for ( const pass_t &pass : this->m_passes )
    {
        for ( C_BaseEntity *entity : pass.m_entities )
        {
            u32 instance{ g_model_render.find_instance( entity->entindex( ) ) };
            if ( instance == model_render::INVALID_INSTANCE && !g_model_render.add_instance( entity, nullptr, 0, instance ) )
            {
                continue;
            }

            g_model_render.add_glow( instance, *pass.m_profile, pass.m_color );
        }
    }

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };
    g_model_render.submit_glow( render_context );
    render_context->Release( );
}

bool glow::begin_local_model( C_BaseEntity *entity, const DrawModelInfo_t &info, const matrix3x4 *bone_to_world, i32 &flags )
{
    const usize kind{ chams::local_model_kind( entity ) };

    if ( !this->m_enabled || kind == config::LOCAL_MODEL_COUNT || !g_model_render.active( ) )
    {
        return false;
    }

    const config::glow_profile_t &profile{ kind == config::LOCAL_MODEL_VIEWMODEL ? g_cfg.m_glow.m_viewmodel : g_cfg.m_glow.m_weapon };

    u32 instance{ };
    if ( profile.off( ) || !g_model_render.take_over( entity, info, bone_to_world, model_render::PLACE_VIEWMODEL, true, flags, instance ) )
    {
        return false;
    }

    g_model_render.add_glow( instance, profile, profile.m_color );
    return true;
}

void glow::render_local_model( C_BaseEntity                 *entity,
                               const config::glow_profile_t &profile,
                               CStudioRender                *thisptr,
                               DrawModelResults_t           *results,
                               DrawModelInfo_t              *info,
                               matrix3x4                    *bone_to_world,
                               float                        *flex_weights,
                               float                        *flex_delayed_weights,
                               const vec3                   *model_origin,
                               i32                           flags )
{
    if ( !this->m_enabled || !entity || profile.off( ) )
    {
        return;
    }

    if ( !this->m_halo )
    {
        this->create_resources( );

        if ( !this->m_halo )
        {
            return;
        }
    }

    const i32 width{ g_cl.m_screen_width };
    const i32 height{ g_cl.m_screen_height };

    if ( width < 1 || height < 1 )
    {
        return;
    }

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };

    const float old_blend{ g_tf2.m_render_view->GetBlend( ) };
    float       old_color[ 3 ]{ };
    g_tf2.m_render_view->GetColorModulation( old_color );

    this->m_local_draw = local_draw_t{ thisptr, results, info, bone_to_world,
                                       flex_weights, flex_delayed_weights, model_origin, flags };

    this->m_local_pass.m_profile = &profile;
    this->m_local_pass.m_color   = profile.m_color;
    this->m_local_pass.m_local   = true;

    // m_driving keeps the redraws below from coming back through our own DrawModel hook
    this->m_driving = true;
    g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render, this->m_glow_color, OVERRIDE_NORMAL );

    this->begin_mask( render_context );
    this->redraw_local( );
    render_context->SetStencilEnable( false );

    this->color_pass( render_context, this->m_local_pass, width, height );

    g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render, static_cast< IMaterial * >( nullptr ), OVERRIDE_NORMAL );
    this->m_driving = false;

    render_context->Release( );

    g_tf2.m_render_view->SetBlend( old_blend );
    g_tf2.m_render_view->SetColorModulation( old_color );
}

void glow::render_preview( IMatRenderContext            *render_context,
                           const config::glow_profile_t &profile,
                           float                         fade,
                           C_BaseEntity                 *entity,
                           C_BaseEntity                 *weapon,
                           i32                           width,
                           i32                           height )
{
    if ( !this->m_enabled || profile.off( ) )
    {
        return;
    }

    if ( !this->m_halo )
    {
        this->create_resources( );

        if ( !this->m_halo )
        {
            return;
        }
    }

    pass_t &pass{ this->m_preview_pass };

    pass.m_profile = &profile;
    pass.m_color   = profile.m_color;
    pass.m_color.a = static_cast< u8 >( static_cast< float >( profile.m_color.a ) * fade );
    pass.m_preview = true;
    pass.m_entities.clear( );
    pass.m_entities.push_back( entity );

    if ( weapon )
    {
        pass.m_entities.push_back( weapon );
    }

    this->m_driving = true;
    g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render, this->m_glow_color, OVERRIDE_NORMAL );

    this->begin_mask( render_context );

    for ( C_BaseEntity *drawn : pass.m_entities )
    {
        drawn->DrawModel( STUDIO_RENDER | STUDIO_NOSHADOWS );
    }

    render_context->SetStencilEnable( false );

    this->color_pass( render_context, pass, width, height );

    g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render, static_cast< IMaterial * >( nullptr ), OVERRIDE_NORMAL );
    this->m_driving = false;
}

void glow::begin_mask( IMatRenderContext *render_context ) const
{
    // chams may have left its own marks in the stencil this frame, so start clean
    render_context->ClearBuffers( false, false, true );

    render_context->SetStencilEnable( true );
    render_context->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_ALWAYS );
    render_context->SetStencilPassOperation( STENCILOPERATION_REPLACE );
    render_context->SetStencilFailOperation( STENCILOPERATION_KEEP );
    render_context->SetStencilZFailOperation( STENCILOPERATION_REPLACE );
    render_context->SetStencilReferenceValue( 1 );
    render_context->SetStencilWriteMask( 0xFF );
    render_context->SetStencilTestMask( 0x00 );

    // blend 0 writes the stencil without the models showing up on screen
    constexpr float white[ 3 ]{ 1.0f, 1.0f, 1.0f };
    this->set_pass_modulation( white, 0.0f );
}

void glow::mask_pass( IMatRenderContext *render_context )
{
    this->begin_mask( render_context );
    this->draw_entities( );
    render_context->SetStencilEnable( false );
}

void glow::color_pass( IMatRenderContext *render_context, const pass_t &pass, i32 width, i32 height )
{
    render_context->PushRenderTargetAndViewport( );
    render_context->SetRenderTarget( this->m_buffer1 );
    render_context->Viewport( 0, 0, width, height );
    render_context->ClearColor4ub( 0, 0, 0, 0 );
    // the shared depth still holds the scene, which means nothing to the preview's own view
    render_context->ClearBuffers( true, pass.m_preview, false );

    const float modulation[ 3 ]{
        static_cast< float >( pass.m_color.r ) / 255.0f,
        static_cast< float >( pass.m_color.g ) / 255.0f,
        static_cast< float >( pass.m_color.b ) / 255.0f,
    };

    this->set_pass_modulation( modulation, static_cast< float >( pass.m_color.a ) / 255.0f );

    {

        if ( pass.m_local )
        {
            this->redraw_local( );
        } else
        {
            for ( C_BaseEntity *entity : pass.m_entities )
            {
                entity->DrawModel( STUDIO_RENDER | STUDIO_NOSHADOWS );
            }
        }
    }

    render_context->PopRenderTargetAndViewport( );

    const i32 stencil{ pass.m_profile->m_stencil };
    const i32 blur{ pass.m_profile->m_blur };

    // the preview only fills a corner of the screen sized buffers
    const i32 buffer_width{ g_cl.m_screen_width };
    const i32 buffer_height{ g_cl.m_screen_height };

    const bool mirrored{ pass.m_local && g_cl.m_vm_flipped };

    if ( mirrored )
    {
        render_context->CullMode( MATERIAL_CULLMODE_CCW );
    }

    if ( blur > 0 && this->m_blur_y )
    {

        void *shader{ material_shader( this->m_blur_y ) };
        if ( shader && shader != this->m_blur_y_shader )
        {
            this->m_blur_y_shader = shader;
            this->m_bloom_amount =
                this->m_blur_y->FindVar( ATLAS_XORSTR( "$bloomamount" ).get( ), nullptr, false );
        }

        if ( shader && this->m_bloom_amount )
        {
            this->m_bloom_amount->SetFloatValue( static_cast< float >( blur ) );
        }

        render_context->PushRenderTargetAndViewport( );
        render_context->Viewport( 0, 0, width, height );
        render_context->SetRenderTarget( this->m_buffer2 );
        render_context->DrawScreenSpaceRectangle( this->m_blur_x, 0, 0, width, height,
                                                  0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                  buffer_width, buffer_height );
        render_context->SetRenderTarget( this->m_buffer1 );
        render_context->DrawScreenSpaceRectangle( this->m_blur_y, 0, 0, width, height,
                                                  0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                  buffer_width, buffer_height );
        render_context->PopRenderTargetAndViewport( );
    }

    // the halo only lands where the mask pass left the stencil untouched, so the models
    // themselves never get painted over
    {

        render_context->SetStencilEnable( true );
        render_context->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
        render_context->SetStencilPassOperation( STENCILOPERATION_KEEP );
        render_context->SetStencilFailOperation( STENCILOPERATION_KEEP );
        render_context->SetStencilZFailOperation( STENCILOPERATION_KEEP );
        render_context->SetStencilReferenceValue( 0 );
        render_context->SetStencilWriteMask( 0x00 );
        render_context->SetStencilTestMask( 0xFF );

        if ( stencil > 0 )
        {
            const i32 side{ ( stencil + 1 ) / 2 };

            render_context->DrawScreenSpaceRectangle( this->m_halo, -side, 0, width, height,
                                                      0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                      buffer_width, buffer_height );
            render_context->DrawScreenSpaceRectangle( this->m_halo, 0, -side, width, height,
                                                      0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                      buffer_width, buffer_height );
            render_context->DrawScreenSpaceRectangle( this->m_halo, side, 0, width, height,
                                                      0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                      buffer_width, buffer_height );
            render_context->DrawScreenSpaceRectangle( this->m_halo, 0, side, width, height,
                                                      0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                      buffer_width, buffer_height );

            if ( const i32 corner{ stencil / 2 } )
            {
                render_context->DrawScreenSpaceRectangle( this->m_halo, -corner, -corner, width, height,
                                                          0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                          buffer_width, buffer_height );
                render_context->DrawScreenSpaceRectangle( this->m_halo, corner, corner, width, height,
                                                          0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                          buffer_width, buffer_height );
                render_context->DrawScreenSpaceRectangle( this->m_halo, corner, -corner, width, height,
                                                          0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                          buffer_width, buffer_height );
                render_context->DrawScreenSpaceRectangle( this->m_halo, -corner, corner, width, height,
                                                          0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                          buffer_width, buffer_height );
            }
        }

        if ( blur > 0 )
        {
            render_context->DrawScreenSpaceRectangle( this->m_halo, 0, 0, width, height,
                                                      0.0f, 0.0f, static_cast< float >( width - 1 ), static_cast< float >( height - 1 ),
                                                      buffer_width, buffer_height );
        }

        render_context->SetStencilEnable( false );
    }

    if ( mirrored )
    {
        render_context->CullMode( MATERIAL_CULLMODE_CW );
    }
}

void glow::set_pass_modulation( const float modulation[ 3 ], float alpha ) const
{
    g_tf2.m_render_view->SetColorModulation( modulation );
    g_tf2.m_render_view->SetBlend( alpha );

    g_tf2.m_studio_render->SetColorModulation( modulation );
    g_tf2.m_studio_render->SetAlphaModulation( alpha );
}

void glow::redraw_local( ) const
{
    const local_draw_t &draw{ this->m_local_draw };

    g_hooks.m_draw_model.thiscall< void >( draw.m_studio_render, draw.m_results, draw.m_info,
                                           draw.m_bone_to_world, draw.m_flex_weights,
                                           draw.m_flex_delayed_weights, draw.m_model_origin,
                                           draw.m_flags );
}

void glow::draw_entities( )
{
    for ( const pass_t &pass : this->m_passes )
    {
        for ( C_BaseEntity *entity : pass.m_entities )
        {
            entity->DrawModel( STUDIO_RENDER | STUDIO_NOSHADOWS );
        }
    }
}

bool glow::player_pass( C_TFPlayer *player, usize &pass ) const
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

    pass = static_cast< usize >( target );
    return !this->m_passes[ pass ].m_profile->off( );
}

bool glow::decide( C_BaseEntity *entity, ETFClassID class_id, usize &pass )
{
    C_TFPlayer *local{ g_cl.m_local };
    if ( !local )
    {
        return false;
    }

    // a pickup with its pass off still falls through, so a carried intel keeps its
    // carrier's glow like any other attachment
    const usize pickup{ g_cl.pickup_kind( entity, class_id ) };
    if ( pickup != config::PICKUP_COUNT )
    {
        const usize candidate{ pickup == config::PICKUP_INTEL ? INTEL_PASS : PICKUP_PASS };
        if ( !this->m_passes[ candidate ].m_profile->off( ) )
        {
            pass = candidate;
            return true;
        }
    }

    switch ( class_id )
    {
        case ETFClassID::CTFPlayer:
        {
            if ( !this->m_players_enabled )
            {
                return false;
            }

            auto       *player{ entity->As< C_TFPlayer * >( ) };
            const auto *identity{ g_entity_cache.find( entity ) };
            if ( !identity || identity->m_index < 1 || identity->m_index >= MAX_TRACKED_PLAYERS )
            {
                return this->player_pass( player, pass );
            }

            player_pass_t &saved{ this->m_player_passes[ identity->m_index ] };
            if ( !saved.m_checked || saved.m_handle != identity->m_handle )
            {
                saved           = player_pass_t{ };
                saved.m_handle  = identity->m_handle;
                saved.m_allowed = this->player_pass( player, saved.m_pass );
                saved.m_checked = true;
            }
            pass = saved.m_pass;
            return saved.m_allowed;
        }

        case ETFClassID::CObjectSentrygun:
        case ETFClassID::CObjectDispenser:
        case ETFClassID::CObjectTeleporter:
        {
            if ( this->m_passes[ config::ESP_TARGET_COUNT ].m_profile->off( ) )
            {
                return false;
            }

            auto *object{ entity->As< C_BaseObject * >( ) };
            if ( object->m_iHealth( ) <= 0 || object->m_bCarried( ) )
            {
                return false;
            }

            if ( entity->m_iTeamNum( ) == local->m_iTeamNum( ) )
            {
                return false;
            }

            pass = config::ESP_TARGET_COUNT;
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
            if ( this->m_passes[ config::ESP_TARGET_COUNT + 1 ].m_profile->off( ) ||
                 !g_cfg.m_esp.shows_projectile( entity->m_iTeamNum( ) == local->m_iTeamNum( ) ) )
            {
                return false;
            }

            pass = config::ESP_TARGET_COUNT + 1;
            break;
        }

        case ETFClassID::CEyeballBoss:
        case ETFClassID::CHeadlessHatman:
        case ETFClassID::CMerasmus:
        case ETFClassID::CZombie:
        {
            if ( this->m_passes[ NPC_PASS ].m_profile->off( ) )
            {
                return false;
            }

            if ( entity->m_iTeamNum( ) == local->m_iTeamNum( ) )
            {
                return false;
            }

            pass = NPC_PASS;
            break;
        }

        default:
        {
            if ( !this->m_players_enabled )
            {
                return false;
            }

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
                    return this->decide( owner, ETFClassID::CTFPlayer, pass );
                }

                entity = owner;
            }

            return false;
        }
    }

    return !this->m_passes[ pass ].m_profile->off( );
}

void glow::collect( )
{
    const config::glow_t &settings{ g_cfg.m_glow };

    this->m_players_enabled = false;
    for ( player_pass_t &player : this->m_player_passes )
    {
        player.m_checked = false;
    }

    for ( usize target{ }; target < config::ESP_TARGET_COUNT; ++target )
    {
        this->m_passes[ target ].m_profile = &settings.m_players[ target ];
        this->m_passes[ target ].m_color   = settings.m_players[ target ].m_color;
        this->m_passes[ target ].m_entities.clear( );
        this->m_players_enabled |= !settings.m_players[ target ].off( );
    }

    this->m_passes[ config::ESP_TARGET_COUNT ].m_profile = &settings.m_buildings;
    this->m_passes[ config::ESP_TARGET_COUNT ].m_color   = settings.m_buildings.m_color;
    this->m_passes[ config::ESP_TARGET_COUNT ].m_entities.clear( );

    this->m_passes[ config::ESP_TARGET_COUNT + 1 ].m_profile = &settings.m_projectiles;
    this->m_passes[ config::ESP_TARGET_COUNT + 1 ].m_color   = settings.m_projectiles.m_color;
    this->m_passes[ config::ESP_TARGET_COUNT + 1 ].m_entities.clear( );

    this->m_passes[ PICKUP_PASS ].m_profile = &settings.m_pickups;
    this->m_passes[ PICKUP_PASS ].m_color   = settings.m_pickups.m_color;
    this->m_passes[ PICKUP_PASS ].m_entities.clear( );

    this->m_passes[ INTEL_PASS ].m_profile = &settings.m_intel;
    this->m_passes[ INTEL_PASS ].m_color   = settings.m_intel.m_color;
    this->m_passes[ INTEL_PASS ].m_entities.clear( );

    this->m_passes[ NPC_PASS ].m_profile = &settings.m_npcs;
    this->m_passes[ NPC_PASS ].m_color   = settings.m_npcs.m_color;
    this->m_passes[ NPC_PASS ].m_entities.clear( );

    if ( !this->m_players_enabled && settings.m_buildings.off( ) && settings.m_projectiles.off( ) &&
         settings.m_pickups.off( ) && settings.m_intel.off( ) && settings.m_npcs.off( ) )
    {
        return;
    }

    const auto entities{ g_entity_cache.networked( ) };

    for ( const auto *cached : entities )
    {
        C_BaseEntity *entity{ cached->m_entity };
        if ( !cached->m_client_class || entity->IsDormant( ) || !entity->ShouldDraw( ) )
        {
            continue;
        }

        usize pass{ };
        if ( !this->decide( entity, cached->m_class_id, pass ) )
        {
            continue;
        }

        if ( !g_cl.is_on_screen( entity ) )
        {
            continue;
        }

        this->m_passes[ pass ].m_entities.push_back( entity );
    }
}

void glow::on_level_init( )
{
    this->destroy_resources( );
    this->create_resources( );
}

void glow::on_entity_deleted( C_BaseEntity *entity )
{
    const auto forget{ [ entity ]( pass_t &pass )
    {
        for ( usize at{ }; at < pass.m_entities.size( ); )
        {
            if ( pass.m_entities[ at ] == entity )
            {
                pass.m_entities.erase( at );
            } else
            {
                ++at;
            }
        }
    } };
    for ( pass_t &pass : this->m_passes )
    {
        forget( pass );
    }
    forget( this->m_preview_pass );
    forget( this->m_local_pass );
}

void glow::on_level_shutdown( )
{
    this->destroy_resources( );

    for ( pass_t &pass : this->m_passes )
    {
        pass.m_entities.clear( );
    }
}

void glow::create_resources( )
{
    const i32 width{ g_cl.m_screen_width };
    const i32 height{ g_cl.m_screen_height };
    if ( width < 1 || height < 1 )
    {
        return;
    }

    if ( !this->m_glow_color )
    {
        IMaterial *glow_color{ g_tf2.m_material_system->FindMaterial( ATLAS_XORSTR( "dev/glow_color" ).get( ), ATLAS_XORSTR( "Other textures" ).get( ) ) };
        if ( !glow_color || glow_color->IsErrorMaterial( ) )
        {
            return;
        }

        glow_color->IncrementReferenceCount( );
        this->m_glow_color = glow_color;
    }

    // the buffers share the scene depth buffer so the color pass keeps proper self-occlusion
    if ( !this->m_buffer1 )
    {
        this->m_buffer1 = g_tf2.m_material_system->CreateNamedRenderTargetTextureEx(
            ATLAS_XORSTR( "atlas_glow_buffer1" ).get( ), width, height, RT_SIZE_LITERAL, IMAGE_FORMAT_RGB888,
            MATERIAL_RT_DEPTH_SHARED, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_EIGHTBITALPHA,
            CREATERENDERTARGETFLAGS_HDR );
        if ( this->m_buffer1 )
        {
            this->m_buffer1->IncrementReferenceCount( );
        }
    }

    if ( !this->m_buffer2 )
    {
        this->m_buffer2 = g_tf2.m_material_system->CreateNamedRenderTargetTextureEx(
            ATLAS_XORSTR( "atlas_glow_buffer2" ).get( ), width, height, RT_SIZE_LITERAL, IMAGE_FORMAT_RGB888,
            MATERIAL_RT_DEPTH_SHARED, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_EIGHTBITALPHA,
            CREATERENDERTARGETFLAGS_HDR );
        if ( this->m_buffer2 )
        {
            this->m_buffer2->IncrementReferenceCount( );
        }
    }

    if ( !this->m_buffer1 || !this->m_buffer2 )
    {
        return;
    }

    if ( !this->m_blur_x )
    {
        KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "BlurFilterX" ).get( ) ) };
        kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "atlas_glow_buffer1" ).get( ) );
        this->m_blur_x = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_glow_blur_x" ).get( ), kv );
    }

    if ( !this->m_blur_y )
    {
        KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "BlurFilterY" ).get( ) ) };
        kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "atlas_glow_buffer2" ).get( ) );
        this->m_blur_y = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_glow_blur_y" ).get( ), kv );

        if ( this->m_blur_y )
        {
            this->m_bloom_amount =
                this->m_blur_y->FindVar( ATLAS_XORSTR( "$bloomamount" ).get( ), nullptr, false );
            this->m_blur_y_shader = material_shader( this->m_blur_y );
        }
    }

    // created last so a partial setup never leaves render_main thinking everything is ready
    if ( !this->m_halo && this->m_blur_x && this->m_blur_y )
    {
        KeyValues *kv{ new KeyValues( ATLAS_XORSTR( "UnlitGeneric" ).get( ) ) };
        kv->SetString( ATLAS_XORSTR( "$basetexture" ).get( ), ATLAS_XORSTR( "atlas_glow_buffer1" ).get( ) );
        kv->SetInt( ATLAS_XORSTR( "$additive" ).get( ), 1 );
        this->m_halo = g_tf2.m_material_system->CreateMaterial( ATLAS_XORSTR( "atlas_glow_halo" ).get( ), kv );
    }
}

void glow::destroy_resources( )
{
    this->m_bloom_amount  = nullptr;
    this->m_blur_y_shader = nullptr;

    IMaterial *materials[]{ this->m_halo, this->m_blur_x, this->m_blur_y };

    this->m_halo   = nullptr;
    this->m_blur_x = nullptr;
    this->m_blur_y = nullptr;

    for ( IMaterial *material : materials )
    {
        if ( material )
        {
            material->DecrementReferenceCount( );
            material->DeleteIfUnreferenced( );
        }
    }

    // released after the materials so their $basetexture references are already gone
    ITexture *buffers[]{ this->m_buffer1, this->m_buffer2 };

    this->m_buffer1 = nullptr;
    this->m_buffer2 = nullptr;

    for ( ITexture *buffer : buffers )
    {
        if ( buffer )
        {
            buffer->DecrementReferenceCount( );
            buffer->DeleteIfUnreferenced( );
        }
    }
}
