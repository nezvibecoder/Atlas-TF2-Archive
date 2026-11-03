#include "global.hpp"
#include "gui/theme.hpp"

#include "chams.hpp"
#include "esp_builder.hpp"
#include "glow.hpp"
#include "model_render.hpp"

ATLAS_NO_DESTROY esp_builder g_esp_builder{ };

void esp_builder::begin_frame( )
{
    this->m_pending.m_viewport = rect_t{ };
}

void esp_builder::end_frame( )
{
    this->m_pending.m_chams = g_cfg.previewed_chams( );
    this->m_pending.m_glow  = g_cfg.previewed_glow( );

    if ( this->m_pending.m_local_model != config::LOCAL_MODEL_PLAYER )
    {
        const bool arms{ this->m_pending.m_local_model == config::LOCAL_MODEL_VIEWMODEL };
        this->m_pending.m_chams.m_visible = arms ? g_cfg.m_chams.m_viewmodel : g_cfg.m_chams.m_weapon;
        this->m_pending.m_chams.m_visible_overlay = arms ? g_cfg.m_chams.m_viewmodel_overlay : g_cfg.m_chams.m_weapon_overlay;
        this->m_pending.m_chams.m_visible_color = arms ? g_cfg.m_chams.m_viewmodel_color : g_cfg.m_chams.m_weapon_color;
        this->m_pending.m_chams.m_visible_overlay_color = arms ? g_cfg.m_chams.m_viewmodel_overlay_color : g_cfg.m_chams.m_weapon_overlay_color;
        this->m_pending.m_glow = arms ? g_cfg.m_glow.m_viewmodel : g_cfg.m_glow.m_weapon;
    }

    this->m_pending.m_weapon_chams.m_visible               = g_cfg.m_chams.m_weapon;
    this->m_pending.m_weapon_chams.m_visible_overlay       = g_cfg.m_chams.m_weapon_overlay;
    this->m_pending.m_weapon_chams.m_visible_color         = g_cfg.m_chams.m_weapon_color;
    this->m_pending.m_weapon_chams.m_visible_overlay_color = g_cfg.m_chams.m_weapon_overlay_color;
    this->m_pending.m_weapon_glow                          = g_cfg.m_glow.m_weapon;
    ++this->m_pending.m_serial;

    atl::scoped_lock lock{ this->m_publish };
    this->m_requested = this->m_pending;
}

void esp_builder::capture_t::operator()( )
{
    this->m_owner->publish_target( this->m_frame, this->m_target );
}

bool esp_builder::ensure_targets( )
{
    static const char *const names[ 2 ]{
        ATLAS_XORSTR_STATIC( "atlas_esp_preview_0" ),
        ATLAS_XORSTR_STATIC( "atlas_esp_preview_1" )
    };

    for ( usize index{ }; index < 2; ++index )
    {
        if ( this->m_targets[ index ] )
        {
            continue;
        }

        this->m_targets[ index ] = g_tf2.m_material_system->CreateNamedRenderTargetTextureEx(
            names[ index ], MENU_COLUMN_WIDTH, MENU_BASE_HEIGHT, RT_SIZE_LITERAL,
            IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_SEPARATE,
            TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_EIGHTBITALPHA, 0 );
        if ( !this->m_targets[ index ] )
        {
            return false;
        }
        this->m_targets[ index ]->IncrementReferenceCount( );
    }
    return true;
}

void esp_builder::publish_target( const frame_t &frame, usize target )
{
    atl::scoped_lock lock{ this->m_publish };
    if ( frame.m_epoch != this->m_frame.m_epoch ||
         !g_render.adopt_model_target( frame.m_serial,
                                      static_cast< u32 >( frame.m_viewport.right ),
                                      static_cast< u32 >( frame.m_viewport.bottom ),
                                      MENU_COLUMN_WIDTH, MENU_BASE_HEIGHT ) )
    {
        return;
    }

    this->m_frame = frame;
    this->m_visible_target = target;
}

void esp_builder::on_device_lost( )
{
    this->invalidate_frame( );
}

void esp_builder::invalidate_frame( )
{
    atl::scoped_lock lock{ this->m_publish };
    ++this->m_frame.m_epoch;
    this->m_frame.m_valid = false;
}

bool esp_builder::assign_model( C_BaseEntity *entity, const char *path, void *&model, i32 index )
{
    model = g_tf2.m_model_loader->GetModelForName( path, FMODELLOADER_CLIENT );

    if ( !model )
    {
        return false;
    }

    // SetModelByIndex resolves this private index through hkGetModel, including while disconnected.
    g_tf2.m_CBaseEntity_SetModelByIndex.call_as< set_model_by_index_fn >( entity, index );
    return true;
}

void esp_builder::detach_model( C_BaseEntity *entity, void *&model )
{
    if ( entity )
    {
        g_tf2.m_CBaseEntity_SetModelByIndex.call_as< set_model_by_index_fn >( entity, -1 );
    }

    model = nullptr;
}

C_BaseEntity *esp_builder::spawn_entity( C_BaseEntity *existing )
{
    if ( existing )
    {
        return existing;
    }

    auto *entity{ static_cast< C_BaseEntity * >( g_tf2.m_alloc->Alloc( 0x2800 ) ) };

    if ( !entity )
    {
        return nullptr;
    }

    // Alloc hands back raw memory, so the block still has to be cleared before the constructor
    atl::memset( entity, 0, 0x2800 );
    g_tf2.m_CBaseFlex_ctor.call_as< base_flex_ctor_fn >( entity );
    return entity;
}

bool esp_builder::ensure_entity( )
{
    if ( this->m_entity_ready )
    {
        return true;
    }

    if ( g_tf2.m_CBaseFlex_ctor.is_null( ) )
    {
        return false;
    }

    this->m_entity = this->spawn_entity( this->m_entity );

    if ( !this->m_entity )
    {
        return false;
    }

    if ( !this->assign_model( this->m_entity, this->m_model_path, this->m_active_model, ENTITY_MODEL_INDEX ) )
    {
        return false;
    }

    this->m_entity_ready = true;
    return true;
}

bool esp_builder::ensure_weapon( )
{
    if ( this->m_weapon_ready )
    {
        return true;
    }

    if ( !this->m_weapon_path || g_tf2.m_CBaseFlex_ctor.is_null( ) )
    {
        return false;
    }

    this->m_weapon = this->spawn_entity( this->m_weapon );

    if ( !this->m_weapon )
    {
        return false;
    }

    if ( !this->assign_model( this->m_weapon, this->m_weapon_path, this->m_weapon_model, WEAPON_MODEL_INDEX ) )
    {
        return false;
    }

    this->m_weapon_ready = true;
    return true;
}

bool esp_builder::on_setup_bones( const void *entity, matrix3x4 *out, i32 max_bones )
{
    if ( !out || !this->m_weapon || entity != this->m_weapon || !this->m_player_bones_valid )
    {
        return false;
    }

    const studiohdr_t *studio{ g_tf2.m_model_info->GetStudiomodel( this->m_weapon->GetModel( ) ) };
    void              *player_hdr{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< void * >( this->m_entity ) };

    if ( !studio || !player_hdr )
    {
        return false;
    }

    if ( studio->numbones <= 0 || studio->numbones > max_bones )
    {
        return false;
    }

    for ( i32 index{ }; index < studio->numbones; ++index )
    {
        const i32 source{ g_tf2.m_Studio_BoneIndexByName.call_as< i32 >(
            player_hdr, studio->pBone( index )->pszName( ) ) };

        if ( source >= 0 && source < MAX_STUDIO_BONES )
        {
            atl::memcpy( out[ index ], this->m_player_bones[ source ], sizeof( matrix3x4 ) );
        }
    }

    return true;
}

i32 esp_builder::find_idle_sequence( )
{
    if ( this->m_idle_sequence >= 0 )
    {
        return this->m_idle_sequence;
    }

    this->m_idle_sequence = 0;

    if ( !this->m_entity || g_tf2.m_CBaseAnimating_GetModelPtr.is_null( ) ||
         g_tf2.m_CStudioHdr_LookupSequence.is_null( ) )
    {
        return this->m_idle_sequence;
    }

    void *header{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< void * >( this->m_entity ) };

    if ( !header )
    {
        return this->m_idle_sequence;
    }

    const i32 sequence{ g_tf2.m_CStudioHdr_LookupSequence.call_as< i32 >( header, this->m_stance ) };

    if ( sequence > -1 )
    {
        this->m_idle_sequence = sequence;
    }

    return this->m_idle_sequence;
}

void esp_builder::set_profile( const char *model, const char *weapon, const char *stance,
                               i32 team, usize local_model )
{
    if ( !model || !*model )
    {
        return;
    }

    this->m_pending.m_model     = model;
    this->m_pending.m_weapon    = weapon;
    this->m_pending.m_stance    = stance;
    this->m_pending.m_team      = team;
    this->m_pending.m_local_model = local_model;
}

void esp_builder::step_swap( float frametime )
{
    // full fades per second, so a class swap dips out and back in under half a second
    const float step{ frametime * 5.0f };

    if ( !this->m_next_model )
    {
        this->m_fade += step;
        this->m_fade = this->m_fade > 1.0f ? 1.0f : this->m_fade;
        return;
    }

    this->m_fade -= step;

    if ( this->m_fade > 0.0f )
    {
        return;
    }

    this->m_fade        = 0.0f;
    this->m_model_path  = this->m_next_model;
    this->m_weapon_path = this->m_next_weapon;
    this->m_stance      = this->m_next_stance;
    this->m_team        = this->m_next_team;
    this->m_local_model   = this->m_next_local_model;
    this->m_next_model  = nullptr;

    // the entities keep their allocation and constructor; only the model on them changes
    this->m_idle_sequence = -1;
    this->m_entity_ready  = false;
    this->m_weapon_ready  = false;
}

void esp_builder::on_level_shutdown( )
{
    this->invalidate_frame( );

    this->detach_model( this->m_entity, this->m_active_model );
    this->detach_model( this->m_weapon, this->m_weapon_model );

    this->m_entity_ready       = false;
    this->m_weapon_ready       = false;
    this->m_idle_sequence      = -1;
    this->m_animation_time     = 0.0;
    this->m_bounds_valid       = false;
    this->m_skeleton_count     = 0;
    this->m_bones_ready        = false;
    this->m_player_bones_valid = false;

}

void esp_builder::on_level_init( )
{
    this->invalidate_frame( );
    if ( this->m_entity && this->m_entity_ready )
    {
        this->assign_model( this->m_entity, this->m_model_path, this->m_active_model, ENTITY_MODEL_INDEX );
    }

    if ( this->m_weapon && this->m_weapon_ready )
    {
        this->assign_model( this->m_weapon, this->m_weapon_path, this->m_weapon_model, WEAPON_MODEL_INDEX );
    }
}

void esp_builder::route( C_BaseEntity *entity, const DrawModelInfo_t *info, const matrix3x4 *bone_to_world, i32 &flags )
{
    if ( !this->m_drawing || !this->m_route.m_chams || !entity )
    {
        return;
    }

    const config::cham_profile_t &profile{ *this->m_route.m_chams };
    const config::glow_profile_t &glow{ *this->m_route.m_glow };
    this->m_route.m_chams = nullptr;

    this->m_route.m_routed = chams::route_layers( entity, *info, bone_to_world, model_render::PLACE_PREVIEW, profile.m_visible,
                                                  this->faded( profile.m_visible_color ), profile.m_visible_overlay,
                                                  this->faded( profile.m_visible_overlay_color ), flags );

    u32 instance{ };
    if ( g_glow.m_enabled && !glow.off( ) && g_model_render.active( ) &&
         g_model_render.take_over( entity, *info, bone_to_world, model_render::PLACE_PREVIEW, true, flags, instance ) )
    {
        g_model_render.add_glow( instance, glow, this->faded( glow.m_color ) );
    }
}

color_t esp_builder::faded( const color_t &color ) const
{
    color_t out{ color };
    out.a = static_cast< u8 >( static_cast< float >( color.a ) * this->m_fade );
    return out;
}

void esp_builder::draw_model( C_BaseEntity *entity, const config::cham_profile_t &profile, const config::glow_profile_t &glow )
{
    const usize   layers[ 2 ]{ profile.m_visible, profile.m_visible_overlay };
    const color_t tints[ 2 ]{ profile.m_visible_color, profile.m_visible_overlay_color };

    this->m_route = route_t{ &profile, &glow, false };

    for ( usize layer{ }; layer < 2; ++layer )
    {
        const usize mode{ layers[ layer ] };

        // the base layer keeps drawing the model skin when it is off, the overlay just skips
        if ( layer && ( mode == chams::CHAM_OFF || this->m_route.m_routed ) )
        {
            continue;
        }

        const color_t tint{ mode ? tints[ layer ] : color_t{ 255, 255, 255 } };

        const float modulation[ 3 ]{ static_cast< float >( tint.r ) / 255.0f,
                                     static_cast< float >( tint.g ) / 255.0f,
                                     static_cast< float >( tint.b ) / 255.0f };
        g_tf2.m_render_view->SetColorModulation( modulation );
        g_tf2.m_render_view->SetBlend( this->m_fade * static_cast< float >( tint.a ) / 255.0f );

        g_hooks.m_forced_material_override.thiscall< void >( g_tf2.m_studio_render,
                                                             g_chams.material_for( mode ),
                                                             OVERRIDE_NORMAL );

        entity->DrawModel( STUDIO_RENDER );
    }

    this->m_route = route_t{ };
}

void esp_builder::project_skeleton( const VMatrix &world_to_pixels )
{
    this->m_skeleton_count = 0;

    if ( !this->m_bones_ready || g_tf2.m_CBaseAnimating_GetModelPtr.is_null( ) )
    {
        return;
    }

    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( this->m_entity ) };

    if ( !studio || !studio->m_pStudioHdr )
    {
        return;
    }

    const studiohdr_t *header{ studio->m_pStudioHdr };

    // the same projection the render bounds take, so the segments land on the captured model
    const auto project{ [ & ]( const matrix3x4 &bone, point_t &out )
    {
        const vec3 at{ bone[ 0 ][ 3 ], bone[ 1 ][ 3 ], bone[ 2 ][ 3 ] };

        const float w{ world_to_pixels[ 3 ][ 0 ] * at.x + world_to_pixels[ 3 ][ 1 ] * at.y +
                       world_to_pixels[ 3 ][ 2 ] * at.z + world_to_pixels[ 3 ][ 3 ] };

        if ( w < 0.001f )
        {
            return false;
        }

        const float inverse{ 1.0f / w };

        out.x = static_cast< i32 >( ( world_to_pixels[ 0 ][ 0 ] * at.x + world_to_pixels[ 0 ][ 1 ] * at.y +
                                      world_to_pixels[ 0 ][ 2 ] * at.z + world_to_pixels[ 0 ][ 3 ] ) *
                                    inverse );
        out.y = static_cast< i32 >( ( world_to_pixels[ 1 ][ 0 ] * at.x + world_to_pixels[ 1 ][ 1 ] * at.y +
                                      world_to_pixels[ 1 ][ 2 ] * at.z + world_to_pixels[ 1 ][ 3 ] ) *
                                    inverse );
        return true;
    } };

    const i32 count{ header->numbones < MAX_STUDIO_BONES ? header->numbones : MAX_STUDIO_BONES };

    for ( i32 index{ }; index < count; ++index )
    {
        const mstudiobone_t *bone{ header->pBone( index ) };

        if ( bone->parent < 0 || bone->parent >= count || !( bone->flags & BONE_USED_BY_HITBOX ) ||
             !( header->pBone( bone->parent )->flags & BONE_USED_BY_HITBOX ) )
        {
            continue;
        }

        bone_segment_t segment{ };

        if ( !project( this->m_player_bones[ index ], segment.m_from ) ||
             !project( this->m_player_bones[ bone->parent ], segment.m_to ) )
        {
            continue;
        }

        this->m_skeleton[ this->m_skeleton_count++ ] = segment;
    }
}

void esp_builder::compute_bounds( const CViewSetup &view )
{
    this->m_bounds_valid = false;

    if ( !this->m_entity )
    {
        return;
    }

    vec3 mins{ };
    vec3 maxs{ };
    this->m_entity->GetRenderBounds( mins, maxs );

    const vec3 origin{ this->m_entity->GetRenderOrigin( ) };

    // every out-param is written unconditionally; handing it a null for the matrices we
    // do not care about faults inside the engine
    VMatrix world_to_view{ };
    VMatrix view_to_projection{ };
    VMatrix world_to_projection{ };
    VMatrix world_to_pixels{ };
    g_tf2.m_render_view->GetMatricesForView( view, &world_to_view, &view_to_projection,
                                             &world_to_projection, &world_to_pixels );

    this->project_skeleton( world_to_pixels );

    bool  started{ };
    float left{ };
    float top{ };
    float right{ };
    float bottom{ };

    for ( i32 corner{ }; corner < 8; ++corner )
    {
        const vec3 point{
            origin.x + ( corner & 1 ? maxs.x : mins.x ),
            origin.y + ( corner & 2 ? maxs.y : mins.y ),
            origin.z + ( corner & 4 ? maxs.z : mins.z )
        };

        const float w{ world_to_pixels[ 3 ][ 0 ] * point.x + world_to_pixels[ 3 ][ 1 ] * point.y +
                       world_to_pixels[ 3 ][ 2 ] * point.z + world_to_pixels[ 3 ][ 3 ] };

        if ( w < 0.001f )
        {
            return;
        }

        const float inverse{ 1.0f / w };
        const float screen_x{ ( world_to_pixels[ 0 ][ 0 ] * point.x + world_to_pixels[ 0 ][ 1 ] * point.y +
                                world_to_pixels[ 0 ][ 2 ] * point.z + world_to_pixels[ 0 ][ 3 ] ) *
                              inverse };
        const float screen_y{ ( world_to_pixels[ 1 ][ 0 ] * point.x + world_to_pixels[ 1 ][ 1 ] * point.y +
                                world_to_pixels[ 1 ][ 2 ] * point.z + world_to_pixels[ 1 ][ 3 ] ) *
                              inverse };

        if ( !started )
        {
            left = right = screen_x;
            top = bottom = screen_y;
            started      = true;
            continue;
        }

        left   = screen_x < left ? screen_x : left;
        right  = screen_x > right ? screen_x : right;
        top    = screen_y < top ? screen_y : top;
        bottom = screen_y > bottom ? screen_y : bottom;
    }

    if ( !started )
    {
        return;
    }

    this->m_bounds       = rect_t{ static_cast< i32 >( left ), static_cast< i32 >( top ),
                             static_cast< i32 >( right ), static_cast< i32 >( bottom ) };
    this->m_bounds_valid = this->m_bounds.right > this->m_bounds.left &&
                          this->m_bounds.bottom > this->m_bounds.top;
}

void esp_builder::paint( )
{
    request_t request{ };
    u64       epoch{ };
    usize     target{ };

    {
        atl::scoped_lock lock{ this->m_publish };
        request = this->m_requested;
        epoch   = this->m_frame.m_epoch;
    }

    if ( !request.m_serial || request.m_serial == this->m_painted_request )
    {
        return;
    }
    this->m_painted_request = request.m_serial;

    if ( !this->m_model_path )
    {
        this->m_model_path  = ATLAS_XORSTR_STATIC( "models/player/soldier.mdl" );
        this->m_weapon_path = ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_rocketlauncher.mdl" );
        this->m_stance      = ATLAS_XORSTR_STATIC( "stand_PRIMARY" );
    }

    this->m_bounds_valid = false;
    const rect_t region{ request.m_viewport };
    const i32    width{ region.right - region.left };
    const i32    height{ region.bottom - region.top };

    if ( width <= 0 || height <= 0 || width > MENU_COLUMN_WIDTH || height > MENU_BASE_HEIGHT )
    {
        this->m_animation_time = 0.0;
        return;
    }

    if ( !this->m_capture.acquire( ) )
    {
        return;
    }
    struct release_job_t
    {
        capture_t &m_job;
        ~release_job_t( ) { this->m_job.Release( ); }
    } release_job{ this->m_capture };

    {
        atl::scoped_lock lock{ this->m_publish };
        target = 1u - this->m_visible_target;
    }

    if ( request.m_model )
    {
        const bool same{ request.m_model == this->m_model_path &&
                         request.m_weapon == this->m_weapon_path &&
                         request.m_stance == this->m_stance && request.m_team == this->m_team &&
                         request.m_local_model == this->m_local_model };
        this->m_next_model     = same ? nullptr : request.m_model;
        this->m_next_weapon    = request.m_weapon;
        this->m_next_stance    = request.m_stance;
        this->m_next_team      = request.m_team;
        this->m_next_local_model = request.m_local_model;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const float frametime{ this->m_animation_time > 0.0
                               ? math::clamp( static_cast< float >( now - this->m_animation_time ),
                                              0.0f, 0.1f )
                               : 0.0f };
    this->m_animation_time = now;
    this->step_swap( frametime );

    if ( !this->ensure_entity( ) )
    {
        return;
    }

    auto *animating{ static_cast< C_BaseAnimating * >( this->m_entity ) };
    animating->m_nSequence( ) = this->find_idle_sequence( );

    animating->m_nSkin( )         = this->m_team == TF_TEAM_BLUE ? 1 : 0;
    this->m_entity->m_iTeamNum( ) = this->m_team;

    if ( !g_tf2.m_CBaseAnimating_FrameAdvance.is_null( ) && g_tf2.m_globals && frametime > 0.0f )
    {
        animating->FrameAdvance( frametime );
    }

    if ( !this->m_cubemap )
    {
        this->m_cubemap = g_tf2.m_material_system->FindTexture( ATLAS_XORSTR( "editor/cubemap.hdr" ).get( ), ATLAS_XORSTR( "CubeMap textures" ).get( ), true );
    }

    this->ensure_weapon( );
    this->m_bones_ready = this->m_entity->SetupBones( this->m_player_bones, MAX_STUDIO_BONES,
                                                     BONE_USED_BY_ANYTHING,
                                                     g_tf2.m_globals ? g_tf2.m_globals->curtime : 0.0f );

    CViewSetup view{ };
    view.x        = 0;
    view.y        = 0;
    view.width    = width;
    view.height   = height;
    view.fov      = 20.0f;
    view.zNear    = 7.0f;
    view.zFar     = 1000.0f;
    view.m_bOrtho = false;

    if ( this->m_local_model == config::LOCAL_MODEL_VIEWMODEL )
    {
        view.fov    = 54.0f;
        view.zNear  = 1.0f;
        view.origin = vec3{ 0.0f, -8.3f, 0.0f };
        view.angles = vec3{ };
    } else if ( this->m_local_model == config::LOCAL_MODEL_WEAPON )
    {
        vec3 mins{ };
        vec3 maxs{ };
        this->m_entity->GetRenderBounds( mins, maxs );

        const vec3 center{ ( mins + maxs ) * 0.5f };
        const float radius{ ( maxs - mins ).length( ) * 0.5f };
        const float horizontal{ math::tan( DEG2RAD( view.fov ) * 0.5f ) };
        const float vertical{ horizontal * static_cast< float >( height ) / static_cast< float >( width ) };
        const float distance{ radius / ( math::smaller( horizontal, vertical ) * 0.8f ) + radius };
        const float yaw{ request.m_yaw + 90.0f };
        const float radians{ DEG2RAD( yaw ) };
        view.origin = center + vec3{ distance * math::cos( radians ), distance * math::sin( radians ), 0.0f };
        view.angles = vec3{ 0.0f, yaw + 180.0f, 0.0f };
    } else
    {
        constexpr float VIEWPORT_FILL{ 0.62f };
        constexpr float PLAYER_HALF_SPAN{ 41.5f };
        constexpr float PLAYER_EYE{ 40.0f };

        vec3 mins{ };
        vec3 maxs{ };
        this->m_entity->GetRenderBounds( mins, maxs );

        float half_width{ ( maxs.x - mins.x ) * 0.5f };
        half_width = ( maxs.y - mins.y ) * 0.5f > half_width ? ( maxs.y - mins.y ) * 0.5f : half_width;

        const float half_height{ ( maxs.z - mins.z ) * 0.5f };
        const bool  framed{ half_width > 0.5f || half_height > 0.5f };

        const float horizontal{ math::tan( DEG2RAD( view.fov ) * 0.5f ) * VIEWPORT_FILL };
        const float vertical{ horizontal * static_cast< float >( height ) /
                              static_cast< float >( width ) };

        const float distance{ math::bigger(
            ( framed ? half_width : PLAYER_HALF_SPAN ) / horizontal,
            ( framed ? half_height : PLAYER_HALF_SPAN ) / vertical ) };
        const float eye{ framed ? ( mins.z + maxs.z ) * 0.5f : PLAYER_EYE };

        const float radians{ DEG2RAD( request.m_yaw ) };
        view.origin = vec3{ distance * math::cos( radians ), distance * math::sin( radians ), eye };
        view.angles = vec3{ 0.0f, request.m_yaw + 180.0f, 0.0f };
    }

    static constexpr vec3 ambient[ 6 ]{
        { 0.4f, 0.4f, 0.4f }, { 0.4f, 0.4f, 0.4f }, { 0.4f, 0.4f, 0.4f }, { 0.4f, 0.4f, 0.4f }, { 0.4f, 0.4f, 0.4f }, { 0.4f, 0.4f, 0.4f }
    };
    static constexpr float ambient_cube[ 6 ][ 4 ]{
        { 0.4f, 0.4f, 0.4f, 1.0f }, { 0.4f, 0.4f, 0.4f, 1.0f }, { 0.4f, 0.4f, 0.4f, 1.0f }, { 0.4f, 0.4f, 0.4f, 1.0f }, { 0.4f, 0.4f, 0.4f, 1.0f }, { 0.4f, 0.4f, 0.4f, 1.0f }
    };

    if ( !this->ensure_targets( ) )
    {
        return;
    }

    if ( this->m_local_model != config::LOCAL_MODEL_PLAYER )
    {
        this->m_bounds         = rect_t{ 0, 0, width, height };
        this->m_bounds_valid   = true;
        this->m_skeleton_count = 0;
    } else
    {
        this->compute_bounds( view );
    }
    if ( !this->m_bounds_valid )
    {
        return;
    }

    IMatRenderContext *context{ g_tf2.m_material_system->GetRenderContext( ) };

    this->m_capture.m_owner = this;
    this->m_capture.m_target = target;
    frame_t &frame{ this->m_capture.m_frame };
    frame.m_viewport = rect_t{ 0, 0, width, height };
    frame.m_bounds = this->m_bounds;
    frame.m_skeleton_count = this->m_skeleton_count;
    atl::memcpy( frame.m_skeleton, this->m_skeleton,
                 sizeof( bone_segment_t ) * static_cast< usize >( this->m_skeleton_count ) );
    frame.m_epoch = epoch;
    frame.m_serial = ++this->m_serial;
    frame.m_valid = true;
    frame.m_local_model = this->m_local_model;

    const float old_blend{ g_tf2.m_render_view->GetBlend( ) };
    float       old_color[ 3 ]{ };
    g_tf2.m_render_view->GetColorModulation( old_color );

    if ( g_tf2.m_studio_render )
    {
        g_tf2.m_studio_render->SetAmbientLightColors( ambient );
        g_tf2.m_studio_render->SetLocalLights( 0, nullptr );
    }

    context->SetLightingOrigin( vec3{ 0.0f, 0.0f, 0.0f } );
    context->SetAmbientLightCube( ambient_cube );

    if ( this->m_cubemap )
    {
        context->BindLocalCubemap( this->m_cubemap );
    }

    VPlane frustum[ FRUSTUM_NUMPLANES ]{ };
    context->ClearColor4ub( 0, 0, 0, 0 );
    // VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL
    g_tf2.m_render_view->Push3DView( view, 1 | 2 | 32,
                                     this->m_targets[ target ], frustum );

    // the preview wears the visible chams of the profile being edited; the occluded ones have
    // nothing to hide behind in here
    if ( g_tf2.m_model_render )
    {
        g_tf2.m_model_render->SuppressEngineLighting( true );
    }

    this->m_drawing = true;

    const bool armed{ this->m_bones_ready && this->m_weapon_ready };
    const bool arms{ this->m_local_model == config::LOCAL_MODEL_VIEWMODEL };
    this->draw_model( this->m_entity, request.m_chams, request.m_glow );

    if ( armed )
    {
        auto *weapon_animating{ this->m_weapon->As< C_BaseAnimating * >( ) };
        weapon_animating->m_nSkin( )  = this->m_team == TF_TEAM_BLUE ? 1 : 0;
        this->m_weapon->m_iTeamNum( ) = this->m_team;
        this->m_player_bones_valid = true;
        this->draw_model( this->m_weapon, arms ? request.m_weapon_chams : request.m_chams, arms ? request.m_weapon_glow : request.m_glow );
    }

    if ( !g_model_render.active( ) )
    {
        g_glow.render_preview( context, request.m_glow, this->m_fade, this->m_entity,
                               armed && !arms ? this->m_weapon : nullptr, width, height );
        if ( armed && arms )
        {
            g_glow.render_preview( context, request.m_weapon_glow, this->m_fade,
                                   this->m_weapon, nullptr, width, height );
        }
    }
    this->m_player_bones_valid = false;

    g_hooks.m_forced_material_override.thiscall< void >(
        g_tf2.m_studio_render, static_cast< IMaterial * >( nullptr ), OVERRIDE_NORMAL );

    this->m_drawing = false;

    if ( g_tf2.m_model_render )
    {
        g_tf2.m_model_render->SuppressEngineLighting( false );
    }

    if ( ICallQueue *queue{ context->GetCallQueue( ) } )
    {
        queue->QueueFunctor( &this->m_capture );
    } else
    {
        this->m_capture( );
    }

    g_tf2.m_render_view->PopView( frustum );
    context->ClearColor4ub( 0, 0, 0, 255 );

    if ( this->m_cubemap )
    {
        context->BindLocalCubemap( nullptr );
    }

    context->Release( );

    g_tf2.m_render_view->SetBlend( old_blend );
    g_tf2.m_render_view->SetColorModulation( old_color );
}
