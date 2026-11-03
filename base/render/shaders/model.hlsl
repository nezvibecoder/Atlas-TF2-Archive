// fxc /nologo /O3 /T vs_3_0 /E skinned     /Fo skinned.cso     model.hlsl
// fxc /nologo /O3 /T vs_3_0 /E rigid       /Fo rigid.cso       model.hlsl
// fxc /nologo /O3 /T vs_3_0 /E skinned_lit /Fo skinned_lit.cso model.hlsl
// fxc /nologo /O3 /T vs_3_0 /E rigid_lit   /Fo rigid_lit.cso   model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E flat        /Fo flat.cso        model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E shaded      /Fo shaded.cso      model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E fresnel     /Fo fresnel.cso     model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E lava        /Fo lava.cso        model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E pearl       /Fo pearl.cso       model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E bubble      /Fo bubble.cso      model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E lsd         /Fo lsd.cso         model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E network     /Fo network.cso     model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E soap        /Fo soap.cso        model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E aurora      /Fo aurora.cso      model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E aquarium    /Fo aquarium.cso    model.hlsl
// fxc /nologo /O3 /T ps_3_0 /E sky         /Fo sky.cso         model.hlsl

float4 g_view_projection[ 4 ] : register( c0 );

float4 g_weight_decode : register( c4 );

float4 g_eye : register( c5 );

float4 g_palette[ 159 ] : register( c8 );

float4 g_color : register( c0 );

float4 g_time : register( c1 );

struct lit_vertex
{
    float4 position : POSITION;
    float3 normal   : TEXCOORD0;
    float3 eye      : TEXCOORD1;
    float3 model    : TEXCOORD2;
};

float4 to_world( float4 position, float4 row0, float4 row1, float4 row2 )
{
    return float4( dot( row0, position ), dot( row1, position ), dot( row2, position ), 1.0f );
}

float4 project( float4 world )
{
    return float4( dot( g_view_projection[ 0 ], world ), dot( g_view_projection[ 1 ], world ),
                   dot( g_view_projection[ 2 ], world ), dot( g_view_projection[ 3 ], world ) );
}

float3 decode_normal( float4 normal )
{
    if ( g_weight_decode.w == 0.0f )
    {
        return normal.xyz;
    }

    const float2 zt_signs = ( normal.xy - 128.0f ) < 0.0f;
    const float2 xy_abs   = abs( normal.xy - 128.0f ) - zt_signs;
    const float2 xy_signs = ( xy_abs - 64.0f ) < 0.0f;

    float3 result;
    result.xy = ( abs( xy_abs - 64.0f ) - xy_signs ) / 63.0f;
    result.z  = 1.0f - result.x - result.y;
    result    = normalize( result );
    result.xy *= lerp( 1.0f, -1.0f, xy_signs );
    result.z  *= lerp( 1.0f, -1.0f, zt_signs.x );
    return result;
}

void blend_bones( float4 weights, float4 indices, out float4 row0, out float4 row1, out float4 row2 )
{
    const int3   slots = D3DCOLORtoUBYTE4( indices ).xyz * 3;
    const float2 blend = weights.xy * g_weight_decode.y + g_weight_decode.z;
    const float3 share = float3( blend, 1.0f - blend.x - blend.y );

    row0 = g_palette[ slots.x ] * share.x + g_palette[ slots.y ] * share.y + g_palette[ slots.z ] * share.z;
    row1 = g_palette[ slots.x + 1 ] * share.x + g_palette[ slots.y + 1 ] * share.y + g_palette[ slots.z + 1 ] * share.z;
    row2 = g_palette[ slots.x + 2 ] * share.x + g_palette[ slots.y + 2 ] * share.y + g_palette[ slots.z + 2 ] * share.z;
}

lit_vertex light_vertex( float4 position, float4 normal, float4 row0, float4 row1, float4 row2 )
{
    const float4 world = to_world( float4( position.xyz, 1.0f ), row0, row1, row2 );
    const float3 local = decode_normal( normal );

    lit_vertex output;
    output.position = project( world );
    output.normal   = float3( dot( row0.xyz, local ), dot( row1.xyz, local ), dot( row2.xyz, local ) );
    output.eye      = g_eye.xyz - world.xyz;
    output.model    = position.xyz;
    return output;
}

float4 skinned( float4 position : POSITION, float4 weights : BLENDWEIGHT, float4 indices : BLENDINDICES ) : POSITION
{
    float4 row0, row1, row2;
    blend_bones( weights, indices, row0, row1, row2 );

    return project( to_world( float4( position.xyz, 1.0f ), row0, row1, row2 ) );
}

float4 rigid( float4 position : POSITION ) : POSITION
{
    return project( to_world( float4( position.xyz, 1.0f ), g_palette[ 0 ], g_palette[ 1 ], g_palette[ 2 ] ) );
}

lit_vertex skinned_lit( float4 position : POSITION, float4 weights : BLENDWEIGHT, float4 indices : BLENDINDICES, float4 normal : NORMAL )
{
    float4 row0, row1, row2;
    blend_bones( weights, indices, row0, row1, row2 );

    return light_vertex( position, normal, row0, row1, row2 );
}

lit_vertex rigid_lit( float4 position : POSITION, float4 normal : NORMAL )
{
    return light_vertex( position, normal, g_palette[ 0 ], g_palette[ 1 ], g_palette[ 2 ] );
}

float light( float3 normal, float facing )
{
    const float sky = lerp( 0.35f, 1.0f, normal.z * 0.5f + 0.5f );
    return saturate( sky * 0.75f + facing * 0.25f );
}

float hash( float3 position )
{
    position = frac( position * 0.1031f );
    position += dot( position, position.zyx + 31.32f );
    return frac( ( position.x + position.y ) * position.z );
}

float value_noise( float3 position )
{
    const float3 cell = floor( position );
    const float3 part = frac( position );
    const float3 ease = part * part * ( 3.0f - 2.0f * part );

    const float x00 = lerp( hash( cell ), hash( cell + float3( 1, 0, 0 ) ), ease.x );
    const float x10 = lerp( hash( cell + float3( 0, 1, 0 ) ), hash( cell + float3( 1, 1, 0 ) ), ease.x );
    const float x01 = lerp( hash( cell + float3( 0, 0, 1 ) ), hash( cell + float3( 1, 0, 1 ) ), ease.x );
    const float x11 = lerp( hash( cell + float3( 0, 1, 1 ) ), hash( cell + float3( 1, 1, 1 ) ), ease.x );

    return lerp( lerp( x00, x10, ease.y ), lerp( x01, x11, ease.y ), ease.z );
}

float3 hue( float value )
{
    return saturate( abs( frac( value + float3( 0.0f, 2.0f / 3.0f, 1.0f / 3.0f ) ) * 6.0f - 3.0f ) - 1.0f );
}

float3 ramp( float value )
{
    const float  position = saturate( value * 4.0f - 0.5f ) * 3.0f;
    const float3 black    = float3( 0.0f, 0.0f, 0.0f );
    const float3 cyan     = float3( 0.0f, 230.0f, 255.0f ) / 255.0f;
    const float3 yellow   = float3( 255.0f, 230.0f, 0.0f ) / 255.0f;
    const float3 magenta  = float3( 255.0f, 60.0f, 230.0f ) / 255.0f;

    return position < 1.0f ? lerp( black, cyan, position ) : position < 2.0f ? lerp( cyan, yellow, position - 1.0f ) : lerp( yellow, magenta, position - 2.0f );
}

void surface( float3 normal, float3 eye, out float3 unit, out float facing )
{
    unit   = normalize( normal );
    facing = saturate( dot( unit, normalize( eye ) ) );
}

float4 flat( ) : COLOR0
{
    return g_color;
}

float4 shaded( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1 ) : COLOR0
{
    float3 unit;
    float  facing;
    surface( normal, eye, unit, facing );

    const float mask = saturate( facing * 1.25f - 0.25f );
    return float4( lerp( g_color.rgb * light( unit, facing ), g_color.rgb, mask ), g_color.a );
}

float4 fresnel( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1 ) : COLOR0
{
    float3 unit;
    float  facing;
    surface( normal, eye, unit, facing );

    return float4( g_color.rgb * 5.5f * light( unit, facing ) * ( 1.0f - pow( facing, 0.75f ) ), g_color.a );
}

float4 lava( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1, float3 model : TEXCOORD2 ) : COLOR0
{
    float3 unit;
    float  facing;
    surface( normal, eye, unit, facing );

    const float  time   = g_time.x;
    const float  scale  = 0.75f + 0.22f * sin( time * ( 6.2831853f / 12.7f ) );
    const float3 flow   = model * ( 0.09f / scale ) - float3( 0.0f, 0.0f, time * 0.6f );
    const float  heat   = saturate( value_noise( flow ) * 0.65f + value_noise( flow * 2.3f + 17.0f ) * 0.35f );
    const float  blend  = 0.78f + 0.17f * sin( time * ( 6.2831853f / 6.1f ) ) + 0.05f * sin( time * ( 6.2831853f / 2.3f ) );
    const float3 albedo = g_color.rgb * lerp( 1.0f, heat * 2.0f, blend ) + pow( heat, 6.0f ) * 0.5f;
    const float  mask   = saturate( facing * 1.25f - 0.25f );

    return float4( lerp( albedo * light( unit, facing ), albedo, mask ), g_color.a );
}

float4 pearl( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1, float3 model : TEXCOORD2 ) : COLOR0
{
    float3 unit;
    float  facing;
    surface( normal, eye, unit, facing );

    const float  shift      = ( 1.0f - facing ) * 0.8f + value_noise( model * 0.05f ) * 0.6f + g_time.x * 0.05f;
    const float3 iridescent = lerp( float3( 1.0f, 1.0f, 1.0f ), hue( shift ), 0.55f );
    const float3 reflected  = reflect( -normalize( eye ), unit );
    const float  sheen      = 0.2f + pow( saturate( reflected.z ), 24.0f ) * 0.8f + pow( facing, 48.0f ) * 0.5f;

    return float4( iridescent * g_color.rgb + sheen * 0.6f, g_color.a );
}

float4 bubble( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1 ) : COLOR0
{
    float3 unit;
    float  facing;
    surface( normal, eye, unit, facing );

    const float3 key = normalize( normalize( eye ) + float3( 0.0f, 0.0f, 0.5f ) );

    return float4( g_color.rgb * 2.0f * ramp( saturate( dot( unit, key ) ) ), g_color.a );
}

float4 lsd( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1, float3 model : TEXCOORD2 ) : COLOR0
{
    float3 unit;
    float  facing;
    surface( normal, eye, unit, facing );

    const float  rim  = 1.0f - facing;
    const float  edge = rim * rim * rim;
    const float3 neon = hue( rim * 0.6f + ( model.z + model.x * 0.5f ) * ( 1.0f / 64.0f ) - g_time.x * 0.2f );

    return float4( g_color.rgb * ( 0.2f + rim * 0.3f ) + neon * ( 0.1f + edge * 1.5f ) + edge * edge * 0.4f, g_color.a );
}

float4 network( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1, float3 model : TEXCOORD2 ) : COLOR0
{
    const float  cell_size = 18.0f;
    const float3 position  = model / cell_size;
    const float3 corner    = floor( position - 0.5f );
    const float  time      = g_time.x;

    float nearest = 2.0f;

    [unroll] for ( int x = 0; x < 2; ++x )
    {
        [unroll] for ( int y = 0; y < 2; ++y )
        {
            [unroll] for ( int z = 0; z < 2; ++z )
            {
                const float3 cell  = corner + float3( x, y, z );
                const float  seed  = hash( cell );
                const float3 phase = frac( seed * float3( 1.0f, 7.31f, 13.7f ) ) * 6.2831853f;
                const float3 node  = cell + 0.5f + 0.4f * sin( time * ( 0.6f + seed * 0.5f ) + phase );

                nearest = min( nearest, length( position - node ) );
            }
        }
    }

    const float link = pow( saturate( 1.0f - nearest / 0.6f ), 2.0f );

    return float4( g_color.rgb * ( 0.45f + link * 1.2f ), g_color.a * saturate( 0.2f + link ) );
}

float4 aero( float3 body, float3 glow, float rim )
{
    const float inner   = lerp( 0.15f, 1.0f, pow( rim, 1.6f ) );
    const float outline = smoothstep( 0.80f, 0.90f, rim );

    return float4( body * inner + glow + outline, g_color.a );
}

float4 soap( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1, float3 model : TEXCOORD2 ) : COLOR0
{
    float3 unit;
    float  facing;
    surface( normal, eye, unit, facing );

    const float  time      = g_time.x;
    const float  rim       = 1.0f - facing;
    const float3 view      = normalize( eye );
    const float3 marble    = model * 0.035f;
    const float3 warp      = float3( value_noise( marble + float3( 0.0f, time * 0.12f, 0.0f ) ), value_noise( marble + float3( 5.2f, 1.3f, time * 0.1f ) ), 0.0f );
    const float  swirl     = value_noise( marble * 1.7f + warp * 2.5f + float3( 0.0f, time * 0.18f, 0.0f ) );
    const float  thickness = swirl * 1.4f + model.y * 0.012f + rim * 0.5f - time * 0.05f;
    const float3 film      = 0.5f + 0.5f * cos( 6.2831853f * ( thickness * float3( 1.0f, 1.15f, 1.3f ) + float3( 0.0f, 0.3f, 0.6f ) ) );
    const float  window    = smoothstep( 0.90f, 0.92f, dot( unit, normalize( view + float3( 0.0f, 0.0f, 1.1f ) ) ) );
    const float  bounce    = smoothstep( 0.95f, 0.97f, dot( unit, normalize( view - float3( 0.0f, 0.0f, 0.9f ) ) ) ) * 0.6f;

    return aero( lerp( float3( 0.85f, 0.92f, 1.0f ), film, 0.75f ) * g_color.rgb * 1.2f, window + bounce, rim );
}

float4 aurora( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1, float3 model : TEXCOORD2 ) : COLOR0
{
    float3 unit;
    float  facing;
    surface( normal, eye, unit, facing );

    const float  time  = g_time.x;
    const float  warp  = value_noise( model * 0.03f + float3( 0.0f, time * 0.08f, 0.0f ) ) * 3.0f;
    const float  green = pow( 1.0f - abs( sin( dot( model, float3( 0.04f, 0.02f, 0.07f ) ) + warp - time * 0.7f ) ), 24.0f );
    const float  blue  = pow( 1.0f - abs( sin( dot( model, float3( -0.03f, 0.05f, 0.06f ) ) + warp * 1.3f - time * 0.5f + 1.7f ) ), 30.0f );
    const float3 glow  = ( green * float3( 0.35f, 1.0f, 0.55f ) + blue * float3( 0.30f, 0.70f, 1.0f ) ) * g_color.rgb;

    return aero( float3( 0.05f, 0.35f, 0.75f ) * g_color.rgb, glow, 1.0f - facing );
}

float4 aquarium( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1, float3 model : TEXCOORD2 ) : COLOR0
{
    float3 unit;
    float  facing;
    surface( normal, eye, unit, facing );

    const float  time    = g_time.x;
    const float3 water   = model * 0.06f + float3( time * 0.2f, 0.0f, time * 0.15f );
    const float  caustic = pow( saturate( 1.0f - abs( value_noise( water ) - value_noise( water * 1.4f + float3( 5.2f, 1.3f, -time * 0.25f ) ) ) * 3.0f ), 10.0f );
    const float  shaft   = pow( 0.5f + 0.5f * sin( model.x * 0.18f + model.z * 0.11f + value_noise( model * 0.02f + float3( 0.0f, 0.0f, time * 0.1f ) ) * 4.0f + time * 0.6f ), 6.0f ) *
                         smoothstep( 10.0f, 80.0f, model.y );
    const float3 body    = lerp( float3( 0.0f, 0.10f, 0.45f ), float3( 0.05f, 0.45f, 1.0f ), unit.z * 0.5f + 0.5f ) * g_color.rgb * 1.3f;
    const float3 glow    = ( caustic * float3( 0.44f, 0.68f, 0.8f ) + shaft * float3( 0.075f, 0.225f, 0.5f ) ) * g_color.rgb;

    return aero( body, glow, 1.0f - facing );
}

float4 sky( float3 normal : TEXCOORD0, float3 eye : TEXCOORD1, float3 model : TEXCOORD2 ) : COLOR0
{
    float3 unit;
    float  facing;
    surface( normal, eye, unit, facing );

    const float  time   = g_time.x;
    const float  height = model.y + ( value_noise( model * 0.08f ) - 0.5f ) * 6.0f;
    const float  above  = smoothstep( 30.0f, 34.0f, height );
    const float  cloud  = value_noise( model * 0.07f + float3( time * 0.8f, 0.0f, 0.0f ) ) * 0.65f +
                         value_noise( model * 0.16f + float3( time * 1.3f, 3.0f, 0.0f ) ) * 0.35f;
    const float3 air    = lerp( float3( 0.45f, 0.78f, 1.0f ), float3( 0.04f, 0.32f, 0.92f ), smoothstep( 30.0f, 85.0f, height ) );
    const float3 grass  = lerp( float3( 0.10f, 0.45f, 0.05f ), float3( 0.35f, 0.80f, 0.15f ), value_noise( model * 0.35f ) );
    const float  gloss  = smoothstep( 0.80f, 0.88f, dot( unit, normalize( normalize( eye ) + float3( 0.0f, 0.0f, 1.1f ) ) ) ) * 0.3f;
    const float  glow   = smoothstep( 0.55f, 0.72f, cloud ) * above * 0.45f + gloss;

    return aero( lerp( grass, air, above ) * g_color.rgb * 1.2f, glow, 1.0f - facing );
}
