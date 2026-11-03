#pragma once

#include "util/typedefs.hpp"

struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11Buffer;
struct ID3D11BlendState;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11SamplerState;

namespace render::d3d11
{
    class texture
    {
    public:
        explicit operator bool( ) const
        {
            return this->m_view != nullptr;
        }

        void Release( );

        void Detach( )
        {
            *this = texture{ };
        }

        ID3D11Texture2D          *m_texture{ };
        ID3D11ShaderResourceView *m_view{ };
        ID3D11RenderTargetView   *m_target{ };
    };
}
