#include "math/math.hpp"
#include "render/render.hpp"

#include "gui.hpp"
#include "particles.hpp"
#include "theme.hpp"

namespace gui
{
    constexpr usize PARTICLE_COUNT{ 90 };
    constexpr float LINK_DISTANCE{ 150.0f };
    constexpr usize LINK_LIMIT{ 4 }; // per particle, so a dense clump stays legible

    constexpr float SNOW_FALL{ 28.0f };  // units per second
    constexpr float SNOW_DRIFT{ 14.0f };
    constexpr float NETWORK_SPEED{ 20.0f };
    constexpr float EMBER_RISE{ 24.0f };
    constexpr float EMBER_DRIFT{ 8.0f };
    constexpr float BAT_SPEED{ 70.0f };
    constexpr usize BAT_COUNT{ 6 };
    constexpr float SIDES[ 2 ]{ -1.0f, 1.0f };

    struct particle_t
    {
        float m_x{ };
        float m_y{ };
        float m_velocity_x{ };
        float m_velocity_y{ };
        float m_size{ };
    };

    struct field_t
    {
        particle_t m_particles[ PARTICLE_COUNT ]{ };
        rect_t     m_screen{ };
        usize      m_kind{ MENU_PARTICLES_OFF };
        u64        m_tick{ };
        u32        m_seed{ 0x9E3779B9u };
    };

    ATLAS_NO_DESTROY static field_t g_field{ };

    static u32 next_random( )
    {
        u32 value{ g_field.m_seed };

        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;

        g_field.m_seed = value;
        return value;
    }

    static float random_range( float minimum, float maximum )
    {
        const float unit{ static_cast< float >( next_random( ) & 0xFFFFFFu ) /
                          static_cast< float >( 0xFFFFFF ) };

        return minimum + ( maximum - minimum ) * unit;
    }

    static void seed_field( rect_t screen, usize kind )
    {
        const float width{ static_cast< float >( screen.right - screen.left ) };
        const float height{ static_cast< float >( screen.bottom - screen.top ) };

        for ( particle_t &particle : g_field.m_particles )
        {
            particle.m_x    = random_range( 0.0f, width );
            particle.m_y    = random_range( 0.0f, height );
            particle.m_size = random_range( 1.0f, 2.6f );

            if ( kind == MENU_PARTICLES_SNOW )
            {
                particle.m_velocity_x = random_range( -SNOW_DRIFT, SNOW_DRIFT );
                particle.m_velocity_y = random_range( SNOW_FALL * 0.4f, SNOW_FALL );
            } else if ( kind == MENU_PARTICLES_HALLOWEEN )
            {
                particle.m_velocity_x = random_range( -EMBER_DRIFT, EMBER_DRIFT );
                particle.m_velocity_y = -random_range( EMBER_RISE * 0.4f, EMBER_RISE );
            } else
            {
                particle.m_velocity_x = random_range( -NETWORK_SPEED, NETWORK_SPEED );
                particle.m_velocity_y = random_range( -NETWORK_SPEED, NETWORK_SPEED );
            }
        }

        if ( kind == MENU_PARTICLES_HALLOWEEN )
        {
            for ( usize index{ }; index < BAT_COUNT; ++index )
            {
                particle_t &bat{ g_field.m_particles[ index ] };
                const float speed{ random_range( BAT_SPEED * 0.6f, BAT_SPEED ) };

                bat.m_velocity_x = random_range( -1.0f, 1.0f ) < 0.0f ? -speed : speed;
                bat.m_velocity_y = 0.0f;
                bat.m_size       = random_range( 6.0f, 10.0f );
            }
        }

        g_field.m_screen = screen;
        g_field.m_kind   = kind;
    }

    static float wrap( float value, float span )
    {
        if ( span <= 0.0f )
        {
            return value;
        }

        while ( value < 0.0f )
        {
            value += span;
        }

        while ( value >= span )
        {
            value -= span;
        }

        return value;
    }

    static void advance( float width, float height, float delta )
    {
        for ( particle_t &particle : g_field.m_particles )
        {
            particle.m_x = wrap( particle.m_x + particle.m_velocity_x * delta, width );
            particle.m_y = wrap( particle.m_y + particle.m_velocity_y * delta, height );
        }
    }

    static void draw_dot( rect_t screen, const particle_t &particle, color_t value )
    {
        const i32 x{ screen.left + static_cast< i32 >( particle.m_x ) };
        const i32 y{ screen.top + static_cast< i32 >( particle.m_y ) };
        const i32 size{ static_cast< i32 >( particle.m_size ) };

        g_render.box( rect_t{ x, y, x + size + 1, y + size + 1 }, value, true );
    }

    static void draw_links( rect_t screen, u8 opacity )
    {
        color_t line_color{ g_theme.m_accent };

        for ( usize index{ }; index < PARTICLE_COUNT; ++index )
        {
            usize drawn{ };

            for ( usize other{ index + 1 }; other < PARTICLE_COUNT && drawn < LINK_LIMIT;
                  ++other )
            {
                const float dx{ g_field.m_particles[ index ].m_x -
                                g_field.m_particles[ other ].m_x };
                const float dy{ g_field.m_particles[ index ].m_y -
                                g_field.m_particles[ other ].m_y };

                const float distance{ math::sqrt( dx * dx + dy * dy ) };

                if ( distance >= LINK_DISTANCE )
                {
                    continue;
                }

                // the closer the pair, the more solid the thread between them
                const float closeness{ 1.0f - distance / LINK_DISTANCE };

                line_color.a = static_cast< u8 >( static_cast< float >( opacity ) * closeness *
                                                  0.45f );

                if ( !line_color.a )
                {
                    continue;
                }

                g_render.line(
                    point_t{ screen.left + static_cast< i32 >( g_field.m_particles[ index ].m_x ),
                             screen.top + static_cast< i32 >( g_field.m_particles[ index ].m_y ) },
                    point_t{ screen.left + static_cast< i32 >( g_field.m_particles[ other ].m_x ),
                             screen.top + static_cast< i32 >( g_field.m_particles[ other ].m_y ) },
                    line_color );

                ++drawn;
            }
        }
    }

    static float cycle( u64 now, u32 period )
    {
        return static_cast< float >( now % period ) / static_cast< float >( period ) * PI * 2.0f;
    }

    static void draw_bat( rect_t screen, const particle_t &particle, u64 now, usize index,
                          color_t value )
    {
        const u64   offset{ now + index * 397u };
        const float span{ particle.m_size };
        const float tip{ ( math::sin( cycle( offset, 180 ) ) * 0.6f - 0.5f ) * span };
        const vec2  center{ static_cast< float >( screen.left ) + particle.m_x,
                            static_cast< float >( screen.top ) + particle.m_y +
                                math::sin( cycle( offset, 1400 ) ) * span };

        const vec2 points[ 8 ]{
            center + vec2{ 0.0f, -0.3f * span },
            center + vec2{ 0.3f * span, -0.15f * span },
            center + vec2{ span, tip },
            center + vec2{ 0.45f * span, 0.15f * span },
            center + vec2{ 0.0f, 0.35f * span },
            center + vec2{ -0.45f * span, 0.15f * span },
            center + vec2{ -span, tip },
            center + vec2{ -0.3f * span, -0.15f * span },
        };

        const color_t colors[ 8 ]{ value, value, value, value, value, value, value, value };

        g_render.polygon( points, colors, 8 );
    }

    static void draw_halloween( rect_t screen, u8 opacity )
    {
        const u64 now{ clock( ) };

        for ( usize index{ BAT_COUNT }; index < PARTICLE_COUNT; ++index )
        {
            const u8 flicker{ pulse( now + index * 211u, 900 ) };

            color_t value{ mix_color( color_t{ 214, 72, 18 }, color_t{ 255, 196, 84 }, flicker ) };
            value.a = static_cast< u8 >( static_cast< float >( opacity ) *
                                         ( 0.35f + 0.5f * static_cast< float >( flicker ) / 255.0f ) );

            draw_dot( screen, g_field.m_particles[ index ], value );
        }

        const color_t bat{ 46, 24, 60, static_cast< u8 >( static_cast< float >( opacity ) * 0.9f ) };

        for ( usize index{ }; index < BAT_COUNT; ++index )
        {
            draw_bat( screen, g_field.m_particles[ index ], now, index, bat );
        }
    }

    static void draw_cobweb( point_t corner, i32 unit, color_t silk )
    {
        constexpr usize SPOKES{ 5 };
        constexpr usize RINGS{ 4 };

        const float reach{ static_cast< float >( unit ) * 3.5f };
        const vec2  origin{ static_cast< float >( corner.x ), static_cast< float >( corner.y ) };

        vec2 directions[ SPOKES ]{ };

        for ( usize spoke{ }; spoke < SPOKES; ++spoke )
        {
            const float angle{ PI * 0.5f * static_cast< float >( spoke ) /
                               static_cast< float >( SPOKES - 1 ) };

            directions[ spoke ] = vec2{ math::cos( angle ), -math::sin( angle ) };
            g_render.line( origin, origin + directions[ spoke ] * reach, silk );
        }

        for ( usize ring{ 1 }; ring <= RINGS; ++ring )
        {
            const float radius{ reach * 0.9f * static_cast< float >( ring ) /
                                static_cast< float >( RINGS ) };

            for ( usize spoke{ }; spoke + 1 < SPOKES; ++spoke )
            {
                const vec2 from{ origin + directions[ spoke ] * radius };
                const vec2 to{ origin + directions[ spoke + 1 ] * radius };

                const vec2 sag{ origin + ( directions[ spoke ] + directions[ spoke + 1 ] ) *
                                             ( radius * 0.42f ) };

                g_render.line( from, sag, silk );
                g_render.line( sag, to, silk );
            }
        }
    }

    static void draw_pumpkin( point_t base, i32 unit, u64 now, u8 opacity )
    {
        const i32    width{ unit * 2 };
        const i32    height{ unit * 3 / 2 };
        const i32    stem{ math::bigger( unit / 6, 2 ) };
        const rect_t body{ base.x - width / 2, base.y - height, base.x + width / 2, base.y };

        g_render.box( rect_t{ base.x - stem / 2, body.top - unit / 3, base.x + stem - stem / 2,
                              body.top + 2 },
                      with_alpha( color_t{ 92, 112, 44 }, opacity ), true );

        fill_rounded_box( body, height / 2, with_alpha( color_t{ 232, 116, 24 }, opacity ) );

        const color_t rib{ with_alpha( color_t{ 190, 84, 14 }, opacity ) };

        for ( const float side : SIDES )
        {
            const i32 x{ base.x + static_cast< i32 >( side ) * width / 6 };

            g_render.line( point_t{ x, body.top + height / 6 },
                           point_t{ x, body.bottom - height / 6 }, rib );
        }

        color_t flame{ g_theme.m_accent };
        flame.a = mix_channel( 150, 255, pulse( now, 620 ) );
        flame   = with_alpha( flame, opacity );

        const float w{ static_cast< float >( width ) };
        const float h{ static_cast< float >( height ) };
        const float eye{ h * 0.14f };

        for ( const float side : SIDES )
        {
            const vec2 center{ static_cast< float >( base.x ) + side * w * 0.2f,
                               static_cast< float >( body.top ) + h * 0.4f };

            const vec2    points[ 3 ]{ center + vec2{ -eye, eye * 0.6f },
                                       center + vec2{ eye, eye * 0.6f },
                                       center + vec2{ 0.0f, -eye * 0.8f } };
            const color_t colors[ 3 ]{ flame, flame, flame };

            g_render.polygon( points, colors, 3 );
        }

        const vec2 mouth{ static_cast< float >( base.x ), static_cast< float >( body.top ) + h * 0.62f };

        const vec2 grin[ 8 ]{
            mouth + vec2{ -0.30f * w, 0.0f },
            mouth + vec2{ -0.10f * w, 0.06f * h },
            mouth + vec2{ 0.0f, 0.0f },
            mouth + vec2{ 0.10f * w, 0.06f * h },
            mouth + vec2{ 0.30f * w, 0.0f },
            mouth + vec2{ 0.16f * w, 0.16f * h },
            mouth + vec2{ 0.0f, 0.2f * h },
            mouth + vec2{ -0.16f * w, 0.16f * h },
        };

        const color_t grin_colors[ 8 ]{ flame, flame, flame, flame, flame, flame, flame, flame };

        g_render.polygon( grin, grin_colors, 8 );
    }

    static void draw_spider( point_t anchor, i32 unit, u64 now, color_t silk, u8 opacity )
    {
        const float size{ static_cast< float >( unit ) };
        const float radius{ size * 0.3f };
        const vec2  top{ static_cast< float >( anchor.x ), static_cast< float >( anchor.y ) };
        const vec2  body{ top + vec2{ 0.0f, size * ( 2.2f + 0.5f * math::sin( cycle( now, 3900 ) ) ) } };

        g_render.line( top, body, silk );

        const color_t shell{ with_alpha( color_t{ 30, 22, 38 }, opacity ) };

        for ( const float side : SIDES )
        {
            for ( usize leg{ }; leg < 4; ++leg )
            {
                const float twitch{ math::sin( cycle( now + leg * 173u, 1000 ) ) * 0.08f };
                const float angle{ -0.7f + static_cast< float >( leg ) * 0.45f + twitch };

                const vec2 knee{ body + vec2{ side * math::cos( angle ) * radius * 2.2f,
                                              math::sin( angle ) * radius * 2.2f - radius * 0.9f } };
                const vec2 foot{ knee + vec2{ side * radius * 0.9f, radius * 1.6f } };

                g_render.line( body, knee, shell );
                g_render.line( knee, foot, shell );
            }
        }

        const point_t center{ static_cast< i32 >( body.x ), static_cast< i32 >( body.y ) };
        const i32     abdomen{ static_cast< i32 >( radius ) };
        const i32     head{ math::bigger( abdomen * 3 / 5, 2 ) };
        const i32     head_y{ center.y - abdomen - head / 2 };

        fill_rounded_box( rect_t{ center.x - abdomen, center.y - abdomen, center.x + abdomen,
                                  center.y + abdomen },
                          abdomen, shell );
        fill_rounded_box( rect_t{ center.x - head, head_y - head, center.x + head, head_y + head },
                          head, shell );

        const color_t eyes{ with_alpha( color_t{ 230, 40, 40 }, opacity ) };

        g_render.box( rect_t{ center.x - 2, head_y, center.x - 1, head_y + 1 }, eyes, true );
        g_render.box( rect_t{ center.x + 1, head_y, center.x + 2, head_y + 1 }, eyes, true );
    }

    void draw_menu_particles( rect_t screen, u8 opacity )
    {
        const usize kind{ g_theme.m_particles };

        if ( kind == MENU_PARTICLES_OFF || kind >= MENU_PARTICLES_COUNT || !opacity )
        {
            g_field.m_kind = MENU_PARTICLES_OFF;
            return;
        }

        const float width{ static_cast< float >( screen.right - screen.left ) };
        const float height{ static_cast< float >( screen.bottom - screen.top ) };

        if ( width <= 0.0f || height <= 0.0f )
        {
            return;
        }

        // a resolution change or a different kind starts the field over rather than carrying
        // positions that no longer belong to the screen
        if ( g_field.m_kind != kind || g_field.m_screen.right != screen.right ||
             g_field.m_screen.bottom != screen.bottom )
        {
            seed_field( screen, kind );
        }

        advance( width, height,
                 static_cast< float >( animation_delta( g_field.m_tick ) ) / 1000.0f );

        if ( kind == MENU_PARTICLES_HALLOWEEN )
        {
            draw_halloween( screen, opacity );
            return;
        }

        if ( kind == MENU_PARTICLES_NETWORK )
        {
            draw_links( screen, opacity );
        }

        const color_t dot_color{ kind == MENU_PARTICLES_NETWORK ? g_theme.m_accent
                                                                : color_t{ 236, 238, 245 } };

        for ( const particle_t &particle : g_field.m_particles )
        {
            color_t value{ dot_color };
            value.a = static_cast< u8 >( static_cast< float >( opacity ) * 0.7f );

            draw_dot( screen, particle, value );
        }
    }

    void draw_menu_decorations( rect_t bounds, rect_t subnav, i32 unit, u8 opacity )
    {
        if ( g_theme.m_particles != MENU_PARTICLES_HALLOWEEN || !opacity )
        {
            return;
        }

        const u64     now{ clock( ) };
        const color_t silk{ with_alpha( color_t{ 225, 225, 235, 110 }, opacity ) };

        draw_cobweb( point_t{ subnav.left, subnav.bottom - 1 }, unit, silk );
        draw_pumpkin( point_t{ bounds.right - unit * 3, bounds.top }, unit, now, opacity );
        draw_spider( point_t{ bounds.left + unit * 3, bounds.bottom }, unit, now, silk, opacity );
    }
} // namespace gui
