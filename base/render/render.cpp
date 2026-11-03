#include "render.hpp"

#include "input/input.hpp"
#include "steam.hpp"

#include "icons.hpp"

using render_backend   = render::backend;
using render_draw_list = render::draw_list;

ATLAS_NO_DESTROY render_backend   g_render{ };
ATLAS_NO_DESTROY render_draw_list g_draw{ };

namespace render
{
    // -------------------------------------------------------------------------
    // Rendering helpers.
    // -------------------------------------------------------------------------

    static u32 next_power_of_two( u32 value )
    {
        if ( value <= 1 )
        {
            return 1;
        }

        --value;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        return value + 1;
    }

    static u32 glyph_bitmap_pitch( u32 width )
    {
        return ( width + 3u ) & ~3u;
    }

    static u32 glyph_mono_pitch( u32 width )
    {
        return ( ( width + 31u ) / 32u ) * 4u;
    }

    static u8 glyph_coverage_alpha( u8 coverage )
    {
        if ( !coverage )
        {
            return 0;
        }

        u32 result{ ( static_cast< u32 >( coverage ) * 255u + 32u ) / 64u };
        result += result / 6u + 6u;
        return result > 255u ? 255u : static_cast< u8 >( result );
    }

    static bool push_vertex( atl::vector< d3d9::overlay_vertex > &vertices, float x, float y, u32 color,
                             float u = 0.0f, float v = 0.0f )
    {
        return vertices.push_back( d3d9::overlay_vertex{ x, y, 0.0f, 1.0f, color, u, v } );
    }

    static void set_gradient_corners( render_command &command, color_t from, color_t to, bool vertical )
    {
        command.m_point_colors[ 0 ] = from;
        command.m_point_colors[ 1 ] = vertical ? from : to;
        command.m_point_colors[ 2 ] = vertical ? to : from;
        command.m_point_colors[ 3 ] = to;
    }

    static float transform_coordinate( float value, i32 origin, float scale )
    {
        return static_cast< float >( origin ) +
               ( value - static_cast< float >( origin ) ) * scale;
    }

    // i fucking hate d3d9
    static bool push_feathered_segment( atl::vector< d3d9::overlay_vertex > &vertices, float from_x, float from_y,
                                        float to_x, float to_y, float thickness, color_t color )
    {
        constexpr i32   RINGS{ 4 };
        constexpr float RING_ALPHA[ RINGS ]{ 1.0f, 0.8317f, 0.6069f, 0.0f };
        constexpr float FADE{ 0.5f };

        const float delta_x{ to_x - from_x };
        const float delta_y{ to_y - from_y };
        const float length{ math::sqrt( delta_x * delta_x + delta_y * delta_y ) };

        if ( length < 0.001f )
        {
            return true;
        }

        const float normal_x{ -delta_y / length };
        const float normal_y{ delta_x / length };
        const float core{ math::bigger( ( thickness - FADE ) * 0.5f, 0.0f ) };

        float offsets[ RINGS * 2 ]{ };
        u32   colors[ RINGS * 2 ]{ };

        for ( i32 ring{ }; ring < RINGS; ++ring )
        {
            const float offset{ core + FADE * static_cast< float >( ring ) / static_cast< float >( RINGS - 1 ) };
            const u8    alpha{ static_cast< u8 >( static_cast< float >( color.a ) * RING_ALPHA[ ring ] + 0.5f ) };
            const u32   value{ color_t{ color.r, color.g, color.b, alpha }.d3d( ) };

            offsets[ RINGS - 1 - ring ] = -offset;
            colors[ RINGS - 1 - ring ]  = value;
            offsets[ RINGS + ring ]     = offset;
            colors[ RINGS + ring ]      = value;
        }

        for ( i32 band{ }; band < RINGS * 2 - 1; ++band )
        {
            const float inner{ offsets[ band ] };
            const float outer{ offsets[ band + 1 ] };

            if ( inner == outer )
            {
                continue;
            }

            const u32 inner_color{ colors[ band ] };
            const u32 outer_color{ colors[ band + 1 ] };

            const float inner_from_x{ from_x + normal_x * inner };
            const float inner_from_y{ from_y + normal_y * inner };
            const float inner_to_x{ to_x + normal_x * inner };
            const float inner_to_y{ to_y + normal_y * inner };
            const float outer_from_x{ from_x + normal_x * outer };
            const float outer_from_y{ from_y + normal_y * outer };
            const float outer_to_x{ to_x + normal_x * outer };
            const float outer_to_y{ to_y + normal_y * outer };

            if ( !push_vertex( vertices, inner_from_x, inner_from_y, inner_color ) ||
                 !push_vertex( vertices, outer_from_x, outer_from_y, outer_color ) ||
                 !push_vertex( vertices, inner_to_x, inner_to_y, inner_color ) ||
                 !push_vertex( vertices, inner_to_x, inner_to_y, inner_color ) ||
                 !push_vertex( vertices, outer_from_x, outer_from_y, outer_color ) ||
                 !push_vertex( vertices, outer_to_x, outer_to_y, outer_color ) )
            {
                return false;
            }
        }

        return true;
    }

    static float cross( vec2 left, vec2 right )
    {
        return left.x * right.y - left.y * right.x;
    }

    static bool inside_triangle( vec2 a, vec2 b, vec2 c, vec2 point, float winding )
    {
        return cross( b - a, point - a ) * winding >= 0.0f &&
               cross( c - b, point - b ) * winding >= 0.0f &&
               cross( a - c, point - c ) * winding >= 0.0f;
    }

    static bool is_utf8_continuation( u8 value )
    {
        return ( value & 0xC0 ) == 0x80;
    }

    static usize decode_utf8_codepoint( const char *text, usize size, usize index, u32 *code )
    {
        if ( !text || !code || index >= size )
        {
            return 0;
        }

        u8 first{ static_cast< u8 >( text[ index ] ) };
        *code = '?';

        if ( first < 0x80 )
        {
            *code = first;
            return 1;
        }

        if ( first >= 0xC2 && first <= 0xDF && index + 1 < size )
        {
            u8 second{ static_cast< u8 >( text[ index + 1 ] ) };

            if ( is_utf8_continuation( second ) )
            {
                *code = ( static_cast< u32 >( first & 0x1F ) << 6 ) | static_cast< u32 >( second & 0x3F );
                return 2;
            }
        }

        if ( first >= 0xE0 && first <= 0xEF && index + 2 < size )
        {
            u8 second{ static_cast< u8 >( text[ index + 1 ] ) };
            u8 third{ static_cast< u8 >( text[ index + 2 ] ) };

            if ( is_utf8_continuation( second ) && is_utf8_continuation( third ) &&
                 ( first != 0xE0 || second >= 0xA0 ) && ( first != 0xED || second < 0xA0 ) )
            {
                *code = ( static_cast< u32 >( first & 0x0F ) << 12 ) |
                        ( static_cast< u32 >( second & 0x3F ) << 6 ) | static_cast< u32 >( third & 0x3F );
                return 3;
            }
        }

        if ( first >= 0xF0 && first <= 0xF4 && index + 3 < size )
        {
            u8 second{ static_cast< u8 >( text[ index + 1 ] ) };
            u8 third{ static_cast< u8 >( text[ index + 2 ] ) };
            u8 fourth{ static_cast< u8 >( text[ index + 3 ] ) };

            if ( is_utf8_continuation( second ) && is_utf8_continuation( third ) &&
                 is_utf8_continuation( fourth ) && ( first != 0xF0 || second >= 0x90 ) &&
                 ( first != 0xF4 || second < 0x90 ) )
            {
                *code = ( static_cast< u32 >( first & 0x07 ) << 18 ) |
                        ( static_cast< u32 >( second & 0x3F ) << 12 ) |
                        ( static_cast< u32 >( third & 0x3F ) << 6 ) | static_cast< u32 >( fourth & 0x3F );
                return 4;
            }
        }

        return 1;
    }

    static bool push_glyph_quad( atl::vector< d3d9::overlay_vertex > &vertices,
                                 const render_command &command, const text_glyph &glyph,
                                 float cursor_x, float cursor_y, float shift_x, float shift_y,
                                 u32 color )
    {
        const float crop{ command.m_crop_top < 0.0f   ? 0.0f
                          : command.m_crop_top > 1.0f ? 1.0f
                                                      : command.m_crop_top };

        if ( crop >= 1.0f )
        {
            return true;
        }

        const float cropped_offset{ glyph.m_offset_y + glyph.m_height * crop };
        const float left{
            transform_coordinate( cursor_x + glyph.m_offset_x + shift_x - 0.5f,
                                  command.m_transform_origin.x, command.m_transform_scale )
        };
        const float top{
            transform_coordinate( cursor_y + cropped_offset + shift_y - 0.5f,
                                  command.m_transform_origin.y, command.m_transform_scale )
        };
        const float right{
            transform_coordinate( cursor_x + glyph.m_offset_x + glyph.m_width + shift_x - 0.5f,
                                  command.m_transform_origin.x, command.m_transform_scale )
        };
        const float bottom{
            transform_coordinate( cursor_y + glyph.m_offset_y + glyph.m_height + shift_y - 0.5f,
                                  command.m_transform_origin.y, command.m_transform_scale )
        };

        const float v0{ glyph.m_v0 + ( glyph.m_v1 - glyph.m_v0 ) * crop };

        return push_vertex( vertices, left, top, color, glyph.m_u0, v0 ) &&
               push_vertex( vertices, right, top, color, glyph.m_u1, v0 ) &&
               push_vertex( vertices, left, bottom, color, glyph.m_u0, glyph.m_v1 ) &&
               push_vertex( vertices, left, bottom, color, glyph.m_u0, glyph.m_v1 ) &&
               push_vertex( vertices, right, top, color, glyph.m_u1, v0 ) &&
               push_vertex( vertices, right, bottom, color, glyph.m_u1, glyph.m_v1 );
    }

    // -------------------------------------------------------------------------
    // Command encoder.
    // -------------------------------------------------------------------------

    const icon_atlas_t &backend::icon_atlas_for( batch_type type ) const
    {
        switch ( type )
        {
            case batch_type::icon_large: return this->m_icon_atlases[ icons::KIND_LARGE ];
            case batch_type::icon_logo: return this->m_icon_atlases[ icons::KIND_LOGO ];
            case batch_type::icon_vacc: return this->m_icon_atlases[ icons::KIND_VACC ];
            default: return this->m_icon_atlases[ icons::KIND_SMALL ];
        }
    }

    bool backend::count_vertices( const atl::vector< render_command > &commands, usize *required ) const
    {
        if ( !required )
        {
            return false;
        }

        constexpr usize maximum{ static_cast< usize >( -1 ) };

        for ( const render_command &command : commands )
        {
            usize count{ };

            switch ( command.m_type )
            {
                case command_type::line:
                    count = 2;
                    break;

                case command_type::box:
                    count = command.m_filled ? 6 : 8;
                    break;

                case command_type::gradient:
                    count = 6;
                    break;

                case command_type::polygon:
                    count = ( command.m_point_count - 2 ) * 3 + command.m_point_count * 6;
                    break;

                case command_type::blur:
                case command_type::model:
                    count = 6;
                    break;

                case command_type::text:
                {
                    const usize per_glyph{ command.m_outlined ? 30u : 6u };
                    if ( command.m_text.size( ) > maximum / per_glyph )
                    {
                        return false;
                    }

                    count = command.m_text.size( ) * per_glyph;

                    if ( command.m_avatar_account && command.m_avatar_shape != AVATAR_NONE )
                    {
                        count += 96;
                    }
                    break;
                }

                case command_type::icon:
                    count = command.m_outlined ? 30 : 6;
                    break;

                case command_type::html:
                    count = 6;
                    break;

                default:
                    break;
            }

            if ( count > maximum - *required )
            {
                return false;
            }

            *required += count;
        }

        return true;
    }

    bool backend::begin_batch( batch_type type )
    {
        if ( this->m_batch_type == type )
        {
            return true;
        }

        if ( !this->flush_batch( ) )
        {
            return false;
        }

        this->m_batch_type = type;
        return true;
    }

    bool backend::append_line( const render_command &command )
    {
        if ( !this->begin_batch( batch_type::solid ) )
        {
            return false;
        }

        const float from_x{ transform_coordinate( command.m_from.x, command.m_transform_origin.x,
                                                  command.m_transform_scale ) };
        const float from_y{ transform_coordinate( command.m_from.y, command.m_transform_origin.y,
                                                  command.m_transform_scale ) };
        const float to_x{ transform_coordinate( command.m_to.x, command.m_transform_origin.x,
                                                command.m_transform_scale ) };
        const float to_y{ transform_coordinate( command.m_to.y, command.m_transform_origin.y,
                                                command.m_transform_scale ) };

        return push_feathered_segment( this->m_vertices, from_x, from_y, to_x, to_y, command.m_thickness,
                                       command.m_color );
    }

    bool backend::append_box( const render_command &command )
    {
        if ( command.m_bounds.right <= command.m_bounds.left ||
             command.m_bounds.bottom <= command.m_bounds.top )
        {
            return true;
        }

        const u32 color{ command.m_color.d3d( ) };

        if ( command.m_filled )
        {
            const float left{
                transform_coordinate( static_cast< float >( command.m_bounds.left ) - 0.5f,
                                      command.m_transform_origin.x,
                                      command.m_transform_scale )
            };
            const float top{
                transform_coordinate( static_cast< float >( command.m_bounds.top ) - 0.5f,
                                      command.m_transform_origin.y,
                                      command.m_transform_scale )
            };
            const float right{
                transform_coordinate( static_cast< float >( command.m_bounds.right ) - 0.5f,
                                      command.m_transform_origin.x,
                                      command.m_transform_scale )
            };
            const float bottom{
                transform_coordinate( static_cast< float >( command.m_bounds.bottom ) - 0.5f,
                                      command.m_transform_origin.y,
                                      command.m_transform_scale )
            };

            return this->begin_batch( batch_type::solid ) &&
                   push_vertex( this->m_vertices, left, top, color ) &&
                   push_vertex( this->m_vertices, right, top, color ) &&
                   push_vertex( this->m_vertices, left, bottom, color ) &&
                   push_vertex( this->m_vertices, left, bottom, color ) &&
                   push_vertex( this->m_vertices, right, top, color ) &&
                   push_vertex( this->m_vertices, right, bottom, color );
        }

        if ( !this->begin_batch( batch_type::lines ) )
        {
            return false;
        }

        const float left{
            transform_coordinate( static_cast< float >( command.m_bounds.left ),
                                  command.m_transform_origin.x,
                                  command.m_transform_scale )
        };
        const float top{
            transform_coordinate( static_cast< float >( command.m_bounds.top ),
                                  command.m_transform_origin.y,
                                  command.m_transform_scale )
        };
        const float right{
            transform_coordinate( static_cast< float >( command.m_bounds.right ),
                                  command.m_transform_origin.x,
                                  command.m_transform_scale )
        };
        const float bottom{
            transform_coordinate( static_cast< float >( command.m_bounds.bottom ),
                                  command.m_transform_origin.y,
                                  command.m_transform_scale )
        };

        return push_vertex( this->m_vertices, left, top, color ) &&
               push_vertex( this->m_vertices, right, top, color ) &&
               push_vertex( this->m_vertices, right, top, color ) &&
               push_vertex( this->m_vertices, right, bottom, color ) &&
               push_vertex( this->m_vertices, right, bottom, color ) &&
               push_vertex( this->m_vertices, left, bottom, color ) &&
               push_vertex( this->m_vertices, left, bottom, color ) &&
               push_vertex( this->m_vertices, left, top, color );
    }

    bool backend::append_gradient( const render_command &command )
    {
        if ( command.m_bounds.right <= command.m_bounds.left ||
             command.m_bounds.bottom <= command.m_bounds.top )
        {
            return true;
        }

        const float left{
            transform_coordinate( static_cast< float >( command.m_bounds.left ) - 0.5f,
                                  command.m_transform_origin.x,
                                  command.m_transform_scale )
        };
        const float top{
            transform_coordinate( static_cast< float >( command.m_bounds.top ) - 0.5f,
                                  command.m_transform_origin.y,
                                  command.m_transform_scale )
        };
        const float right{
            transform_coordinate( static_cast< float >( command.m_bounds.right ) - 0.5f,
                                  command.m_transform_origin.x,
                                  command.m_transform_scale )
        };
        const float bottom{
            transform_coordinate( static_cast< float >( command.m_bounds.bottom ) - 0.5f,
                                  command.m_transform_origin.y,
                                  command.m_transform_scale )
        };

        const u32 top_left{ command.m_point_colors[ 0 ].d3d( ) };
        const u32 top_right{ command.m_point_colors[ 1 ].d3d( ) };
        const u32 bottom_left{ command.m_point_colors[ 2 ].d3d( ) };
        const u32 bottom_right{ command.m_point_colors[ 3 ].d3d( ) };

        return this->begin_batch( batch_type::solid ) &&
               push_vertex( this->m_vertices, left, top, top_left ) &&
               push_vertex( this->m_vertices, right, top, top_right ) &&
               push_vertex( this->m_vertices, left, bottom, bottom_left ) &&
               push_vertex( this->m_vertices, left, bottom, bottom_left ) &&
               push_vertex( this->m_vertices, right, top, top_right ) &&
               push_vertex( this->m_vertices, right, bottom, bottom_right );
    }

    bool backend::append_circle( const render_command &command )
    {
        if ( !this->begin_batch( batch_type::solid ) )
        {
            return false;
        }

        const auto draw_a_circle_segment = [ & ]( vec2 from, vec2 to ) -> bool
        {
            if ( !this->begin_batch( batch_type::solid ) )
            {
                return false;
            }

            const f32 from_x{ transform_coordinate( from.x, command.m_transform_origin.x, command.m_transform_scale ) };
            const f32 from_y{ transform_coordinate( from.y, command.m_transform_origin.y, command.m_transform_scale ) };
            const f32 to_x{ transform_coordinate( to.x, command.m_transform_origin.x, command.m_transform_scale ) };
            const f32 to_y{ transform_coordinate( to.y, command.m_transform_origin.y, command.m_transform_scale ) };

            return push_feathered_segment( this->m_vertices, from_x, from_y, to_x, to_y, command.m_thickness,
                                           command.m_color );
        };

        static constexpr i32 segments{ 128 };

        vec2 previous{ };
        for ( i32 segment{ }; segment <= segments; ++segment )
        {
            const f32 theta{ ( PI * 2.0f ) * ( static_cast< f32 >( segment ) / segments ) };

            const vec2 current{ ( f32 )command.m_position.x + math::cos( theta ) * command.m_radius,
                                ( f32 )command.m_position.y + math::sin( theta ) * command.m_radius };

            if ( segment > 0 )
            {
                if ( !draw_a_circle_segment( previous, current ) )
                {
                    continue;
                }
            }

            previous = current;
        }

        return true;
    }

    bool backend::append_arc( const render_command &command )
    {
        if ( !this->begin_batch( batch_type::solid ) )
        {
            return false;
        }

        constexpr float FADE{ 0.5f };

        const float scale{ command.m_transform_scale };
        const float centre_x{ transform_coordinate( command.m_from.x - 0.5f, command.m_transform_origin.x, scale ) };
        const float centre_y{ transform_coordinate( command.m_from.y - 0.5f, command.m_transform_origin.y, scale ) };
        const float outer{ command.m_radius * scale };
        const float inner{ math::bigger( command.m_radius - command.m_thickness, 0.0f ) * scale };
        const float middle{ ( inner + outer ) * 0.5f };

        const float rims[ 4 ]{ inner > 0.0f ? math::bigger( inner - FADE, 0.0f ) : 0.0f,
                               inner > 0.0f ? math::smaller( inner + FADE, middle ) : 0.0f,
                               math::bigger( outer - FADE, middle ), outer + FADE };

        const u32 solid{ command.m_color.d3d( ) };
        const u32 clear{ color_t{ command.m_color.r, command.m_color.g, command.m_color.b, 0 }.d3d( ) };
        const u32 shades[ 4 ]{ inner > 0.0f ? clear : solid, solid, solid, clear };

        const i32   segments{ math::clamp( static_cast< i32 >( math::abs( command.m_sweep ) * outer / 3.0f ) + 1, 4, 128 ) };
        const float step{ command.m_sweep / static_cast< float >( segments ) };

        for ( i32 segment{ }; segment < segments; ++segment )
        {
            const float from{ command.m_start + step * static_cast< float >( segment ) };
            const float from_cos{ math::cos( from ) };
            const float from_sin{ math::sin( from ) };
            const float to_cos{ math::cos( from + step ) };
            const float to_sin{ math::sin( from + step ) };

            for ( i32 band{ }; band < 3; ++band )
            {
                const float low{ rims[ band ] };
                const float high{ rims[ band + 1 ] };

                if ( high <= low )
                {
                    continue;
                }

                if ( !push_vertex( this->m_vertices, centre_x + from_cos * low, centre_y + from_sin * low, shades[ band ] ) ||
                     !push_vertex( this->m_vertices, centre_x + from_cos * high, centre_y + from_sin * high, shades[ band + 1 ] ) ||
                     !push_vertex( this->m_vertices, centre_x + to_cos * low, centre_y + to_sin * low, shades[ band ] ) ||
                     !push_vertex( this->m_vertices, centre_x + to_cos * low, centre_y + to_sin * low, shades[ band ] ) ||
                     !push_vertex( this->m_vertices, centre_x + from_cos * high, centre_y + from_sin * high, shades[ band + 1 ] ) ||
                     !push_vertex( this->m_vertices, centre_x + to_cos * high, centre_y + to_sin * high, shades[ band + 1 ] ) )
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool backend::append_polygon( const render_command &command )
    {
        if ( !this->begin_batch( batch_type::solid ) )
        {
            return false;
        }

        const u32 count{ command.m_point_count };
        vec2      points[ POLYGON_CAPACITY ]{ };
        float     area{ };

        for ( u32 index{ }; index < count; ++index )
        {
            points[ index ] = vec2{ transform_coordinate( command.m_points[ index ].x, command.m_transform_origin.x,
                                                          command.m_transform_scale ),
                                    transform_coordinate( command.m_points[ index ].y, command.m_transform_origin.y,
                                                          command.m_transform_scale ) };
        }

        for ( u32 index{ }; index < count; ++index )
        {
            area += cross( points[ index ], points[ ( index + 1 ) % count ] );
        }

        const float winding{ area > 0.0f ? 1.0f : -1.0f };
        vec2        normals[ POLYGON_CAPACITY ]{ };
        vec2        inner[ POLYGON_CAPACITY ]{ };
        vec2        outer[ POLYGON_CAPACITY ]{ };

        for ( u32 index{ }; index < count; ++index )
        {
            const vec2 edge{ points[ ( index + 1 ) % count ] - points[ index ] };
            normals[ index ] = vec2{ edge.y * winding, -edge.x * winding }.normalized( );
        }

        for ( u32 index{ }; index < count; ++index )
        {
            vec2        miter{ ( normals[ ( index + count - 1 ) % count ] + normals[ index ] ) * 0.5f };
            const float length_sqr{ miter.length_sqr( ) };

            if ( length_sqr > 0.000001f )
            {
                miter = miter * math::smaller( 1.0f / length_sqr, 100.0f );
            }

            inner[ index ] = points[ index ] - miter * 0.5f;
            outer[ index ] = points[ index ] + miter * 0.5f;
        }

        u32 order[ POLYGON_CAPACITY ]{ };
        u32 remaining{ count };

        for ( u32 index{ }; index < count; ++index )
        {
            order[ index ] = index;
        }

        while ( remaining > 3 )
        {
            u32 ear{ remaining };

            for ( u32 slot{ }; slot < remaining && ear == remaining; ++slot )
            {
                const u32 before{ order[ ( slot + remaining - 1 ) % remaining ] };
                const u32 at{ order[ slot ] };
                const u32 after{ order[ ( slot + 1 ) % remaining ] };

                if ( cross( inner[ at ] - inner[ before ], inner[ after ] - inner[ at ] ) * winding <= 0.0f )
                {
                    continue; // reflex corner
                }

                bool empty{ true };

                for ( u32 other{ }; other < remaining && empty; ++other )
                {
                    const u32 candidate{ order[ other ] };

                    if ( candidate != before && candidate != at && candidate != after )
                    {
                        empty = !inside_triangle( inner[ before ], inner[ at ], inner[ after ], inner[ candidate ], winding );
                    }
                }

                if ( empty )
                {
                    ear = slot;
                }
            }

            // every simple polygon has an ear; not finding one means the outline crosses itself
            if ( ear == remaining )
            {
                return true;
            }

            const u32 before{ order[ ( ear + remaining - 1 ) % remaining ] };
            const u32 at{ order[ ear ] };
            const u32 after{ order[ ( ear + 1 ) % remaining ] };

            if ( !push_vertex( this->m_vertices, inner[ before ].x, inner[ before ].y, command.m_point_colors[ before ].d3d( ) ) ||
                 !push_vertex( this->m_vertices, inner[ at ].x, inner[ at ].y, command.m_point_colors[ at ].d3d( ) ) ||
                 !push_vertex( this->m_vertices, inner[ after ].x, inner[ after ].y, command.m_point_colors[ after ].d3d( ) ) )
            {
                return false;
            }

            for ( u32 slot{ ear }; slot + 1 < remaining; ++slot )
            {
                order[ slot ] = order[ slot + 1 ];
            }

            --remaining;
        }

        for ( u32 slot{ }; slot < 3; ++slot )
        {
            const u32 at{ order[ slot ] };

            if ( !push_vertex( this->m_vertices, inner[ at ].x, inner[ at ].y, command.m_point_colors[ at ].d3d( ) ) )
            {
                return false;
            }
        }

        for ( u32 index{ }; index < count; ++index )
        {
            const u32     next{ ( index + 1 ) % count };
            const color_t from{ command.m_point_colors[ index ] };
            const color_t to{ command.m_point_colors[ next ] };
            const u32     solid_from{ from.d3d( ) };
            const u32     solid_to{ to.d3d( ) };
            const u32     clear_from{ color_t{ from.r, from.g, from.b, 0 }.d3d( ) };
            const u32     clear_to{ color_t{ to.r, to.g, to.b, 0 }.d3d( ) };

            if ( !push_vertex( this->m_vertices, inner[ index ].x, inner[ index ].y, solid_from ) ||
                 !push_vertex( this->m_vertices, outer[ index ].x, outer[ index ].y, clear_from ) ||
                 !push_vertex( this->m_vertices, inner[ next ].x, inner[ next ].y, solid_to ) ||
                 !push_vertex( this->m_vertices, inner[ next ].x, inner[ next ].y, solid_to ) ||
                 !push_vertex( this->m_vertices, outer[ index ].x, outer[ index ].y, clear_from ) ||
                 !push_vertex( this->m_vertices, outer[ next ].x, outer[ next ].y, clear_to ) )
            {
                return false;
            }
        }

        return true;
    }

    bool backend::append_blur( const render_command &command )
    {
        if ( !this->m_blur_frame_ready || !command.m_color.a ||
             command.m_bounds.right <= command.m_bounds.left ||
             command.m_bounds.bottom <= command.m_bounds.top )
        {
            return true;
        }

        const float left{
            transform_coordinate( static_cast< float >( command.m_bounds.left ) - 0.5f,
                                  command.m_transform_origin.x,
                                  command.m_transform_scale )
        };
        const float top{
            transform_coordinate( static_cast< float >( command.m_bounds.top ) - 0.5f,
                                  command.m_transform_origin.y,
                                  command.m_transform_scale )
        };
        const float right{
            transform_coordinate( static_cast< float >( command.m_bounds.right ) - 0.5f,
                                  command.m_transform_origin.x,
                                  command.m_transform_scale )
        };
        const float bottom{
            transform_coordinate( static_cast< float >( command.m_bounds.bottom ) - 0.5f,
                                  command.m_transform_origin.y,
                                  command.m_transform_scale )
        };

        const float u0{ left / static_cast< float >( this->m_width ) };
        const float v0{ top / static_cast< float >( this->m_height ) };
        const float u1{ right / static_cast< float >( this->m_width ) };
        const float v1{ bottom / static_cast< float >( this->m_height ) };
        const u32   color{ command.m_color.d3d( ) };

        if ( !command.m_color.a )
        {
            return true;
        }

        return this->begin_batch( batch_type::blur ) &&
               push_vertex( this->m_vertices, left, top, color, u0, v0 ) &&
               push_vertex( this->m_vertices, right, top, color, u1, v0 ) &&
               push_vertex( this->m_vertices, left, bottom, color, u0, v1 ) &&
               push_vertex( this->m_vertices, left, bottom, color, u0, v1 ) &&
               push_vertex( this->m_vertices, right, top, color, u1, v0 ) &&
               push_vertex( this->m_vertices, right, bottom, color, u1, v1 );
    }

#if !defined( ATLAS_RENDER_D3D11 )
    bool backend::append_model( const render_command &command )
    {
        if ( !this->m_model_ready ||
             command.m_bounds.right <= command.m_bounds.left ||
             command.m_bounds.bottom <= command.m_bounds.top || !command.m_color.a )
        {
            return true;
        }

        const float left{
            transform_coordinate( static_cast< float >( command.m_bounds.left ) - 0.5f,
                                  command.m_transform_origin.x,
                                  command.m_transform_scale )
        };
        const float top{
            transform_coordinate( static_cast< float >( command.m_bounds.top ) - 0.5f,
                                  command.m_transform_origin.y,
                                  command.m_transform_scale )
        };
        const float right{
            transform_coordinate( static_cast< float >( command.m_bounds.right ) - 0.5f,
                                  command.m_transform_origin.x,
                                  command.m_transform_scale )
        };
        const float bottom{
            transform_coordinate( static_cast< float >( command.m_bounds.bottom ) - 0.5f,
                                  command.m_transform_origin.y,
                                  command.m_transform_scale )
        };

        const u32 color{ command.m_color.d3d( ) };

        return this->begin_batch( batch_type::model ) &&
               push_vertex( this->m_vertices, left, top, color, 0.0f, 0.0f ) &&
               push_vertex( this->m_vertices, right, top, color, this->m_model_uv.x, 0.0f ) &&
               push_vertex( this->m_vertices, left, bottom, color, 0.0f, this->m_model_uv.y ) &&
               push_vertex( this->m_vertices, left, bottom, color, 0.0f, this->m_model_uv.y ) &&
               push_vertex( this->m_vertices, right, top, color, this->m_model_uv.x, 0.0f ) &&
               push_vertex( this->m_vertices, right, bottom, color, this->m_model_uv.x, this->m_model_uv.y );
    }
#endif

    bool backend::apply_scissor( const render_command &command )
    {
        rect_t bounds{ };

        if ( command.m_clipped )
        {
            bounds.left = static_cast< i32 >(
                transform_coordinate( static_cast< float >( command.m_clip.left ),
                                      command.m_transform_origin.x,
                                      command.m_transform_scale ) +
                0.5f );
            bounds.top = static_cast< i32 >(
                transform_coordinate( static_cast< float >( command.m_clip.top ),
                                      command.m_transform_origin.y,
                                      command.m_transform_scale ) +
                0.5f );
            bounds.right = static_cast< i32 >(
                transform_coordinate( static_cast< float >( command.m_clip.right ),
                                      command.m_transform_origin.x,
                                      command.m_transform_scale ) +
                0.5f );
            bounds.bottom = static_cast< i32 >(
                transform_coordinate( static_cast< float >( command.m_clip.bottom ),
                                      command.m_transform_origin.y,
                                      command.m_transform_scale ) +
                0.5f );
        }

        if ( command.m_clipped == this->m_scissor_enabled &&
             ( !command.m_clipped ||
               ( bounds.left == this->m_scissor.left && bounds.top == this->m_scissor.top &&
                 bounds.right == this->m_scissor.right &&
                 bounds.bottom == this->m_scissor.bottom ) ) )
        {
            return true;
        }

        if ( !this->flush_batch( ) || !this->set_scissor( bounds, command.m_clipped ) )
        {
            return false;
        }

        this->m_scissor         = bounds;
        this->m_scissor_enabled = command.m_clipped;
        return true;
    }

    bool backend::append_command( const render_command &command )
    {
        if ( !this->apply_scissor( command ) )
        {
            return false;
        }

        switch ( command.m_type )
        {
            case command_type::line: return this->append_line( command );
            case command_type::box: return this->append_box( command );
            case command_type::gradient: return this->append_gradient( command );
            case command_type::circle: return this->append_circle( command );
            case command_type::arc: return this->append_arc( command );
            case command_type::polygon: return this->append_polygon( command );
            case command_type::blur: return this->append_blur( command );
#if !defined( ATLAS_RENDER_D3D11 )
            case command_type::model: return this->append_model( command );
#endif
            case command_type::text: return this->append_text( command );
            case command_type::icon: return this->append_icon( command );
            case command_type::html: return this->append_html( command );
            default: return true;
        }
    }

    // -------------------------------------------------------------------------
    // Immediate draw list.
    // -------------------------------------------------------------------------

    void backend::line( point_t from, point_t to, color_t value, float thickness )
    {
        if ( value.a == 0 )
        {
            return;
        }

        this->line( vec2{ static_cast< float >( from.x ) + 0.5f, static_cast< float >( from.y ) + 0.5f },
                    vec2{ static_cast< float >( to.x ) + 0.5f, static_cast< float >( to.y ) + 0.5f },
                    value, thickness );
    }

    void backend::line( vec2 from, vec2 to, color_t value, float thickness )
    {
        if ( value.a == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::line;
        command.m_from             = from;
        command.m_to               = to;
        command.m_thickness        = thickness;
        command.m_color            = value;
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;
        this->push_command( command );
    }

    void backend::circle( vec2 point, color_t col, f32 radius, f32 thickness )
    {
        if ( col.a == 0 )
        {
            return;
        }

        this->circle( point_t{ ( i32 )point.x, ( i32 )point.y }, col, radius, thickness );
    }

    void backend::circle( point_t point, color_t col, f32 radius, float thickness )
    {
        if ( col.a == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::circle;
        command.m_position         = point;
        command.m_radius           = radius;
        command.m_thickness        = thickness;
        command.m_color            = col;
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;
        this->push_command( command );
    }

    void backend::arc( vec2 centre, f32 radius, f32 thickness, f32 start, f32 sweep, color_t value )
    {
        if ( value.a == 0 || radius <= 0.0f || thickness <= 0.0f || sweep == 0.0f )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::arc;
        command.m_from             = centre;
        command.m_radius           = radius;
        command.m_thickness        = thickness;
        command.m_start            = start;
        command.m_sweep            = sweep;
        command.m_color            = value;
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;
        this->push_command( command );
    }

    void backend::box( rect_t bounds, color_t value, bool filled )
    {
        if ( value.a == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::box;
        command.m_bounds           = bounds;
        command.m_color            = value;
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;
        command.m_filled           = filled;
        this->push_command( command );
    }

    void backend::gradient( rect_t bounds, color_t from, color_t to, bool vertical )
    {
        if ( from.a == 0 && to.a == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::gradient;
        command.m_bounds           = bounds;
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;
        set_gradient_corners( command, from, to, vertical );
        this->push_command( command );
    }

    void backend::polygon( const vec2 *points, const color_t *colors, usize count )
    {
        if ( count < 3 || count > POLYGON_CAPACITY )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::polygon;
        command.m_point_count      = static_cast< u32 >( count );
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;

        for ( usize index{ }; index < count; ++index )
        {
            command.m_points[ index ]       = points[ index ];
            command.m_point_colors[ index ] = colors[ index ];
        }

        this->push_command( command );
    }

    void backend::text( const char *font, point_t position, color_t value, string_view text,
                        bool outlined, text_align align, float crop_top, u32 avatar_account,
                        u32 avatar_shape, u32 avatar_size )
    {
        if ( !this->m_inside_frame || text.empty( ) || value.a == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::text;
        command.m_position         = position;
        command.m_color            = value;
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;
        command.m_outlined         = outlined;
        command.m_align            = align;
        command.m_crop_top         = crop_top;
        command.m_avatar_account   = avatar_account;
        command.m_avatar_shape     = avatar_shape;
        command.m_avatar_size      = avatar_size;

        if ( font && !command.m_font.assign( font ) )
        {
            this->m_frame_build_failed = true;
            return;
        }

        if ( !command.m_text.assign( text ) )
        {
            this->m_frame_build_failed = true;
            return;
        }

        this->push_command( command );
    }

    // forward declaration...
    f32 line_advance( const text_atlas &atlas, const render_command &command, usize start );
    i32 backend::get_text_width( const char *font, string_view text, text_align align )
    {
        if ( text.empty( ) )
        {
            return 0;
        }

        // Hoku; 10/1/26 - This function didn't already exist in our renderer, and quite frankly the lack of its existence really pissed me off.
        // Anyways, we create a fake render_command here to trick the renderer into ensuring our actual text atlas, otherwise we're gonna be calculating off of nothing.
        // This function is a gross hack and should be redone at some point...
        render_command command{ };
        command.m_type           = command_type::text;
        command.m_position       = { 0, 0 };
        command.m_color          = color_t{ 255, 255, 255, 255 };
        command.m_outlined       = false;
        command.m_align          = align;
        command.m_crop_top       = 0.0f;
        command.m_avatar_account = 0;
        command.m_avatar_shape   = AVATAR_NONE;
        command.m_text.assign( text );

        if ( font && !command.m_font.assign( font ) )
        {
            return 0;
        }

        text_atlas *atlas{ };

        // a font we cannot atlas drops its own string, never the frame it shares with everything else;
        if ( !this->ensure_text_atlas( command, &atlas ) )
        {
            return 0;
        }

        // TODO: Make me support newlines!
        return line_advance( *atlas, command, 0 );
    }

    void backend::blur( rect_t bounds, u8 opacity )
    {
        if ( opacity == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::blur;
        command.m_bounds           = bounds;
        command.m_color            = color_t{ 255, 255, 255, opacity };
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;
        this->push_command( command );
    }

#if !defined( ATLAS_RENDER_D3D11 )
    void backend::model( rect_t bounds, u8 opacity )
    {
        if ( opacity == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::model;
        command.m_bounds           = bounds;
        command.m_color            = color_t{ 255, 255, 255, opacity };
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;
        this->push_command( command );
    }
#endif

    void backend::icon( u32 slot, u32 atlas_kind, point_t position, color_t value,
                        float crop_top, bool outlined )
    {
        if ( !this->m_inside_frame || atlas_kind >= this->m_icon_atlases.size( ) || value.a == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::icon;
        command.m_position         = position;
        command.m_color            = value;
        command.m_icon_slot        = slot;
        command.m_icon_atlas       = atlas_kind;
        command.m_crop_top         = crop_top;
        command.m_outlined         = outlined;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        this->push_command( command );
    }

    void backend::html( u32 slot, rect_t bounds, u8 opacity )
    {
        if ( opacity == 0 || slot >= HTML_PANEL_CAPACITY )
        {
            return;
        }

        render_command command{ };
        command.m_type             = command_type::html;
        command.m_icon_slot        = slot;
        command.m_bounds           = bounds;
        command.m_color            = color_t{ 255, 255, 255, opacity };
        command.m_transform_origin = this->m_transform_origin;
        command.m_transform_scale  = this->m_transform_scale;
        command.m_clip             = this->m_clip;
        command.m_clipped          = this->m_clip_enabled;
        this->push_command( command );
    }

    void backend::set_transform( point_t origin, float scale )
    {
        this->m_transform_origin = origin;
        this->m_transform_scale  = scale;
    }

    void backend::reset_transform( )
    {
        this->m_transform_origin = point_t{ };
        this->m_transform_scale  = 1.0f;
    }

    void backend::set_clip( rect_t bounds )
    {
        this->m_clip         = bounds;
        this->m_clip_enabled = bounds.right > bounds.left && bounds.bottom > bounds.top;
    }

    void backend::reset_clip( )
    {
        this->m_clip         = rect_t{ };
        this->m_clip_enabled = false;
    }

    bool backend::push_command( const render_command &command )
    {
        if ( !this->m_inside_frame || this->m_frame_build_failed )
        {
            return false;
        }

        bool result{ this->m_frame_commands.push_back( command ) };

        if ( !result )
        {
            this->m_frame_build_failed = true;
        }

        return result;
    }

    // -------------------------------------------------------------------------
    // Batched draw list.
    // -------------------------------------------------------------------------

    void draw_list::line( point_t from, point_t to, color_t value, float thickness )
    {
        if ( value.a == 0 )
        {
            return;
        }

        this->line( vec2{ static_cast< float >( from.x ) + 0.5f, static_cast< float >( from.y ) + 0.5f },
                    vec2{ static_cast< float >( to.x ) + 0.5f, static_cast< float >( to.y ) + 0.5f },
                    value, thickness );
    }

    void draw_list::line( vec2 from, vec2 to, color_t value, float thickness )
    {
        if ( value.a == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type      = command_type::line;
        command.m_from      = from;
        command.m_to        = to;
        command.m_thickness = thickness;
        command.m_color     = value;
        this->push_command( command );
    }

    void draw_list::circle( vec2 point, color_t col, f32 radius, f32 thickness )
    {
        if ( col.a == 0 )
        {
            return;
        }

        this->circle( point_t{ ( i32 )point.x, ( i32 )point.y }, col, radius, thickness );
    }

    void draw_list::circle( point_t point, color_t col, f32 radius, f32 thickness )
    {
        if ( col.a == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type      = command_type::circle;
        command.m_position  = point;
        command.m_color     = col;
        command.m_radius    = radius;
        command.m_thickness = thickness;
        this->push_command( command );
    }

    void draw_list::arc( vec2 centre, f32 radius, f32 thickness, f32 start, f32 sweep, color_t value )
    {
        if ( value.a == 0 || radius <= 0.0f || thickness <= 0.0f || sweep == 0.0f )
        {
            return;
        }

        render_command command{ };
        command.m_type      = command_type::arc;
        command.m_from      = centre;
        command.m_radius    = radius;
        command.m_thickness = thickness;
        command.m_start     = start;
        command.m_sweep     = sweep;
        command.m_color     = value;
        this->push_command( command );
    }

    void draw_list::box( rect_t bounds, color_t value, bool filled )
    {
        if ( value.a == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type   = command_type::box;
        command.m_bounds = bounds;
        command.m_color  = value;
        command.m_filled = filled;
        this->push_command( command );
    }

    void draw_list::gradient( rect_t bounds, color_t from, color_t to, bool vertical )
    {
        if ( from.a == 0 && to.a == 0 )
        {
            return;
        }

        render_command command{ };
        command.m_type   = command_type::gradient;
        command.m_bounds = bounds;
        set_gradient_corners( command, from, to, vertical );
        this->push_command( command );
    }

    void draw_list::polygon( const vec2 *points, const color_t *colors, usize count )
    {
        if ( count < 3 || count > POLYGON_CAPACITY )
        {
            return;
        }

        render_command command{ };
        command.m_type        = command_type::polygon;
        command.m_point_count = static_cast< u32 >( count );

        for ( usize index{ }; index < count; ++index )
        {
            command.m_points[ index ]       = points[ index ];
            command.m_point_colors[ index ] = colors[ index ];
        }

        this->push_command( command );
    }

    void draw_list::icon( u32 slot, u32 atlas_kind, point_t position, color_t value,
                          float crop_top, bool outlined )
    {
        if ( value.a == 0 )
        {
            return;
        }

        atl::scoped_lock lock{ this->m_mutex };
        render_command   command{ };
        command.m_type       = command_type::icon;
        command.m_position   = position;
        command.m_color      = value;
        command.m_icon_slot  = slot;
        command.m_icon_atlas = atlas_kind;
        command.m_crop_top   = crop_top;
        command.m_outlined   = outlined;

        if ( !this->m_commands.push_back( command ) )
        {
            this->m_build_failed = true;
        }
    }

    void draw_list::html( u32 slot, rect_t bounds, u8 opacity )
    {
        if ( opacity == 0 )
        {
            return;
        }

        atl::scoped_lock lock{ this->m_mutex };
        render_command   command{ };
        command.m_type      = command_type::html;
        command.m_icon_slot = slot;
        command.m_bounds    = bounds;
        command.m_color     = color_t{ 255, 255, 255, opacity };

        if ( !this->m_commands.push_back( command ) )
        {
            this->m_build_failed = true;
        }
    }

    void draw_list::text( const char *font, point_t position, color_t value, string_view text,
                          bool outlined, text_align align, float crop_top, u32 avatar_account,
                          u32 avatar_shape )
    {
        if ( text.empty( ) || value.a == 0 )
        {
            return;
        }

        atl::scoped_lock lock{ this->m_mutex };
        render_command   command{ };
        command.m_type           = command_type::text;
        command.m_position       = position;
        command.m_color          = value;
        command.m_outlined       = outlined;
        command.m_align          = align;
        command.m_crop_top       = crop_top;
        command.m_avatar_account = avatar_account;
        command.m_avatar_shape   = avatar_shape;

        if ( font && !command.m_font.assign( font ) )
        {
            this->m_build_failed = true;
            return;
        }

        if ( !command.m_text.assign( text ) )
        {
            this->m_build_failed = true;
            return;
        }

        if ( !this->m_commands.push_back( command ) )
        {
            this->m_build_failed = true;
        }
    }

    bool draw_list::send( )
    {
        atl::scoped_lock lock{ this->m_mutex };

        if ( this->m_build_failed )
        {
            this->m_safe_commands.clear( );
            this->m_commands.clear( );
            this->m_build_failed   = false;
            this->m_snapshot_valid = false;
            return false;
        }

        this->m_commands.swap( this->m_safe_commands );
        this->m_commands.clear( );
        this->m_build_failed   = false;
        this->m_snapshot_valid = true;
        return true;
    }

    void draw_list::traverse( )
    {
        atl::scoped_lock lock{ this->m_mutex };

        if ( !this->m_snapshot_valid )
        {
            return;
        }

        for ( const render_command &command : this->m_safe_commands )
        {
            this->dispatch( command );
        }
    }

    void draw_list::reset( )
    {
        atl::scoped_lock lock{ this->m_mutex };

        this->m_safe_commands.reset( );
        this->m_commands.reset( );
        this->m_build_failed   = false;
        this->m_snapshot_valid = false;
    }

    bool draw_list::push_command( const render_command &command )
    {
        atl::scoped_lock lock{ this->m_mutex };
        bool             result{ this->m_commands.push_back( command ) };

        if ( !result )
        {
            this->m_build_failed = true;
        }

        return result;
    }

    void draw_list::dispatch( const render_command &command ) const
    {
        switch ( command.m_type )
        {
            case command_type::line:
                g_render.line( command.m_from, command.m_to, command.m_color, command.m_thickness );
                break;

            case command_type::box:
                g_render.box( command.m_bounds, command.m_color, command.m_filled );
                break;

            case command_type::gradient:
                g_render.gradient( command.m_bounds, command.m_point_colors[ 0 ], command.m_point_colors[ 3 ],
                                   command.m_point_colors[ 1 ].d3d( ) == command.m_point_colors[ 0 ].d3d( ) );
                break;

            case command_type::circle:
                g_render.circle( command.m_position, command.m_color, command.m_radius, command.m_thickness );
                break;

            case command_type::arc:
                g_render.arc( command.m_from, command.m_radius, command.m_thickness, command.m_start, command.m_sweep,
                              command.m_color );
                break;

            case command_type::polygon:
                g_render.polygon( command.m_points, command.m_point_colors, command.m_point_count );
                break;

            case command_type::text:
                g_render.text( command.m_font.empty( ) ? nullptr : command.m_font.c_str( ), command.m_position,
                               command.m_color, command.m_text.c_str( ), command.m_outlined,
                               command.m_align, command.m_crop_top, command.m_avatar_account,
                               command.m_avatar_shape, command.m_avatar_size );
                break;

            case command_type::icon:
                g_render.icon( command.m_icon_slot, command.m_icon_atlas, command.m_position,
                               command.m_color, command.m_crop_top, command.m_outlined );
                break;

            case command_type::html:
                g_render.html( command.m_icon_slot, command.m_bounds, command.m_color.a );
                break;

            default:
                break;
        }
    }

    // -------------------------------------------------------------------------
    // Text renderer and glyph atlas.
    // -------------------------------------------------------------------------

    f32 line_advance( const text_atlas &atlas, const render_command &command, usize start )
    {
        float width{ };

        for ( usize index{ start }; index < command.m_text.size( ); )
        {
            u32   code{ };
            usize consumed{ decode_utf8_codepoint( command.m_text.data( ), command.m_text.size( ), index, &code ) };

            if ( !consumed || code == '\n' )
            {
                break;
            }

            index += consumed;

            if ( code < TEXT_ATLAS_FIRST_CHAR || code > TEXT_ATLAS_LAST_CHAR )
            {
                code = '?';
            }

            width += atlas.m_glyphs[ code ].m_advance;
        }

        return width;
    }

    static float aligned_cursor( const text_atlas &atlas, const render_command &command,
                                 usize start, float origin_x )
    {
        if ( command.m_align == text_align::left )
        {
            return origin_x;
        }

        const float width{ line_advance( atlas, command, start ) };
        const float shifted{ command.m_align == text_align::center ? origin_x - width * 0.5f : origin_x - width };

        // centering an odd width lands on a half pixel; with point sampling every glyph
        // then sits half a texel off center and its edge columns get chopped, so the
        // aligned start snaps to whole pixels
        return static_cast< float >( static_cast< i32 >( shifted >= 0.0f ? shifted + 0.5f : shifted - 0.5f ) );
    }

    bool backend::append_text( const render_command &command )
    {
        if ( command.m_text.empty( ) )
        {
            return true;
        }

        text_atlas *atlas{ };

        // a font we cannot atlas drops its own string, never the frame it shares with everything else;
        if ( !this->ensure_text_atlas( command, &atlas ) )
        {
            return true;
        }

        if ( this->m_active_text_atlas != atlas )
        {
            if ( !this->flush_batch( ) )
            {
                return false;
            }

            this->m_active_text_atlas = atlas;
        }

        const float line_height{ static_cast< float >( atlas->m_line_height ) };
        const bool  with_avatar{ command.m_avatar_account != 0 && command.m_avatar_shape != AVATAR_NONE };
        const float avatar_size{ command.m_avatar_size ? static_cast< float >( command.m_avatar_size ) : line_height };
        const float avatar_pad{ with_avatar ? avatar_size + 4.0f : 0.0f };

        float origin_x{ static_cast< float >( command.m_position.x ) };

        if ( with_avatar && command.m_align == text_align::center )
        {
            origin_x += avatar_pad * 0.5f;
        } else if ( with_avatar && command.m_align == text_align::left )
        {
            origin_x += avatar_pad;
        }

        float cursor_x{ aligned_cursor( *atlas, command, 0, origin_x ) };
        float cursor_y{ static_cast< float >( command.m_position.y ) };

        if ( with_avatar &&
             !this->append_avatar( command, cursor_x - avatar_pad, cursor_y, avatar_size ) )
        {
            return false;
        }

        if ( !this->begin_batch( batch_type::text ) )
        {
            return false;
        }

        const u32 color{ command.m_color.d3d( ) };

        const u32 outline_color{ color_t{ 0, 0, 0, command.m_color.a }.d3d( ) };
        for ( usize index{ }; index < command.m_text.size( ); )
        {
            u32   code{ };
            usize consumed{ decode_utf8_codepoint( command.m_text.data( ), command.m_text.size( ), index, &code ) };

            if ( !consumed )
            {
                break;
            }

            index += consumed;

            if ( code == '\n' )
            {
                cursor_x = aligned_cursor( *atlas, command, index, origin_x );
                cursor_y += line_height + ( line_height * 0.25f );
                continue;
            }

            // @ unsupported unicode code points are rendered as a single '?' glyph
            if ( code < TEXT_ATLAS_FIRST_CHAR || code > TEXT_ATLAS_LAST_CHAR )
            {
                code = '?';
            }

            const text_glyph &glyph{ atlas->m_glyphs[ code ] };

            if ( glyph.m_visible )
            {
                if ( command.m_outlined )
                {
                    static constexpr float shifts[ 4 ][ 2 ]{
                        { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f }
                    };

                    for ( const float ( &shift )[ 2 ] : shifts )
                    {
                        if ( !push_glyph_quad( this->m_vertices, command, glyph, cursor_x, cursor_y,
                                               shift[ 0 ], shift[ 1 ], outline_color ) )
                        {
                            return false;
                        }
                    }
                }

                if ( !push_glyph_quad( this->m_vertices, command, glyph, cursor_x, cursor_y,
                                       0.0f, 0.0f, color ) )
                {
                    return false;
                }
            }

            cursor_x += glyph.m_advance;
        }

        return true;
    }

    avatar_entry_t *backend::ensure_avatar( u32 account )
    {
        avatar_entry_t *oldest{ };

        for ( usize index{ }; index < this->m_avatar_count; ++index )
        {
            avatar_entry_t &entry{ this->m_avatars[ index ] };

            if ( entry.m_account == account )
            {
                entry.m_last_used = this->m_frame_ms;
                return entry.m_ready || this->fetch_avatar( entry ) ? &entry : nullptr;
            }

            if ( !oldest || entry.m_last_used < oldest->m_last_used )
            {
                oldest = &entry;
            }
        }

        avatar_entry_t *slot{ };

        if ( this->m_avatar_count < this->m_avatars.size( ) )
        {
            slot = &this->m_avatars[ this->m_avatar_count++ ];
        } else
        {
            slot = oldest;
            slot->m_texture.Release( );
            *slot = avatar_entry_t{ };
        }

        slot->m_account   = account;
        slot->m_last_used = this->m_frame_ms;
        return this->fetch_avatar( *slot ) ? slot : nullptr;
    }

    static u32 avatar_mip_levels( u32 width, u32 height )
    {
        u32 levels{ 1 };

        while ( width > 1 && height > 1 && !( width & 1 ) && !( height & 1 ) )
        {
            width >>= 1;
            height >>= 1;
            ++levels;
        }

        return levels;
    }

    static void downsample_half( u8 *pixels, u32 width, u32 height )
    {
        const u32 half_width{ width >> 1 };
        const u32 half_height{ height >> 1 };

        // the destination index always trails the source index, so the halving runs in place
        for ( u32 y{ }; y < half_height; ++y )
        {
            const u8 *top{ pixels + static_cast< usize >( y ) * 2 * width * 4 };
            const u8 *bottom{ top + static_cast< usize >( width ) * 4 };
            u8       *out{ pixels + static_cast< usize >( y ) * half_width * 4 };

            for ( u32 x{ }; x < half_width; ++x )
            {
                for ( u32 channel{ }; channel < 4; ++channel )
                {
                    const u32 sum{ static_cast< u32 >( top[ x * 8 + channel ] ) +
                                   top[ x * 8 + 4 + channel ] + bottom[ x * 8 + channel ] +
                                   bottom[ x * 8 + 4 + channel ] };

                    out[ x * 4 + channel ] = static_cast< u8 >( ( sum + 2 ) >> 2 );
                }
            }
        }
    }

    struct avatar_edge_t
    {
        vec2  m_inner{ };
        vec2  m_outer{ };
        float m_u{ };
        float m_v{ };
    };

    bool backend::fetch_avatar( avatar_entry_t &entry )
    {
        if ( this->m_frame_ms < entry.m_next_attempt )
        {
            return false;
        }

        entry.m_next_attempt = this->m_frame_ms + 1000;

        const i32 image{ g_steam.medium_avatar( entry.m_account ) };
        if ( image <= 0 )
        {
            g_steam.request_info( entry.m_account );
            return false;
        }

        u32 width{ };
        u32 height{ };
        if ( !g_steam.image_size( image, width, height ) || !width || !height || width > 128 ||
             height > 128 )
        {
            return false;
        }

        const usize bytes{ static_cast< usize >( width ) * height * 4 };
        if ( !this->m_avatar_scratch.resize( bytes ) ||
             !g_steam.image_rgba( image, this->m_avatar_scratch.data( ),
                                  static_cast< i32 >( bytes ) ) )
        {
            return false;
        }

        u8 *pixels{ this->m_avatar_scratch.data( ) };

        for ( usize index{ }; index < bytes; index += 4 )
        {
            const u8 red{ pixels[ index ] };
            pixels[ index ]     = pixels[ index + 2 ];
            pixels[ index + 2 ] = red;
        }

        const u32 levels{ avatar_mip_levels( width, height ) };

        texture_t texture{ };

        if ( !this->create_texture( width, height, levels, texture ) )
        {
            return false;
        }

        u32 level_width{ width };
        u32 level_height{ height };

        for ( u32 level{ }; level < levels; ++level )
        {
            if ( level )
            {
                downsample_half( pixels, level_width, level_height );
                level_width >>= 1;
                level_height >>= 1;
            }

            const rect_t area{ 0, 0, static_cast< i32 >( level_width ), static_cast< i32 >( level_height ) };

            if ( !this->upload_texture( texture, level, area, pixels, static_cast< usize >( level_width ) * 4 ) )
            {
                texture.Release( );
                return false;
            }
        }

        entry.m_texture.Release( );
        entry.m_texture = texture;
        texture.Detach( );
        entry.m_ready = true;
        return true;
    }

    void backend::release_avatars( )
    {
        for ( avatar_entry_t &entry : this->m_avatars )
        {
            entry.m_texture.Release( );
            entry = avatar_entry_t{ };
        }

        this->m_avatar_count = 0;
        this->m_avatar_scratch.reset( );
        this->m_avatar_bound = nullptr;
    }

    bool backend::append_avatar( const render_command &command, float left, float top, float size )
    {
        avatar_entry_t *entry{ this->ensure_avatar( command.m_avatar_account ) };
        const auto      place{ [ & ]( float x, float y, vec2 &out )
        {
            out.x = transform_coordinate( x - 0.5f, command.m_transform_origin.x,
                                          command.m_transform_scale );
            out.y = transform_coordinate( y - 0.5f, command.m_transform_origin.y,
                                          command.m_transform_scale );
        } };

        if ( entry )
        {
            if ( this->m_avatar_bound != &entry->m_texture )
            {
                if ( !this->flush_batch( ) )
                {
                    return false;
                }

                this->m_avatar_bound = &entry->m_texture;
            }

            if ( !this->begin_batch( batch_type::avatar ) )
            {
                return false;
            }
        } else if ( !this->begin_batch( batch_type::solid ) )
        {
            return false;
        }

        const float half{ size * 0.5f };
        const float center_x{ left + half };
        const float center_y{ top + half };
        const u8    alpha{ entry ? command.m_color.a : static_cast< u8 >( command.m_color.a / 6 ) };
        const u32   core{ color_t{ 255, 255, 255, alpha }.d3d( ) };
        const u32   rim{ color_t{ 255, 255, 255, 0 }.d3d( ) };

        vec2 center{ };
        place( center_x, center_y, center );

        if ( command.m_avatar_shape == AVATAR_SQUARE )
        {
            vec2 top_left{ };
            vec2 top_right{ };
            vec2 bottom_right{ };
            vec2 bottom_left{ };
            place( left, top, top_left );
            place( left + size, top, top_right );
            place( left + size, top + size, bottom_right );
            place( left, top + size, bottom_left );

            const d3d9::overlay_vertex geometry[ 6 ]{
                { top_left.x, top_left.y, 0.0f, 1.0f, core, 0.0f, 0.0f },
                { top_right.x, top_right.y, 0.0f, 1.0f, core, 1.0f, 0.0f },
                { bottom_right.x, bottom_right.y, 0.0f, 1.0f, core, 1.0f, 1.0f },
                { top_left.x, top_left.y, 0.0f, 1.0f, core, 0.0f, 0.0f },
                { bottom_right.x, bottom_right.y, 0.0f, 1.0f, core, 1.0f, 1.0f },
                { bottom_left.x, bottom_left.y, 0.0f, 1.0f, core, 0.0f, 1.0f },
            };

            if ( !this->m_vertices.reserve( this->m_vertices.size( ) + 6 ) )
            {
                return false;
            }

            for ( const d3d9::overlay_vertex &vertex : geometry )
            {
                if ( !this->m_vertices.push_back( vertex ) )
                {
                    return false;
                }
            }

            return this->flush_batch( );
        }

        const i32 point_count{ static_cast< i32 >( math::clamp( 5.0f * math::sqrt( size ), 16.0f, 64.0f ) ) };

        constexpr float FEATHER{ 0.5f };

        const auto edge_at{ [ & ]( i32 index )
        {
            const float theta{ ( PI * 2.0f ) * ( static_cast< float >( index ) /
                                                 static_cast< float >( point_count ) ) };

            const float normal_x{ math::cos( theta ) };
            const float normal_y{ math::sin( theta ) };
            const float inset_x{ center_x + normal_x * ( half - FEATHER ) };
            const float inset_y{ center_y + normal_y * ( half - FEATHER ) };

            avatar_edge_t edge{ };
            place( inset_x, inset_y, edge.m_inner );
            place( center_x + normal_x * ( half + FEATHER ),
                   center_y + normal_y * ( half + FEATHER ), edge.m_outer );

            edge.m_u = math::clamp( ( inset_x - left ) / size, 0.0f, 1.0f );
            edge.m_v = math::clamp( ( inset_y - top ) / size, 0.0f, 1.0f );
            return edge;
        } };

        if ( !this->m_vertices.reserve( this->m_vertices.size( ) +
                                        static_cast< usize >( point_count ) * 9 ) )
        {
            return false;
        }

        const avatar_edge_t first{ edge_at( 0 ) };
        avatar_edge_t       previous{ first };

        for ( i32 index{ 1 }; index <= point_count; ++index )
        {
            const avatar_edge_t current{ index == point_count ? first : edge_at( index ) };

            const d3d9::overlay_vertex geometry[ 9 ]{
                { center.x, center.y, 0.0f, 1.0f, core, 0.5f, 0.5f },
                { previous.m_inner.x, previous.m_inner.y, 0.0f, 1.0f, core, previous.m_u,
                  previous.m_v },
                { current.m_inner.x, current.m_inner.y, 0.0f, 1.0f, core, current.m_u,
                  current.m_v },

                { previous.m_inner.x, previous.m_inner.y, 0.0f, 1.0f, core, previous.m_u,
                  previous.m_v },
                { current.m_inner.x, current.m_inner.y, 0.0f, 1.0f, core, current.m_u,
                  current.m_v },
                { current.m_outer.x, current.m_outer.y, 0.0f, 1.0f, rim, current.m_u,
                  current.m_v },

                { previous.m_inner.x, previous.m_inner.y, 0.0f, 1.0f, core, previous.m_u,
                  previous.m_v },
                { current.m_outer.x, current.m_outer.y, 0.0f, 1.0f, rim, current.m_u,
                  current.m_v },
                { previous.m_outer.x, previous.m_outer.y, 0.0f, 1.0f, rim, previous.m_u,
                  previous.m_v },
            };

            for ( const d3d9::overlay_vertex &vertex : geometry )
            {
                if ( !this->m_vertices.push_back( vertex ) )
                {
                    return false;
                }
            }

            previous = current;
        }

        return this->flush_batch( );
    }

    bool backend::append_icon( const render_command &command )
    {
        if ( command.m_icon_atlas >= this->m_icon_atlases.size( ) )
        {
            return true;
        }

        const icon_atlas_t &atlas{ this->m_icon_atlases[ command.m_icon_atlas ] };

        if ( !atlas.m_ready || !atlas.m_slot_count || command.m_icon_slot >= atlas.m_slot_count )
        {
            return true;
        }

        static constexpr batch_type BATCH_FOR[]{ batch_type::icon_small, batch_type::icon_large,
                                                 batch_type::icon_logo, batch_type::icon_vacc };
        static_assert( sizeof( BATCH_FOR ) / sizeof( BATCH_FOR[ 0 ] ) == icons::KIND_COUNT );
        const batch_type type{ BATCH_FOR[ command.m_icon_atlas ] };

        if ( !this->begin_batch( type ) )
        {
            return false;
        }

        const u32   slot_size{ atlas.m_slot_size };
        const float atlas_width{ static_cast< float >( slot_size * atlas.m_slot_count ) };
        const float atlas_height{ static_cast< float >( slot_size ) };

        const float crop{ command.m_crop_top < 0.0f   ? 0.0f
                          : command.m_crop_top > 1.0f ? 1.0f
                                                      : command.m_crop_top };

        if ( crop >= 1.0f )
        {
            return true;
        }

        const float span{ static_cast< float >( slot_size ) };
        const float quad_left{ static_cast< float >( command.m_position.x ) };
        const float quad_top{ static_cast< float >( command.m_position.y ) + span * crop };
        const float quad_right{ quad_left + span };
        const float quad_bottom{ static_cast< float >( command.m_position.y ) + span };

        const float u0{ static_cast< float >( command.m_icon_slot * slot_size ) / atlas_width };
        const float u1{ static_cast< float >( ( command.m_icon_slot + 1u ) * slot_size ) /
                        atlas_width };
        const float v1{ span / atlas_height };
        const float v0{ v1 * crop };

        const u32 color{ command.m_color.d3d( ) };

        auto quad{ [ & ]( float shift_x, float shift_y, u32 value )
        {
            const float left{ transform_coordinate( quad_left + shift_x - 0.5f,
                                                    command.m_transform_origin.x,
                                                    command.m_transform_scale ) };
            const float top{ transform_coordinate( quad_top + shift_y - 0.5f,
                                                   command.m_transform_origin.y,
                                                   command.m_transform_scale ) };
            const float right{ transform_coordinate( quad_right + shift_x - 0.5f,
                                                     command.m_transform_origin.x,
                                                     command.m_transform_scale ) };
            const float bottom{ transform_coordinate( quad_bottom + shift_y - 0.5f,
                                                      command.m_transform_origin.y,
                                                      command.m_transform_scale ) };

            return push_vertex( this->m_vertices, left, top, value, u0, v0 ) &&
                   push_vertex( this->m_vertices, right, top, value, u1, v0 ) &&
                   push_vertex( this->m_vertices, left, bottom, value, u0, v1 ) &&
                   push_vertex( this->m_vertices, left, bottom, value, u0, v1 ) &&
                   push_vertex( this->m_vertices, right, top, value, u1, v0 ) &&
                   push_vertex( this->m_vertices, right, bottom, value, u1, v1 );
        } };

        // the esp draws over the world, so an untraced silhouette vanishes on anything bright;
        // this is the same four-way pass the glyph path uses
        if ( command.m_outlined )
        {
            static constexpr float SHIFTS[ 4 ][ 2 ]{
                { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f }
            };

            const u32 outline{ color_t{ 0, 0, 0, command.m_color.a }.d3d( ) };

            for ( const float ( &shift )[ 2 ] : SHIFTS )
            {
                if ( !quad( shift[ 0 ], shift[ 1 ], outline ) )
                {
                    return false;
                }
            }
        }

        return quad( 0.0f, 0.0f, color );
    }

    bool backend::append_html( const render_command &command )
    {
        // the panel takes input where it is on screen this frame, and only while it is; this runs outside the draw
        // list lock because the steam calls block on ipc
        g_html.forward_input( command.m_icon_slot, command.m_bounds );

        html_texture_t &entry{ this->m_html_textures[ command.m_icon_slot ] };

        const auto upload{ [ & ]( const html_frame_t &frame )
        {
            const bool fresh{ !entry.m_texture || entry.m_generation != frame.m_generation ||
                              entry.m_width != frame.m_width || entry.m_height != frame.m_height };

            if ( fresh )
            {
                entry.m_texture.Release( );
                entry = html_texture_t{ };

                if ( !this->create_texture( frame.m_width, frame.m_height, 1, entry.m_texture ) )
                {
                    return false;
                }

                entry.m_width      = frame.m_width;
                entry.m_height     = frame.m_height;
                entry.m_generation = frame.m_generation;
            }

            const rect_t area{ fresh ? rect_t{ 0, 0, static_cast< i32 >( frame.m_width ), static_cast< i32 >( frame.m_height ) }
                                     : frame.m_dirty };

            if ( area.right <= area.left || area.bottom <= area.top )
            {
                return true;
            }

            // steam paints B8G8R8A8, which is A8R8G8B8 in memory
            return this->upload_texture( entry.m_texture, 0, area,
                                         frame.m_pixels + ( static_cast< usize >( area.top ) * frame.m_width + static_cast< usize >( area.left ) ) * 4,
                                         static_cast< usize >( frame.m_width ) * 4 );
        } };

        g_html.visit_frame( command.m_icon_slot, upload );

        // nothing to show before the first paint lands
        if ( !entry.m_texture )
        {
            return true;
        }

        // every quad flushes on its own, so the batch never spans two panels
        this->m_html_bound = &entry.m_texture;

        if ( !this->begin_batch( batch_type::html ) )
        {
            return false;
        }

        const auto place{ [ & ]( i32 value, i32 origin )
        {
            return transform_coordinate( static_cast< float >( value ) - 0.5f, origin, command.m_transform_scale );
        } };

        const float left{ place( command.m_bounds.left, command.m_transform_origin.x ) };
        const float top{ place( command.m_bounds.top, command.m_transform_origin.y ) };
        const float right{ place( command.m_bounds.right, command.m_transform_origin.x ) };
        const float bottom{ place( command.m_bounds.bottom, command.m_transform_origin.y ) };
        const u32   color{ command.m_color.d3d( ) };

        return push_vertex( this->m_vertices, left, top, color, 0.0f, 0.0f ) &&
               push_vertex( this->m_vertices, right, top, color, 1.0f, 0.0f ) &&
               push_vertex( this->m_vertices, left, bottom, color, 0.0f, 1.0f ) &&
               push_vertex( this->m_vertices, left, bottom, color, 0.0f, 1.0f ) &&
               push_vertex( this->m_vertices, right, top, color, 1.0f, 0.0f ) &&
               push_vertex( this->m_vertices, right, bottom, color, 1.0f, 1.0f ) && this->flush_batch( );
    }

    void backend::release_html_textures( )
    {
        for ( html_texture_t &entry : this->m_html_textures )
        {
            entry.m_texture.Release( );
            entry = html_texture_t{ };
        }

        this->m_html_bound = nullptr;
    }

    bool backend::text_atlas_matches( const text_atlas &atlas, const text_font_t &font ) const
    {
        return atlas.m_ready && atlas.m_texture &&
               atlas.m_font_height == font.m_height &&
               atlas.m_font_weight == font.m_weight &&
               atlas.m_crisp == font.m_crisp &&
               str::equals( atlas.m_face.c_str( ), font.m_face.c_str( ) );
    }

    text_atlas *backend::find_text_atlas( const text_font_t &font )
    {
        for ( text_atlas &atlas : this->m_text_atlases )
        {
            if ( this->text_atlas_matches( atlas, font ) )
            {
                return &atlas;
            }
        }

        return nullptr;
    }

    text_atlas *backend::select_text_atlas( )
    {
        for ( usize index{ }; index < this->m_text_atlases.size( ); ++index )
        {
            if ( !this->m_text_atlases[ index ].m_ready &&
                 !this->m_text_atlases[ index ].m_texture )
            {
                this->m_text_atlas_cursor = ( index + 1u ) % this->m_text_atlases.size( );
                return &this->m_text_atlases[ index ];
            }
        }

        for ( usize offset{ }; offset < this->m_text_atlases.size( ); ++offset )
        {
            usize       index{ ( this->m_text_atlas_cursor + offset ) %
                               this->m_text_atlases.size( ) };
            text_atlas *atlas{ &this->m_text_atlases[ index ] };

            if ( atlas != this->m_active_text_atlas )
            {
                this->m_text_atlas_cursor = ( index + 1u ) % this->m_text_atlases.size( );
                return atlas;
            }
        }

        return nullptr;
    }

    bool backend::ensure_text_atlas( const render_command &command, text_atlas **atlas )
    {
        if ( !atlas )
        {
            return false;
        }

        *atlas = nullptr;
        text_font_t font{ };

        if ( !this->font_for( command, &font ) || !this->m_device || !this->m_gdi.GlyphReady( ) )
        {
            return false;
        }

        if ( text_atlas * cached{ this->find_text_atlas( font ) } )
        {
            *atlas = cached;
            return true;
        }

        text_atlas *selected{ this->select_text_atlas( ) };

        if ( !selected || !this->build_text_atlas( *selected, font ) )
        {
            return false;
        }

        *atlas = selected;
        return true;
    }

    bool backend::build_text_atlas( text_atlas &atlas, const text_font_t &font )
    {
        if ( font.m_face.empty( ) || !this->m_device || !this->m_gdi.GlyphReady( ) )
        {
            return false;
        }

        this->release_text_atlas( atlas );

        const MAT2 identity{
            FIXED{ 0, 1 },
            FIXED{ 0, 0 },
            FIXED{ 0, 0 },
            FIXED{ 0, 1 },
        };

        gdi::bitmap_t measure{ };

        if ( !this->m_gdi.CreateBitmap( 16, 16, &measure ) )
        {
            return false;
        }

        HDC measure_context{ measure.m_context };

        const auto quality{ font.m_crisp ? NONANTIALIASED_QUALITY : PROOF_QUALITY };
        const auto outline_format{ font.m_crisp ? GGO_BITMAP : GGO_GRAY8_BITMAP };

        HGDIOBJ gdi_font{ this->m_gdi.CreateFontA( font.m_height, font.m_weight, font.m_face.c_str( ), quality ) };

        if ( !gdi_font )
        {
            this->m_gdi.DeleteBitmap( &measure );
            return false;
        }

        HGDIOBJ previous_font{ this->m_gdi.SelectObject( measure_context, gdi_font ) };

        if ( !previous_font )
        {
            this->m_gdi.DeleteObject( gdi_font );
            this->m_gdi.DeleteBitmap( &measure );
            return false;
        }

        atlas.m_glyphs.reset( );
        atl::array< i32, 256 > glyph_origins_y{ };
        u32                    max_width{ 1 };
        u32                    max_height{ 1 };
        u32                    ascent{ };
        u32                    descent{ };
        bool                   glyph_measurement_failed{ };

        // @ GetGlyphOutline is TrueType-only..
        GLYPHMETRICS probe{ };
        const bool   raster{ this->m_gdi.GetGlyphOutlineA( measure_context, 'M', outline_format,
                                                           &probe, 0, nullptr, &identity ) == 0xFFFFFFFFu };

        for ( u32 code{ TEXT_ATLAS_FIRST_CHAR }; code <= TEXT_ATLAS_LAST_CHAR; ++code )
        {
            if ( raster )
            {
                const char text[ 2 ]{ static_cast< char >( code ), 0 };
                SIZE       extent{ };

                if ( !this->m_gdi.GetTextExtentPoint32A( measure_context, text, 1, &extent ) ||
                     extent.cx <= 0 || extent.cy <= 0 )
                {
                    glyph_measurement_failed = true;
                    break;
                }

                text_glyph &raster_glyph{ atlas.m_glyphs[ code ] };
                raster_glyph.m_width         = static_cast< float >( extent.cx );
                raster_glyph.m_height        = static_cast< float >( extent.cy );
                raster_glyph.m_advance       = static_cast< float >( extent.cx );
                raster_glyph.m_bitmap_width  = static_cast< u32 >( extent.cx );
                raster_glyph.m_bitmap_height = static_cast< u32 >( extent.cy );
                raster_glyph.m_visible       = true;

                max_width  = math::bigger( max_width, raster_glyph.m_bitmap_width );
                max_height = math::bigger( max_height, raster_glyph.m_bitmap_height );
                continue;
            }

            GLYPHMETRICS metrics{ };
            u32          bitmap_size{ this->m_gdi.GetGlyphOutlineA( measure_context, code, outline_format, &metrics, 0,
                                                                    nullptr, &identity ) };

            if ( bitmap_size == 0xFFFFFFFFu )
            {
                glyph_measurement_failed = true;
                break;
            }

            i32 advance{ static_cast< i32 >( metrics.gmCellIncX ) };

            if ( advance <= 0 )
            {
                advance = static_cast< i32 >( metrics.gmBlackBoxX );
            }

            text_glyph &glyph{ atlas.m_glyphs[ code ] };
            glyph.m_width           = static_cast< float >( metrics.gmBlackBoxX );
            glyph.m_height          = static_cast< float >( metrics.gmBlackBoxY );
            glyph.m_offset_x        = static_cast< float >( metrics.gmptGlyphOrigin.x );
            glyph.m_advance         = static_cast< float >( advance );
            glyph.m_bitmap_width    = metrics.gmBlackBoxX;
            glyph.m_bitmap_height   = metrics.gmBlackBoxY;
            glyph.m_visible         = metrics.gmBlackBoxX && metrics.gmBlackBoxY && bitmap_size;
            glyph_origins_y[ code ] = metrics.gmptGlyphOrigin.y;

            if ( !glyph.m_visible )
            {
                continue;
            }

            max_width  = math::bigger( max_width, glyph.m_bitmap_width );
            max_height = math::bigger( max_height, glyph.m_bitmap_height );

            if ( metrics.gmptGlyphOrigin.y > 0 )
            {
                ascent = math::bigger( ascent, static_cast< u32 >( metrics.gmptGlyphOrigin.y ) );
            }

            i32 glyph_descent{ static_cast< i32 >( metrics.gmBlackBoxY ) - metrics.gmptGlyphOrigin.y };

            if ( glyph_descent > 0 )
            {
                descent = math::bigger( descent, static_cast< u32 >( glyph_descent ) );
            }
        }

        if ( glyph_measurement_failed )
        {
            this->m_gdi.SelectObject( measure_context, previous_font );
            this->m_gdi.DeleteBitmap( &measure );
            this->m_gdi.DeleteObject( gdi_font );
            return false;
        }

        u32 fallback_height{ font.m_height < 0 ? static_cast< u32 >( -font.m_height ) : static_cast< u32 >( font.m_height ) };

        if ( !raster && !ascent && !descent )
        {
            ascent = fallback_height;
        }

        u32 line_height{ raster ? max_height : math::bigger( ascent + descent, fallback_height ) };

        if ( !raster )
        {
            for ( u32 code{ TEXT_ATLAS_FIRST_CHAR }; code <= TEXT_ATLAS_LAST_CHAR; ++code )
            {
                text_glyph &glyph{ atlas.m_glyphs[ code ] };

                if ( glyph.m_advance > 0.0f )
                {
                    glyph.m_offset_y = static_cast< float >( static_cast< i32 >( ascent ) - glyph_origins_y[ code ] );
                }
            }
        }

        u32 glyph_count{ static_cast< u32 >( TEXT_ATLAS_LAST_CHAR - TEXT_ATLAS_FIRST_CHAR + 1 ) };
        u32 rows{ ( glyph_count + TEXT_ATLAS_COLUMNS - 1 ) / TEXT_ATLAS_COLUMNS };
        u32 cell_width{ max_width + TEXT_ATLAS_PADDING * 2 };
        u32 cell_height{ max_height + TEXT_ATLAS_PADDING * 2 };
        u32 atlas_width{ next_power_of_two( cell_width * TEXT_ATLAS_COLUMNS ) };
        u32 atlas_height{ next_power_of_two( cell_height * rows ) };

        atl::vector< u32 > pixels{ };
        const usize        pitch{ static_cast< usize >( atlas_width ) * sizeof( u32 ) };

        if ( !pixels.resize( static_cast< usize >( atlas_width ) * atlas_height ) )
        {
            this->m_gdi.SelectObject( measure_context, previous_font );
            this->m_gdi.DeleteBitmap( &measure );
            this->m_gdi.DeleteObject( gdi_font );
            return false;
        }

        atl::vector< u8 > glyph_buffer{ };
        bool              allocation_failed{ };
        bool              glyph_raster_failed{ };

        gdi::bitmap_t raster_bitmap{ };

        if ( raster && !this->build_raster_glyphs( atlas, gdi_font, atlas_width, atlas_height,
                                                   cell_width, cell_height, raster_bitmap ) )
        {
            glyph_raster_failed = true;
        }

        for ( u32 code{ TEXT_ATLAS_FIRST_CHAR }; !glyph_raster_failed && code <= TEXT_ATLAS_LAST_CHAR; ++code )
        {
            text_glyph &glyph{ atlas.m_glyphs[ code ] };

            if ( glyph.m_advance <= 0.0f )
            {
                continue;
            }

            u32 glyph_index{ code - TEXT_ATLAS_FIRST_CHAR };
            u32 column{ glyph_index % TEXT_ATLAS_COLUMNS };
            u32 row{ glyph_index / TEXT_ATLAS_COLUMNS };
            u32 x{ column * cell_width + TEXT_ATLAS_PADDING };
            u32 y{ row * cell_height + TEXT_ATLAS_PADDING };

            glyph.m_u0 = static_cast< float >( x ) / static_cast< float >( atlas_width );
            glyph.m_v0 = static_cast< float >( y ) / static_cast< float >( atlas_height );
            glyph.m_u1 = static_cast< float >( x + glyph.m_bitmap_width ) /
                         static_cast< float >( atlas_width );
            glyph.m_v1 = static_cast< float >( y + glyph.m_bitmap_height ) /
                         static_cast< float >( atlas_height );

            if ( glyph.m_visible && raster )
            {
                for ( u32 glyph_y{ }; glyph_y < glyph.m_bitmap_height; ++glyph_y )
                {
                    const auto *source_row{ reinterpret_cast< const u8 * >( raster_bitmap.m_pixels ) +
                                            static_cast< usize >( y + glyph_y ) * pitch };
                    auto       *destination_row{ reinterpret_cast< u8 * >( pixels.data( ) ) +
                                                 static_cast< usize >( y + glyph_y ) * pitch };

                    for ( u32 glyph_x{ }; glyph_x < glyph.m_bitmap_width; ++glyph_x )
                    {
                        // the dib is X8R8G8B8..
                        const usize offset{ static_cast< usize >( x + glyph_x ) * sizeof( u32 ) };
                        const bool  covered{ ( *reinterpret_cast< const u32 * >( source_row + offset ) &
                                               0x00FFFFFFu ) != 0 };

                        *reinterpret_cast< u32 * >( destination_row + offset ) = covered ? 0xFFFFFFFFu : 0u;
                    }
                }
            } else if ( glyph.m_visible )
            {
                GLYPHMETRICS metrics{ };
                u32          bitmap_size{ this->m_gdi.GetGlyphOutlineA( measure_context, code, outline_format, &metrics,
                                                                        0, nullptr, &identity ) };

                if ( bitmap_size == 0xFFFFFFFFu || !bitmap_size )
                {
                    glyph_raster_failed = true;
                    break;
                }

                if ( !glyph_buffer.resize( bitmap_size ) )
                {
                    allocation_failed = true;
                    break;
                }

                bitmap_size = this->m_gdi.GetGlyphOutlineA( measure_context, code, outline_format, &metrics,
                                                            bitmap_size, glyph_buffer.data( ), &identity );

                if ( bitmap_size == 0xFFFFFFFFu || !bitmap_size ||
                     static_cast< usize >( bitmap_size ) > glyph_buffer.size( ) ||
                     metrics.gmBlackBoxX != glyph.m_bitmap_width ||
                     metrics.gmBlackBoxY != glyph.m_bitmap_height )
                {
                    glyph_raster_failed = true;
                    break;
                }

                u32 source_pitch{ font.m_crisp ? glyph_mono_pitch( metrics.gmBlackBoxX )
                                               : glyph_bitmap_pitch( metrics.gmBlackBoxX ) };
                u64 required_bitmap_size{ static_cast< u64 >( source_pitch ) *
                                          static_cast< u64 >( metrics.gmBlackBoxY ) };

                if ( required_bitmap_size > static_cast< u64 >( glyph_buffer.size( ) ) )
                {
                    glyph_raster_failed = true;
                    break;
                }

                for ( u32 glyph_y{ }; glyph_y < glyph.m_bitmap_height; ++glyph_y )
                {
                    const u8 *source_row{ glyph_buffer.data( ) + static_cast< usize >( glyph_y ) * static_cast< usize >( source_pitch ) };
                    auto     *destination_row{ reinterpret_cast< u8 * >( pixels.data( ) ) + static_cast< usize >( y + glyph_y ) * pitch };

                    for ( u32 glyph_x{ }; glyph_x < glyph.m_bitmap_width; ++glyph_x )
                    {
                        u8  alpha{ font.m_crisp
                                       ? ( ( source_row[ glyph_x >> 3u ] >> ( 7u - ( glyph_x & 7u ) ) ) & 1u
                                               ? static_cast< u8 >( 255 )
                                               : static_cast< u8 >( 0 ) )
                                       : glyph_coverage_alpha( source_row[ glyph_x ] ) };
                        u32 output{ alpha ? ( static_cast< u32 >( alpha ) << 24 ) | 0x00FFFFFFu : 0 };

                        *reinterpret_cast< u32 * >( destination_row + static_cast< usize >( x + glyph_x ) * sizeof( u32 ) ) = output;
                    }
                }
            }
        }

        this->m_gdi.DeleteBitmap( &raster_bitmap );
        this->m_gdi.SelectObject( measure_context, previous_font );
        this->m_gdi.DeleteBitmap( &measure );
        this->m_gdi.DeleteObject( gdi_font );

        if ( allocation_failed || glyph_raster_failed )
        {
            return false;
        }

        texture_t    atlas_texture{ };
        const rect_t area{ 0, 0, static_cast< i32 >( atlas_width ), static_cast< i32 >( atlas_height ) };

        if ( !this->create_texture( atlas_width, atlas_height, 1, atlas_texture ) )
        {
            return false;
        }

        if ( !this->upload_texture( atlas_texture, 0, area, reinterpret_cast< const u8 * >( pixels.data( ) ), pitch ) )
        {
            atlas_texture.Release( );
            return false;
        }

        if ( !atlas.m_face.assign( font.m_face ) )
        {
            atlas_texture.Release( );
            return false;
        }

        atlas.m_texture = atlas_texture;
        atlas_texture.Detach( );
        atlas.m_font_height = font.m_height;
        atlas.m_font_weight = font.m_weight;
        atlas.m_crisp       = font.m_crisp;
        atlas.m_width       = atlas_width;
        atlas.m_height      = atlas_height;
        atlas.m_line_height = line_height;
        atlas.m_ready       = true;
        return true;
    }

    bool backend::build_raster_glyphs( const text_atlas &atlas, HGDIOBJ font, u32 atlas_width,
                                       u32 atlas_height, u32 cell_width, u32 cell_height,
                                       gdi::bitmap_t &bitmap ) const
    {
        if ( !this->m_gdi.CreateBitmap( atlas_width, atlas_height, &bitmap ) )
        {
            return false;
        }

        HDC     context{ bitmap.m_context };
        HGDIOBJ previous{ this->m_gdi.SelectObject( context, font ) };
        bool    drawn{ previous != nullptr && this->m_gdi.Fill( context,
                                                                rect_t{ 0, 0, static_cast< i32 >( atlas_width ), static_cast< i32 >( atlas_height ) },
                                                                color_t{ 0, 0, 0, 255 } ) };

        for ( u32 code{ TEXT_ATLAS_FIRST_CHAR }; drawn && code <= TEXT_ATLAS_LAST_CHAR; ++code )
        {
            if ( !atlas.m_glyphs[ code ].m_visible )
            {
                continue;
            }

            const u32  glyph_index{ code - TEXT_ATLAS_FIRST_CHAR };
            const char text[ 2 ]{ static_cast< char >( code ), 0 };

            this->m_gdi.TextOutA(
                context,
                point_t{ static_cast< i32 >( ( glyph_index % TEXT_ATLAS_COLUMNS ) * cell_width + TEXT_ATLAS_PADDING ),
                         static_cast< i32 >( ( glyph_index / TEXT_ATLAS_COLUMNS ) * cell_height + TEXT_ATLAS_PADDING ) },
                color_t{ 255, 255, 255, 255 },
                text );
        }

        if ( previous )
        {
            this->m_gdi.SelectObject( context, previous );
        }

        // the dib is read straight from memory, so the queued gdi drawing has to land first
        I( GdiFlush )( );
        return drawn;
    }

    void backend::release_text_atlas( text_atlas &atlas )
    {
        atlas.m_texture.Release( );
        atlas.m_glyphs.reset( );
        atlas.m_face.clear( );
        atlas.m_font_height = 0;
        atlas.m_font_weight = 0;
        atlas.m_width       = 0;
        atlas.m_height      = 0;
        atlas.m_line_height = 0;
        atlas.m_ready       = false;
    }

    void backend::release_icon_atlases( )
    {
        for ( icon_atlas_t &atlas : this->m_icon_atlases )
        {
            atlas.m_texture.Release( );
            atlas.m_ready = false;
        }
    }

    bool backend::build_icon_atlas( icon_atlas_t &atlas, const u8 *encrypted, u32 size,
                                    u64 seed, u32 slot_size )
    {
        if ( !this->m_device || !encrypted || !size || !slot_size )
        {
            return false;
        }

        const u32 slot_count{ size / ( slot_size * slot_size ) };

        if ( !slot_count )
        {
            return false;
        }

        const u32 atlas_width{ slot_size * slot_count };
        const u32 atlas_height{ slot_size };

        atl::vector< u32 > pixels{ };

        if ( !pixels.resize( static_cast< usize >( atlas_width ) * atlas_height ) )
        {
            return false;
        }

        u64 state{ seed ^ ( static_cast< u64 >( size ) * 0x9E3779B97F4A7C15ULL ) };
        u64 block{ };
        u32 taken{ 8u };

        for ( u32 slot{ }; slot < slot_count; ++slot )
        {
            for ( u32 y{ }; y < slot_size; ++y )
            {
                u32 *destination{ pixels.data( ) + static_cast< usize >( y ) * atlas_width };

                for ( u32 x{ }; x < slot_size; ++x )
                {
                    if ( taken == 8u )
                    {
                        state += 0x9E3779B97F4A7C15ULL;
                        u64 z{ state };
                        z     = ( z ^ ( z >> 30u ) ) * 0xBF58476D1CE4E5B9ULL;
                        z     = ( z ^ ( z >> 27u ) ) * 0x94D049BB133111EBULL;
                        block = z ^ ( z >> 31u );
                        taken = 0u;
                    }

                    const u32 source_index{ slot * ( slot_size * slot_size ) + y * slot_size + x };
                    const u8  key{ static_cast< u8 >( block >> ( taken * 8u ) ) };
                    const u8  coverage{ static_cast< u8 >( encrypted[ source_index ] ^ key ) };
                    ++taken;

                    destination[ slot * slot_size + x ] = coverage
                                                              ? ( static_cast< u32 >( coverage )
                                                                  << 24 ) |
                                                                    0x00FFFFFFu
                                                              : 0u;
                }
            }
        }

        texture_t    texture{ };
        const rect_t area{ 0, 0, static_cast< i32 >( atlas_width ), static_cast< i32 >( atlas_height ) };

        if ( !this->create_texture( atlas_width, atlas_height, 1, texture ) )
        {
            return false;
        }

        if ( !this->upload_texture( texture, 0, area, reinterpret_cast< const u8 * >( pixels.data( ) ),
                                    static_cast< usize >( atlas_width ) * 4 ) )
        {
            texture.Release( );
            return false;
        }

        atlas.m_texture.Release( );
        atlas.m_texture = texture;
        texture.Detach( );
        atlas.m_slot_size  = slot_size;
        atlas.m_slot_count = slot_count;
        atlas.m_ready      = true;
        return true;
    }

    bool backend::build_icon_atlases( )
    {
        return this->build_icon_atlas( this->m_icon_atlases[ icons::KIND_SMALL ],
                                       icons::SMALL_DATA, icons::SMALL_DATA_SIZE,
                                       icons::SMALL_DATA_SEED, icons::SIZE_SMALL ) &&
               this->build_icon_atlas( this->m_icon_atlases[ icons::KIND_LARGE ],
                                       icons::LARGE_DATA, icons::LARGE_DATA_SIZE,
                                       icons::LARGE_DATA_SEED, icons::SIZE_LARGE ) &&
               this->build_icon_atlas( this->m_icon_atlases[ icons::KIND_LOGO ],
                                       icons::LOGO_DATA, icons::LOGO_DATA_SIZE,
                                       icons::LOGO_DATA_SEED, icons::SIZE_LOGO ) &&
               this->build_icon_atlas( this->m_icon_atlases[ icons::KIND_VACC ],
                                       icons::VACC_DATA, icons::VACC_DATA_SIZE,
                                       icons::VACC_DATA_SEED, icons::SIZE_VACC );
    }

    void backend::release_text_atlases( )
    {
        for ( text_atlas &atlas : this->m_text_atlases )
        {
            this->release_text_atlas( atlas );
        }

        this->m_active_text_atlas = nullptr;
        this->m_text_atlas_cursor = 0;
    }

    bool backend::font_for( const render_command &command, text_font_t *font ) const
    {
        if ( !font )
        {
            return false;
        }

        if ( const font_resource_t *resource{ this->m_fonts.font( command.m_font.c_str( ) ) } )
        {
            if ( !font->m_face.assign( resource->m_face ) )
            {
                return false;
            }

            font->m_height = -static_cast< i32 >( resource->m_pixel_size );
            font->m_weight = resource->m_weight;
            font->m_crisp  = resource->m_crisp;
            return true;
        }

        const char *default_name{ ATLAS_XORSTR_STATIC( "Default" ) };

        if ( command.m_font.empty( ) || str::equals( command.m_font.c_str( ), default_name ) )
        {
            const char *default_face{ ATLAS_XORSTR_STATIC( "Arial" ) };

            if ( !font->m_face.assign( default_face ) )
            {
                return false;
            }

            font->m_height = -static_cast< i32 >( DEFAULT_FONT_PIXEL_SIZE );
            font->m_weight = FW_NORMAL;
            return true;
        }

        if ( !font->m_face.assign( command.m_font ) )
        {
            return false;
        }

        font->m_height = -static_cast< i32 >( DEFAULT_FONT_PIXEL_SIZE );
        font->m_weight = FW_NORMAL;
        return true;
    }

    // -------------------------------------------------------------------------
    // Renderer lifetime.
    // -------------------------------------------------------------------------

    bool backend::setup_fonts( )
    {
        return this->m_fonts.register_system( ATLAS_XORSTR_STATIC( "Atlas Compact" ), ATLAS_XORSTR_STATIC( "Tahoma" ), 11u, FW_NORMAL, true ) &&
               this->m_fonts.register_system( ATLAS_XORSTR_STATIC( "Atlas Compact Bd" ), ATLAS_XORSTR_STATIC( "Tahoma" ), 11u, FW_BOLD, true ) &&
               this->m_fonts.register_system( ATLAS_XORSTR_STATIC( "Atlas Compact Sm" ), ATLAS_XORSTR_STATIC( "Tahoma" ), 10u, FW_NORMAL, true ) &&
               this->m_fonts.register_system( ATLAS_XORSTR_STATIC( "ESP Small" ), ATLAS_XORSTR_STATIC( "Small Fonts" ), 7u, FW_NORMAL ) &&
               this->m_fonts.register_system( ATLAS_XORSTR_STATIC( "ESP Name" ), ATLAS_XORSTR_STATIC( "Tahoma" ), 11u, FW_NORMAL, true ) &&
               this->m_fonts.register_system( ATLAS_XORSTR_STATIC( "Debug Overlay" ), ATLAS_XORSTR_STATIC( "Consolas" ), 14u, FW_NORMAL, true ) &&
               this->m_fonts.register_system( ATLAS_XORSTR_STATIC( "Atlas Mono" ), ATLAS_XORSTR_STATIC( "Consolas" ), 12u, FW_NORMAL );
    }

    bool backend::init( )
    {
        if ( this->m_initialized )
        {
            return true;
        }

        if ( !this->m_gdi.Ready( ) || !this->m_fonts.init( &this->m_gdi ) ||
             !this->setup_fonts( ) )
        {
            this->shutdown( );
            return false;
        }

        this->m_initialized = true;
        return true;
    }

    // -------------------------------------------------------------------------
    // Frame controller.
    // -------------------------------------------------------------------------

    void backend::end_frame( )
    {
        if ( !this->m_inside_frame )
        {
            return;
        }

        this->m_inside_frame         = false;
        this->m_last_frame_succeeded = !this->m_frame_build_failed && this->render_frame( );
        this->m_frame_commands.clear( );
        this->m_frame_build_failed = false;
    }
} // namespace render
