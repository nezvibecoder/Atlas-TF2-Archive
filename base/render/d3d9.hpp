#pragma once

#include "util/address.hpp"

namespace render::d3d9
{
    inline constexpr i32 D3D_OK{ 0 };
    inline constexpr i32 D3DERR_NOTFOUND{ static_cast< i32 >( 0x88760866u ) };
    inline constexpr i32 D3DERR_INVALIDCALL{ static_cast< i32 >( 0x8876086Cu ) };
    inline constexpr u32 D3DBACKBUFFER_TYPE_MONO{ 0 };
    inline constexpr u32 D3DFMT_R8G8B8{ 20 };
    inline constexpr u32 D3DFMT_A8R8G8B8{ 21 };
    inline constexpr u32 D3DFMT_X8R8G8B8{ 22 };
    inline constexpr u32 D3DFMT_R5G6B5{ 23 };
    inline constexpr u32 D3DFMT_X1R5G5B5{ 24 };
    inline constexpr u32 D3DPOOL_DEFAULT{ 0 };
    inline constexpr u32 D3DPOOL_MANAGED{ 1 };
    inline constexpr u32 D3DPOOL_SYSTEMMEM{ 2 };
    inline constexpr u32 D3DUSAGE_RENDERTARGET{ 0x00000001 };
    inline constexpr u32 D3DUSAGE_DYNAMIC{ 0x00000200 };
    inline constexpr u32 D3DLOCK_READONLY{ 0x00000010 };
    inline constexpr u32 D3DLOCK_DISCARD{ 0x00002000 };
    inline constexpr u32 D3DQUERYTYPE_EVENT{ 8 };
    inline constexpr u32 D3DISSUE_END{ 0x00000001 };
    inline constexpr u32 D3DSBT_ALL{ 1 };
    inline constexpr u32 D3DCLEAR_TARGET{ 0x1 };
    inline constexpr u32 D3DCLEAR_STENCIL{ 0x4 };
    inline constexpr u32 D3DPT_LINELIST{ 2 };
    inline constexpr u32 D3DPT_TRIANGLELIST{ 4 };
    inline constexpr u32 D3DPT_TRIANGLESTRIP{ 5 };
    inline constexpr u32 D3DTEXF_NONE{ 0 };
    inline constexpr u32 D3DTEXF_POINT{ 1 };
    inline constexpr u32 D3DTEXF_LINEAR{ 2 };
    inline constexpr u32 D3DTADDRESS_CLAMP{ 3 };
    inline constexpr u32 D3DFVF_XYZRHW{ 0x00000004 };
    inline constexpr u32 D3DFVF_DIFFUSE{ 0x00000040 };
    inline constexpr u32 D3DFVF_TEX1{ 0x00000100 };
    inline constexpr u32 D3DFVF_OVERLAY_VERTEX{ D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 };
    inline constexpr u32 D3DBLEND_ONE{ 2 };
    inline constexpr u32 D3DBLEND_SRCALPHA{ 5 };
    inline constexpr u32 D3DBLEND_INVSRCALPHA{ 6 };
    inline constexpr u32 D3DBLENDOP_ADD{ 1 };
    inline constexpr u32 D3DCULL_NONE{ 1 };
    inline constexpr u32 D3DCULL_CW{ 2 };
    inline constexpr u32 D3DCULL_CCW{ 3 };
    inline constexpr u32 D3DFILL_WIREFRAME{ 2 };
    inline constexpr u32 D3DFILL_SOLID{ 3 };
    inline constexpr u32 D3DSHADE_GOURAUD{ 2 };
    inline constexpr u32 D3DCMP_EQUAL{ 3 };
    inline constexpr u32 D3DCMP_LESSEQUAL{ 4 };
    inline constexpr u32 D3DCMP_GREATER{ 5 };
    inline constexpr u32 D3DCMP_ALWAYS{ 8 };
    inline constexpr u32 D3DSTENCILOP_KEEP{ 1 };
    inline constexpr u32 D3DSTENCILOP_REPLACE{ 3 };
    inline constexpr u8  D3DDECLTYPE_FLOAT2{ 1 };
    inline constexpr u8  D3DDECLTYPE_FLOAT3{ 2 };
    inline constexpr u8  D3DDECLTYPE_D3DCOLOR{ 4 };
    inline constexpr u8  D3DDECLTYPE_UBYTE4{ 5 };
    inline constexpr u8  D3DDECLTYPE_SHORT2{ 6 };
    inline constexpr u8  D3DDECLTYPE_UNUSED{ 17 };
    inline constexpr u8  D3DDECLUSAGE_POSITION{ 0 };
    inline constexpr u8  D3DDECLUSAGE_BLENDWEIGHT{ 1 };
    inline constexpr u8  D3DDECLUSAGE_BLENDINDICES{ 2 };
    inline constexpr u8  D3DDECLUSAGE_NORMAL{ 3 };
    inline constexpr u32 D3DRS_ZENABLE{ 7 };
    inline constexpr u32 D3DRS_FILLMODE{ 8 };
    inline constexpr u32 D3DRS_SHADEMODE{ 9 };
    inline constexpr u32 D3DRS_ZWRITEENABLE{ 14 };
    inline constexpr u32 D3DRS_ALPHATESTENABLE{ 15 };
    inline constexpr u32 D3DRS_LASTPIXEL{ 16 };
    inline constexpr u32 D3DRS_SRCBLEND{ 19 };
    inline constexpr u32 D3DRS_DESTBLEND{ 20 };
    inline constexpr u32 D3DRS_CULLMODE{ 22 };
    inline constexpr u32 D3DRS_ZFUNC{ 23 };
    inline constexpr u32 D3DRS_ALPHABLENDENABLE{ 27 };
    inline constexpr u32 D3DRS_FOGENABLE{ 28 };
    inline constexpr u32 D3DRS_STENCILENABLE{ 52 };
    inline constexpr u32 D3DRS_STENCILFAIL{ 53 };
    inline constexpr u32 D3DRS_STENCILZFAIL{ 54 };
    inline constexpr u32 D3DRS_STENCILPASS{ 55 };
    inline constexpr u32 D3DRS_STENCILFUNC{ 56 };
    inline constexpr u32 D3DRS_STENCILREF{ 57 };
    inline constexpr u32 D3DRS_STENCILMASK{ 58 };
    inline constexpr u32 D3DRS_STENCILWRITEMASK{ 59 };
    inline constexpr u32 D3DRS_WRAP0{ 128 };
    inline constexpr u32 D3DRS_CLIPPING{ 136 };
    inline constexpr u32 D3DRS_LIGHTING{ 137 };
    inline constexpr u32 D3DRS_CLIPPLANEENABLE{ 152 };
    inline constexpr u32 D3DRS_MULTISAMPLEANTIALIAS{ 161 };
    inline constexpr u32 D3DRS_COLORWRITEENABLE{ 168 };
    inline constexpr u32 D3DRS_BLENDOP{ 171 };
    inline constexpr u32 D3DRS_SCISSORTESTENABLE{ 174 };
    inline constexpr u32 D3DRS_SLOPESCALEDEPTHBIAS{ 175 };
    inline constexpr u32 D3DRS_ANTIALIASEDLINEENABLE{ 176 };
    inline constexpr u32 D3DRS_COLORWRITEENABLE1{ 190 };
    inline constexpr u32 D3DRS_COLORWRITEENABLE2{ 191 };
    inline constexpr u32 D3DRS_COLORWRITEENABLE3{ 192 };
    inline constexpr u32 D3DRS_SRGBWRITEENABLE{ 194 };
    inline constexpr u32 D3DRS_DEPTHBIAS{ 195 };
    inline constexpr u32 D3DRS_SEPARATEALPHABLENDENABLE{ 206 };
    inline constexpr u32 D3DRS_SRCBLENDALPHA{ 207 };
    inline constexpr u32 D3DRS_DESTBLENDALPHA{ 208 };
    inline constexpr u32 D3DRS_BLENDOPALPHA{ 209 };
    inline constexpr u32 D3DTSS_COLOROP{ 1 };
    inline constexpr u32 D3DTSS_COLORARG1{ 2 };
    inline constexpr u32 D3DTSS_COLORARG2{ 3 };
    inline constexpr u32 D3DTSS_ALPHAOP{ 4 };
    inline constexpr u32 D3DTSS_ALPHAARG1{ 5 };
    inline constexpr u32 D3DTSS_ALPHAARG2{ 6 };
    inline constexpr u32 D3DTSS_TEXCOORDINDEX{ 11 };
    inline constexpr u32 D3DTSS_TEXTURETRANSFORMFLAGS{ 24 };
    inline constexpr u32 D3DTSS_RESULTARG{ 28 };
    inline constexpr u32 D3DTOP_DISABLE{ 1 };
    inline constexpr u32 D3DTOP_SELECTARG1{ 2 };
    inline constexpr u32 D3DTOP_SELECTARG2{ 3 };
    inline constexpr u32 D3DTOP_MODULATE{ 4 };
    inline constexpr u32 D3DTA_DIFFUSE{ 0 };
    inline constexpr u32 D3DTA_CURRENT{ 1 };
    inline constexpr u32 D3DTA_TEXTURE{ 2 };
    inline constexpr u32 D3DSAMP_ADDRESSU{ 1 };
    inline constexpr u32 D3DSAMP_ADDRESSV{ 2 };
    inline constexpr u32 D3DSAMP_MAGFILTER{ 5 };
    inline constexpr u32 D3DSAMP_MINFILTER{ 6 };
    inline constexpr u32 D3DSAMP_MIPFILTER{ 7 };
    inline constexpr u32 D3DSAMP_SRGBTEXTURE{ 11 };

    enum class DeviceMethod : u32
    {
        GetCreationParameters       = 9,
        GetSwapChain                = 14,
        Reset                       = 16,
        Present                     = 17,
        GetBackBuffer               = 18,
        CreateTexture               = 23,
        UpdateSurface               = 30,
        GetRenderTargetData         = 32,
        StretchRect                 = 34,
        ColorFill                   = 35,
        CreateOffscreenPlainSurface = 36,
        SetRenderTarget             = 37,
        GetRenderTarget             = 38,
        SetDepthStencilSurface      = 39,
        GetDepthStencilSurface      = 40,
        BeginScene                  = 41,
        EndScene                    = 42,
        Clear                       = 43,
        SetViewport                 = 47,
        GetViewport                 = 48,
        SetRenderState              = 57,
        GetRenderState              = 58,
        CreateStateBlock            = 59,
        BeginStateBlock             = 60,
        EndStateBlock               = 61,
        GetTexture                  = 64,
        SetTexture                  = 65,
        GetTextureStageState        = 66,
        SetTextureStageState        = 67,
        GetSamplerState             = 68,
        SetSamplerState             = 69,
        SetScissorRect              = 75,
        DrawIndexedPrimitive        = 82,
        DrawPrimitiveUP             = 83,
        CreateVertexDeclaration     = 86,
        SetVertexDeclaration        = 87,
        SetFVF                      = 89,
        GetFVF                      = 90,
        CreateVertexShader          = 91,
        SetVertexShader             = 92,
        GetVertexShader             = 93,
        SetVertexShaderConstantF    = 94,
        SetStreamSource             = 100,
        SetIndices                  = 104,
        CreatePixelShader           = 106,
        SetPixelShader              = 107,
        GetPixelShader              = 108,
        SetPixelShaderConstantF     = 109,
        CreateQuery                 = 118,
    };

    enum class QueryMethod : u32
    {
        Release = 2,
        Issue   = 6,
        GetData = 7,
    };

    enum class SurfaceMethod : u32
    {
        Release    = 2,
        GetContainer = 11,
        GetDesc    = 12,
        LockRect   = 13,
        UnlockRect = 14,
        GetDC      = 15,
        ReleaseDC  = 16,
    };

    enum class SwapChainMethod : u32
    {
        GetPresentParameters = 9,
    };

    enum class StateBlockMethod : u32
    {
        Release = 2,
        Capture = 4,
        Apply   = 5,
    };

    struct D3DPRESENT_PARAMETERS
    {
        u32   back_buffer_width{ };
        u32   back_buffer_height{ };
        u32   back_buffer_format{ };
        u32   back_buffer_count{ };
        u32   multi_sample_type{ };
        u32   multi_sample_quality{ };
        u32   swap_effect{ };
        void *device_window{ };
        i32   windowed{ };
        i32   enable_auto_depth_stencil{ };
        u32   auto_depth_stencil_format{ };
        u32   flags{ };
        u32   refresh_rate{ };
        u32   presentation_interval{ };
    };

    struct D3DDEVICE_CREATION_PARAMETERS
    {
        u32   adapter_ordinal{ };
        u32   device_type{ };
        void *focus_window{ };
        u32   behavior_flags{ };
    };

    struct D3DSURFACE_DESC
    {
        u32 format{ };
        u32 type{ };
        u32 usage{ };
        u32 pool{ };
        u32 multi_sample_type{ };
        u32 multi_sample_quality{ };
        u32 width{ };
        u32 height{ };
    };

    struct D3DLOCKED_RECT
    {
        i32   pitch{ };
        void *bits{ };
    };

    struct D3DVIEWPORT9
    {
        u32   x{ };
        u32   y{ };
        u32   width{ };
        u32   height{ };
        float min_z{ };
        float max_z{ };
    };

    struct D3DVERTEXELEMENT9
    {
        u16 stream{ };
        u16 offset{ };
        u8  type{ };
        u8  method{ };
        u8  usage{ };
        u8  usage_index{ };
    };

    inline constexpr D3DVERTEXELEMENT9 D3DDECL_END{ 0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0 };

    struct overlay_vertex
    {
        float x{ };
        float y{ };
        float z{ };
        float rhw{ };
        u32   color{ };
        float u{ };
        float v{ };
    };

    class object9
    {
    public:
        constexpr object9( ) = default;

        explicit constexpr object9( void *instance ) :
            m_instance{ instance }
        {
        }

        constexpr void Attach( void *instance )
        {
            this->m_instance = instance;
        }

        constexpr void *Get( ) const
        {
            return this->m_instance;
        }

        explicit constexpr operator bool( ) const
        {
            return this->m_instance != nullptr;
        }

        u32 Release( )
        {
            if ( !this->m_instance )
            {
                return 0;
            }

            void *instance{ this->m_instance };
            this->m_instance = nullptr;
            return util::call_virtual< u32( __stdcall * )( void * ) >( instance, 2 )( instance );
        }

    private:
        void *m_instance{ };
    };

    class state_block9
    {
    public:
        constexpr state_block9( ) = default;

        explicit constexpr state_block9( void *instance ) :
            m_instance{ instance }
        {
        }

        constexpr void Attach( void *instance )
        {
            this->m_instance = instance;
        }

        constexpr void *Get( ) const
        {
            return this->m_instance;
        }

        explicit constexpr operator bool( ) const
        {
            return this->m_instance != nullptr;
        }

        u32 Release( )
        {
            if ( !this->m_instance )
            {
                return 0;
            }

            void *instance{ this->m_instance };
            this->m_instance = nullptr;
            return util::call_virtual< u32( __stdcall * )( void * ) >(
                instance, static_cast< u32 >( StateBlockMethod::Release ) )( instance );
        }

        i32 Capture( ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void * ) >( this->m_instance, static_cast< u32 >( StateBlockMethod::Capture ) )( m_instance );
        }

        i32 Apply( ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void * ) >( this->m_instance, static_cast< u32 >( StateBlockMethod::Apply ) )( m_instance );
        }

    private:
        void *m_instance{ };
    };

    class query9
    {
    public:
        constexpr query9( ) = default;

        constexpr void Attach( void *instance )
        {
            this->m_instance = instance;
        }

        explicit constexpr operator bool( ) const
        {
            return this->m_instance != nullptr;
        }

        u32 Release( )
        {
            if ( !this->m_instance )
            {
                return 0;
            }

            void *instance{ this->m_instance };
            this->m_instance = nullptr;
            return util::call_virtual< u32( __stdcall * )( void * ) >(
                instance, static_cast< u32 >( QueryMethod::Release ) )( instance );
        }

        i32 Issue( u32 flags ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32 ) >( this->m_instance, static_cast< u32 >( QueryMethod::Issue ) )( m_instance, flags );
        }

        i32 GetData( u32 flags ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, void *, u32, u32 ) >( this->m_instance, static_cast< u32 >( QueryMethod::GetData ) )( m_instance, nullptr, 0, flags );
        }

    private:
        void *m_instance{ };
    };

    class surface9;

    class texture9
    {
    public:
        constexpr texture9( ) = default;

        explicit constexpr texture9( void *instance ) :
            m_instance{ instance }
        {
        }

        constexpr void Attach( void *instance )
        {
            this->m_instance = instance;
        }

        constexpr void Detach( )
        {
            this->m_instance = nullptr;
        }

        constexpr void *Get( ) const
        {
            return this->m_instance;
        }

        explicit constexpr operator bool( ) const
        {
            return this->m_instance != nullptr;
        }

        i32 GetSurfaceLevel( u32 level, surface9 *surface ) const;

        u32 Release( )
        {
            if ( !this->m_instance )
            {
                return 0;
            }

            void *instance{ this->m_instance };
            this->m_instance = nullptr;
            return util::call_virtual< u32( __stdcall * )( void * ) >( instance, 2 )( instance );
        }

        i32 LockRect( u32 level, D3DLOCKED_RECT *locked, const rect_t *bounds, u32 flags ) const
        {
            if ( !this->m_instance || !locked )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, D3DLOCKED_RECT *, const rect_t *, u32 ) >( this->m_instance, 19 )( m_instance, level, locked, bounds, flags );
        }

        i32 UnlockRect( u32 level ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32 ) >( this->m_instance, 20 )( m_instance, level );
        }

    private:
        void *m_instance{ };
    };

    class surface9
    {
    public:
        constexpr surface9( ) = default;

        explicit constexpr surface9( void *instance ) :
            m_instance{ instance }
        {
        }

        constexpr void Attach( void *instance )
        {
            this->m_instance = instance;
        }

        constexpr void Detach( )
        {
            this->m_instance = nullptr;
        }

        constexpr void *Get( ) const
        {
            return this->m_instance;
        }

        explicit constexpr operator bool( ) const
        {
            return this->m_instance != nullptr;
        }

        u32 Release( )
        {
            if ( !this->m_instance )
            {
                return 0;
            }

            void *instance{ this->m_instance };
            this->m_instance = nullptr;
            return util::call_virtual< u32( __stdcall * )( void * ) >(
                instance, static_cast< u32 >( SurfaceMethod::Release ) )( instance );
        }

        i32 GetDesc( D3DSURFACE_DESC *description ) const
        {
            if ( !this->m_instance || !description )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, D3DSURFACE_DESC * ) >( this->m_instance, static_cast< u32 >( SurfaceMethod::GetDesc ) )( m_instance, description );
        }

        i32 GetTextureContainer( texture9 *texture ) const
        {
            if ( !this->m_instance || !texture )
            {
                return -1;
            }

            struct guid_t
            {
                u32 m_data1;
                u16 m_data2;
                u16 m_data3;
                u8  m_data4[ 8 ];
            };
            static constexpr guid_t iid{ 0x85C31227u, 0x3DE5u, 0x4F00u,
                                          { 0x9B, 0x3A, 0xF1, 0x1A, 0xC3, 0x8C, 0x18, 0xB5 } };
            static_assert( sizeof( guid_t ) == 16 );

            void *instance{ };
            const i32 status{ util::call_virtual< i32( __stdcall * )( void *, const guid_t &, void ** ) >(
                this->m_instance, static_cast< u32 >( SurfaceMethod::GetContainer ) )(
                this->m_instance, iid, &instance ) };
            texture->Attach( instance );
            return status;
        }

        i32 GetDC( void **context ) const
        {
            if ( !this->m_instance || !context )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, void ** ) >( this->m_instance, static_cast< u32 >( SurfaceMethod::GetDC ) )( m_instance, context );
        }

        i32 ReleaseDC( void *context ) const
        {
            if ( !this->m_instance || !context )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, void * ) >( this->m_instance, static_cast< u32 >( SurfaceMethod::ReleaseDC ) )( m_instance, context );
        }

        i32 LockRect( D3DLOCKED_RECT *locked, const rect_t *bounds, u32 flags ) const
        {
            if ( !this->m_instance || !locked )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, D3DLOCKED_RECT *, const rect_t *, u32 ) >( this->m_instance, static_cast< u32 >( SurfaceMethod::LockRect ) )( m_instance, locked, bounds, flags );
        }

        i32 UnlockRect( ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void * ) >( this->m_instance, static_cast< u32 >( SurfaceMethod::UnlockRect ) )( m_instance );
        }

    private:
        void *m_instance{ };
    };

    inline i32 texture9::GetSurfaceLevel( u32 level, surface9 *surface ) const
    {
        if ( !this->m_instance || !surface )
        {
            return -1;
        }

        void *instance{ };
        i32   status{ util::call_virtual< i32( __stdcall * )( void *, u32, void ** ) >(
            this->m_instance, 18 )( this->m_instance, level, &instance ) };

        if ( status >= D3D_OK )
        {
            surface->Attach( instance );
        }

        return status;
    }

    class device9
    {
    public:
        constexpr device9( ) = default;

        explicit constexpr device9( void *instance ) :
            m_instance{ instance }
        {
        }

        constexpr void Attach( void *instance )
        {
            this->m_instance = instance;
        }

        constexpr void Detach( )
        {
            this->m_instance = nullptr;
        }

        constexpr void *Get( ) const
        {
            return this->m_instance;
        }

        explicit constexpr operator bool( ) const
        {
            return this->m_instance != nullptr;
        }

        i32 BeginScene( ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void * ) >( this->m_instance, static_cast< u32 >( DeviceMethod::BeginScene ) )( m_instance );
        }

        i32 EndScene( ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void * ) >( this->m_instance, static_cast< u32 >( DeviceMethod::EndScene ) )( m_instance );
        }

        i32 GetCreationParameters( D3DDEVICE_CREATION_PARAMETERS *parameters ) const
        {
            if ( !this->m_instance || !parameters )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, D3DDEVICE_CREATION_PARAMETERS * ) >( this->m_instance,
                                                                                                        static_cast< u32 >( DeviceMethod::GetCreationParameters ) )( m_instance, parameters );
        }

        i32 GetViewport( D3DVIEWPORT9 *viewport ) const
        {
            if ( !this->m_instance || !viewport )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, D3DVIEWPORT9 * ) >( this->m_instance, static_cast< u32 >( DeviceMethod::GetViewport ) )( m_instance, viewport );
        }

        i32 SetViewport( const D3DVIEWPORT9 *viewport ) const
        {
            if ( !this->m_instance || !viewport )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, const D3DVIEWPORT9 * ) >( this->m_instance, static_cast< u32 >( DeviceMethod::SetViewport ) )( m_instance, viewport );
        }

        i32 SetRenderState( u32 state, u32 value ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, u32 ) >( this->m_instance, static_cast< u32 >( DeviceMethod::SetRenderState ) )( m_instance, state, value );
        }

        i32 SetScissorRect( const rect_t *bounds ) const
        {
            if ( !this->m_instance || !bounds )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, const rect_t * ) >( this->m_instance, static_cast< u32 >( DeviceMethod::SetScissorRect ) )( m_instance, bounds );
        }

        i32 GetRenderState( u32 state, u32 *value ) const
        {
            if ( !this->m_instance || !value )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, u32 * ) >( this->m_instance, static_cast< u32 >( DeviceMethod::GetRenderState ) )( m_instance, state, value );
        }

        i32 CreateStateBlock( u32 type, state_block9 *state_block ) const
        {
            if ( !this->m_instance || !state_block )
            {
                return -1;
            }

            void *instance{ };
            i32   status{ util::call_virtual< i32( __stdcall * )( void *, u32, void ** ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::CreateStateBlock ) )(
                this->m_instance, type, &instance ) };

            if ( status >= D3D_OK )
            {
                state_block->Attach( instance );
            }

            return status;
        }

        i32 CreateQuery( u32 type, query9 *query ) const
        {
            if ( !this->m_instance || !query )
            {
                return -1;
            }

            void *instance{ };
            i32   status{ util::call_virtual< i32( __stdcall * )( void *, u32, void ** ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::CreateQuery ) )(
                this->m_instance, type, &instance ) };

            if ( status >= D3D_OK )
            {
                query->Attach( instance );
            }

            return status;
        }

        i32 BeginStateBlock( ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void * ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::BeginStateBlock ) )( this->m_instance );
        }

        i32 EndStateBlock( state_block9 *state_block ) const
        {
            if ( !this->m_instance || !state_block )
            {
                return -1;
            }

            void *instance{ };
            i32   status{ util::call_virtual< i32( __stdcall * )( void *, void ** ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::EndStateBlock ) )( this->m_instance, &instance ) };

            if ( status >= D3D_OK )
            {
                state_block->Attach( instance );
            }

            return status;
        }

        i32 Clear( u32 flags, u32 color, float z, u32 stencil ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, const void *, u32, u32, float, u32 ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::Clear ) )(
                this->m_instance, 0, nullptr, flags, color, z, stencil );
        }

        i32 SetTextureStageState( u32 stage, u32 state, u32 value ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, u32, u32 ) >( this->m_instance, static_cast< u32 >( DeviceMethod::SetTextureStageState ) )( m_instance, stage, state, value );
        }

        i32 SetSamplerState( u32 sampler, u32 state, u32 value ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, u32, u32 ) >( this->m_instance, static_cast< u32 >( DeviceMethod::SetSamplerState ) )( m_instance, sampler, state, value );
        }

        i32 GetSamplerState( u32 sampler, u32 state, u32 *value ) const
        {
            if ( !this->m_instance || !value )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, u32, u32 * ) >( this->m_instance, static_cast< u32 >( DeviceMethod::GetSamplerState ) )( m_instance, sampler, state, value );
        }

        i32 GetTextureStageState( u32 stage, u32 state, u32 *value ) const
        {
            if ( !this->m_instance || !value )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, u32, u32 * ) >( this->m_instance, static_cast< u32 >( DeviceMethod::GetTextureStageState ) )( m_instance, stage, state, value );
        }

        i32 SetTexture( u32 stage, const texture9 *texture ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, void * ) >( this->m_instance, static_cast< u32 >( DeviceMethod::SetTexture ) )( m_instance, stage, texture ? texture->Get( ) : nullptr );
        }

        i32 GetTexture( u32 stage, texture9 *texture ) const
        {
            if ( !this->m_instance || !texture )
            {
                return -1;
            }

            void *instance{ };
            i32   status{ util::call_virtual< i32( __stdcall * )( void *, u32, void ** ) >( this->m_instance, static_cast< u32 >( DeviceMethod::GetTexture ) )( m_instance, stage, &instance ) };

            if ( status >= D3D_OK )
            {
                texture->Attach( instance );
            }

            return status;
        }

        i32 SetVertexShader( const object9 *shader ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, void * ) >( this->m_instance, static_cast< u32 >( DeviceMethod::SetVertexShader ) )( m_instance, shader ? shader->Get( ) : nullptr );
        }

        i32 GetVertexShader( object9 *shader ) const
        {
            if ( !this->m_instance || !shader )
            {
                return -1;
            }

            void *instance{ };
            i32   status{ util::call_virtual< i32( __stdcall * )( void *, void ** ) >( this->m_instance, static_cast< u32 >( DeviceMethod::GetVertexShader ) )( m_instance, &instance ) };

            if ( status >= D3D_OK )
            {
                shader->Attach( instance );
            }

            return status;
        }

        i32 CreateVertexShader( const u32 *function, object9 *shader ) const
        {
            if ( !this->m_instance || !function || !shader )
            {
                return -1;
            }

            void *instance{ };
            i32   status{
                util::call_virtual< i32( __stdcall * )( void *, const u32 *, void ** ) >(
                    this->m_instance,
                    static_cast< u32 >( DeviceMethod::CreateVertexShader ) )(
                    this->m_instance, function, &instance )
            };

            if ( status >= D3D_OK )
            {
                shader->Attach( instance );
            }

            return status;
        }

        i32 SetVertexShaderConstantF( u32 start_register, const float *data, u32 vector_count ) const
        {
            if ( !this->m_instance || !data || !vector_count )
            {
                return -1;
            }

            return util::call_virtual<
                i32( __stdcall * )( void *, u32, const float *, u32 ) >(
                this->m_instance,
                static_cast< u32 >( DeviceMethod::SetVertexShaderConstantF ) )(
                this->m_instance, start_register, data, vector_count );
        }

        i32 CreateVertexDeclaration( const D3DVERTEXELEMENT9 *elements, object9 *declaration ) const
        {
            if ( !this->m_instance || !elements || !declaration )
            {
                return -1;
            }

            void *instance{ };
            i32   status{
                util::call_virtual< i32( __stdcall * )( void *, const D3DVERTEXELEMENT9 *, void ** ) >(
                    this->m_instance,
                    static_cast< u32 >( DeviceMethod::CreateVertexDeclaration ) )(
                    this->m_instance, elements, &instance )
            };

            if ( status >= D3D_OK )
            {
                declaration->Attach( instance );
            }

            return status;
        }

        i32 SetVertexDeclaration( const object9 *declaration ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, void * ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::SetVertexDeclaration ) )(
                this->m_instance, declaration ? declaration->Get( ) : nullptr );
        }

        i32 SetStreamSource( u32 stream, void *buffer, u32 offset, u32 stride ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, void *, u32, u32 ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::SetStreamSource ) )(
                this->m_instance, stream, buffer, offset, stride );
        }

        i32 SetIndices( void *buffer ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, void * ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::SetIndices ) )(
                this->m_instance, buffer );
        }

        i32 DrawIndexedPrimitive( u32 primitive_type, i32 base_vertex, u32 min_index, u32 vertex_count,
                                  u32 start_index, u32 primitive_count ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, i32, u32, u32, u32, u32 ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::DrawIndexedPrimitive ) )(
                this->m_instance, primitive_type, base_vertex, min_index, vertex_count, start_index,
                primitive_count );
        }

        i32 SetPixelShader( const object9 *shader ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, void * ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::SetPixelShader ) )(
                this->m_instance, shader ? shader->Get( ) : nullptr );
        }

        i32 CreatePixelShader( const u32 *function, object9 *shader ) const
        {
            if ( !this->m_instance || !function || !shader )
            {
                return -1;
            }

            void *instance{ };
            i32   status{
                util::call_virtual< i32( __stdcall * )( void *, const u32 *, void ** ) >(
                    this->m_instance,
                    static_cast< u32 >( DeviceMethod::CreatePixelShader ) )(
                    this->m_instance, function, &instance )
            };

            if ( status >= D3D_OK )
            {
                shader->Attach( instance );
            }

            return status;
        }

        i32 GetPixelShader( object9 *shader ) const
        {
            if ( !this->m_instance || !shader )
            {
                return -1;
            }

            void *instance{ };
            i32   status{ util::call_virtual< i32( __stdcall * )( void *, void ** ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::GetPixelShader ) )(
                this->m_instance, &instance ) };

            if ( status >= D3D_OK )
            {
                shader->Attach( instance );
            }

            return status;
        }

        i32 SetPixelShaderConstantF( u32 start_register, const float *data, u32 vector_count ) const
        {
            if ( !this->m_instance || !data || !vector_count )
            {
                return -1;
            }

            return util::call_virtual<
                i32( __stdcall * )( void *, u32, const float *, u32 ) >(
                this->m_instance,
                static_cast< u32 >( DeviceMethod::SetPixelShaderConstantF ) )(
                this->m_instance, start_register, data, vector_count );
        }

        i32 SetFVF( u32 value ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32 ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::SetFVF ) )(
                this->m_instance, value );
        }

        i32 GetFVF( u32 *value ) const
        {
            if ( !this->m_instance || !value )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32 * ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::GetFVF ) )(
                this->m_instance, value );
        }

        i32 GetBackBuffer( u32 swap_chain, u32 index, surface9 *surface ) const
        {
            if ( !this->m_instance || !surface )
            {
                return -1;
            }

            void *instance{ };
            i32   status{
                util::call_virtual< i32( __stdcall * )( void *, u32, u32, u32, void ** ) >(
                    this->m_instance,
                    static_cast< u32 >( DeviceMethod::GetBackBuffer ) )(
                    this->m_instance, swap_chain, index, D3DBACKBUFFER_TYPE_MONO, &instance )
            };

            if ( status >= D3D_OK )
            {
                surface->Attach( instance );
            }

            return status;
        }

        i32 CreateOffscreenPlainSurface( u32 width, u32 height, u32 format, u32 pool, surface9 *surface ) const
        {
            if ( !this->m_instance || !surface )
            {
                return -1;
            }

            void *instance{ };
            i32   status{
                util::call_virtual<
                      i32( __stdcall * )( void *, u32, u32, u32, u32, void **, void ** ) >(
                    this->m_instance,
                    static_cast< u32 >( DeviceMethod::CreateOffscreenPlainSurface ) )(
                    this->m_instance, width, height, format, pool, &instance, nullptr )
            };

            if ( status >= D3D_OK )
            {
                surface->Attach( instance );
            }

            return status;
        }

        i32 ColorFill( const surface9 *surface, const rect_t *bounds, u32 color ) const
        {
            if ( !this->m_instance || !surface || !*surface )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, void *, const rect_t *, u32 ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::ColorFill ) )(
                this->m_instance, surface->Get( ), bounds, color );
        }

        i32 SetRenderTarget( u32 index, const surface9 *surface ) const
        {
            if ( !this->m_instance || ( !index && ( !surface || !*surface ) ) ||
                 ( surface && !*surface ) )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, u32, void * ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::SetRenderTarget ) )(
                this->m_instance, index, surface ? surface->Get( ) : nullptr );
        }

        i32 GetRenderTarget( u32 index, surface9 *surface ) const
        {
            if ( !this->m_instance || !surface )
            {
                return -1;
            }

            void *instance{ };
            i32   status{ util::call_virtual< i32( __stdcall * )( void *, u32, void ** ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::GetRenderTarget ) )(
                this->m_instance, index, &instance ) };

            if ( status >= D3D_OK )
            {
                surface->Attach( instance );
            }

            return status;
        }

        i32 SetDepthStencilSurface( const surface9 *surface ) const
        {
            if ( !this->m_instance )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, void * ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::SetDepthStencilSurface ) )(
                this->m_instance, surface && *surface ? surface->Get( ) : nullptr );
        }

        i32 GetDepthStencilSurface( surface9 *surface ) const
        {
            if ( !this->m_instance || !surface )
            {
                return -1;
            }

            void *instance{ };
            i32   status{ util::call_virtual< i32( __stdcall * )( void *, void ** ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::GetDepthStencilSurface ) )(
                this->m_instance, &instance ) };

            if ( status >= D3D_OK )
            {
                surface->Attach( instance );
            }

            return status;
        }

        i32 CreateTexture( u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool,
                           texture9 *texture ) const
        {
            if ( !this->m_instance || !texture )
            {
                return -1;
            }

            void *instance{ };
            i32   status{
                util::call_virtual<
                      i32( __stdcall * )( void *, u32, u32, u32, u32, u32, u32, void **, void ** ) >(
                    this->m_instance,
                    static_cast< u32 >( DeviceMethod::CreateTexture ) )(
                    this->m_instance, width, height, levels, usage, format, pool, &instance, nullptr )
            };

            if ( status >= D3D_OK )
            {
                texture->Attach( instance );
            }

            return status;
        }

        i32 UpdateSurface( const surface9 *source, const rect_t *source_rect, const surface9 *destination,
                           const point_t *destination_point ) const
        {
            if ( !this->m_instance || !source || !*source || !destination || !*destination )
            {
                return -1;
            }

            return util::call_virtual<
                i32( __stdcall * )( void *, void *, const rect_t *, void *, const point_t * ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::UpdateSurface ) )(
                this->m_instance, source->Get( ), source_rect, destination->Get( ),
                destination_point );
        }

        i32 GetRenderTargetData( const surface9 *source, const surface9 *destination ) const
        {
            if ( !this->m_instance || !source || !*source || !destination || !*destination )
            {
                return -1;
            }

            return util::call_virtual< i32( __stdcall * )( void *, void *, void * ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::GetRenderTargetData ) )(
                this->m_instance, source->Get( ), destination->Get( ) );
        }

        i32 GetPresentParameters( D3DPRESENT_PARAMETERS *parameters ) const
        {
            if ( !this->m_instance || !parameters )
            {
                return -1;
            }

            void *instance{ };
            i32   status{ util::call_virtual< i32( __stdcall * )( void *, u32, void ** ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::GetSwapChain ) )(
                this->m_instance, 0, &instance ) };

            if ( status < D3D_OK || !instance )
            {
                return status;
            }

            object9 swap_chain{ instance };
            status = util::call_virtual< i32( __stdcall * )( void *, D3DPRESENT_PARAMETERS * ) >(
                instance, static_cast< u32 >( SwapChainMethod::GetPresentParameters ) )( instance, parameters );
            swap_chain.Release( );
            return status;
        }

        i32 StretchRect( const surface9 *source, const rect_t *source_rect, const surface9 *destination,
                         const rect_t *destination_rect, u32 filter ) const
        {
            if ( !this->m_instance || !source || !*source || !destination || !*destination )
            {
                return -1;
            }

            return util::call_virtual<
                i32( __stdcall * )( void *, void *, const rect_t *, void *, const rect_t *, u32 ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::StretchRect ) )(
                this->m_instance, source->Get( ), source_rect, destination->Get( ),
                destination_rect, filter );
        }

        i32 DrawPrimitiveUP( u32 primitive_type, u32 primitive_count, const void *data, u32 stride ) const
        {
            if ( !this->m_instance || !data || !stride )
            {
                return -1;
            }

            return util::call_virtual<
                i32( __stdcall * )( void *, u32, u32, const void *, u32 ) >(
                this->m_instance, static_cast< u32 >( DeviceMethod::DrawPrimitiveUP ) )(
                this->m_instance, primitive_type, primitive_count, data, stride );
        }

    private:
        void *m_instance{ };
    };
} // namespace render::d3d9
