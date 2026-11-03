// fxc /nologo /O3 /T ps_3_0 /E dilate /Fo dilate.cso glow.hlsl
// fxc /nologo /O3 /T ps_3_0 /E blur   /Fo blur.cso   glow.hlsl

sampler2D g_source : register( s0 );

float4 g_step : register( c0 );

float4 g_scale : register( c1 );

float4 dilate( float2 uv : TEXCOORD0 ) : COLOR0
{
    float4 result = tex2Dlod( g_source, float4( uv, 0.0f, 0.0f ) );

    [loop] for ( int i = 1; i <= 8; ++i )
    {
        if ( i > g_step.z )
        {
            break;
        }

        const float2 offset = g_step.xy * i;
        result = max( result, tex2Dlod( g_source, float4( uv + offset, 0.0f, 0.0f ) ) );
        result = max( result, tex2Dlod( g_source, float4( uv - offset, 0.0f, 0.0f ) ) );
    }

    return result * g_scale;
}

float4 blur( float2 uv : TEXCOORD0 ) : COLOR0
{
    const float weights[ 5 ] = { 0.2042f, 0.1802f, 0.1238f, 0.0663f, 0.0276f };

    float4 result = tex2D( g_source, uv ) * weights[ 0 ];

    [unroll] for ( int i = 1; i < 5; ++i )
    {
        const float2 offset = g_step.xy * i;
        result += ( tex2D( g_source, uv + offset ) + tex2D( g_source, uv - offset ) ) * weights[ i ];
    }

    return result * g_scale;
}
