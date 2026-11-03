// fxc /nologo /O3 /T ps_3_0 /E grade /Fo grade.cso grade.hlsl

sampler2D g_frame : register( s0 );

sampler2D g_bloom : register( s1 );

float4 g_tone : register( c0 );

float4 g_range : register( c1 );

float4 g_balance : register( c2 );

static const float3 LUMA = float3( 0.2126f, 0.7152f, 0.0722f );

float4 grade( float2 uv : TEXCOORD0 ) : COLOR0
{
    float3 color = tex2D( g_frame, uv ).rgb;

    const float3 glow = saturate( ( tex2D( g_bloom, uv ).rgb - g_balance.w ) / ( 1.0f - g_balance.w ) ) * g_range.w;
    color = 1.0f - ( 1.0f - color ) * ( 1.0f - saturate( glow ) );

    color *= g_balance.rgb * exp2( g_tone.x );

    const float luma = dot( color, LUMA );
    color *= 1.0f + g_range.x * ( 1.0f - smoothstep( 0.0f, 0.5f, luma ) ) + g_range.y * smoothstep( 0.5f, 1.0f, luma );

    color = ( color - 0.5f ) * g_tone.y + 0.5f;

    const float grey   = dot( color, LUMA );
    const float chroma = max( color.r, max( color.g, color.b ) ) - min( color.r, min( color.g, color.b ) );
    color = lerp( grey, color, g_tone.z + g_tone.w * ( 1.0f - saturate( chroma ) ) );

    const float2 offset = uv - 0.5f;
    color *= 1.0f - g_range.z * smoothstep( 0.1f, 0.5f, dot( offset, offset ) );

    return float4( saturate( color ), 1.0f );
}
