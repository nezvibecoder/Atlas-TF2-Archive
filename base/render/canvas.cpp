#include "render.hpp"
#include "runtime/assert.hpp"
#include "shaders/blur.hpp"
#include "shaders/grade.hpp"

#include "icons.hpp"

namespace render
{
    bool backend::flush_batch( )
    {
        if ( this->m_vertices.empty( ) )
        {
            this->m_batch_type = batch_type::none;
            return true;
        }

        if ( this->m_batch_type == batch_type::none )
        {
            return false;
        }

        if ( !this->setup_batch_state( this->m_batch_type ) )
        {
            return false;
        }

        u32   primitive_type{ this->m_batch_type == batch_type::lines ? d3d9::D3DPT_LINELIST : d3d9::D3DPT_TRIANGLELIST };
        usize vertices_per_primitive{ this->m_batch_type == batch_type::lines ? 2u : 3u };

        if ( this->m_vertices.size( ) % vertices_per_primitive )
        {
            return false;
        }

        usize primitive_count_value{ this->m_vertices.size( ) / vertices_per_primitive };

        if ( primitive_count_value > static_cast< usize >( 0xFFFFFFFFu ) )
        {
            return false;
        }

        u32 primitive_count{ static_cast< u32 >( primitive_count_value ) };
        i32 status{ this->m_device.DrawPrimitiveUP( primitive_type, primitive_count, this->m_vertices.data( ),
                                                    sizeof( d3d9::overlay_vertex ) ) };

        if ( status < d3d9::D3D_OK )
        {
            return false;
        }

        this->m_vertices.clear( );
        this->m_batch_type = batch_type::none;
        return true;
    }

    bool backend::set_scissor( const rect_t &bounds, bool enabled )
    {
        if ( enabled && this->m_device.SetScissorRect( &bounds ) < d3d9::D3D_OK )
        {
            return false;
        }

        return this->m_device.SetRenderState( d3d9::D3DRS_SCISSORTESTENABLE, enabled ? 1u : 0u ) >= d3d9::D3D_OK;
    }

    bool backend::create_texture( u32 width, u32 height, u32 levels, texture_t &texture ) const
    {
        const i32 status{ this->m_device.CreateTexture( width, height, levels, d3d9::D3DUSAGE_DYNAMIC,
                                                        d3d9::D3DFMT_A8R8G8B8, d3d9::D3DPOOL_DEFAULT, &texture ) };

        if ( status < d3d9::D3D_OK || !texture )
        {
            texture.Release( );
            return false;
        }

        return true;
    }

    bool backend::upload_texture( const texture_t &texture, u32 level, const rect_t &area, const u8 *pixels,
                                  usize pitch ) const
    {
        d3d9::D3DLOCKED_RECT locked{ };

        if ( texture.LockRect( level, &locked, &area, 0 ) < d3d9::D3D_OK || !locked.bits )
        {
            return false;
        }

        const usize width{ static_cast< usize >( area.right - area.left ) * 4 };

        for ( i32 y{ }; y < area.bottom - area.top; ++y )
        {
            atl::memcpy( reinterpret_cast< u8 * >( locked.bits ) + static_cast< usize >( y ) * static_cast< usize >( locked.pitch ),
                         pixels + static_cast< usize >( y ) * pitch, width );
        }

        return texture.UnlockRect( level ) >= d3d9::D3D_OK;
    }

    bool backend::setup_render_state( ) const
    {
        this->m_device.SetVertexShader( nullptr );
        this->m_device.SetPixelShader( nullptr );
        this->m_device.SetRenderState( d3d9::D3DRS_ZENABLE, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_ZWRITEENABLE, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_ALPHATESTENABLE, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_LASTPIXEL, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_FILLMODE, d3d9::D3DFILL_SOLID );
        this->m_device.SetRenderState( d3d9::D3DRS_SHADEMODE, d3d9::D3DSHADE_GOURAUD );
        this->m_device.SetRenderState( d3d9::D3DRS_CULLMODE, d3d9::D3DCULL_NONE );
        this->m_device.SetRenderState( d3d9::D3DRS_FOGENABLE, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_STENCILENABLE, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_WRAP0, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_CLIPPING, 1 );
        this->m_device.SetRenderState( d3d9::D3DRS_LIGHTING, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_CLIPPLANEENABLE, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_MULTISAMPLEANTIALIAS, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_SCISSORTESTENABLE, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_ANTIALIASEDLINEENABLE, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_COLORWRITEENABLE, 0x0Fu );
        this->m_device.SetRenderState( d3d9::D3DRS_COLORWRITEENABLE1, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_COLORWRITEENABLE2, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_COLORWRITEENABLE3, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_SRGBWRITEENABLE, 0 );
        this->m_device.SetRenderState( d3d9::D3DRS_ALPHABLENDENABLE, 1 );
        this->m_device.SetRenderState( d3d9::D3DRS_SRCBLEND, d3d9::D3DBLEND_SRCALPHA );
        this->m_device.SetRenderState( d3d9::D3DRS_DESTBLEND, d3d9::D3DBLEND_INVSRCALPHA );
        this->m_device.SetRenderState( d3d9::D3DRS_BLENDOP, d3d9::D3DBLENDOP_ADD );
        this->m_device.SetRenderState( d3d9::D3DRS_SEPARATEALPHABLENDENABLE, 1 );
        this->m_device.SetRenderState( d3d9::D3DRS_SRCBLENDALPHA, d3d9::D3DBLEND_ONE );
        this->m_device.SetRenderState( d3d9::D3DRS_DESTBLENDALPHA, d3d9::D3DBLEND_INVSRCALPHA );
        this->m_device.SetRenderState( d3d9::D3DRS_BLENDOPALPHA, d3d9::D3DBLENDOP_ADD );
        this->m_device.SetTextureStageState( 1, d3d9::D3DTSS_COLOROP, d3d9::D3DTOP_DISABLE );
        this->m_device.SetTextureStageState( 1, d3d9::D3DTSS_ALPHAOP, d3d9::D3DTOP_DISABLE );
        this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_TEXCOORDINDEX, 0 );
        this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_TEXTURETRANSFORMFLAGS, 0 );
        this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_RESULTARG, d3d9::D3DTA_CURRENT );
        this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_ADDRESSU, d3d9::D3DTADDRESS_CLAMP );
        this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_ADDRESSV, d3d9::D3DTADDRESS_CLAMP );
        this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MAGFILTER, d3d9::D3DTEXF_POINT );
        this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT );
        this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_NONE );
        this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_SRGBTEXTURE, 0 );
        this->m_device.SetFVF( d3d9::D3DFVF_OVERLAY_VERTEX );
        return true;
    }

    bool backend::setup_batch_state( batch_type type ) const
    {
        // the model batch leaves an additive blend behind, so every other batch restores it
        if ( type != batch_type::model )
        {
            this->m_device.SetRenderState( d3d9::D3DRS_SRCBLEND, d3d9::D3DBLEND_SRCALPHA );
            this->m_device.SetRenderState( d3d9::D3DRS_DESTBLEND, d3d9::D3DBLEND_INVSRCALPHA );
        }

        if ( type == batch_type::blur )
        {
            if ( !this->m_blur_frame_ready || !this->m_blur_texture_a )
            {
                return false;
            }

            this->m_device.SetTexture( 0, &this->m_blur_texture_a );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLOROP, d3d9::D3DTOP_MODULATE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAOP, d3d9::D3DTOP_SELECTARG2 );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MAGFILTER, d3d9::D3DTEXF_LINEAR );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR );
            return true;
        }

        if ( type == batch_type::model )
        {
            if ( !this->m_model_ready || !this->m_model_texture )
            {
                return false;
            }

            this->m_device.SetTexture( 0, &this->m_model_texture );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLOROP, d3d9::D3DTOP_MODULATE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAOP, d3d9::D3DTOP_SELECTARG2 );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MAGFILTER, d3d9::D3DTEXF_LINEAR );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR );
            this->m_device.SetRenderState( d3d9::D3DRS_SRCBLEND, d3d9::D3DBLEND_SRCALPHA );
            this->m_device.SetRenderState( d3d9::D3DRS_DESTBLEND, d3d9::D3DBLEND_ONE );
            return true;
        }

        if ( type == batch_type::icon_small || type == batch_type::icon_large ||
             type == batch_type::icon_logo || type == batch_type::icon_vacc )
        {
            const icon_atlas_t &atlas{ this->icon_atlas_for( type ) };

            if ( !atlas.m_ready || !atlas.m_texture )
            {
                return false;
            }

            this->m_device.SetTexture( 0, &atlas.m_texture );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLOROP, d3d9::D3DTOP_MODULATE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAOP, d3d9::D3DTOP_MODULATE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MAGFILTER, d3d9::D3DTEXF_POINT );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT );
            return true;
        }

        if ( type == batch_type::avatar )
        {
            if ( !this->m_avatar_bound || !*this->m_avatar_bound )
            {
                return false;
            }

            this->m_device.SetTexture( 0, this->m_avatar_bound );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLOROP, d3d9::D3DTOP_MODULATE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAOP, d3d9::D3DTOP_SELECTARG2 );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MAGFILTER, d3d9::D3DTEXF_LINEAR );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_LINEAR );
            return true;
        }

        if ( type == batch_type::html )
        {
            if ( !this->m_html_bound || !*this->m_html_bound )
            {
                return false;
            }

            this->m_device.SetTexture( 0, this->m_html_bound );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLOROP, d3d9::D3DTOP_MODULATE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAOP, d3d9::D3DTOP_MODULATE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MAGFILTER, d3d9::D3DTEXF_LINEAR );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR );
            return true;
        }

        if ( type == batch_type::text )
        {
            if ( !this->m_active_text_atlas || !this->m_active_text_atlas->m_ready ||
                 !this->m_active_text_atlas->m_texture )
            {
                return false;
            }

            this->m_device.SetTexture( 0, &this->m_active_text_atlas->m_texture );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLOROP, d3d9::D3DTOP_MODULATE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAOP, d3d9::D3DTOP_MODULATE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG1, d3d9::D3DTA_TEXTURE );
            this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG2, d3d9::D3DTA_DIFFUSE );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MAGFILTER, d3d9::D3DTEXF_POINT );
            this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT );
            return true;
        }

        this->m_device.SetTexture( 0, nullptr );
        this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLOROP, d3d9::D3DTOP_SELECTARG1 );
        this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG1, d3d9::D3DTA_DIFFUSE );
        this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAOP, d3d9::D3DTOP_SELECTARG1 );
        this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG1, d3d9::D3DTA_DIFFUSE );
        return true;
    }

    bool backend::capture_state( )
    {
        if ( this->m_state_block )
        {
            if ( this->m_state_block.Capture( ) >= d3d9::D3D_OK )
            {
                return true;
            }

            this->m_state_block.Release( );
            return false;
        }

        i32 status{ this->m_device.CreateStateBlock( d3d9::D3DSBT_ALL, &this->m_state_block ) };

        if ( status >= d3d9::D3D_OK && this->m_state_block )
        {
            return true;
        }

        this->m_state_block.Release( );
        return false;
    }

    void backend::shutdown( )
    {
        this->m_inside_frame         = false;
        this->m_frame_build_failed   = false;
        this->m_last_frame_succeeded = false;
        this->m_state_block.Release( );
        this->release_text_atlases( );
        this->release_icon_atlases( );
        this->release_avatars( );
        this->release_html_textures( );
        this->release_composite_target( );
        this->release_blur_resources( );
        this->release_grade_resources( );
        this->release_model_resources( );
        this->m_frame_commands.reset( );
        this->m_vertices.reset( );
        this->m_batch_type = batch_type::none;
        this->m_fonts.shutdown( );
        this->m_device.Detach( );
        this->m_initialized = false;
    }

    bool backend::start_frame( void *device )
    {
        Assert( this->m_clip_enabled != true );
        this->m_last_frame_succeeded = false;
        this->m_clip_enabled         = false; // never let a clip survive into a new frame

        if ( !this->m_initialized || this->m_inside_frame || !device )
        {
            return false;
        }

        if ( this->m_device && this->m_device.Get( ) != device )
        {
            this->m_state_block.Release( );
            this->release_text_atlases( );
            this->release_icon_atlases( );
            this->release_html_textures( );
            this->release_composite_target( );
            this->release_blur_resources( );
            this->release_grade_resources( );
            this->release_model_resources( );
        }

        this->m_device.Attach( device );

        if ( !this->m_icon_atlases[ icons::KIND_SMALL ].m_ready )
        {
            this->build_icon_atlases( );
        }

        d3d9::D3DVIEWPORT9 viewport{ };
        i32                status{ this->m_device.GetViewport( &viewport ) };

        if ( status < d3d9::D3D_OK || !viewport.width || !viewport.height )
        {
            return false;
        }

        this->m_frame_commands.clear( );
        this->m_frame_build_failed = false;
        this->m_blur_frame_ready   = false;
        this->m_frame_ms           = I( GetTickCount64 )( );
        this->reset_transform( );
        this->m_inside_frame = true;
        return true;
    }

    bool backend::render_frame( )
    {
        if ( !this->m_device )
        {
            return false;
        }

        if ( this->m_frame_commands.empty( ) )
        {
            this->m_overlay.hide( ); // otherwise the window keeps showing the last frame that had commands
            return true;
        }

        d3d9::surface9 original_target{ };
        i32            status{ this->m_device.GetRenderTarget( 0, &original_target ) };

        if ( status < d3d9::D3D_OK || !original_target )
        {
            return false;
        }

        d3d9::D3DSURFACE_DESC target_description{ };
        status = original_target.GetDesc( &target_description );

        if ( status < d3d9::D3D_OK || !target_description.width || !target_description.height )
        {
            original_target.Release( );
            return false;
        }

        d3d9::D3DVIEWPORT9 viewport{ };
        status = this->m_device.GetViewport( &viewport );

        if ( status < d3d9::D3D_OK || !viewport.width || !viewport.height ||
             viewport.x >= target_description.width || viewport.y >= target_description.height ||
             viewport.width > target_description.width - viewport.x ||
             viewport.height > target_description.height - viewport.y )
        {
            original_target.Release( );
            return false;
        }

        this->m_vertices.clear( );
        this->m_batch_type        = batch_type::none;
        this->m_active_text_atlas = nullptr;
        this->m_blur_frame_ready  = false;
        this->m_scissor           = rect_t{ };
        this->m_scissor_enabled   = false; // setup_render_state turns the test off

        usize required_vertices{ };
        bool  blur_requested{ };

        if ( !this->count_vertices( this->m_frame_commands, &required_vertices ) ||
             !this->m_vertices.reserve( required_vertices ) )
        {
            original_target.Release( );
            return false;
        }

        for ( const render_command &command : this->m_frame_commands )
        {
            if ( command.m_type == command_type::blur )
            {
                blur_requested = true;
            }

            if ( command.m_type == command_type::text && !command.m_text.empty( ) )
            {
                text_atlas *atlas{ };
                this->ensure_text_atlas( command, &atlas );
            }
        }

        if ( !this->ensure_composite_target( viewport.width, viewport.height ) )
        {
            original_target.Release( );
            return false;
        }

        bool blur_resources_ready{
            blur_requested &&
            this->ensure_blur_resources( viewport.width, viewport.height,
                                         target_description.format,
                                         target_description.multi_sample_type != 0 )
        };

        d3d9::surface9 original_depth{ };
        i32            depth_status{ this->m_device.GetDepthStencilSurface( &original_depth ) };

        if ( depth_status < d3d9::D3D_OK && depth_status != d3d9::D3DERR_NOTFOUND )
        {
            original_depth.Release( );
            original_target.Release( );
            return false;
        }

        atl::array< d3d9::surface9, 3 > additional_targets{ };
        atl::array< bool, 3 >           additional_targets_unbound{ };
        bool                            render_targets_queried{ true };

        for ( usize index{ }; index < additional_targets.size( ); ++index )
        {
            i32 target_status{ this->m_device.GetRenderTarget( static_cast< u32 >( index + 1 ),
                                                               &additional_targets[ index ] ) };

            if ( target_status < d3d9::D3D_OK && target_status != d3d9::D3DERR_NOTFOUND &&
                 target_status != d3d9::D3DERR_INVALIDCALL )
            {
                render_targets_queried = false;
                break;
            }
        }

        if ( !render_targets_queried || !this->capture_state( ) )
        {
            for ( d3d9::surface9 &additional_target : additional_targets )
            {
                additional_target.Release( );
            }

            original_depth.Release( );
            original_target.Release( );
            return false;
        }

        u32 original_srgb_write{ };

        if ( this->m_device.GetRenderState( d3d9::D3DRS_SRGBWRITEENABLE, &original_srgb_write ) < d3d9::D3D_OK )
        {
            for ( d3d9::surface9 &additional_target : additional_targets )
            {
                additional_target.Release( );
            }

            original_depth.Release( );
            original_target.Release( );
            return false;
        }

        bool depth_unbound{ };
        bool target_bound{ };
        bool frame_status{ this->m_device.ColorFill( &this->m_composite_surface, nullptr, 0x00000000u ) >=
                           d3d9::D3D_OK };

        if ( frame_status )
        {
            frame_status  = this->m_device.SetDepthStencilSurface( nullptr ) >= d3d9::D3D_OK;
            depth_unbound = frame_status;
        }

        if ( frame_status )
        {
            for ( usize index{ }; index < additional_targets.size( ); ++index )
            {
                if ( additional_targets[ index ] )
                {
                    frame_status                        = this->m_device.SetRenderTarget( static_cast< u32 >( index + 1 ), nullptr ) >=
                                                          d3d9::D3D_OK;
                    additional_targets_unbound[ index ] = frame_status;

                    if ( !frame_status )
                    {
                        break;
                    }
                }
            }
        }

        if ( frame_status && blur_resources_ready )
        {
            this->m_blur_frame_ready = this->render_blur( original_target );
            target_bound             = true;
        }

        if ( frame_status )
        {
            bool composite_target_bound{
                this->m_device.SetRenderTarget( 0, &this->m_composite_surface ) >= d3d9::D3D_OK
            };
            frame_status = composite_target_bound;
            target_bound = target_bound || composite_target_bound;
        }

        d3d9::D3DVIEWPORT9 composite_viewport{ 0, 0, viewport.width, viewport.height, 0.0f, 1.0f };

        if ( frame_status )
        {
            frame_status = this->m_device.SetViewport( &composite_viewport ) >= d3d9::D3D_OK &&
                           this->setup_render_state( );
        }

        if ( frame_status )
        {
            for ( const render_command &command : this->m_frame_commands )
            {
                if ( !this->append_command( command ) )
                {
                    frame_status = false;
                    break;
                }
            }
        }

        if ( frame_status )
        {
            frame_status = this->flush_batch( );
        }

        // the composite draw below must not inherit the last command's clip
        if ( this->m_scissor_enabled )
        {
            frame_status            = this->m_device.SetRenderState( d3d9::D3DRS_SCISSORTESTENABLE, 0 ) >=
                                          d3d9::D3D_OK &&
                                      frame_status;
            this->m_scissor_enabled = false;
        }

        bool restore_status{ true };

        if ( target_bound )
        {
            restore_status = this->m_device.SetRenderTarget( 0, &original_target ) >= d3d9::D3D_OK &&
                             restore_status;
        }

        if ( target_bound )
        {
            restore_status = this->m_device.SetViewport( &viewport ) >= d3d9::D3D_OK && restore_status;
        }

        bool composite_status{ frame_status && restore_status &&
                               this->m_device.SetRenderState( d3d9::D3DRS_SRGBWRITEENABLE,
                                                              original_srgb_write ) >= d3d9::D3D_OK &&
                               this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_SRGBTEXTURE,
                                                               original_srgb_write ? 1u : 0u ) >= d3d9::D3D_OK &&
                               this->present_composite( viewport ) };

        for ( usize index{ }; index < additional_targets.size( ); ++index )
        {
            if ( additional_targets_unbound[ index ] )
            {
                restore_status = this->m_device.SetRenderTarget( static_cast< u32 >( index + 1 ),
                                                                 &additional_targets[ index ] ) >= d3d9::D3D_OK &&
                                 restore_status;
            }
        }

        if ( depth_unbound )
        {
            const d3d9::surface9 *depth{ original_depth ? &original_depth : nullptr };
            restore_status = this->m_device.SetDepthStencilSurface( depth ) >= d3d9::D3D_OK && restore_status;
        }

        bool state_status{ this->m_state_block.Apply( ) >= d3d9::D3D_OK };

        if ( !state_status )
        {
            this->m_state_block.Release( );
        }

        this->m_vertices.clear( );
        this->m_batch_type        = batch_type::none;
        this->m_active_text_atlas = nullptr;

        for ( d3d9::surface9 &additional_target : additional_targets )
        {
            additional_target.Release( );
        }

        original_depth.Release( );
        original_target.Release( );

        return composite_status && restore_status && state_status;
    }

    void backend::on_present( void *device, i32 status )
    {
        if ( this->m_initialized && device && status >= d3d9::D3D_OK )
        {
            if ( this->m_device && this->m_device.Get( ) != device )
            {
                this->m_state_block.Release( );
                this->release_text_atlases( );
                this->release_html_textures( );
                this->release_composite_target( );
                this->release_blur_resources( );
                this->release_grade_resources( );
                this->release_model_resources( );
            }

            this->m_device.Attach( device );
        }
    }

    void backend::on_device_lost( )
    {
        this->m_inside_frame         = false;
        this->m_frame_build_failed   = false;
        this->m_last_frame_succeeded = false;
        this->m_state_block.Release( );
        this->m_device.SetTexture( 0, nullptr );
        this->m_device.SetPixelShader( nullptr );
        this->release_text_atlases( );
        this->release_icon_atlases( );
        this->release_html_textures( );
        this->release_composite_target( );
        this->release_blur_resources( );
        this->release_grade_resources( );
        this->release_model_resources( );
        this->m_frame_commands.clear( );
        this->m_vertices.clear( );
        this->m_batch_type = batch_type::none;
        this->m_device.Detach( );
    }

    void backend::on_device_reset( void *device, i32 status )
    {
        if ( status < d3d9::D3D_OK || !device )
        {
            return;
        }

        this->m_device.Attach( device );
    }

    void find_device_methods( void *&present, void *&reset )
    {
        present = nullptr;
        reset   = nullptr;

        const util::pe_module d3d9_dll{ util::find_module( ATLAS_XORWSTR( L"d3d9.dll" ).get( ) ) };
        if ( d3d9_dll.is_null( ) || !d3d9_dll.is_valid( ) )
        {
            return;
        }

        util::address create9_export{ d3d9_dll.find_export( ATLAS_XORSTR( "Direct3DCreate9" ).get( ) ) };
        if ( create9_export.is_null( ) )
        {
            return;
        }

        d3d9::object9 direct3d{ create9_export.get_as< void *( ATLAS_STDCALL * )( u32 ) >( )( 32 ) }; // D3D_SDK_VERSION
        if ( !direct3d )
        {
            return;
        }

        void *window{ I( FindWindowA )( ATLAS_XORSTR( "Valve001" ).get( ), nullptr ) };

        d3d9::D3DPRESENT_PARAMETERS parameters{ };
        parameters.back_buffer_width     = 1;
        parameters.back_buffer_height    = 1;
        parameters.swap_effect           = 1;
        parameters.windowed              = TRUE;
        parameters.device_window         = window;
        parameters.presentation_interval = 0x80000000u;

        using create_device_t = i32( ATLAS_STDCALL * )( void *, u32, u32, void *, u32, d3d9::D3DPRESENT_PARAMETERS *, void ** );

        void     *device{ };
        const i32 status{ util::call_virtual< create_device_t >( direct3d.Get( ), 16 )(
            direct3d.Get( ), 0, 1, window, 0x20, &parameters, &device ) };

        if ( status >= d3d9::D3D_OK && device )
        {
            void *raw_reset{ util::get_virtual( device, static_cast< u16 >( d3d9::DeviceMethod::Reset ) ) };
            void *raw_present{ util::get_virtual( device, static_cast< u16 >( d3d9::DeviceMethod::Present ) ) };

            present = util::is_executable_address( raw_present ) ? raw_present : nullptr;
            reset   = util::is_executable_address( raw_reset ) ? raw_reset : nullptr;

            d3d9::object9{ device }.Release( );
        }

        direct3d.Release( );
    }

    bool backend::ensure_composite_target( u32 width, u32 height )
    {
        if ( !width || !height || !this->m_device )
        {
            return false;
        }

        if ( this->m_composite_texture && this->m_composite_surface && this->m_width == width &&
             this->m_height == height )
        {
            return true;
        }

        this->release_composite_target( );

        d3d9::texture9 texture{ };
        i32            status{ this->m_device.CreateTexture( width, height, 1, d3d9::D3DUSAGE_RENDERTARGET,
                                                             d3d9::D3DFMT_A8R8G8B8, d3d9::D3DPOOL_DEFAULT, &texture ) };

        if ( status < d3d9::D3D_OK || !texture )
        {
            texture.Release( );
            return false;
        }

        d3d9::surface9 surface{ };
        status = texture.GetSurfaceLevel( 0, &surface );

        if ( status < d3d9::D3D_OK || !surface )
        {
            surface.Release( );
            texture.Release( );
            return false;
        }

        this->m_composite_texture.Attach( texture.Get( ) );
        texture.Detach( );
        this->m_composite_surface.Attach( surface.Get( ) );
        surface.Detach( );
        this->m_width  = width;
        this->m_height = height;
        return true;
    }

    void backend::release_composite_target( )
    {
        this->m_composite_surface.Release( );
        this->m_composite_texture.Release( );
        this->m_overlay.release_device_resources( );
        this->m_width  = 0;
        this->m_height = 0;
    }

    bool backend::draw_composite( const d3d9::D3DVIEWPORT9 &viewport ) const
    {
        if ( !this->m_composite_texture || !viewport.width || !viewport.height )
        {
            return false;
        }

        if ( this->m_device.SetRenderState( d3d9::D3DRS_SRCBLEND, d3d9::D3DBLEND_ONE ) < d3d9::D3D_OK ||
             this->m_device.SetRenderState( d3d9::D3DRS_DESTBLEND, d3d9::D3DBLEND_INVSRCALPHA ) < d3d9::D3D_OK ||
             this->m_device.SetRenderState( d3d9::D3DRS_SEPARATEALPHABLENDENABLE, 0 ) < d3d9::D3D_OK ||
             this->m_device.SetTexture( 0, &this->m_composite_texture ) < d3d9::D3D_OK ||
             this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLOROP, d3d9::D3DTOP_MODULATE ) <
                 d3d9::D3D_OK ||
             this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG1, d3d9::D3DTA_TEXTURE ) <
                 d3d9::D3D_OK ||
             this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_COLORARG2, d3d9::D3DTA_DIFFUSE ) <
                 d3d9::D3D_OK ||
             this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAOP, d3d9::D3DTOP_MODULATE ) <
                 d3d9::D3D_OK ||
             this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG1, d3d9::D3DTA_TEXTURE ) <
                 d3d9::D3D_OK ||
             this->m_device.SetTextureStageState( 0, d3d9::D3DTSS_ALPHAARG2, d3d9::D3DTA_DIFFUSE ) <
                 d3d9::D3D_OK )
        {
            return false;
        }

        const float   left{ static_cast< float >( viewport.x ) - 0.5f };
        const float   top{ static_cast< float >( viewport.y ) - 0.5f };
        const float   right{ left + static_cast< float >( viewport.width ) };
        const float   bottom{ top + static_cast< float >( viewport.height ) };
        constexpr u32 color{ 0xFFFFFFFFu };

        d3d9::overlay_vertex vertices[ 6 ]{
            { left, top, 0.0f, 1.0f, color, 0.0f, 0.0f },
            { right, top, 0.0f, 1.0f, color, 1.0f, 0.0f },
            { left, bottom, 0.0f, 1.0f, color, 0.0f, 1.0f },
            { left, bottom, 0.0f, 1.0f, color, 0.0f, 1.0f },
            { right, top, 0.0f, 1.0f, color, 1.0f, 0.0f },
            { right, bottom, 0.0f, 1.0f, color, 1.0f, 1.0f },
        };

        i32 status{ this->m_device.DrawPrimitiveUP( d3d9::D3DPT_TRIANGLELIST, 2, vertices,
                                                    sizeof( d3d9::overlay_vertex ) ) };
        return status >= d3d9::D3D_OK;
    }

    bool backend::present_composite( const d3d9::D3DVIEWPORT9 &viewport )
    {
        this->m_stream_proof_fallback = false;

        if ( !this->m_stream_proof )
        {
            return this->draw_composite( viewport );
        }

        if ( !this->m_overlay.can_present( this->m_device ) )
        {
            this->m_stream_proof_fallback = true;
            return this->draw_composite( viewport );
        }

        return this->m_overlay.present( this->m_device, this->m_composite_surface, viewport );
    }

    void backend::set_stream_proof( bool enabled )
    {
        if ( this->m_stream_proof == enabled )
        {
            return;
        }

        this->m_stream_proof                   = enabled;
        this->m_stream_proof_fallback_reported = false;

        if ( !enabled )
        {
            this->m_overlay.hide( );
        }
    }

    bool backend::consume_stream_proof_fallback( u32 *stage, u32 *error )
    {
        if ( !this->m_stream_proof_fallback || this->m_stream_proof_fallback_reported )
        {
            return false;
        }

        this->m_stream_proof_fallback_reported = true;
        this->m_overlay.describe_failure( stage, error );
        return true;
    }

    bool backend::ensure_blur_resources( u32 width, u32 height, u32 format, bool resolve )
    {
        if ( !width || !height || !format || !this->m_device )
        {
            return false;
        }

        u32 blur_width{ width / 6u };
        u32 blur_height{ height / 6u };
        blur_width  = blur_width ? blur_width : 1u;
        blur_height = blur_height ? blur_height : 1u;

        bool resources_match{
            this->m_blur_shader &&
            this->m_blur_texture_a && this->m_blur_surface_a &&
            this->m_blur_texture_b && this->m_blur_surface_b &&
            this->m_blur_width == blur_width &&
            this->m_blur_height == blur_height &&
            this->m_blur_source_width == width &&
            this->m_blur_source_height == height &&
            this->m_blur_format == format &&
            this->m_blur_resolve == resolve &&
            ( !resolve || ( this->m_blur_resolve_texture && this->m_blur_resolve_surface ) )
        };

        if ( resources_match )
        {
            return true;
        }

        this->release_blur_resources( );

        i32 status{ this->m_device.CreateTexture( blur_width, blur_height, 1,
                                                  d3d9::D3DUSAGE_RENDERTARGET,
                                                  format, d3d9::D3DPOOL_DEFAULT,
                                                  &this->m_blur_texture_a ) };

        if ( status < d3d9::D3D_OK || !this->m_blur_texture_a )
        {
            this->release_blur_resources( );
            return false;
        }

        status = this->m_blur_texture_a.GetSurfaceLevel( 0, &this->m_blur_surface_a );

        if ( status < d3d9::D3D_OK || !this->m_blur_surface_a )
        {
            this->release_blur_resources( );
            return false;
        }

        status = this->m_device.CreateTexture( blur_width, blur_height, 1,
                                               d3d9::D3DUSAGE_RENDERTARGET,
                                               format, d3d9::D3DPOOL_DEFAULT,
                                               &this->m_blur_texture_b );

        if ( status < d3d9::D3D_OK || !this->m_blur_texture_b )
        {
            this->release_blur_resources( );
            return false;
        }

        status = this->m_blur_texture_b.GetSurfaceLevel( 0, &this->m_blur_surface_b );

        if ( status < d3d9::D3D_OK || !this->m_blur_surface_b )
        {
            this->release_blur_resources( );
            return false;
        }

        if ( resolve )
        {
            status = this->m_device.CreateTexture( width, height, 1,
                                                   d3d9::D3DUSAGE_RENDERTARGET,
                                                   format, d3d9::D3DPOOL_DEFAULT,
                                                   &this->m_blur_resolve_texture );

            if ( status < d3d9::D3D_OK || !this->m_blur_resolve_texture )
            {
                this->release_blur_resources( );
                return false;
            }

            status = this->m_blur_resolve_texture.GetSurfaceLevel( 0, &this->m_blur_resolve_surface );

            if ( status < d3d9::D3D_OK || !this->m_blur_resolve_surface )
            {
                this->release_blur_resources( );
                return false;
            }
        }

        status = this->m_device.CreatePixelShader( shaders::BLUR_PIXEL_SHADER,
                                                   &this->m_blur_shader );

        if ( status < d3d9::D3D_OK || !this->m_blur_shader )
        {
            this->release_blur_resources( );
            return false;
        }

        this->m_blur_width         = blur_width;
        this->m_blur_height        = blur_height;
        this->m_blur_source_width  = width;
        this->m_blur_source_height = height;
        this->m_blur_format        = format;
        this->m_blur_resolve       = resolve;
        return true;
    }

    void backend::release_blur_resources( )
    {
        this->m_blur_frame_ready = false;
        this->m_blur_shader.Release( );
        this->m_blur_surface_a.Release( );
        this->m_blur_texture_a.Release( );
        this->m_blur_surface_b.Release( );
        this->m_blur_texture_b.Release( );
        this->m_blur_resolve_surface.Release( );
        this->m_blur_resolve_texture.Release( );
        this->m_blur_width         = 0;
        this->m_blur_height        = 0;
        this->m_blur_source_width  = 0;
        this->m_blur_source_height = 0;
        this->m_blur_format        = 0;
        this->m_blur_resolve       = false;
    }

    bool backend::adopt_model_target( u64 serial, u32 width, u32 height,
                                      u32 target_width, u32 target_height )
    {
        if ( !this->m_initialized || !this->m_device || !serial || !width || !height )
        {
            return false;
        }

        d3d9::surface9 surface{ };
        if ( this->m_device.GetRenderTarget( 0, &surface ) < d3d9::D3D_OK || !surface )
        {
            surface.Release( );
            return false;
        }

        d3d9::D3DSURFACE_DESC description{ };
        d3d9::texture9 texture{ };
        const bool ready{ surface.GetDesc( &description ) >= d3d9::D3D_OK &&
                          description.width == target_width && description.height == target_height &&
                          description.multi_sample_type == 0 && width <= description.width &&
                          height <= description.height &&
                          surface.GetTextureContainer( &texture ) >= d3d9::D3D_OK && texture };
        surface.Release( );
        if ( !ready )
        {
            texture.Release( );
            return false;
        }

        this->m_model_texture.Release( );
        this->m_model_texture.Attach( texture.Get( ) );
        texture.Detach( );
        this->m_model_uv = vec2{ static_cast< float >( width ) / description.width,
                                 static_cast< float >( height ) / description.height };
        this->m_model_serial = serial;
        this->m_model_ready = true;
        return true;
    }

    void backend::release_model_resources( )
    {
        this->m_model_texture.Release( );
        this->m_model_uv = vec2{ };
        this->m_model_ready = false;
        this->m_model_serial = 0;
    }

    bool backend::render_blur( const d3d9::surface9 &source )
    {
        if ( !source || !this->m_blur_shader ||
             !this->m_blur_texture_a || !this->m_blur_surface_a ||
             !this->m_blur_texture_b || !this->m_blur_surface_b )
        {
            return false;
        }

        const d3d9::surface9 *downsample_source{ &source };

        if ( this->m_blur_resolve )
        {
            if ( !this->m_blur_resolve_surface ||
                 this->m_device.StretchRect( &source, nullptr,
                                             &this->m_blur_resolve_surface, nullptr,
                                             d3d9::D3DTEXF_NONE ) < d3d9::D3D_OK )
            {
                return false;
            }

            downsample_source = &this->m_blur_resolve_surface;
        }

        if ( this->m_device.StretchRect( downsample_source, nullptr,
                                         &this->m_blur_surface_a, nullptr,
                                         d3d9::D3DTEXF_LINEAR ) < d3d9::D3D_OK )
        {
            return false;
        }

        constexpr u32 pass_count{ 2 };

        for ( u32 pass{ }; pass < pass_count; ++pass )
        {
            if ( !this->render_blur_pass( this->m_blur_texture_a,
                                          this->m_blur_surface_b,
                                          1.0f / static_cast< float >( this->m_blur_width ),
                                          0.0f ) ||
                 !this->render_blur_pass( this->m_blur_texture_b,
                                          this->m_blur_surface_a,
                                          0.0f,
                                          1.0f / static_cast< float >( this->m_blur_height ) ) )
            {
                this->m_device.SetTexture( 0, nullptr );
                this->m_device.SetPixelShader( nullptr );
                return false;
            }
        }

        this->m_device.SetTexture( 0, nullptr );
        this->m_device.SetPixelShader( nullptr );
        return true;
    }

    bool backend::render_blur_pass( const d3d9::texture9 &source,
                                    const d3d9::surface9 &destination,
                                    float direction_x, float direction_y )
    {
        if ( !source || !destination || !this->m_blur_shader )
        {
            return false;
        }

        d3d9::D3DVIEWPORT9 viewport{
            0, 0, this->m_blur_width, this->m_blur_height, 0.0f, 1.0f
        };
        float direction[ 4 ]{ direction_x, direction_y, 0.0f, 0.0f };

        if ( this->m_device.SetTexture( 0, nullptr ) < d3d9::D3D_OK ||
             this->m_device.SetRenderTarget( 0, &destination ) < d3d9::D3D_OK ||
             this->m_device.SetViewport( &viewport ) < d3d9::D3D_OK ||
             !this->setup_render_state( ) ||
             this->m_device.SetRenderState( d3d9::D3DRS_ALPHABLENDENABLE, 0 ) < d3d9::D3D_OK ||
             this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MAGFILTER,
                                             d3d9::D3DTEXF_LINEAR ) < d3d9::D3D_OK ||
             this->m_device.SetSamplerState( 0, d3d9::D3DSAMP_MINFILTER,
                                             d3d9::D3DTEXF_LINEAR ) < d3d9::D3D_OK ||
             this->m_device.SetTexture( 0, &source ) < d3d9::D3D_OK ||
             this->m_device.SetPixelShader( &this->m_blur_shader ) < d3d9::D3D_OK ||
             this->m_device.SetPixelShaderConstantF( 0, direction, 1 ) < d3d9::D3D_OK )
        {
            return false;
        }

        const float     right{ static_cast< float >( this->m_blur_width ) - 0.5f };
        const float     bottom{ static_cast< float >( this->m_blur_height ) - 0.5f };
        constexpr float left{ -0.5f };
        constexpr float top{ -0.5f };
        constexpr u32   color{ 0xFFFFFFFFu };

        d3d9::overlay_vertex vertices[ 6 ]{
            { left, top, 0.0f, 1.0f, color, 0.0f, 0.0f },
            { right, top, 0.0f, 1.0f, color, 1.0f, 0.0f },
            { left, bottom, 0.0f, 1.0f, color, 0.0f, 1.0f },
            { left, bottom, 0.0f, 1.0f, color, 0.0f, 1.0f },
            { right, top, 0.0f, 1.0f, color, 1.0f, 0.0f },
            { right, bottom, 0.0f, 1.0f, color, 1.0f, 1.0f },
        };

        return this->m_device.DrawPrimitiveUP( d3d9::D3DPT_TRIANGLELIST, 2,
                                               vertices,
                                               sizeof( d3d9::overlay_vertex ) ) >= d3d9::D3D_OK;
    }

    bool backend::ensure_grade_resources( u32 width, u32 height, u32 format )
    {
        if ( this->m_grade_shader && this->m_grade_texture && this->m_grade_surface &&
             this->m_grade_width == width && this->m_grade_height == height &&
             this->m_grade_format == format )
        {
            return true;
        }

        this->release_grade_resources( );

        i32 status{ this->m_device.CreateTexture( width, height, 1,
                                                  d3d9::D3DUSAGE_RENDERTARGET,
                                                  format, d3d9::D3DPOOL_DEFAULT,
                                                  &this->m_grade_texture ) };

        if ( status < d3d9::D3D_OK || !this->m_grade_texture )
        {
            this->release_grade_resources( );
            return false;
        }

        status = this->m_grade_texture.GetSurfaceLevel( 0, &this->m_grade_surface );

        if ( status < d3d9::D3D_OK || !this->m_grade_surface )
        {
            this->release_grade_resources( );
            return false;
        }

        status = this->m_device.CreatePixelShader( shaders::GRADE_PIXEL_SHADER,
                                                   &this->m_grade_shader );

        if ( status < d3d9::D3D_OK || !this->m_grade_shader )
        {
            this->release_grade_resources( );
            return false;
        }

        this->m_grade_width  = width;
        this->m_grade_height = height;
        this->m_grade_format = format;
        return true;
    }

    void backend::release_grade_resources( )
    {
        this->m_grade_shader.Release( );
        this->m_grade_surface.Release( );
        this->m_grade_texture.Release( );
        this->m_grade_width  = 0;
        this->m_grade_height = 0;
        this->m_grade_format = 0;
    }

    bool backend::grade( const grade_t &grade )
    {
        if ( !this->m_inside_frame )
        {
            return false;
        }

        d3d9::surface9 target{ };

        if ( this->m_device.GetRenderTarget( 0, &target ) < d3d9::D3D_OK || !target )
        {
            target.Release( );
            return false;
        }

        d3d9::D3DSURFACE_DESC description{ };
        d3d9::D3DVIEWPORT9    viewport{ };

        if ( target.GetDesc( &description ) < d3d9::D3D_OK ||
             this->m_device.GetViewport( &viewport ) < d3d9::D3D_OK ||
             viewport.x || viewport.y ||
             viewport.width != description.width || viewport.height != description.height )
        {
            target.Release( );
            return false;
        }

        const bool bloom{ grade.m_bloom > 0.0f };

        if ( !this->ensure_grade_resources( description.width, description.height, description.format ) ||
             ( bloom && !this->ensure_blur_resources( viewport.width, viewport.height, description.format,
                                                      description.multi_sample_type != 0 ) ) ||
             !this->capture_state( ) )
        {
            target.Release( );
            return false;
        }

        d3d9::surface9                  depth{ };
        atl::array< d3d9::surface9, 3 > extra_targets{ };

        this->m_device.GetDepthStencilSurface( &depth );
        this->m_device.SetDepthStencilSurface( nullptr );

        for ( usize index{ }; index < extra_targets.size( ); ++index )
        {
            if ( this->m_device.GetRenderTarget( static_cast< u32 >( index + 1 ), &extra_targets[ index ] ) >= d3d9::D3D_OK &&
                 extra_targets[ index ] )
            {
                this->m_device.SetRenderTarget( static_cast< u32 >( index + 1 ), nullptr );
            }
        }

        bool status{ this->m_device.StretchRect( &target, nullptr, &this->m_grade_surface, nullptr,
                                                 d3d9::D3DTEXF_NONE ) >= d3d9::D3D_OK };

        if ( status && bloom )
        {
            status = this->render_blur( target );
        }

        if ( status )
        {
            status = this->draw_grade( target, viewport, grade, bloom );
        }

        this->m_device.SetTexture( 0, nullptr );
        this->m_device.SetTexture( 1, nullptr );
        this->m_device.SetPixelShader( nullptr );
        this->m_device.SetRenderTarget( 0, &target );

        for ( usize index{ }; index < extra_targets.size( ); ++index )
        {
            if ( extra_targets[ index ] )
            {
                this->m_device.SetRenderTarget( static_cast< u32 >( index + 1 ), &extra_targets[ index ] );
                extra_targets[ index ].Release( );
            }
        }

        this->m_device.SetDepthStencilSurface( depth ? &depth : nullptr );
        depth.Release( );

        if ( this->m_state_block.Apply( ) < d3d9::D3D_OK )
        {
            this->m_state_block.Release( );
            status = false;
        }

        target.Release( );
        return status;
    }

    bool backend::draw_grade( const d3d9::surface9     &target,
                              const d3d9::D3DVIEWPORT9 &viewport,
                              const grade_t            &grade,
                              bool                      bloom )
    {
        Assert( grade.m_bloom_threshold < 1.0f );

        const float red{ 1.0f + grade.m_temperature * 0.2f };
        const float green{ 1.0f - grade.m_tint * 0.2f };
        const float blue{ 1.0f - grade.m_temperature * 0.2f };

        const float balance_luma{ 0.2126f * red + 0.7152f * green + 0.0722f * blue };

        const float constants[ 12 ]{
            grade.m_exposure,
            grade.m_contrast,
            grade.m_saturation,
            grade.m_vibrance,
            grade.m_shadows,
            grade.m_highlights,
            grade.m_vignette,
            bloom ? grade.m_bloom : 0.0f,
            red / balance_luma,
            green / balance_luma,
            blue / balance_luma,
            grade.m_bloom_threshold,
        };

        if ( this->m_device.SetRenderTarget( 0, &target ) < d3d9::D3D_OK ||
             this->m_device.SetViewport( &viewport ) < d3d9::D3D_OK ||
             !this->setup_render_state( ) ||
             this->m_device.SetRenderState( d3d9::D3DRS_ALPHABLENDENABLE, 0 ) < d3d9::D3D_OK ||
             this->m_device.SetTexture( 0, &this->m_grade_texture ) < d3d9::D3D_OK ||
             this->m_device.SetTexture( 1, bloom ? &this->m_blur_texture_a : nullptr ) < d3d9::D3D_OK ||
             this->m_device.SetSamplerState( 1, d3d9::D3DSAMP_ADDRESSU, d3d9::D3DTADDRESS_CLAMP ) < d3d9::D3D_OK ||
             this->m_device.SetSamplerState( 1, d3d9::D3DSAMP_ADDRESSV, d3d9::D3DTADDRESS_CLAMP ) < d3d9::D3D_OK ||
             this->m_device.SetSamplerState( 1, d3d9::D3DSAMP_MAGFILTER, d3d9::D3DTEXF_LINEAR ) < d3d9::D3D_OK ||
             this->m_device.SetSamplerState( 1, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR ) < d3d9::D3D_OK ||
             this->m_device.SetSamplerState( 1, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_NONE ) < d3d9::D3D_OK ||
             this->m_device.SetSamplerState( 1, d3d9::D3DSAMP_SRGBTEXTURE, 0 ) < d3d9::D3D_OK ||
             this->m_device.SetPixelShader( &this->m_grade_shader ) < d3d9::D3D_OK ||
             this->m_device.SetPixelShaderConstantF( 0, constants, 3 ) < d3d9::D3D_OK )
        {
            return false;
        }

        const float     right{ static_cast< float >( viewport.width ) - 0.5f };
        const float     bottom{ static_cast< float >( viewport.height ) - 0.5f };
        constexpr float left{ -0.5f };
        constexpr float top{ -0.5f };
        constexpr u32   color{ 0xFFFFFFFFu };

        d3d9::overlay_vertex vertices[ 6 ]{
            { left, top, 0.0f, 1.0f, color, 0.0f, 0.0f },
            { right, top, 0.0f, 1.0f, color, 1.0f, 0.0f },
            { left, bottom, 0.0f, 1.0f, color, 0.0f, 1.0f },
            { left, bottom, 0.0f, 1.0f, color, 0.0f, 1.0f },
            { right, top, 0.0f, 1.0f, color, 1.0f, 0.0f },
            { right, bottom, 0.0f, 1.0f, color, 1.0f, 1.0f },
        };

        return this->m_device.DrawPrimitiveUP( d3d9::D3DPT_TRIANGLELIST, 2,
                                               vertices,
                                               sizeof( d3d9::overlay_vertex ) ) >= d3d9::D3D_OK;
    }
} // namespace render
