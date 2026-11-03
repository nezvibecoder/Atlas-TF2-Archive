#include "render.hpp"
#include "runtime/assert.hpp"
#include "shaders/overlay11.hpp"

#include "icons.hpp"

#include <d3d11.h>

namespace render
{
    template< typename type >
    static void release( type *&object )
    {
        if ( object )
        {
            object->Release( );
            object = nullptr;
        }
    }

    static DXGI_FORMAT unorm_format( DXGI_FORMAT format )
    {
        switch ( format )
        {
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
            default: return format;
        }
    }

    struct saved_state_t
    {
        u32                       m_scissor_count{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };
        u32                       m_viewport_count{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };
        D3D11_RECT                m_scissors[ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ]{ };
        D3D11_VIEWPORT            m_viewports[ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ]{ };
        ID3D11RenderTargetView   *m_targets[ D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT ]{ };
        ID3D11DepthStencilView   *m_depth_view{ };
        ID3D11RasterizerState    *m_raster_state{ };
        ID3D11BlendState         *m_blend_state{ };
        float                     m_blend_factor[ 4 ]{ };
        u32                       m_sample_mask{ };
        ID3D11DepthStencilState  *m_depth_state{ };
        u32                       m_stencil_ref{ };
        ID3D11ShaderResourceView *m_resource{ };
        ID3D11SamplerState       *m_sampler{ };
        ID3D11PixelShader        *m_pixel_shader{ };
        ID3D11VertexShader       *m_vertex_shader{ };
        ID3D11GeometryShader     *m_geometry_shader{ };
        ID3D11HullShader         *m_hull_shader{ };
        ID3D11DomainShader       *m_domain_shader{ };
        ID3D11Buffer             *m_vertex_constants{ };
        ID3D11Buffer             *m_pixel_constants{ };
        D3D11_PRIMITIVE_TOPOLOGY  m_topology{ };
        ID3D11Buffer             *m_index_buffer{ };
        DXGI_FORMAT               m_index_format{ };
        u32                       m_index_offset{ };
        ID3D11Buffer             *m_vertex_buffer{ };
        u32                       m_vertex_stride{ };
        u32                       m_vertex_offset{ };
        ID3D11InputLayout        *m_input_layout{ };
    };

    static void save_state( ID3D11DeviceContext *context, saved_state_t &state )
    {
        context->RSGetScissorRects( &state.m_scissor_count, state.m_scissors );
        context->RSGetViewports( &state.m_viewport_count, state.m_viewports );
        context->RSGetState( &state.m_raster_state );
        context->OMGetRenderTargets( D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, state.m_targets, &state.m_depth_view );
        context->OMGetBlendState( &state.m_blend_state, state.m_blend_factor, &state.m_sample_mask );
        context->OMGetDepthStencilState( &state.m_depth_state, &state.m_stencil_ref );
        context->PSGetShaderResources( 0, 1, &state.m_resource );
        context->PSGetSamplers( 0, 1, &state.m_sampler );
        context->PSGetShader( &state.m_pixel_shader, nullptr, nullptr );
        context->VSGetShader( &state.m_vertex_shader, nullptr, nullptr );
        context->GSGetShader( &state.m_geometry_shader, nullptr, nullptr );
        context->HSGetShader( &state.m_hull_shader, nullptr, nullptr );
        context->DSGetShader( &state.m_domain_shader, nullptr, nullptr );
        context->VSGetConstantBuffers( 0, 1, &state.m_vertex_constants );
        context->PSGetConstantBuffers( 0, 1, &state.m_pixel_constants );
        context->IAGetPrimitiveTopology( &state.m_topology );
        context->IAGetIndexBuffer( &state.m_index_buffer, &state.m_index_format, &state.m_index_offset );
        context->IAGetVertexBuffers( 0, 1, &state.m_vertex_buffer, &state.m_vertex_stride, &state.m_vertex_offset );
        context->IAGetInputLayout( &state.m_input_layout );
    }

    static void restore_state( ID3D11DeviceContext *context, saved_state_t &state )
    {
        context->RSSetScissorRects( state.m_scissor_count, state.m_scissors );
        context->RSSetViewports( state.m_viewport_count, state.m_viewports );
        context->RSSetState( state.m_raster_state );
        context->OMSetRenderTargets( D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, state.m_targets, state.m_depth_view );
        context->OMSetBlendState( state.m_blend_state, state.m_blend_factor, state.m_sample_mask );
        context->OMSetDepthStencilState( state.m_depth_state, state.m_stencil_ref );
        context->PSSetShaderResources( 0, 1, &state.m_resource );
        context->PSSetSamplers( 0, 1, &state.m_sampler );
        context->PSSetShader( state.m_pixel_shader, nullptr, 0 );
        context->VSSetShader( state.m_vertex_shader, nullptr, 0 );
        context->GSSetShader( state.m_geometry_shader, nullptr, 0 );
        context->HSSetShader( state.m_hull_shader, nullptr, 0 );
        context->DSSetShader( state.m_domain_shader, nullptr, 0 );
        context->VSSetConstantBuffers( 0, 1, &state.m_vertex_constants );
        context->PSSetConstantBuffers( 0, 1, &state.m_pixel_constants );
        context->IASetPrimitiveTopology( state.m_topology );
        context->IASetIndexBuffer( state.m_index_buffer, state.m_index_format, state.m_index_offset );
        context->IASetVertexBuffers( 0, 1, &state.m_vertex_buffer, &state.m_vertex_stride, &state.m_vertex_offset );
        context->IASetInputLayout( state.m_input_layout );

        for ( ID3D11RenderTargetView *&target : state.m_targets )
        {
            release( target );
        }

        release( state.m_depth_view );
        release( state.m_raster_state );
        release( state.m_blend_state );
        release( state.m_depth_state );
        release( state.m_resource );
        release( state.m_sampler );
        release( state.m_pixel_shader );
        release( state.m_vertex_shader );
        release( state.m_geometry_shader );
        release( state.m_hull_shader );
        release( state.m_domain_shader );
        release( state.m_vertex_constants );
        release( state.m_pixel_constants );
        release( state.m_index_buffer );
        release( state.m_vertex_buffer );
        release( state.m_input_layout );
    }

    void d3d11::texture::Release( )
    {
        release( this->m_target );
        release( this->m_view );
        release( this->m_texture );
    }

    bool backend::flush_batch( )
    {
        if ( this->m_vertices.empty( ) )
        {
            this->m_batch_type = batch_type::none;
            return true;
        }

        if ( this->m_batch_type == batch_type::none || !this->setup_batch_state( this->m_batch_type ) ||
             !this->draw_vertices( this->m_vertices.data( ), static_cast< u32 >( this->m_vertices.size( ) ),
                                   this->m_batch_type == batch_type::lines ) )
        {
            return false;
        }

        this->m_vertices.clear( );
        this->m_batch_type = batch_type::none;
        return true;
    }

    bool backend::draw_vertices( const d3d9::overlay_vertex *vertices, u32 count, bool lines )
    {
        if ( count > this->m_vertex_capacity )
        {
            if ( !this->ensure_vertex_capacity( count ) )
            {
                return false;
            }

            const u32 stride{ sizeof( d3d9::overlay_vertex ) };
            const u32 offset{ };
            this->m_context->IASetVertexBuffers( 0, 1, &this->m_vertex_buffer, &stride, &offset );
            this->m_vertex_cursor = 0;
        }

        if ( this->m_vertex_cursor + count > this->m_vertex_capacity )
        {
            this->m_vertex_cursor = 0;
        }

        D3D11_MAPPED_SUBRESOURCE mapped{ };
        const D3D11_MAP          mode{ this->m_vertex_cursor ? D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD };

        if ( FAILED( this->m_context->Map( this->m_vertex_buffer, 0, mode, 0, &mapped ) ) )
        {
            return false;
        }

        atl::memcpy( static_cast< d3d9::overlay_vertex * >( mapped.pData ) + this->m_vertex_cursor, vertices,
                     count * sizeof( d3d9::overlay_vertex ) );
        this->m_context->Unmap( this->m_vertex_buffer, 0 );

        this->m_context->IASetPrimitiveTopology( lines ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        this->m_context->Draw( count, this->m_vertex_cursor );
        this->m_vertex_cursor += count;
        return true;
    }

    bool backend::set_scissor( const rect_t &bounds, bool enabled )
    {
        const D3D11_RECT rect{ enabled ? D3D11_RECT{ bounds.left, bounds.top, bounds.right, bounds.bottom }
                                       : D3D11_RECT{ 0, 0, static_cast< LONG >( this->m_width ), static_cast< LONG >( this->m_height ) } };

        this->m_context->RSSetScissorRects( 1, &rect );
        return true;
    }

    bool backend::create_texture( u32 width, u32 height, u32 levels, texture_t &texture ) const
    {
        D3D11_TEXTURE2D_DESC description{ };
        description.Width            = width;
        description.Height           = height;
        description.MipLevels        = levels;
        description.ArraySize        = 1;
        description.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage            = D3D11_USAGE_DEFAULT;
        description.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        if ( FAILED( this->m_device->CreateTexture2D( &description, nullptr, &texture.m_texture ) ) ||
             FAILED( this->m_device->CreateShaderResourceView( texture.m_texture, nullptr, &texture.m_view ) ) )
        {
            texture.Release( );
            return false;
        }

        return true;
    }

    bool backend::upload_texture( const texture_t &texture, u32 level, const rect_t &area, const u8 *pixels,
                                  usize pitch ) const
    {
        const D3D11_BOX box{ static_cast< UINT >( area.left ), static_cast< UINT >( area.top ), 0,
                             static_cast< UINT >( area.right ), static_cast< UINT >( area.bottom ), 1 };

        this->m_context->UpdateSubresource( texture.m_texture, level, &box, pixels, static_cast< UINT >( pitch ), 0 );
        return true;
    }

    bool backend::setup_render_state( ID3D11RenderTargetView *target, u32 width, u32 height, float step_x,
                                      float step_y ) const
    {
        D3D11_MAPPED_SUBRESOURCE mapped{ };

        if ( FAILED( this->m_context->Map( this->m_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) ) )
        {
            return false;
        }

        const float constants[ 4 ]{ 2.0f / static_cast< float >( width ), 2.0f / static_cast< float >( height ), step_x, step_y };
        atl::memcpy( mapped.pData, constants, sizeof( constants ) );
        this->m_context->Unmap( this->m_constant_buffer, 0 );

        const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, static_cast< float >( width ), static_cast< float >( height ), 0.0f, 1.0f };
        const D3D11_RECT     scissor{ 0, 0, static_cast< LONG >( width ), static_cast< LONG >( height ) };
        const u32            stride{ sizeof( d3d9::overlay_vertex ) };
        const u32            offset{ };
        constexpr float      blend_factor[ 4 ]{ };

        this->m_context->OMSetRenderTargets( 1, &target, nullptr );
        this->m_context->OMSetBlendState( this->m_blend_state, blend_factor, 0xFFFFFFFFu );
        this->m_context->OMSetDepthStencilState( this->m_depth_state, 0 );
        this->m_context->RSSetViewports( 1, &viewport );
        this->m_context->RSSetScissorRects( 1, &scissor );
        this->m_context->RSSetState( this->m_raster_state );
        this->m_context->IASetInputLayout( this->m_input_layout );
        this->m_context->IASetVertexBuffers( 0, 1, &this->m_vertex_buffer, &stride, &offset );
        this->m_context->IASetIndexBuffer( nullptr, DXGI_FORMAT_UNKNOWN, 0 );
        this->m_context->VSSetShader( this->m_vertex_shader, nullptr, 0 );
        this->m_context->VSSetConstantBuffers( 0, 1, &this->m_constant_buffer );
        this->m_context->PSSetConstantBuffers( 0, 1, &this->m_constant_buffer );
        this->m_context->GSSetShader( nullptr, nullptr, 0 );
        this->m_context->HSSetShader( nullptr, nullptr, 0 );
        this->m_context->DSSetShader( nullptr, nullptr, 0 );
        return true;
    }

    bool backend::setup_batch_state( batch_type type ) const
    {
        ID3D11ShaderResourceView *resource{ };
        ID3D11PixelShader        *shader{ this->m_textured_shader };
        ID3D11SamplerState       *sampler{ this->m_point_sampler };

        switch ( type )
        {
            case batch_type::lines:
            case batch_type::solid:
            {
                this->m_context->PSSetShader( this->m_solid_shader, nullptr, 0 );
                return true;
            }

            case batch_type::blur:
            {
                if ( !this->m_blur_frame_ready )
                {
                    return false;
                }

                resource = this->m_blur_texture_a.m_view;
                shader   = this->m_tinted_shader;
                sampler  = this->m_linear_sampler;
                break;
            }

            case batch_type::text:
            {
                if ( !this->m_active_text_atlas || !this->m_active_text_atlas->m_ready )
                {
                    return false;
                }

                resource = this->m_active_text_atlas->m_texture.m_view;
                break;
            }

            case batch_type::icon_small:
            case batch_type::icon_large:
            case batch_type::icon_logo:
            case batch_type::icon_vacc:
            {
                const icon_atlas_t &atlas{ this->icon_atlas_for( type ) };

                if ( !atlas.m_ready )
                {
                    return false;
                }

                resource = atlas.m_texture.m_view;
                break;
            }

            case batch_type::avatar:
            {
                if ( !this->m_avatar_bound || !*this->m_avatar_bound )
                {
                    return false;
                }

                resource = this->m_avatar_bound->m_view;
                shader   = this->m_tinted_shader;
                sampler  = this->m_linear_sampler;
                break;
            }

            case batch_type::html:
            {
                if ( !this->m_html_bound || !*this->m_html_bound )
                {
                    return false;
                }

                resource = this->m_html_bound->m_view;
                sampler  = this->m_linear_sampler;
                break;
            }

            default: return false;
        }

        this->m_context->PSSetShader( shader, nullptr, 0 );
        this->m_context->PSSetShaderResources( 0, 1, &resource );
        this->m_context->PSSetSamplers( 0, 1, &sampler );
        return true;
    }

    bool backend::ensure_blur_resources( )
    {
        if ( this->m_blur_source && this->m_blur_texture_a && this->m_blur_texture_b )
        {
            return true;
        }

        this->release_blur_resources( );
        this->m_blur_width  = math::bigger( this->m_width / 6u, 1u );
        this->m_blur_height = math::bigger( this->m_height / 6u, 1u );

        const auto create{ [ & ]( u32 width, u32 height, bool target, d3d11::texture &texture )
        {
            D3D11_TEXTURE2D_DESC description{ };
            description.Width            = width;
            description.Height           = height;
            description.MipLevels        = 1;
            description.ArraySize        = 1;
            description.Format           = static_cast< DXGI_FORMAT >( this->m_target_format );
            description.SampleDesc.Count = 1;
            description.Usage            = D3D11_USAGE_DEFAULT;
            description.BindFlags        = D3D11_BIND_SHADER_RESOURCE | ( target ? D3D11_BIND_RENDER_TARGET : 0u );

            D3D11_SHADER_RESOURCE_VIEW_DESC source_view{ };
            source_view.Format        = unorm_format( description.Format );
            source_view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            source_view.Texture2D.MipLevels = 1;

            D3D11_RENDER_TARGET_VIEW_DESC target_view{ };
            target_view.Format        = unorm_format( description.Format );
            target_view.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

            return SUCCEEDED( this->m_device->CreateTexture2D( &description, nullptr, &texture.m_texture ) ) &&
                   SUCCEEDED( this->m_device->CreateShaderResourceView( texture.m_texture, &source_view, &texture.m_view ) ) &&
                   ( !target || SUCCEEDED( this->m_device->CreateRenderTargetView( texture.m_texture, &target_view, &texture.m_target ) ) );
        } };

        if ( !create( this->m_width, this->m_height, false, this->m_blur_source ) ||
             !create( this->m_blur_width, this->m_blur_height, true, this->m_blur_texture_a ) ||
             !create( this->m_blur_width, this->m_blur_height, true, this->m_blur_texture_b ) )
        {
            this->release_blur_resources( );
            return false;
        }

        return true;
    }

    void backend::release_blur_resources( )
    {
        this->m_blur_frame_ready = false;
        this->m_blur_source.Release( );
        this->m_blur_texture_a.Release( );
        this->m_blur_texture_b.Release( );
        this->m_blur_width  = 0;
        this->m_blur_height = 0;
    }

    bool backend::render_blur( ID3D11Texture2D *back_buffer )
    {
        if ( this->m_target_samples > 1 )
        {
            this->m_context->ResolveSubresource( this->m_blur_source.m_texture, 0, back_buffer, 0,
                                                 static_cast< DXGI_FORMAT >( this->m_target_format ) );
        } else
        {
            this->m_context->CopyResource( this->m_blur_source.m_texture, back_buffer );
        }

        const float step_x{ 1.0f / static_cast< float >( this->m_blur_width ) };
        const float step_y{ 1.0f / static_cast< float >( this->m_blur_height ) };

        return this->render_blur_pass( this->m_textured_shader, this->m_blur_source, this->m_blur_texture_a, 0.0f, 0.0f ) &&
               this->render_blur_pass( this->m_blur_shader, this->m_blur_texture_a, this->m_blur_texture_b, step_x, 0.0f ) &&
               this->render_blur_pass( this->m_blur_shader, this->m_blur_texture_b, this->m_blur_texture_a, 0.0f, step_y ) &&
               this->render_blur_pass( this->m_blur_shader, this->m_blur_texture_a, this->m_blur_texture_b, step_x, 0.0f ) &&
               this->render_blur_pass( this->m_blur_shader, this->m_blur_texture_b, this->m_blur_texture_a, 0.0f, step_y );
    }

    bool backend::render_blur_pass( ID3D11PixelShader *shader, const d3d11::texture &source,
                                    const d3d11::texture &destination, float step_x, float step_y )
    {
        ID3D11ShaderResourceView *unbound{ };

        this->m_context->PSSetShaderResources( 0, 1, &unbound );

        if ( !this->setup_render_state( destination.m_target, this->m_blur_width, this->m_blur_height, step_x, step_y ) )
        {
            return false;
        }

        constexpr float     blend_factor[ 4 ]{ };
        const float         right{ static_cast< float >( this->m_blur_width ) - 0.5f };
        const float         bottom{ static_cast< float >( this->m_blur_height ) - 0.5f };
        constexpr float     left{ -0.5f };
        constexpr float     top{ -0.5f };
        constexpr u32       color{ 0xFFFFFFFFu };
        ID3D11SamplerState *sampler{ this->m_linear_sampler };

        const d3d9::overlay_vertex vertices[ 6 ]{
            { left, top, 0.0f, 1.0f, color, 0.0f, 0.0f },
            { right, top, 0.0f, 1.0f, color, 1.0f, 0.0f },
            { left, bottom, 0.0f, 1.0f, color, 0.0f, 1.0f },
            { left, bottom, 0.0f, 1.0f, color, 0.0f, 1.0f },
            { right, top, 0.0f, 1.0f, color, 1.0f, 0.0f },
            { right, bottom, 0.0f, 1.0f, color, 1.0f, 1.0f },
        };

        this->m_context->OMSetBlendState( this->m_opaque_state, blend_factor, 0xFFFFFFFFu );
        this->m_context->PSSetShader( shader, nullptr, 0 );
        this->m_context->PSSetShaderResources( 0, 1, &source.m_view );
        this->m_context->PSSetSamplers( 0, 1, &sampler );
        return this->draw_vertices( vertices, 6, false );
    }

    bool backend::ensure_vertex_capacity( usize count )
    {
        if ( count <= this->m_vertex_capacity )
        {
            return true;
        }

        release( this->m_vertex_buffer );
        this->m_vertex_capacity = 0;

        const usize capacity{ math::bigger( count + count / 2, static_cast< usize >( 4096 ) ) };

        D3D11_BUFFER_DESC description{ };
        description.ByteWidth      = static_cast< UINT >( capacity * sizeof( d3d9::overlay_vertex ) );
        description.Usage          = D3D11_USAGE_DYNAMIC;
        description.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if ( FAILED( this->m_device->CreateBuffer( &description, nullptr, &this->m_vertex_buffer ) ) )
        {
            return false;
        }

        this->m_vertex_capacity = static_cast< u32 >( capacity );
        return true;
    }

    bool backend::create_device_objects( )
    {
        const D3D11_INPUT_ELEMENT_DESC layout[ 3 ]{
            { ATLAS_XORSTR_STATIC( "POSITION" ), 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { ATLAS_XORSTR_STATIC( "COLOR" ), 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { ATLAS_XORSTR_STATIC( "TEXCOORD" ), 0, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        static_assert( sizeof( d3d9::overlay_vertex ) == 28 && __builtin_offsetof( d3d9::overlay_vertex, color ) == 16 &&
                       __builtin_offsetof( d3d9::overlay_vertex, u ) == 20 );

        D3D11_BUFFER_DESC constants{ };
        constants.ByteWidth      = 16;
        constants.Usage          = D3D11_USAGE_DYNAMIC;
        constants.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_BLEND_DESC blend{ };
        blend.RenderTarget[ 0 ].BlendEnable           = TRUE;
        blend.RenderTarget[ 0 ].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        blend.RenderTarget[ 0 ].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[ 0 ].BlendOp               = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[ 0 ].SrcBlendAlpha         = D3D11_BLEND_ONE;
        blend.RenderTarget[ 0 ].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[ 0 ].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        D3D11_BLEND_DESC opaque{ };
        opaque.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        D3D11_RASTERIZER_DESC raster{ };
        raster.FillMode        = D3D11_FILL_SOLID;
        raster.CullMode        = D3D11_CULL_NONE;
        raster.DepthClipEnable = TRUE;
        raster.ScissorEnable   = TRUE;

        const D3D11_DEPTH_STENCIL_DESC depth{ };

        D3D11_SAMPLER_DESC point{ };
        point.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
        point.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        point.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        point.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        point.MaxLOD   = D3D11_FLOAT32_MAX;

        D3D11_SAMPLER_DESC linear{ point };
        linear.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

        return SUCCEEDED( this->m_device->CreateVertexShader( shaders::OVERLAY11_VERTEX_SHADER, sizeof( shaders::OVERLAY11_VERTEX_SHADER ), nullptr, &this->m_vertex_shader ) ) &&
               SUCCEEDED( this->m_device->CreateInputLayout( layout, 3, shaders::OVERLAY11_VERTEX_SHADER, sizeof( shaders::OVERLAY11_VERTEX_SHADER ), &this->m_input_layout ) ) &&
               SUCCEEDED( this->m_device->CreatePixelShader( shaders::OVERLAY11_SOLID_PIXEL_SHADER, sizeof( shaders::OVERLAY11_SOLID_PIXEL_SHADER ), nullptr, &this->m_solid_shader ) ) &&
               SUCCEEDED( this->m_device->CreatePixelShader( shaders::OVERLAY11_TEXTURED_PIXEL_SHADER, sizeof( shaders::OVERLAY11_TEXTURED_PIXEL_SHADER ), nullptr, &this->m_textured_shader ) ) &&
               SUCCEEDED( this->m_device->CreatePixelShader( shaders::OVERLAY11_TINTED_PIXEL_SHADER, sizeof( shaders::OVERLAY11_TINTED_PIXEL_SHADER ), nullptr, &this->m_tinted_shader ) ) &&
               SUCCEEDED( this->m_device->CreatePixelShader( shaders::OVERLAY11_BLUR_PIXEL_SHADER, sizeof( shaders::OVERLAY11_BLUR_PIXEL_SHADER ), nullptr, &this->m_blur_shader ) ) &&
               SUCCEEDED( this->m_device->CreateBuffer( &constants, nullptr, &this->m_constant_buffer ) ) &&
               SUCCEEDED( this->m_device->CreateBlendState( &blend, &this->m_blend_state ) ) &&
               SUCCEEDED( this->m_device->CreateBlendState( &opaque, &this->m_opaque_state ) ) &&
               SUCCEEDED( this->m_device->CreateRasterizerState( &raster, &this->m_raster_state ) ) &&
               SUCCEEDED( this->m_device->CreateDepthStencilState( &depth, &this->m_depth_state ) ) &&
               SUCCEEDED( this->m_device->CreateSamplerState( &point, &this->m_point_sampler ) ) &&
               SUCCEEDED( this->m_device->CreateSamplerState( &linear, &this->m_linear_sampler ) );
    }

    void backend::release_device_objects( )
    {
        this->release_text_atlases( );
        this->release_icon_atlases( );
        this->release_avatars( );
        this->release_html_textures( );
        this->release_target( );
        release( this->m_vertex_shader );
        release( this->m_input_layout );
        release( this->m_solid_shader );
        release( this->m_textured_shader );
        release( this->m_tinted_shader );
        release( this->m_blur_shader );
        release( this->m_vertex_buffer );
        release( this->m_constant_buffer );
        release( this->m_blend_state );
        release( this->m_opaque_state );
        release( this->m_raster_state );
        release( this->m_depth_state );
        release( this->m_point_sampler );
        release( this->m_linear_sampler );
        release( this->m_context );
        release( this->m_device );
        this->m_vertex_capacity = 0;
        this->m_swap_chain      = nullptr;
    }

    bool backend::attach( IDXGISwapChain *swap_chain )
    {
        this->release_device_objects( );

        if ( FAILED( swap_chain->GetDevice( __uuidof( ID3D11Device ), reinterpret_cast< void ** >( &this->m_device ) ) ) )
        {
            return false;
        }

        this->m_device->GetImmediateContext( &this->m_context );

        if ( !this->create_device_objects( ) )
        {
            this->release_device_objects( );
            return false;
        }

        this->m_swap_chain = swap_chain;
        return true;
    }

    bool backend::ensure_target( )
    {
        if ( this->m_target )
        {
            return true;
        }

        ID3D11Texture2D *back_buffer{ };

        if ( FAILED( this->m_swap_chain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast< void ** >( &back_buffer ) ) ) )
        {
            return false;
        }

        D3D11_TEXTURE2D_DESC description{ };
        back_buffer->GetDesc( &description );

        D3D11_RENDER_TARGET_VIEW_DESC view{ };
        view.Format        = unorm_format( description.Format );
        view.ViewDimension = description.SampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS
                                                             : D3D11_RTV_DIMENSION_TEXTURE2D;

        const bool created{ SUCCEEDED( this->m_device->CreateRenderTargetView( back_buffer, &view, &this->m_target ) ) };
        back_buffer->Release( );

        if ( !created )
        {
            return false;
        }

        this->m_width          = description.Width;
        this->m_height         = description.Height;
        this->m_target_format  = description.Format;
        this->m_target_samples = description.SampleDesc.Count;
        return true;
    }

    void backend::release_target( )
    {
        release( this->m_target );
        this->release_blur_resources( );
    }

    void backend::shutdown( )
    {
        this->m_inside_frame         = false;
        this->m_frame_build_failed   = false;
        this->m_last_frame_succeeded = false;
        this->release_device_objects( );
        this->m_frame_commands.reset( );
        this->m_vertices.reset( );
        this->m_batch_type = batch_type::none;
        this->m_fonts.shutdown( );
        this->m_initialized = false;
    }

    bool backend::start_frame( IDXGISwapChain *swap_chain )
    {
        Assert( this->m_clip_enabled != true );
        this->m_last_frame_succeeded = false;
        this->m_clip_enabled         = false;

        if ( !this->m_initialized || this->m_inside_frame )
        {
            return false;
        }

        if ( this->m_swap_chain != swap_chain && !this->attach( swap_chain ) )
        {
            return false;
        }

        if ( !this->ensure_target( ) )
        {
            return false;
        }

        if ( !this->m_icon_atlases[ icons::KIND_SMALL ].m_ready )
        {
            this->build_icon_atlases( );
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
        if ( this->m_frame_commands.empty( ) )
        {
            return true;
        }

        this->m_vertices.clear( );
        this->m_batch_type        = batch_type::none;
        this->m_active_text_atlas = nullptr;
        this->m_blur_frame_ready  = false;
        this->m_scissor           = rect_t{ };
        this->m_scissor_enabled   = false;

        usize required_vertices{ };
        bool  blur_requested{ };

        if ( !this->count_vertices( this->m_frame_commands, &required_vertices ) ||
             !this->m_vertices.reserve( required_vertices ) )
        {
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

        saved_state_t state{ };
        save_state( this->m_context, state );

        if ( blur_requested && this->ensure_blur_resources( ) )
        {
            ID3D11Resource *back_buffer{ };
            this->m_target->GetResource( &back_buffer );
            this->m_blur_frame_ready = this->render_blur( static_cast< ID3D11Texture2D * >( back_buffer ) );
            back_buffer->Release( );
        }

        bool frame_status{ this->setup_render_state( this->m_target, this->m_width, this->m_height, 0.0f, 0.0f ) };

        for ( const render_command &command : this->m_frame_commands )
        {
            if ( !frame_status )
            {
                break;
            }

            frame_status = this->append_command( command );
        }

        frame_status = frame_status && this->flush_batch( );

        restore_state( this->m_context, state );

        this->m_vertices.clear( );
        this->m_batch_type        = batch_type::none;
        this->m_active_text_atlas = nullptr;
        return frame_status;
    }
}
