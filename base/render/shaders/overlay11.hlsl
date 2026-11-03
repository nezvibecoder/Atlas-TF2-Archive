// fxc /nologo /O3 /T vs_4_0 /E vertex   /Fo vertex.cso   overlay11.hlsl
// fxc /nologo /O3 /T ps_4_0 /E solid    /Fo solid.cso    overlay11.hlsl
// fxc /nologo /O3 /T ps_4_0 /E textured /Fo textured.cso overlay11.hlsl
// fxc /nologo /O3 /T ps_4_0 /E tinted   /Fo tinted.cso   overlay11.hlsl
// fxc /nologo /O3 /T ps_4_0 /E blur     /Fo blur.cso     overlay11.hlsl

cbuffer frame : register( b0 )
{
    float4 g_screen; // xy: 2 / target size, zw: blur step in uv
};

Texture2D    g_texture : register( t0 );
SamplerState g_sampler : register( s0 );

struct vertex_t
{
    float4 position : POSITION;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD;
};

struct pixel_t
{
    float4 position : SV_Position;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD;
};

// The encoder emits d3d9 pre-transformed vertices, whose pixel centres sit on integers; d3d10+ centres sit on
// halves, so the half pixel goes back on before mapping to clip space.
pixel_t vertex( vertex_t input )
{
    pixel_t output;
    output.position = float4( ( input.position.x + 0.5f ) * g_screen.x - 1.0f,
                              1.0f - ( input.position.y + 0.5f ) * g_screen.y, 0.0f, 1.0f );
    output.color    = input.color;
    output.uv       = input.uv;
    return output;
}

float4 solid( pixel_t input ) : SV_Target
{
    return input.color;
}

float4 textured( pixel_t input ) : SV_Target
{
    return g_texture.Sample( g_sampler, input.uv ) * input.color;
}

float4 tinted( pixel_t input ) : SV_Target
{
    return float4( g_texture.Sample( g_sampler, input.uv ).rgb * input.color.rgb, input.color.a );
}

// Same 11-tap kernel and 0.98 gain as the d3d9 BLUR_PIXEL_SHADER (taken from its fxc /dumpbin).
float4 blur( pixel_t input ) : SV_Target
{
    static const float WEIGHTS[ 6 ] = { 0.227f, 0.1662f, 0.0943f, 0.0689f, 0.0476f, 0.0239f };

    float3 result = g_texture.Sample( g_sampler, input.uv ).rgb * WEIGHTS[ 0 ];

    [unroll] for ( int tap = 1; tap < 6; ++tap )
    {
        const float2 offset = g_screen.zw * tap;
        result += g_texture.Sample( g_sampler, input.uv + offset ).rgb * WEIGHTS[ tap ];
        result += g_texture.Sample( g_sampler, input.uv - offset ).rgb * WEIGHTS[ tap ];
    }

    return float4( result * 0.98f, 1.0f );
}
