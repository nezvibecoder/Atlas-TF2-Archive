#include "model_render.hpp"

#include "render/shaders/glow.hpp"
#include "render/shaders/model.hpp"

ATLAS_NO_DESTROY model_render g_model_render{ };

constexpr u32 VIEW_PROJECTION_REGISTER{ 0 };
constexpr u32 WEIGHT_DECODE_REGISTER{ 4 };
constexpr u32 EYE_REGISTER{ 5 };
constexpr u32 PALETTE_REGISTER{ 8 };
constexpr u32 VERTEX_REGISTER_COUNT{ PALETTE_REGISTER + 3 * MAX_HARDWARE_BONES };

constexpr float CLEARED_PALETTE[ 3 * MAX_HARDWARE_BONES ][ 4 ]{ };

constexpr u32 PIXEL_REGISTER_COUNT{ 2 };

constexpr float VIEWMODEL_DEPTH_MAX{ 0.1f };
constexpr float DECAL_SLOPE_SCALE_BIAS{ 1.0f / -0.5f };
constexpr float DECAL_DEPTH_BIAS{ 1.0f / -262144.0f };
constexpr u32 TIME_REGISTER{ 1 };

constexpr f64 TIME_WRAP{ 1024.0 };

constexpr const u32 *PIXEL_SHADERS[ model_render::SHADE_COUNT ]{
    render::shaders::MODEL_FLAT_PIXEL_SHADER,  render::shaders::MODEL_SHADED_PIXEL_SHADER, render::shaders::MODEL_FRESNEL_PIXEL_SHADER,
    render::shaders::MODEL_LAVA_PIXEL_SHADER,  render::shaders::MODEL_PEARL_PIXEL_SHADER,  render::shaders::MODEL_BUBBLE_PIXEL_SHADER,
    render::shaders::MODEL_LSD_PIXEL_SHADER,   render::shaders::MODEL_NETWORK_PIXEL_SHADER, render::shaders::MODEL_SOAP_PIXEL_SHADER,
    render::shaders::MODEL_AURORA_PIXEL_SHADER, render::shaders::MODEL_AQUARIUM_PIXEL_SHADER, render::shaders::MODEL_SKY_PIXEL_SHADER,
};

constexpr float GLOW_BLUR_SPACING{ 2.5f };

constexpr render::d3d9::D3DVERTEXELEMENT9 RIGID_ELEMENTS[]{
    { 0, 0, render::d3d9::D3DDECLTYPE_FLOAT3, 0, render::d3d9::D3DDECLUSAGE_POSITION, 0 },
    render::d3d9::D3DDECL_END,
};

constexpr render::d3d9::D3DVERTEXELEMENT9 FLOAT_WEIGHT_ELEMENTS[]{
    { 0, 0, render::d3d9::D3DDECLTYPE_FLOAT3, 0, render::d3d9::D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, render::d3d9::D3DDECLTYPE_FLOAT2, 0, render::d3d9::D3DDECLUSAGE_BLENDWEIGHT, 0 },
    { 0, 20, render::d3d9::D3DDECLTYPE_D3DCOLOR, 0, render::d3d9::D3DDECLUSAGE_BLENDINDICES, 0 },
    render::d3d9::D3DDECL_END,
};

constexpr render::d3d9::D3DVERTEXELEMENT9 SHORT_WEIGHT_ELEMENTS[]{
    { 0, 0, render::d3d9::D3DDECLTYPE_FLOAT3, 0, render::d3d9::D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, render::d3d9::D3DDECLTYPE_SHORT2, 0, render::d3d9::D3DDECLUSAGE_BLENDWEIGHT, 0 },
    { 0, 16, render::d3d9::D3DDECLTYPE_D3DCOLOR, 0, render::d3d9::D3DDECLUSAGE_BLENDINDICES, 0 },
    render::d3d9::D3DDECL_END,
};

ATLAS_FORCE_INLINE usize lit_index( u64 format )
{
    return ( ( format & VERTEX_FORMAT_COMPRESSED ) ? 1 : 0 ) | ( ( format & VERTEX_BONE_WEIGHT_MASK ) ? 2 : 0 ) |
           ( ( format & VERTEX_BONE_INDEX ) ? 4 : 0 );
}

ATLAS_FORCE_INLINE void lit_elements( usize index, render::d3d9::D3DVERTEXELEMENT9 ( &elements )[ 5 ] )
{
    const bool compressed{ ( index & 1 ) != 0 };
    usize      count{ };
    u16        offset{ 12 };

    elements[ count++ ] = { 0, 0, render::d3d9::D3DDECLTYPE_FLOAT3, 0, render::d3d9::D3DDECLUSAGE_POSITION, 0 };

    if ( index & 2 )
    {
        elements[ count++ ] = { 0, offset, compressed ? render::d3d9::D3DDECLTYPE_SHORT2 : render::d3d9::D3DDECLTYPE_FLOAT2, 0,
                                render::d3d9::D3DDECLUSAGE_BLENDWEIGHT, 0 };
        offset += compressed ? 4 : 8;
    }

    if ( index & 4 )
    {
        elements[ count++ ] = { 0, offset, render::d3d9::D3DDECLTYPE_D3DCOLOR, 0, render::d3d9::D3DDECLUSAGE_BLENDINDICES, 0 };
        offset += 4;
    }

    elements[ count++ ] = { 0, offset, compressed ? render::d3d9::D3DDECLTYPE_UBYTE4 : render::d3d9::D3DDECLTYPE_FLOAT3, 0,
                            render::d3d9::D3DDECLUSAGE_NORMAL, 0 };
    elements[ count ]   = render::d3d9::D3DDECL_END;
}

constexpr u32 RECORDED_STATES[]{
    render::d3d9::D3DRS_ZENABLE,
    render::d3d9::D3DRS_ZFUNC,
    render::d3d9::D3DRS_ZWRITEENABLE,
    render::d3d9::D3DRS_CULLMODE,
    render::d3d9::D3DRS_FILLMODE,
    render::d3d9::D3DRS_ALPHATESTENABLE,
    render::d3d9::D3DRS_ALPHABLENDENABLE,
    render::d3d9::D3DRS_SRCBLEND,
    render::d3d9::D3DRS_DESTBLEND,
    render::d3d9::D3DRS_BLENDOP,
    render::d3d9::D3DRS_FOGENABLE,
    render::d3d9::D3DRS_SCISSORTESTENABLE,
    render::d3d9::D3DRS_CLIPPLANEENABLE,
    render::d3d9::D3DRS_SRGBWRITEENABLE,
    render::d3d9::D3DRS_COLORWRITEENABLE,
    render::d3d9::D3DRS_DEPTHBIAS,
    render::d3d9::D3DRS_SLOPESCALEDEPTHBIAS,
    render::d3d9::D3DRS_STENCILENABLE,
    render::d3d9::D3DRS_STENCILFUNC,
    render::d3d9::D3DRS_STENCILREF,
    render::d3d9::D3DRS_STENCILPASS,
    render::d3d9::D3DRS_STENCILFAIL,
    render::d3d9::D3DRS_STENCILZFAIL,
    render::d3d9::D3DRS_STENCILMASK,
    render::d3d9::D3DRS_STENCILWRITEMASK,
};

constexpr u32 RECORDED_SAMPLER_STATES[]{
    render::d3d9::D3DSAMP_ADDRESSU,
    render::d3d9::D3DSAMP_ADDRESSV,
    render::d3d9::D3DSAMP_MAGFILTER,
    render::d3d9::D3DSAMP_MINFILTER,
    render::d3d9::D3DSAMP_MIPFILTER,
    render::d3d9::D3DSAMP_SRGBTEXTURE,
};

template< typename F >
ATLAS_FORCE_INLINE bool for_each_group( const studiohdr_t    *header,
                                        const studiohwdata_t *hardware,
                                        i32                   body,
                                        i32                   lod,
                                        F                   &&visit )
{
    const studioloddata_t &lod_data{ hardware->m_pLODs[ lod ] };

    for ( i32 part{ }; part < header->numbodyparts; ++part )
    {
        const mstudiobodyparts_t *bodypart{ header->pBodypart( part ) };
        const mstudiomodel_t     *model{ bodypart->pModel( body / bodypart->base % bodypart->nummodels ) };

        for ( i32 index{ }; index < model->nummeshes; ++index )
        {
            const studiomeshdata_t &mesh{ lod_data.m_pMeshData[ model->pMesh( index )->meshid ] };

            for ( i32 group{ }; group < mesh.m_NumGroup; ++group )
            {
                if ( !visit( mesh.m_pMeshGroup[ group ] ) )
                {
                    return false;
                }
            }
        }
    }

    return true;
}

ATLAS_FORCE_INLINE void view_projection( IMatRenderContext *render_context, VMatrix &out, float ( &eye )[ 4 ] )
{
    VMatrix view{ };
    VMatrix projection{ };
    render_context->GetMatrix( MATERIAL_VIEW, &view );
    render_context->GetMatrix( MATERIAL_PROJECTION, &projection );

    for ( i32 row{ }; row < 4; ++row )
    {
        for ( i32 column{ }; column < 4; ++column )
        {
            out[ row ][ column ] = projection[ row ][ 0 ] * view[ 0 ][ column ] + projection[ row ][ 1 ] * view[ 1 ][ column ] +
                                   projection[ row ][ 2 ] * view[ 2 ][ column ] + projection[ row ][ 3 ] * view[ 3 ][ column ];
        }
    }

    for ( i32 axis{ }; axis < 3; ++axis )
    {
        eye[ axis ] = -( view[ 0 ][ axis ] * view[ 0 ][ 3 ] + view[ 1 ][ axis ] * view[ 1 ][ 3 ] + view[ 2 ][ axis ] * view[ 2 ][ 3 ] );
    }

    eye[ 3 ] = 1.0f;
}

ATLAS_FORCE_INLINE void set_decal_bias( const render::d3d9::device9 &device, bool decal )
{
    device.SetRenderState( render::d3d9::D3DRS_SLOPESCALEDEPTHBIAS, decal ? __builtin_bit_cast( u32, DECAL_SLOPE_SCALE_BIAS ) : 0 );
    device.SetRenderState( render::d3d9::D3DRS_DEPTHBIAS, decal ? __builtin_bit_cast( u32, DECAL_DEPTH_BIAS ) : 0 );
}

ATLAS_FORCE_INLINE void set_layer_state( const render::d3d9::device9 &device, usize layer )
{
    const bool occluded{ layer == model_render::LAYER_OCCLUDED_BASE || layer == model_render::LAYER_OCCLUDED_OVERLAY };

    set_decal_bias( device, layer == model_render::LAYER_VISIBLE_OVERLAY );

    device.SetRenderState( render::d3d9::D3DRS_ZFUNC, occluded ? render::d3d9::D3DCMP_GREATER : render::d3d9::D3DCMP_LESSEQUAL );
    device.SetRenderState( render::d3d9::D3DRS_ZWRITEENABLE, layer == model_render::LAYER_VISIBLE_BASE );
    device.SetRenderState( render::d3d9::D3DRS_STENCILFUNC, occluded ? render::d3d9::D3DCMP_EQUAL : render::d3d9::D3DCMP_ALWAYS );
    device.SetRenderState( render::d3d9::D3DRS_STENCILREF, occluded ? 0 : 1 );
    device.SetRenderState( render::d3d9::D3DRS_STENCILPASS, occluded ? render::d3d9::D3DSTENCILOP_KEEP : render::d3d9::D3DSTENCILOP_REPLACE );
    device.SetRenderState( render::d3d9::D3DRS_STENCILFAIL, render::d3d9::D3DSTENCILOP_KEEP );
    device.SetRenderState( render::d3d9::D3DRS_STENCILZFAIL, render::d3d9::D3DSTENCILOP_KEEP );
    device.SetRenderState( render::d3d9::D3DRS_STENCILWRITEMASK, occluded ? 0x00 : 0xFF );
}

i32 model_render::phase_t::AddRef( )
{
    return _InterlockedIncrement( &this->m_job->m_references );
}

i32 model_render::phase_t::Release( )
{
    return _InterlockedDecrement( &this->m_job->m_references );
}

void model_render::phase_t::operator()( )
{
    if ( this->m_run )
    {
        ( this->m_job->m_owner->*this->m_run )( *this->m_job );
    }
}

void model_render::begin_frame( )
{
    if ( this->m_job )
    {
        this->end_frame( );
    }

    ++this->m_serial;
    this->m_routing = true;

    for ( job_t &job : this->m_jobs )
    {
        if ( job.acquire( ) )
        {
            this->m_job = &job;
            break;
        }
    }

    if ( !this->m_job )
    {
        return;
    }

    const f64 seconds{ g_tf2.m_engine->Plat_FloatTime( ) };

    job_t &job{ *this->m_job };
    job.m_owner         = this;
    job.m_clear_stencil = false;
    job.m_time          = static_cast< float >( seconds - static_cast< f64 >( static_cast< i64 >( seconds / TIME_WRAP ) ) * TIME_WRAP );
    job.m_view          = 0;
    job.m_instances.clear( );
    job.m_poses.clear( );
    job.m_glows.clear( );

    for ( atl::vector< layer_t > &layer : job.m_layers )
    {
        layer.clear( );
    }

    job.m_view_phase.m_job   = &job;
    job.m_view_phase.m_run   = &model_render::draw_view;
    job.m_layers_phase.m_job = &job;
    job.m_layers_phase.m_run = &model_render::draw;
    job.m_glow_phase.m_job   = &job;
    job.m_glow_phase.m_run   = &model_render::draw_glow;
    job.m_end_phase.m_job    = &job;
}

void model_render::end_frame( )
{
    if ( !this->m_job )
    {
        return;
    }

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };
    queue( render_context, this->m_job->m_end_phase );
    render_context->Release( );

    _InterlockedDecrement( &this->m_job->m_references );
    this->m_job = nullptr;
}

bool model_render::take_over( C_BaseEntity          *entity,
                              const DrawModelInfo_t &info,
                              const matrix3x4       *bone_to_world,
                              placement_t            placement,
                              bool                   engine_base,
                              i32                   &flags,
                              u32                   &instance )
{
    if ( flags & ( ROUTE_TAKEN_OVER | ROUTE_ENGINE_BASE ) )
    {
        instance = static_cast< u32 >( ( flags >> ROUTE_INSTANCE_SHIFT ) & ROUTE_INSTANCE_MASK );
        return true;
    }

    const i32 index{ entity->entindex( ) };

    if ( !this->m_job || !bone_to_world || !info.m_pStudioHdr || !info.m_pHardwareData ||
         ( placement == PLACE_WORLD && ( !this->m_routing || index < 0 || index >= MAX_EDICTS ) ) )
    {
        return false;
    }

    job_t &job{ *this->m_job };
    if ( job.m_instances.size( ) > static_cast< usize >( ROUTE_INSTANCE_MASK ) )
    {
        return false;
    }

    const studiohdr_t    *header{ info.m_pStudioHdr };
    const studiohwdata_t *hardware{ info.m_pHardwareData };
    const i32             bone_count{ header->numbones > 1 ? entity->As< C_BaseAnimating * >( )->m_CachedBoneData( ).m_count : header->numbones };

    if ( !this->push_instance( job, header, hardware, info.m_Body, hardware->m_RootLOD, bone_to_world, bone_count, instance ) )
    {
        return false;
    }

    instance_t &entry{ job.m_instances[ instance ] };
    entry.m_placement = placement;

    if ( placement == PLACE_WORLD )
    {
        this->m_latest[ index ] = latest_t{ this->m_serial, instance };
    } else
    {
        const matrix3x4 &pose{ job.m_poses[ entry.m_first_pose ].m_matrix };
        const float      determinant{ pose[ 0 ][ 0 ] * ( pose[ 1 ][ 1 ] * pose[ 2 ][ 2 ] - pose[ 1 ][ 2 ] * pose[ 2 ][ 1 ] ) -
                                      pose[ 0 ][ 1 ] * ( pose[ 1 ][ 0 ] * pose[ 2 ][ 2 ] - pose[ 1 ][ 2 ] * pose[ 2 ][ 0 ] ) +
                                      pose[ 0 ][ 2 ] * ( pose[ 1 ][ 0 ] * pose[ 2 ][ 1 ] - pose[ 1 ][ 1 ] * pose[ 2 ][ 0 ] ) };

        entry.m_mirrored = determinant < 0.0f;
    }

    flags |= ( engine_base ? ROUTE_ENGINE_BASE : ROUTE_TAKEN_OVER ) | static_cast< i32 >( this->m_job - this->m_jobs ) << ROUTE_JOB_SHIFT |
             static_cast< i32 >( instance ) << ROUTE_INSTANCE_SHIFT;
    return true;
}

void model_render::add_in_place( u32 instance, const color_t &color, shade_t shade, bool wireframe )
{
    instance_t &entry{ this->m_job->m_instances[ instance ] };
    Assert( entry.m_surface_count < 2 );

    entry.m_surfaces[ entry.m_surface_count++ ] = surface_t{ color, shade, wireframe };
    entry.m_lit |= shade != SHADE_FLAT;
}

u32 model_render::find_instance( i32 index ) const
{
    const latest_t &latest{ this->m_latest[ index ] };
    return latest.m_serial == this->m_serial ? latest.m_instance : INVALID_INSTANCE;
}

void model_render::note_lod( const DrawModelInfo_t *info )
{
    if ( !info || !info->m_pClientEntity || !info->m_pStudioHdr )
    {
        return;
    }

    IClientUnknown *unknown{ static_cast< IClientRenderable * >( info->m_pClientEntity )->GetIClientUnknown( ) };
    C_BaseEntity   *entity{ unknown ? unknown->GetBaseEntity( ) : nullptr };
    const i32       index{ entity ? entity->entindex( ) : -1 };

    if ( index >= 0 && index < MAX_EDICTS )
    {
        this->m_engine_lods[ index ] = engine_lod_t{ this->m_serial, info->m_pStudioHdr, info->m_Lod };
    }
}

bool model_render::add_instance( C_BaseEntity *entity, const matrix3x4 *bone_to_world, i32 bone_count, u32 &instance )
{
    Assert( this->m_job != nullptr );

    const model_t *model{ entity->GetModel( ) };
    if ( !model )
    {
        return false;
    }

    const MDLHandle_t handle{ g_tf2.m_model_info->GetCacheHandle( model ) };
    if ( handle == MDLHANDLE_INVALID )
    {
        return false;
    }

    const studiohdr_t    *header{ g_tf2.m_mdl_cache->GetStudioHdr( handle ) };
    const studiohwdata_t *hardware{ g_tf2.m_mdl_cache->GetHardwareData( handle ) };
    if ( !header || !hardware )
    {
        return false;
    }

    const i32 index{ entity->entindex( ) };
    Assert( index > 0 && index < MAX_EDICTS );

    const engine_lod_t &engine{ this->m_engine_lods[ index ] };
    const i32           lod{ engine.m_serial == this->m_serial && engine.m_header == header
                                 ? math::clamp( engine.m_lod, hardware->m_RootLOD, hardware->m_NumLODs - 1 )
                                 : hardware->m_RootLOD };

    if ( !bone_to_world )
    {
        if ( !entity->SetupBones( nullptr, -1, BONE_USED_BY_VERTEX_LOD0 << lod, g_tf2.m_globals->curtime ) )
        {
            return false;
        }

        const bone_cache_t &cache{ entity->As< C_BaseAnimating * >( )->m_CachedBoneData( ) };
        bone_to_world = cache.m_bones;
        bone_count    = cache.m_count;
    }

    return this->push_instance( *this->m_job, header, hardware, entity->GetBody( ), lod, bone_to_world, bone_count, instance );
}

void model_render::add_layer( usize layer, u32 instance, const color_t &color, shade_t shade, bool wireframe )
{
    Assert( this->m_job != nullptr && layer < LAYER_COUNT );

    this->m_job->m_layers[ layer ].push_back( layer_t{ instance, surface_t{ color, shade, wireframe } } );
    this->m_job->m_instances[ instance ].m_lit |= shade != SHADE_FLAT;
}

void model_render::submit( IMatRenderContext *render_context, bool stencil_cleared )
{
    Assert( this->m_job != nullptr );

    this->m_job->m_clear_stencil = !stencil_cleared;
    queue( render_context, this->m_job->m_layers_phase );
}

void model_render::add_glow( u32 instance, const config::glow_profile_t &profile, const color_t &color )
{
    Assert( this->m_job != nullptr );

    const i32    radius{ profile.m_stencil > 0 ? ( profile.m_stencil + 1 ) / 2 : 0 };
    const float  blur{ profile.m_blur > 0 ? static_cast< float >( profile.m_blur ) : 0.0f };
    const glow_t glow{ instance, color, radius, blur };
    instance_t  &entry{ this->m_job->m_instances[ instance ] };

    if ( entry.m_placement != PLACE_WORLD )
    {
        entry.m_glow    = glow;
        entry.m_glowing = true;
        return;
    }

    this->m_job->m_glows.push_back( glow );
}

void model_render::submit_glow( IMatRenderContext *render_context )
{
    Assert( this->m_job != nullptr );

    if ( this->m_job->m_glows.empty( ) )
    {
        return;
    }

    queue( render_context, this->m_job->m_glow_phase );
}

void model_render::end_scene( )
{
    this->m_routing = false;
}

void model_render::finish_view( )
{
    if ( !this->m_job )
    {
        return;
    }

    job_t &job{ *this->m_job };
    bool   owed{ };

    for ( usize at{ job.m_instances.size( ) }; at-- > 0 && job.m_instances[ at ].m_view == job.m_view; )
    {
        const instance_t &instance{ job.m_instances[ at ] };
        owed |= instance.m_surface_count && instance.m_surfaces[ 0 ].blends( );
    }

    ++job.m_view;

    if ( !owed )
    {
        return;
    }

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };
    queue( render_context, job.m_view_phase );
    render_context->Release( );
}

void model_render::on_device_lost( )
{
    this->m_state_block.Release( );
    this->release_glow_targets( );
}

bool model_render::draw_in_place( const DrawModelInfo_t &info, i32 flags )
{
    job_t      &job{ this->m_jobs[ ( flags >> ROUTE_JOB_SHIFT ) & 0x3 ] };
    const usize index{ static_cast< usize >( ( flags >> ROUTE_INSTANCE_SHIFT ) & ROUTE_INSTANCE_MASK ) };
    Assert( index < job.m_instances.size( ) );

    instance_t &instance{ job.m_instances[ index ] };
    const bool  engine_base{ ( flags & ROUTE_ENGINE_BASE ) != 0 };

    instance.m_lod = info.m_Lod;

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };
    const bool         queued{ render_context->GetCallQueue( ) != nullptr };
    float              eye[ 4 ]{ };

    if ( !queued )
    {
        view_projection( render_context, instance.m_view_projection, eye );
    }

    render_context->Release( );

    if ( queued )
    {
        return false;
    }

    if ( this->drawn_before( job, index, instance ) )
    {
        instance.m_drawn = true;
        return true;
    }

    const render::d3d9::device9 device{ g_render.device( ) };
    if ( !device || !this->begin_draw( device ) )
    {
        return false;
    }

    const float time[ 4 ]{ job.m_time, 0.0f, 0.0f, 0.0f };

    device.SetVertexShaderConstantF( VIEW_PROJECTION_REGISTER, instance.m_view_projection[ 0 ], 4 );
    device.SetVertexShaderConstantF( EYE_REGISTER, eye, 1 );
    device.SetPixelShaderConstantF( TIME_REGISTER, time, 1 );
    device.SetRenderState( render::d3d9::D3DRS_ZFUNC, render::d3d9::D3DCMP_LESSEQUAL );
    device.SetRenderState( render::d3d9::D3DRS_STENCILENABLE, 0 );

    render::d3d9::D3DVIEWPORT9 viewport{ };

    if ( instance.m_placement != PLACE_WORLD )
    {
        device.GetViewport( &viewport );

        render::d3d9::D3DVIEWPORT9 placed{ viewport };
        placed.min_z = 0.0f;
        placed.max_z = instance.m_placement == PLACE_VIEWMODEL ? VIEWMODEL_DEPTH_MAX : 1.0f;
        device.SetViewport( &placed );

        if ( instance.m_mirrored )
        {
            device.SetRenderState( render::d3d9::D3DRS_CULLMODE, render::d3d9::D3DCULL_CW );
        }
    }

    instance.m_deferred = instance.m_placement == PLACE_WORLD && instance.m_surfaces[ 0 ].blends( );

    if ( instance.m_deferred )
    {
        device.SetRenderState( render::d3d9::D3DRS_ZWRITEENABLE, 1 );
        this->apply_surface( device, instance.m_surfaces[ 0 ] );
        device.SetRenderState( render::d3d9::D3DRS_COLORWRITEENABLE, 0 );
        this->draw_instance( device, job, instance );
    } else
    {
        for ( u32 index{ }; index < instance.m_surface_count; ++index )
        {
            const bool base{ index == 0 && !engine_base };

            device.SetRenderState( render::d3d9::D3DRS_ZWRITEENABLE, base );
            set_decal_bias( device, !base );
            this->apply_surface( device, instance.m_surfaces[ index ] );

            if ( base && instance.m_surfaces[ 0 ].blends( ) )
            {
                this->draw_depth_first( device, job, instance );
            } else
            {
                this->draw_instance( device, job, instance );
            }
        }
    }

    this->m_state_block.Apply( );

    if ( instance.m_placement != PLACE_WORLD )
    {
        device.SetViewport( &viewport );
    }

    instance.m_drawn = true;

    if ( instance.m_glowing )
    {
        bool clear_stencil{ true };

        for ( usize earlier{ }; earlier < index && clear_stencil; ++earlier )
        {
            const instance_t &other{ job.m_instances[ earlier ] };
            clear_stencil = !other.m_glowing || !other.m_drawn || other.m_placement != instance.m_placement ||
                            __builtin_memcmp( &other.m_view_projection, &instance.m_view_projection, sizeof( VMatrix ) ) != 0;
        }

        this->draw_glows( device, job, &instance.m_glow, 1, clear_stencil, instance.m_view_projection, eye );
    }

    return true;
}

bool model_render::drawn_before( const job_t &job, usize index, const instance_t &instance ) const
{
    const pose_t *poses{ job.m_poses.data( ) };
    const usize   pose_bytes{ static_cast< usize >( instance.m_header->numbones ) * sizeof( pose_t ) };

    for ( usize earlier{ }; earlier < index; ++earlier )
    {
        const instance_t &other{ job.m_instances[ earlier ] };

        if ( !other.m_drawn || other.m_view != instance.m_view || other.m_header != instance.m_header || other.m_body != instance.m_body ||
             other.m_lod != instance.m_lod || __builtin_memcmp( &other.m_view_projection, &instance.m_view_projection, sizeof( VMatrix ) ) != 0 )
        {
            continue;
        }

        if ( __builtin_memcmp( poses + other.m_first_pose, poses + instance.m_first_pose, pose_bytes ) == 0 )
        {
            return true;
        }
    }

    return false;
}

bool model_render::push_instance( job_t                &job,
                                  const studiohdr_t    *header,
                                  const studiohwdata_t *hardware,
                                  i32                   body,
                                  i32                   lod,
                                  const matrix3x4      *bone_to_world,
                                  i32                   bone_count,
                                  u32                  &instance )
{
    if ( hardware->m_NumLODs <= 0 || bone_count < header->numbones )
    {
        return false;
    }

    const bool hardware_skinned{ for_each_group( header, hardware, body, lod, []( const studiomeshgroup_t &group )
    {
        return ( group.m_Flags & MESHGROUP_IS_HWSKINNED ) != 0;
    } ) };

    if ( !hardware_skinned )
    {
        return false;
    }

    const u32 first_pose{ static_cast< u32 >( job.m_poses.size( ) ) };

    if ( !job.m_poses.resize( first_pose + static_cast< usize >( header->numbones ) ) )
    {
        return false;
    }

    for ( i32 bone{ }; bone < header->numbones; ++bone )
    {
        math::concat_transforms( bone_to_world[ bone ], header->pBone( bone )->poseToBone,
                                 job.m_poses[ first_pose + static_cast< usize >( bone ) ].m_matrix );
    }

    if ( header->flags & STUDIOHDR_FLAGS_STATIC_PROP )
    {
        atl::memcpy( job.m_poses[ first_pose ].m_matrix, bone_to_world[ 0 ], sizeof( matrix3x4 ) );
    }

    instance_t entry{ };
    entry.m_header     = header;
    entry.m_hardware   = hardware;
    entry.m_body       = body;
    entry.m_lod        = lod;
    entry.m_first_pose = first_pose;
    entry.m_view       = job.m_view;

    instance = static_cast< u32 >( job.m_instances.size( ) );
    return job.m_instances.push_back( entry );
}

void model_render::queue( IMatRenderContext *render_context, phase_t &phase )
{
    if ( ICallQueue *queue{ render_context->GetCallQueue( ) } )
    {
        queue->QueueFunctor( &phase );
    } else
    {
        phase( );
    }
}

void model_render::draw_view( job_t &job )
{
    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };
    const bool         queued{ render_context->GetCallQueue( ) != nullptr };
    VMatrix            view{ };
    float              eye[ 4 ]{ };

    if ( !queued )
    {
        view_projection( render_context, view, eye );
    }

    render_context->Release( );

    const render::d3d9::device9 device{ g_render.device( ) };
    if ( queued || !device || !this->begin_draw( device ) )
    {
        return;
    }

    const float time[ 4 ]{ job.m_time, 0.0f, 0.0f, 0.0f };

    device.SetVertexShaderConstantF( VIEW_PROJECTION_REGISTER, view[ 0 ], 4 );
    device.SetVertexShaderConstantF( EYE_REGISTER, eye, 1 );
    device.SetPixelShaderConstantF( TIME_REGISTER, time, 1 );
    device.SetRenderState( render::d3d9::D3DRS_ZFUNC, render::d3d9::D3DCMP_LESSEQUAL );
    device.SetRenderState( render::d3d9::D3DRS_ZWRITEENABLE, 0 );
    device.SetRenderState( render::d3d9::D3DRS_STENCILENABLE, 0 );

    for ( instance_t &instance : job.m_instances )
    {
        if ( !instance.m_deferred || __builtin_memcmp( &instance.m_view_projection, &view, sizeof( VMatrix ) ) != 0 )
        {
            continue;
        }

        for ( u32 index{ }; index < instance.m_surface_count; ++index )
        {
            this->apply_surface( device, instance.m_surfaces[ index ] );
            this->draw_instance( device, job, instance );
        }

        instance.m_deferred = false;
    }

    this->m_state_block.Apply( );
}

void model_render::draw( job_t &job )
{
    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };
    const bool         queued{ render_context->GetCallQueue( ) != nullptr };
    VMatrix            main_view{ };
    float              eye[ 4 ]{ };

    if ( !queued )
    {
        view_projection( render_context, main_view, eye );
    }

    render_context->Release( );

    const render::d3d9::device9 device{ g_render.device( ) };
    if ( queued || !device || !this->begin_draw( device ) )
    {
        return;
    }

    const float time[ 4 ]{ job.m_time, 0.0f, 0.0f, 0.0f };

    device.SetVertexShaderConstantF( VIEW_PROJECTION_REGISTER, main_view[ 0 ], 4 );
    device.SetVertexShaderConstantF( EYE_REGISTER, eye, 1 );
    device.SetPixelShaderConstantF( TIME_REGISTER, time, 1 );

    if ( job.m_clear_stencil )
    {
        device.Clear( render::d3d9::D3DCLEAR_STENCIL, 0, 1.0f, 0 );
    }

    device.SetRenderState( render::d3d9::D3DRS_STENCILENABLE, 1 );
    device.SetRenderState( render::d3d9::D3DRS_STENCILMASK, 0xFF );

    device.SetRenderState( render::d3d9::D3DRS_COLORWRITEENABLE, 0 );
    set_layer_state( device, LAYER_VISIBLE_OVERLAY );

    for ( const instance_t &instance : job.m_instances )
    {
        if ( instance.m_drawn && __builtin_memcmp( &instance.m_view_projection, &main_view, sizeof( VMatrix ) ) == 0 )
        {
            device.SetRenderState( render::d3d9::D3DRS_FILLMODE,
                                   instance.m_surfaces[ 0 ].m_wireframe ? render::d3d9::D3DFILL_WIREFRAME : render::d3d9::D3DFILL_SOLID );
            this->draw_instance( device, job, instance );
        }
    }

    device.SetRenderState( render::d3d9::D3DRS_COLORWRITEENABLE, 0x7 );

    for ( usize layer{ }; layer < LAYER_COUNT; ++layer )
    {
        if ( job.m_layers[ layer ].empty( ) )
        {
            continue;
        }

        set_layer_state( device, layer );

        for ( const layer_t &entry : job.m_layers[ layer ] )
        {
            const instance_t &instance{ job.m_instances[ entry.m_instance ] };

            if ( instance.m_surface_count && !instance.m_drawn )
            {
                continue;
            }

            this->apply_surface( device, entry.m_surface );

            if ( layer == LAYER_VISIBLE_BASE && entry.m_surface.blends( ) )
            {
                this->draw_depth_first( device, job, instance );
            } else
            {
                this->draw_instance( device, job, instance );
            }
        }
    }

    this->m_state_block.Apply( );
}

void model_render::draw_glow( job_t &job )
{
    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };
    const bool         queued{ render_context->GetCallQueue( ) != nullptr };
    VMatrix            main_view{ };
    float              eye[ 4 ]{ };

    if ( !queued )
    {
        view_projection( render_context, main_view, eye );
    }

    render_context->Release( );

    const render::d3d9::device9 device{ g_render.device( ) };
    if ( queued || !device )
    {
        return;
    }

    this->draw_glows( device, job, job.m_glows.data( ), job.m_glows.size( ), true, main_view, eye );
}

void model_render::draw_glows( const render::d3d9::device9 &device,
                               const job_t                 &job,
                               const glow_t                *glows,
                               usize                        count,
                               bool                         clear_stencil,
                               const VMatrix               &view_projection,
                               const float ( &eye )[ 4 ] )
{
    render::d3d9::surface9 frame{ };
    render::d3d9::surface9 depth{ };
    device.GetRenderTarget( 0, &frame );
    device.GetDepthStencilSurface( &depth );

    render::d3d9::D3DSURFACE_DESC description{ };
    render::d3d9::D3DVIEWPORT9    viewport{ };

    if ( frame && depth && frame.GetDesc( &description ) >= render::d3d9::D3D_OK &&
         device.GetViewport( &viewport ) >= render::d3d9::D3D_OK && this->begin_draw( device ) )
    {
        glow_targets_t &targets{ this->m_glow_targets[ job.m_instances[ glows[ 0 ].m_instance ].m_placement == PLACE_PREVIEW ] };

        if ( this->ensure_glow_targets( device, targets, description.width, description.height ) )
        {
            device.SetVertexShaderConstantF( VIEW_PROJECTION_REGISTER, view_projection[ 0 ], 4 );
            device.SetVertexShaderConstantF( EYE_REGISTER, eye, 1 );

            if ( clear_stencil )
            {
                device.Clear( render::d3d9::D3DCLEAR_STENCIL, 0, 1.0f, 0 );
            }

            device.SetRenderState( render::d3d9::D3DRS_COLORWRITEENABLE, 0 );
            device.SetRenderState( render::d3d9::D3DRS_ZFUNC, render::d3d9::D3DCMP_ALWAYS );
            device.SetRenderState( render::d3d9::D3DRS_ZWRITEENABLE, 0 );
            device.SetRenderState( render::d3d9::D3DRS_ALPHABLENDENABLE, 0 );
            device.SetRenderState( render::d3d9::D3DRS_FILLMODE, render::d3d9::D3DFILL_SOLID );
            device.SetRenderState( render::d3d9::D3DRS_STENCILENABLE, 1 );
            device.SetRenderState( render::d3d9::D3DRS_STENCILFUNC, render::d3d9::D3DCMP_ALWAYS );
            device.SetRenderState( render::d3d9::D3DRS_STENCILREF, 1 );
            device.SetRenderState( render::d3d9::D3DRS_STENCILPASS, render::d3d9::D3DSTENCILOP_REPLACE );
            device.SetRenderState( render::d3d9::D3DRS_STENCILFAIL, render::d3d9::D3DSTENCILOP_KEEP );
            device.SetRenderState( render::d3d9::D3DRS_STENCILZFAIL, render::d3d9::D3DSTENCILOP_KEEP );
            device.SetRenderState( render::d3d9::D3DRS_STENCILMASK, 0xFF );
            device.SetRenderState( render::d3d9::D3DRS_STENCILWRITEMASK, 0xFF );

            for ( usize index{ }; index < count; ++index )
            {
                const instance_t &instance{ job.m_instances[ glows[ index ].m_instance ] };

                device.SetRenderState( render::d3d9::D3DRS_CULLMODE, instance.m_mirrored ? render::d3d9::D3DCULL_CW : render::d3d9::D3DCULL_CCW );
                this->draw_instance( device, job, instance );
            }

            device.SetRenderState( render::d3d9::D3DRS_COLORWRITEENABLE, 0x7 );

            for ( usize index{ }; index < count; ++index )
            {
                const glow_t &settings{ glows[ index ] };
                bool          chained{ };

                for ( usize earlier{ }; earlier < index && !chained; ++earlier )
                {
                    chained = glows[ earlier ].m_radius == settings.m_radius && glows[ earlier ].m_blur == settings.m_blur;
                }

                if ( chained )
                {
                    continue;
                }

                device.SetDepthStencilSurface( nullptr );
                device.SetRenderTarget( 0, &targets.m_surfaces[ 0 ] );
                device.Clear( render::d3d9::D3DCLEAR_TARGET, 0, 1.0f, 0 );
                device.SetRenderState( render::d3d9::D3DRS_ZENABLE, 0 );
                device.SetRenderState( render::d3d9::D3DRS_STENCILENABLE, 0 );
                device.SetRenderState( render::d3d9::D3DRS_ALPHABLENDENABLE, 0 );
                device.SetPixelShader( &this->m_pixel_shaders[ SHADE_FLAT ] );

                for ( usize member{ index }; member < count; ++member )
                {
                    const glow_t &glow{ glows[ member ] };
                    if ( glow.m_radius != settings.m_radius || glow.m_blur != settings.m_blur )
                    {
                        continue;
                    }

                    const instance_t &instance{ job.m_instances[ glow.m_instance ] };
                    const float       alpha{ static_cast< float >( glow.m_color.a ) / 255.0f / 255.0f };
                    const float       color[ 4 ]{
                        static_cast< float >( glow.m_color.r ) * alpha,
                        static_cast< float >( glow.m_color.g ) * alpha,
                        static_cast< float >( glow.m_color.b ) * alpha,
                        1.0f,
                    };

                    device.SetRenderState( render::d3d9::D3DRS_CULLMODE, instance.m_mirrored ? render::d3d9::D3DCULL_CW : render::d3d9::D3DCULL_CCW );
                    device.SetPixelShaderConstantF( 0, color, 1 );
                    this->draw_instance( device, job, instance );
                }

                this->draw_glow_chain( device, targets, settings, frame, depth, viewport );
            }
        }

        device.SetRenderTarget( 0, &frame );
        device.SetDepthStencilSurface( &depth );
        this->m_state_block.Apply( );
    }

    frame.Release( );
    depth.Release( );
}

void model_render::draw_glow_chain( const render::d3d9::device9      &device,
                                    const glow_targets_t             &targets,
                                    const glow_t                     &settings,
                                    const render::d3d9::surface9     &frame,
                                    const render::d3d9::surface9     &depth,
                                    const render::d3d9::D3DVIEWPORT9 &viewport )
{
    struct pass_t
    {
        const render::d3d9::object9 *m_shader{ };
        float                        m_step_x{ };
        float                        m_step_y{ };
        float                        m_radius{ };
        u32                          m_filter{ };
    };

    const float texel_x{ 1.0f / static_cast< float >( targets.m_width ) };
    const float texel_y{ 1.0f / static_cast< float >( targets.m_height ) };
    const float radius{ static_cast< float >( settings.m_radius ) };

    pass_t passes[ 4 ]{ };
    usize  count{ };

    if ( settings.m_radius > 0 )
    {
        passes[ count++ ] = pass_t{ &this->m_dilate_shader, texel_x, 0.0f, radius, render::d3d9::D3DTEXF_POINT };
        passes[ count++ ] = pass_t{ &this->m_dilate_shader, 0.0f, texel_y, radius, render::d3d9::D3DTEXF_POINT };
    }

    if ( settings.m_blur > 0.0f )
    {
        passes[ count++ ] = pass_t{ &this->m_blur_shader, texel_x * GLOW_BLUR_SPACING, 0.0f, 0.0f, render::d3d9::D3DTEXF_LINEAR };
        passes[ count++ ] = pass_t{ &this->m_blur_shader, 0.0f, texel_y * GLOW_BLUR_SPACING, 0.0f, render::d3d9::D3DTEXF_LINEAR };
    }

    Assert( count >= 2 );

    device.SetVertexShader( nullptr );
    device.SetFVF( render::d3d9::D3DFVF_OVERLAY_VERTEX );
    device.SetRenderState( render::d3d9::D3DRS_CULLMODE, render::d3d9::D3DCULL_NONE );
    device.SetSamplerState( 0, render::d3d9::D3DSAMP_ADDRESSU, render::d3d9::D3DTADDRESS_CLAMP );
    device.SetSamplerState( 0, render::d3d9::D3DSAMP_ADDRESSV, render::d3d9::D3DTADDRESS_CLAMP );
    device.SetSamplerState( 0, render::d3d9::D3DSAMP_MIPFILTER, render::d3d9::D3DTEXF_NONE );
    device.SetSamplerState( 0, render::d3d9::D3DSAMP_SRGBTEXTURE, 0 );

    usize source{ };

    for ( usize index{ }; index < count; ++index )
    {
        const pass_t &pass{ passes[ index ] };
        const bool    last{ index + 1 == count };

        device.SetTexture( 0, &targets.m_textures[ source ] );

        if ( last )
        {
            device.SetRenderTarget( 0, &frame );
            device.SetDepthStencilSurface( &depth );
            device.SetViewport( &viewport );
            device.SetRenderState( render::d3d9::D3DRS_STENCILENABLE, 1 );
            device.SetRenderState( render::d3d9::D3DRS_STENCILFUNC, render::d3d9::D3DCMP_EQUAL );
            device.SetRenderState( render::d3d9::D3DRS_STENCILREF, 0 );
            device.SetRenderState( render::d3d9::D3DRS_STENCILPASS, render::d3d9::D3DSTENCILOP_KEEP );
            device.SetRenderState( render::d3d9::D3DRS_STENCILWRITEMASK, 0 );
            device.SetRenderState( render::d3d9::D3DRS_ALPHABLENDENABLE, 1 );
            device.SetRenderState( render::d3d9::D3DRS_SRCBLEND, render::d3d9::D3DBLEND_ONE );
            device.SetRenderState( render::d3d9::D3DRS_DESTBLEND, render::d3d9::D3DBLEND_ONE );
        } else
        {
            device.SetRenderTarget( 0, &targets.m_surfaces[ source ^ 1 ] );
        }

        const float scale{ last && settings.m_blur > 0.0f ? settings.m_blur : 1.0f };
        const float constants[ PIXEL_REGISTER_COUNT ][ 4 ]{
            { pass.m_step_x, pass.m_step_y, pass.m_radius, 0.0f },
            { scale, scale, scale, scale },
        };

        device.SetSamplerState( 0, render::d3d9::D3DSAMP_MAGFILTER, pass.m_filter );
        device.SetSamplerState( 0, render::d3d9::D3DSAMP_MINFILTER, pass.m_filter );
        device.SetPixelShader( pass.m_shader );
        device.SetPixelShaderConstantF( 0, constants[ 0 ], PIXEL_REGISTER_COUNT );

        const render::d3d9::D3DVIEWPORT9 target{ last ? viewport : render::d3d9::D3DVIEWPORT9{ 0, 0, targets.m_width, targets.m_height, 0.0f, 1.0f } };
        const float                      left{ static_cast< float >( target.x ) - 0.5f };
        const float                      top{ static_cast< float >( target.y ) - 0.5f };
        const float                      right{ left + static_cast< float >( target.width ) };
        const float                      bottom{ top + static_cast< float >( target.height ) };

        const render::d3d9::overlay_vertex vertices[ 4 ]{
            { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0.0f, 0.0f },
            { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 1.0f, 0.0f },
            { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0.0f, 1.0f },
            { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 1.0f, 1.0f },
        };

        device.DrawPrimitiveUP( render::d3d9::D3DPT_TRIANGLESTRIP, 2, vertices, sizeof( render::d3d9::overlay_vertex ) );

        source ^= 1;
    }

    device.SetTexture( 0, nullptr );
}

bool model_render::begin_draw( const render::d3d9::device9 &device )
{
    if ( !this->ensure_resources( device ) || this->m_state_block.Capture( ) < render::d3d9::D3D_OK )
    {
        return false;
    }

    device.SetPixelShader( &this->m_pixel_shaders[ SHADE_FLAT ] );
    device.SetRenderState( render::d3d9::D3DRS_ZENABLE, 1 );
    device.SetRenderState( render::d3d9::D3DRS_CULLMODE, render::d3d9::D3DCULL_CCW );
    device.SetRenderState( render::d3d9::D3DRS_DEPTHBIAS, 0 );
    device.SetRenderState( render::d3d9::D3DRS_SLOPESCALEDEPTHBIAS, 0 );
    device.SetRenderState( render::d3d9::D3DRS_ALPHATESTENABLE, 0 );
    device.SetRenderState( render::d3d9::D3DRS_FOGENABLE, 0 );
    device.SetRenderState( render::d3d9::D3DRS_SCISSORTESTENABLE, 0 );
    device.SetRenderState( render::d3d9::D3DRS_CLIPPLANEENABLE, 0 );
    device.SetRenderState( render::d3d9::D3DRS_SRGBWRITEENABLE, 0 );
    device.SetRenderState( render::d3d9::D3DRS_COLORWRITEENABLE, 0x7 );
    device.SetRenderState( render::d3d9::D3DRS_SRCBLEND, render::d3d9::D3DBLEND_SRCALPHA );
    device.SetRenderState( render::d3d9::D3DRS_DESTBLEND, render::d3d9::D3DBLEND_INVSRCALPHA );
    device.SetRenderState( render::d3d9::D3DRS_BLENDOP, render::d3d9::D3DBLENDOP_ADD );
    return true;
}

bool model_render::ensure_resources( const render::d3d9::device9 &device )
{
    if ( this->m_device != device.Get( ) )
    {
        this->release_resources( );
        this->m_device = device.Get( );
    }

    if ( !this->m_pixel_shaders[ SHADE_FLAT ] )
    {
        bool created{ device.CreateVertexShader( render::shaders::MODEL_SKINNED_VERTEX_SHADER, &this->m_skinned_shader ) >= render::d3d9::D3D_OK &&
                      device.CreateVertexShader( render::shaders::MODEL_RIGID_VERTEX_SHADER, &this->m_rigid_shader ) >= render::d3d9::D3D_OK &&
                      device.CreateVertexShader( render::shaders::MODEL_SKINNED_LIT_VERTEX_SHADER, &this->m_skinned_lit_shader ) >= render::d3d9::D3D_OK &&
                      device.CreateVertexShader( render::shaders::MODEL_RIGID_LIT_VERTEX_SHADER, &this->m_rigid_lit_shader ) >= render::d3d9::D3D_OK &&
                      device.CreateVertexDeclaration( RIGID_ELEMENTS, &this->m_rigid_declaration ) >= render::d3d9::D3D_OK &&
                      device.CreateVertexDeclaration( FLOAT_WEIGHT_ELEMENTS, &this->m_float_declaration ) >= render::d3d9::D3D_OK &&
                      device.CreateVertexDeclaration( SHORT_WEIGHT_ELEMENTS, &this->m_short_declaration ) >= render::d3d9::D3D_OK &&
                      device.CreatePixelShader( render::shaders::GLOW_DILATE_PIXEL_SHADER, &this->m_dilate_shader ) >= render::d3d9::D3D_OK &&
                      device.CreatePixelShader( render::shaders::GLOW_BLUR_PIXEL_SHADER, &this->m_blur_shader ) >= render::d3d9::D3D_OK };

        for ( usize index{ }; index < 8 && created; ++index )
        {
            render::d3d9::D3DVERTEXELEMENT9 elements[ 5 ]{ };
            lit_elements( index, elements );
            created = device.CreateVertexDeclaration( elements, &this->m_lit_declarations[ index ] ) >= render::d3d9::D3D_OK;
        }

        for ( usize shade{ SHADE_COUNT }; shade-- > 0 && created; )
        {
            created = device.CreatePixelShader( PIXEL_SHADERS[ shade ], &this->m_pixel_shaders[ shade ] ) >= render::d3d9::D3D_OK;
        }

        if ( !created )
        {
            this->release_resources( );
            return false;
        }
    }

    if ( this->m_state_block )
    {
        return true;
    }

    if ( device.BeginStateBlock( ) < render::d3d9::D3D_OK )
    {
        return false;
    }

    const float                registers[ VERTEX_REGISTER_COUNT ][ 4 ]{ };
    render::d3d9::D3DVIEWPORT9 viewport{ };
    device.GetViewport( &viewport );

    device.SetVertexShader( nullptr );
    device.SetPixelShader( nullptr );
    device.SetVertexDeclaration( nullptr );
    device.SetStreamSource( 0, nullptr, 0, 0 );
    device.SetIndices( nullptr );
    device.SetVertexShaderConstantF( 0, registers[ 0 ], VERTEX_REGISTER_COUNT );
    device.SetPixelShaderConstantF( 0, registers[ 0 ], PIXEL_REGISTER_COUNT );
    device.SetTexture( 0, nullptr );
    device.SetViewport( &viewport );

    for ( const u32 state : RECORDED_STATES )
    {
        device.SetRenderState( state, 0 );
    }

    for ( const u32 state : RECORDED_SAMPLER_STATES )
    {
        device.SetSamplerState( 0, state, 0 );
    }

    return device.EndStateBlock( &this->m_state_block ) >= render::d3d9::D3D_OK;
}

bool model_render::ensure_glow_targets( const render::d3d9::device9 &device, glow_targets_t &targets, u32 width, u32 height )
{
    if ( targets.m_textures[ 1 ] && targets.m_width == width && targets.m_height == height )
    {
        return true;
    }

    targets.release( );

    for ( usize index{ }; index < 2; ++index )
    {
        if ( device.CreateTexture( width, height, 1, render::d3d9::D3DUSAGE_RENDERTARGET, render::d3d9::D3DFMT_A8R8G8B8,
                                   render::d3d9::D3DPOOL_DEFAULT, &targets.m_textures[ index ] ) < render::d3d9::D3D_OK ||
             targets.m_textures[ index ].GetSurfaceLevel( 0, &targets.m_surfaces[ index ] ) < render::d3d9::D3D_OK )
        {
            targets.release( );
            return false;
        }
    }

    targets.m_width  = width;
    targets.m_height = height;
    return true;
}

void model_render::release_glow_targets( )
{
    for ( glow_targets_t &targets : this->m_glow_targets )
    {
        targets.release( );
    }
}

void model_render::release_resources( )
{
    this->release_glow_targets( );
    this->m_state_block.Release( );
    this->m_skinned_shader.Release( );
    this->m_rigid_shader.Release( );
    this->m_skinned_lit_shader.Release( );
    this->m_rigid_lit_shader.Release( );
    this->m_dilate_shader.Release( );
    this->m_blur_shader.Release( );
    this->m_rigid_declaration.Release( );
    this->m_float_declaration.Release( );
    this->m_short_declaration.Release( );

    for ( render::d3d9::object9 &declaration : this->m_lit_declarations )
    {
        declaration.Release( );
    }

    for ( render::d3d9::object9 &shader : this->m_pixel_shaders )
    {
        shader.Release( );
    }
}

void model_render::apply_surface( const render::d3d9::device9 &device, const surface_t &surface ) const
{
    const float constant[ 4 ]{
        static_cast< float >( surface.m_color.r ) / 255.0f,
        static_cast< float >( surface.m_color.g ) / 255.0f,
        static_cast< float >( surface.m_color.b ) / 255.0f,
        static_cast< float >( surface.m_color.a ) / 255.0f,
    };

    const bool additive{ surface.additive( ) };

    device.SetPixelShader( &this->m_pixel_shaders[ surface.m_shade ] );
    device.SetPixelShaderConstantF( 0, constant, 1 );

    device.SetRenderState( render::d3d9::D3DRS_ALPHABLENDENABLE, surface.blends( ) );
    device.SetRenderState( render::d3d9::D3DRS_DESTBLEND, additive ? render::d3d9::D3DBLEND_ONE : render::d3d9::D3DBLEND_INVSRCALPHA );
    device.SetRenderState( render::d3d9::D3DRS_FILLMODE, surface.m_wireframe ? render::d3d9::D3DFILL_WIREFRAME : render::d3d9::D3DFILL_SOLID );
}

void model_render::draw_instance( const render::d3d9::device9 &device, const job_t &job, const instance_t &instance ) const
{
    const pose_t *poses{ job.m_poses.data( ) + instance.m_first_pose };

    device.SetVertexShaderConstantF( PALETTE_REGISTER, CLEARED_PALETTE[ 0 ], 3 * MAX_HARDWARE_BONES );

    if ( instance.m_header->numbones == 1 )
    {
        device.SetVertexShaderConstantF( PALETTE_REGISTER, poses[ 0 ].m_matrix[ 0 ], 3 );
    }

    for_each_group( instance.m_header, instance.m_hardware, instance.m_body, instance.m_lod,
                    [ & ]( const studiomeshgroup_t &group )
    {
        this->draw_group( device, group, poses, instance.m_lit );
        return true;
    } );
}

void model_render::draw_depth_first( const render::d3d9::device9 &device, const job_t &job, const instance_t &instance ) const
{
    device.SetRenderState( render::d3d9::D3DRS_COLORWRITEENABLE, 0 );
    this->draw_instance( device, job, instance );

    device.SetRenderState( render::d3d9::D3DRS_COLORWRITEENABLE, 0x7 );
    device.SetRenderState( render::d3d9::D3DRS_ZWRITEENABLE, 0 );
    device.SetRenderState( render::d3d9::D3DRS_ZFUNC, render::d3d9::D3DCMP_EQUAL );
    this->draw_instance( device, job, instance );

    device.SetRenderState( render::d3d9::D3DRS_ZWRITEENABLE, 1 );
    device.SetRenderState( render::d3d9::D3DRS_ZFUNC, render::d3d9::D3DCMP_LESSEQUAL );
}

void model_render::draw_group( const render::d3d9::device9 &device, const studiomeshgroup_t &group, const pose_t *poses, bool lit ) const
{
    if ( group.m_MeshNeedsRestore )
    {
        return;
    }

    group.m_pMesh->HandleLateCreation( );

    const CMeshDX8         *mesh{ group.m_pMesh };
    const CVertexBufferDx8 *vertices{ mesh->m_vertex_buffer };

    if ( !vertices || !mesh->m_index_buffer )
    {
        return;
    }

    const u64  format{ mesh->m_vertex_format };
    const bool skinned{ ( format & VERTEX_BONE_INDEX ) && ( format & VERTEX_BONE_WEIGHT_MASK ) };
    const bool compressed{ ( format & VERTEX_FORMAT_COMPRESSED ) != 0 };

    const float decode[ 4 ]{ 0.0f, compressed ? 1.0f / 32768.0f : 1.0f, compressed ? 1.0f / 32768.0f : 0.0f, compressed ? 1.0f : 0.0f };
    device.SetVertexShaderConstantF( WEIGHT_DECODE_REGISTER, decode, 1 );

    if ( lit && ( format & VERTEX_NORMAL ) )
    {
        device.SetVertexDeclaration( &this->m_lit_declarations[ lit_index( format ) ] );
        device.SetVertexShader( skinned ? &this->m_skinned_lit_shader : &this->m_rigid_lit_shader );
    } else if ( skinned )
    {
        device.SetVertexDeclaration( compressed ? &this->m_short_declaration : &this->m_float_declaration );
        device.SetVertexShader( &this->m_skinned_shader );
    } else
    {
        device.SetVertexDeclaration( &this->m_rigid_declaration );
        device.SetVertexShader( &this->m_rigid_shader );
    }

    device.SetStreamSource( 0, vertices->m_vertex_buffer, 0, static_cast< u32 >( vertices->m_vertex_size ) );
    device.SetIndices( mesh->m_index_buffer->m_index_buffer );

    for ( i32 index{ }; index < group.m_NumStrips; ++index )
    {
        const StripHeader_t &strip{ group.m_pStripData[ index ] };

        for ( i32 change{ }; change < strip.numBoneStateChanges; ++change )
        {
            const BoneStateChangeHeader_t *state{ strip.pBoneStateChange( change ) };

            if ( state->newBoneID < 0 )
            {
                break;
            }

            Assert( state->hardwareID >= 0 && state->hardwareID < MAX_HARDWARE_BONES );

            device.SetVertexShaderConstantF( PALETTE_REGISTER + 3 * static_cast< u32 >( state->hardwareID ),
                                             poses[ state->newBoneID ].m_matrix[ 0 ], 3 );
        }

        const bool triangle_strip{ ( strip.flags & STRIP_IS_TRISTRIP ) != 0 };

        device.DrawIndexedPrimitive( triangle_strip ? render::d3d9::D3DPT_TRIANGLESTRIP : render::d3d9::D3DPT_TRIANGLELIST,
                                     0,
                                     0,
                                     static_cast< u32 >( vertices->m_vertex_count ),
                                     static_cast< u32 >( strip.indexOffset ),
                                     static_cast< u32 >( triangle_strip ? strip.numIndices - 2 : strip.numIndices / 3 ) );
    }
}
