#include "base.hpp"
#include "gui/gui.hpp"
#include "gui/theme.hpp"
#include "steam.hpp"

#include "config.hpp"
#include "features/features.hpp"
#include "preview.hpp"

constexpr i32 PREVIEW_PANEL_WIDTH{ MENU_COLUMN_WIDTH };

constexpr i32   OVERLAY_HIT_EXPAND{ 4 };
constexpr i32   OVERLAY_GLOW_ALPHA{ 90 };
constexpr u32   OVERLAY_FADE_DURATION{ 160 };
constexpr float OVERLAY_SETTLE_RESPONSE{ 110.0f };
constexpr float ROTATE_SPEED{ 0.5f };
constexpr float PHASE_WRAP{ 62.831853f };

constexpr float FLAG_ROW_STEP{ 9.0f };
constexpr usize FLAG_SAMPLE_COUNT{ 3 };

constexpr usize ELEMENT_HEALTH{ static_cast< usize >( config::esp_element_t::health ) };
constexpr usize ENTITY_ELEMENT_MAX{ 5 };

static u64 s_preview_revision{ };

struct sample_t
{
    const char *m_label;
    color_t     m_color;
};

struct class_scene_t
{
    const char *m_model;
    const char *m_weapon;
    const char *m_stance;
    const char *m_name;
    const char *m_weapon_name;
    i32         m_team;
};

struct entity_scene_t
{
    const char *m_model;
    const char *m_stance;
    const char *m_name;
    i32         m_team;
};

static const class_scene_t &player_scene( usize target )
{
    static const class_scene_t SCENES[ config::ESP_TARGET_COUNT ]{
        { ATLAS_XORSTR_STATIC( "models/player/soldier.mdl" ),
          ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_rocketlauncher.mdl" ),
          ATLAS_XORSTR_STATIC( "stand_PRIMARY" ), ATLAS_XORSTR_STATIC( "Soldier" ),
          ATLAS_XORSTR_STATIC( "ROCKET LAUNCHER" ), TF_TEAM_RED },
        { ATLAS_XORSTR_STATIC( "models/player/medic.mdl" ),
          ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_medigun.mdl" ),
          ATLAS_XORSTR_STATIC( "stand_SECONDARY" ), ATLAS_XORSTR_STATIC( "Medic" ),
          ATLAS_XORSTR_STATIC( "MEDIGUN" ), TF_TEAM_BLUE },
        { ATLAS_XORSTR_STATIC( "models/player/scout.mdl" ),
          ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_scattergun.mdl" ),
          ATLAS_XORSTR_STATIC( "stand_PRIMARY" ), ATLAS_XORSTR_STATIC( "Scout" ),
          ATLAS_XORSTR_STATIC( "SCATTER GUN" ), TF_TEAM_BLUE },
        { ATLAS_XORSTR_STATIC( "models/player/scout.mdl" ),
          ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_scattergun.mdl" ),
          ATLAS_XORSTR_STATIC( "stand_PRIMARY" ), ATLAS_XORSTR_STATIC( "Friend" ),
          ATLAS_XORSTR_STATIC( "SCATTER GUN" ), TF_TEAM_BLUE },
        { ATLAS_XORSTR_STATIC( "models/player/spy.mdl" ),
          ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_revolver.mdl" ),
          ATLAS_XORSTR_STATIC( "stand_SECONDARY" ), ATLAS_XORSTR_STATIC( "Atlas user" ),
          ATLAS_XORSTR_STATIC( "REVOLVER" ), TF_TEAM_RED },
    };

    return SCENES[ target < config::ESP_TARGET_COUNT ? target : 0 ];
}

static bool showing_players( )
{
    return g_cfg.m_esp.m_target_group ==
           static_cast< usize >( config::cham_entity_t::players );
}

static usize preview_local_model( )
{
    return showing_players( ) && g_cfg.m_esp.m_edited == 2
               ? g_cfg.m_chams.m_local_model
               : config::LOCAL_MODEL_PLAYER;
}

static bool showing_buildings( )
{
    return g_cfg.m_esp.m_target_group ==
           static_cast< usize >( config::cham_entity_t::buildings );
}

static const entity_scene_t &entity_scene( )
{
    // the world scenes follow config::WORLD_* order
    static const entity_scene_t SCENES[ 1 + config::WORLD_COUNT ]{
        { ATLAS_XORSTR_STATIC( "models/buildables/sentry1.mdl" ),
          ATLAS_XORSTR_STATIC( "idle" ), ATLAS_XORSTR_STATIC( "SENTRY L1" ), TF_TEAM_RED },
        { ATLAS_XORSTR_STATIC( "models/items/medkit_large.mdl" ),
          ATLAS_XORSTR_STATIC( "idle" ), ATLAS_XORSTR_STATIC( "HEALTH" ), TF_TEAM_RED },
        { ATLAS_XORSTR_STATIC( "models/flag/briefcase.mdl" ),
          ATLAS_XORSTR_STATIC( "idle" ), ATLAS_XORSTR_STATIC( "INTEL" ), TF_TEAM_RED },
        { ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_rocket.mdl" ),
          ATLAS_XORSTR_STATIC( "idle" ), ATLAS_XORSTR_STATIC( "ROCKET" ), TF_TEAM_RED },
        { ATLAS_XORSTR_STATIC( "models/bots/skeleton_sniper/skeleton_sniper.mdl" ),
          ATLAS_XORSTR_STATIC( "stand_MELEE" ), ATLAS_XORSTR_STATIC( "SKELETON" ), TF_TEAM_RED },
    };

    return SCENES[ showing_buildings( ) ? 0 : 1 + g_cfg.m_esp.m_edited ];
}

static bool element_is_bar( usize index )
{
    return index == ELEMENT_HEALTH;
}

static bool element_draggable( usize index )
{
    const auto element{ static_cast< config::esp_element_t >( index ) };

    return element != config::esp_element_t::box &&
           element != config::esp_element_t::skeleton;
}

static point_t element_size( usize index )
{
    switch ( static_cast< config::esp_element_t >( index ) )
    {
        case config::esp_element_t::name:
            return point_t{ 44, ESP_NAME_HEIGHT };
        case config::esp_element_t::tag:
            return point_t{ 48, ESP_NAME_HEIGHT };
        case config::esp_element_t::distance:
            return point_t{ 30, ESP_SMALL_HEIGHT };
        case config::esp_element_t::weapon:
            return point_t{ 66, ESP_SMALL_HEIGHT };
        default:
            return point_t{ 36, static_cast< i32 >( FLAG_ROW_STEP ) *
                                    static_cast< i32 >( FLAG_SAMPLE_COUNT ) };
    }
}

static rect_t block_rect( usize index, point_t at, config::esp_anchor_t anchor )
{
    const point_t size{ element_size( index ) };

    switch ( esp_layout_t::align_for( anchor ) )
    {
        case render::text_align::right:
            return rect_t{ at.x - size.x, at.y, at.x, at.y + size.y };
        case render::text_align::left:
            return rect_t{ at.x, at.y, at.x + size.x, at.y + size.y };
        default:
            return rect_t{ at.x - size.x / 2, at.y, at.x + size.x / 2, at.y + size.y };
    }
}

static config::esp_anchor_t nearest_anchor( point_t position, rect_t box )
{
    const i32 outside[ 4 ]{ box.top - position.y, position.y - box.bottom,
                            box.left - position.x, position.x - box.right };

    usize best{ };

    for ( usize side{ 1 }; side < 4; ++side )
    {
        if ( outside[ side ] > outside[ best ] )
        {
            best = side;
        }
    }

    return static_cast< config::esp_anchor_t >( best );
}

static config::esp_align_t drop_align( point_t position, rect_t box, config::esp_anchor_t side )
{
    if ( side == config::esp_anchor_t::top || side == config::esp_anchor_t::bottom )
    {
        return config::esp_align_t::center;
    }

    const i32 span{ box.bottom - box.top };
    const i32 travelled{ position.y - box.top };

    if ( span < 3 || travelled < span / 3 )
    {
        return config::esp_align_t::start;
    }

    return travelled > span * 2 / 3 ? config::esp_align_t::end : config::esp_align_t::center;
}

static color_t health_tint( float fill )
{
    const float clamped{ math::clamp( fill, 0.0f, 1.0f ) };

    return color_t{ static_cast< u8 >( math::smaller( 510.0f * ( 1.0f - clamped ), 200.0f ) ),
                    static_cast< u8 >( math::smaller( 510.0f * clamped, 200.0f ) ), 32 };
}

static void draw_outlined_box( rect_t box, color_t value, u8 alpha )
{
    const color_t shadow{ gui::with_alpha( color_t{ 0, 0, 0, value.a }, alpha ) };

    g_render.box( rect_t{ box.left - 1, box.top - 1, box.right + 1, box.bottom + 1 },
                  g_gui.color( shadow ) );
    g_render.box( box, g_gui.color( gui::with_alpha( value, alpha ) ) );
    g_render.box( rect_t{ box.left + 1, box.top + 1, box.right - 1, box.bottom - 1 },
                  g_gui.color( shadow ) );
}

static void draw_bar( rect_t bar, float fill, usize style, color_t high, color_t low, i32 divisions,
                      u8 alpha )
{
    g_render.box( rect_t{ bar.left - 1, bar.top - 1, bar.right + 1, bar.bottom + 1 },
                  g_gui.color( gui::with_alpha( ESP_BAR_BACKGROUND, alpha ) ), true );

    const bool vertical{ bar.bottom - bar.top > bar.right - bar.left };

    rect_t filled{ bar };

    if ( vertical )
    {
        filled.top = bar.bottom -
                     static_cast< i32 >( static_cast< float >( bar.bottom - bar.top ) * fill );
    } else
    {
        filled.right = bar.left +
                       static_cast< i32 >( static_cast< float >( bar.right - bar.left ) *
                                           fill );
    }

    const color_t head{ style == config::HEALTH_STYLE_HEALTH ? low.blend( high, fill ) : high };
    const color_t base{ style == config::HEALTH_STYLE_GRADIENT ? low : head };

    g_render.gradient( filled, g_gui.color( gui::with_alpha( vertical ? head : base, alpha ) ),
                       g_gui.color( gui::with_alpha( vertical ? base : head, alpha ) ), vertical );

    if ( divisions < 2 )
    {
        return;
    }

    const color_t separator{ gui::with_alpha( ESP_BAR_BACKGROUND, alpha ) };
    const i32     span{ vertical ? bar.bottom - bar.top : bar.right - bar.left };

    for ( i32 index{ 1 }; index < divisions; ++index )
    {
        const i32 offset{ ( span * index + divisions / 2 ) / divisions };

        if ( vertical )
        {
            const i32 y{ bar.top + offset };
            g_render.box( rect_t{ bar.left, y, bar.right, y + 1 }, g_gui.color( separator ), true );
        } else
        {
            const i32 x{ bar.left + offset };
            g_render.box( rect_t{ x, bar.top, x + 1, bar.bottom }, g_gui.color( separator ), true );
        }
    }
}

static void draw_samples( const sample_t *samples, usize count, point_t at, float phase, u8 alpha,
                          render::text_align align )
{
    float y{ static_cast< float >( at.y ) };

    for ( usize index{ }; index < count; ++index )
    {
        const float wave{ math::sin( phase * 0.8f + static_cast< float >( index ) * 2.3f ) };
        const float pop{ math::smooth_step( math::clamp( 0.5f + wave * 1.2f, 0.0f, 1.0f ) ) };

        if ( pop > 0.02f )
        {
            const i32 slide{ static_cast< i32 >( ( 1.0f - pop ) * 4.0f + 0.5f ) };

            g_render.text( ATLAS_XORSTR( "ESP Small" ).get( ),
                           point_t{ at.x + slide, static_cast< i32 >( y + 0.5f ) },
                           g_gui.color( gui::with_alpha(
                               samples[ index ].m_color,
                               static_cast< u8 >( pop * static_cast< float >( alpha ) ) ) ),
                           samples[ index ].m_label, true, align );
        }

        y += FLAG_ROW_STEP * pop;
    }
}

class esp_preview final : public gui::baseobject
{
public:
    i32 measure( ) const override
    {
        return this->m_bounds.bottom - this->m_bounds.top;
    }

    void think( ) override
    {
        this->apply_scene( );
        if ( g_gui.open( ) )
        {
            g_esp_builder.set_viewport( this->m_bounds );
        }
        this->update_frame( );

        if ( this->m_active_group != g_cfg.m_esp.m_target_group ||
             this->m_active_target != g_cfg.m_esp.m_edited ||
             this->m_revision != s_preview_revision ||
             this->m_active_local_model != preview_local_model( ) )
        {
            this->cancel_drag( );
            this->m_revision           = s_preview_revision;
            this->m_active_group       = g_cfg.m_esp.m_target_group;
            this->m_active_target      = g_cfg.m_esp.m_edited;
            this->m_active_local_model = preview_local_model( );
        }

        if ( preview_local_model( ) != config::LOCAL_MODEL_PLAYER )
        {
            if ( preview_local_model( ) == config::LOCAL_MODEL_WEAPON )
            {
                this->rotate( );
            }
            return;
        }

        if ( showing_players( ) )
        {
            this->update_elements( );
        } else
        {
            this->update_entity_elements( );
        }

        this->rotate( );
    }

    void draw( ) const override
    {
        g_render.box( this->m_bounds, g_gui.color( CARD_BACKGROUND ), true );

        if ( this->m_frame.m_valid && this->m_frame.m_local_model == preview_local_model( ) &&
             g_render.model_capture_serial( ) == this->m_frame.m_serial )
        {
            g_render.model( this->m_bounds, g_gui.color( color_t{ 255, 255, 255 } ).a );
        }

        if ( preview_local_model( ) != config::LOCAL_MODEL_PLAYER ||
             this->m_frame.m_local_model != config::LOCAL_MODEL_PLAYER )
        {
            return;
        }

        if ( showing_players( ) )
        {
            this->draw_elements( );
        } else
        {
            this->draw_entity_elements( );
        }
    }

private:
    void cancel_drag( )
    {
        this->m_grab     = -1;
        this->m_hovered  = -1;
        this->m_pending  = -1;
        this->m_rotating = false;
        for ( usize index{ }; index < config::ESP_ELEMENT_COUNT; ++index )
        {
            this->m_offset_x[ index ] = 0.0f;
            this->m_offset_y[ index ] = 0.0f;
            this->m_glow[ index ]     = 0;
        }
    }

    void update_frame( )
    {
        esp_builder::frame_t candidate{ };
        g_esp_builder.snapshot( candidate );

        if ( this->m_frame.m_valid && !g_render.model_captured( ) )
        {
            // Device loss retires the texture, so its geometry cannot outlive it.
            this->m_frame.m_valid = false;
            this->cancel_drag( );
        }

        if ( candidate.m_epoch != this->m_epoch )
        {
            this->m_epoch         = candidate.m_epoch;
            this->m_frame.m_valid = false;
            this->cancel_drag( );
        }

        if ( candidate.m_valid && g_render.model_capture_serial( ) == candidate.m_serial )
        {
            this->m_frame = candidate;
        }
    }

    point_t model_point( point_t at ) const
    {
        const rect_t source{ this->m_frame.m_viewport };
        const i32    width{ source.right - source.left };
        const i32    height{ source.bottom - source.top };

        if ( width <= 0 || height <= 0 )
        {
            return at;
        }

        return point_t{
            this->m_bounds.left + static_cast< i32 >(
                                      static_cast< i64 >( at.x - source.left ) *
                                      ( this->m_bounds.right - this->m_bounds.left ) / width ),
            this->m_bounds.top + static_cast< i32 >(
                                     static_cast< i64 >( at.y - source.top ) *
                                     ( this->m_bounds.bottom - this->m_bounds.top ) / height )
        };
    }

    void apply_scene( ) const
    {
        if ( preview_local_model( ) == config::LOCAL_MODEL_VIEWMODEL )
        {
            g_esp_builder.set_profile(
                ATLAS_XORSTR_STATIC( "models/weapons/c_models/c_scout_arms.mdl" ),
                ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_scattergun.mdl" ),
                ATLAS_XORSTR_STATIC( "sg_idle" ), TF_TEAM_BLUE, config::LOCAL_MODEL_VIEWMODEL );
            return;
        }

        if ( preview_local_model( ) == config::LOCAL_MODEL_WEAPON )
        {
            g_esp_builder.set_profile(
                ATLAS_XORSTR_STATIC( "models/weapons/w_models/w_scattergun.mdl" ),
                nullptr, ATLAS_XORSTR_STATIC( "idle" ), TF_TEAM_BLUE, config::LOCAL_MODEL_WEAPON );
            return;
        }

        if ( !showing_players( ) )
        {
            const entity_scene_t &scene{ entity_scene( ) };

            g_esp_builder.set_profile( scene.m_model, nullptr, scene.m_stance, scene.m_team );
            return;
        }

        const class_scene_t &scene{ player_scene( g_cfg.m_esp.m_edited ) };

        g_esp_builder.set_profile( scene.m_model, scene.m_weapon, scene.m_stance,
                                   scene.m_team );
    }

    bool grab_model_box( u32 delta )
    {
        this->m_phase += static_cast< float >( delta ) / 1000.0f;

        if ( this->m_phase > PHASE_WRAP )
        {
            this->m_phase -= PHASE_WRAP;
        }

        this->m_box_valid = false;

        if ( !this->m_frame.m_valid )
        {
            return false;
        }

        const rect_t  captured{ this->m_frame.m_bounds };
        const point_t top_left{ this->model_point( point_t{ captured.left, captured.top } ) };
        const point_t bottom_right{ this->model_point( point_t{ captured.right, captured.bottom } ) };
        const rect_t  raw{ top_left.x, top_left.y, bottom_right.x, bottom_right.y };
        const i32     width{ raw.right - raw.left };

        this->m_model_box = rect_t{ raw.left + width / 8, raw.top, raw.right - width / 8,
                                    raw.bottom };
        this->m_box_valid = this->m_model_box.right > this->m_model_box.left &&
                            this->m_model_box.bottom > this->m_model_box.top;
        return this->m_box_valid;
    }

    rect_t floating_rect( usize index, point_t center ) const
    {
        if ( element_is_bar( index ) )
        {
            const config::esp_anchor_t side{ nearest_anchor( center, this->m_model_box ) };
            const bool                 vertical{ side == config::esp_anchor_t::left ||
                                                 side == config::esp_anchor_t::right };
            const i32                  half{
                ( vertical ? this->m_model_box.bottom - this->m_model_box.top
                           : this->m_model_box.right - this->m_model_box.left ) /
                2
            };

            return vertical
                       ? rect_t{ center.x - 1, center.y - half, center.x + 1, center.y + half }
                       : rect_t{ center.x - half, center.y - 1, center.x + half, center.y + 1 };
        }

        const point_t size{ element_size( index ) };

        return rect_t{ center.x - size.x / 2, center.y - size.y / 2, center.x + size.x / 2,
                       center.y + size.y / 2 };
    }

    void reorder_stack( usize grabbed, config::esp_anchor_t side, config::esp_align_t align,
                        point_t mouse )
    {
        config::esp_profile_t &profile{ g_cfg.m_esp.edited( ) };

        if ( side == config::esp_anchor_t::top || side == config::esp_anchor_t::bottom )
        {
            align = config::esp_align_t::center;
        }

        usize stack[ config::ESP_ELEMENT_COUNT ]{ };
        usize members{ };

        for ( usize position{ }; position < config::ESP_ELEMENT_COUNT; ++position )
        {
            const usize index{ profile.m_order[ position ] };

            if ( index == grabbed || element_is_bar( index ) || !element_draggable( index ) ||
                 !this->m_element_laid[ index ] )
            {
                continue;
            }

            const config::esp_anchor_t other_side{ profile.m_anchors[ index ] };
            config::esp_align_t        other_align{ profile.m_aligns[ index ] };

            if ( other_side == config::esp_anchor_t::top ||
                 other_side == config::esp_anchor_t::bottom )
            {
                other_align = config::esp_align_t::center;
            }

            if ( other_side != side || other_align != align )
            {
                continue;
            }

            stack[ members++ ] = index;
        }

        usize insert_pos{ };

        for ( usize position{ }; position < members; ++position )
        {
            const rect_t rect{ this->m_element_rects[ stack[ position ] ] };
            const i32    center_y{ ( rect.top + rect.bottom ) / 2 };

            if ( side == config::esp_anchor_t::top ? center_y > mouse.y : center_y < mouse.y )
            {
                ++insert_pos;
            }
        }

        u8    rebuilt[ config::ESP_ELEMENT_COUNT ]{ };
        usize written{ };
        usize seen{ };
        bool  placed{ };

        for ( usize position{ }; position < config::ESP_ELEMENT_COUNT; ++position )
        {
            const u8 index{ profile.m_order[ position ] };

            if ( index == grabbed )
            {
                continue;
            }

            bool member{ };

            for ( usize walk{ }; walk < members; ++walk )
            {
                member |= stack[ walk ] == index;
            }

            if ( member )
            {
                if ( !placed && seen == insert_pos )
                {
                    rebuilt[ written++ ] = static_cast< u8 >( grabbed );
                    placed               = true;
                }

                ++seen;
            }

            rebuilt[ written++ ] = index;
        }

        if ( !placed )
        {
            rebuilt[ written++ ] = static_cast< u8 >( grabbed );
        }

        for ( usize position{ }; position < config::ESP_ELEMENT_COUNT; ++position )
        {
            profile.m_order[ position ] = rebuilt[ position ];
        }
    }

    void settle( usize index, rect_t rect, u32 delta )
    {
        if ( this->m_pending == static_cast< i32 >( index ) )
        {
            this->m_pending           = -1;
            this->m_offset_x[ index ] = static_cast< float >(
                this->m_pending_center.x - ( rect.left + rect.right ) / 2 );
            this->m_offset_y[ index ] = static_cast< float >(
                this->m_pending_center.y - ( rect.top + rect.bottom ) / 2 );
        }

        this->m_offset_x[ index ] = math::animate( this->m_offset_x[ index ], 0.0f, delta,
                                                   OVERLAY_SETTLE_RESPONSE );
        this->m_offset_y[ index ] = math::animate( this->m_offset_y[ index ], 0.0f, delta,
                                                   OVERLAY_SETTLE_RESPONSE );

        const i32 shift_x{ static_cast< i32 >(
            this->m_offset_x[ index ] + ( this->m_offset_x[ index ] < 0.0f ? -0.5f : 0.5f ) ) };
        const i32 shift_y{ static_cast< i32 >(
            this->m_offset_y[ index ] + ( this->m_offset_y[ index ] < 0.0f ? -0.5f : 0.5f ) ) };

        this->m_element_rects[ index ] = rect_t{ rect.left + shift_x, rect.top + shift_y,
                                                 rect.right + shift_x, rect.bottom + shift_y };
    }

    void place_elements( const config::esp_profile_t &profile, u32 delta, point_t mouse )
    {
        esp_layout_t layout{ this->m_model_box };

        usize order[ config::ESP_ELEMENT_COUNT ]{ ELEMENT_HEALTH };
        usize laid{ 1 };

        for ( usize position{ }; position < config::ESP_ELEMENT_COUNT; ++position )
        {
            const usize block{ profile.m_order[ position ] };

            if ( element_draggable( block ) && !element_is_bar( block ) )
            {
                order[ laid++ ] = block;
            }
        }

        for ( usize walk{ }; walk < laid; ++walk )
        {
            const usize index{ order[ walk ] };

            if ( !this->m_element_laid[ index ] )
            {
                continue;
            }

            if ( this->m_grab == static_cast< i32 >( index ) )
            {
                this->m_element_rects[ index ] = this->floating_rect(
                    index, point_t{ mouse.x - this->m_grab_offset.x,
                                    mouse.y - this->m_grab_offset.y } );
                this->m_offset_x[ index ] = 0.0f;
                this->m_offset_y[ index ] = 0.0f;
                continue;
            }

            const config::esp_anchor_t anchor{ profile.m_anchors[ index ] };

            if ( element_is_bar( index ) )
            {
                this->settle( index, layout.claim_bar( anchor ), delta );
                continue;
            }

            const point_t at{ layout.claim_block( anchor, profile.m_aligns[ index ],
                                                  element_size( index ).y ) };

            this->settle( index, block_rect( index, at, anchor ), delta );
        }
    }

    void drive_drag( point_t mouse )
    {
        if ( this->m_grab >= 0 )
        {
            if ( !this->m_element_shown[ this->m_grab ] )
            {
                this->m_grab = -1;
                return;
            }

            if ( g_input.down( VK_LBUTTON ) )
            {
                return;
            }

            if ( g_gui.hit_test( this->m_bounds ) )
            {
                const rect_t               dropped{ this->m_element_rects[ this->m_grab ] };
                const point_t              center{ ( dropped.left + dropped.right ) / 2,
                                                   ( dropped.top + dropped.bottom ) / 2 };
                const config::esp_anchor_t side{ nearest_anchor( center, this->m_model_box ) };
                const config::esp_align_t  align{ drop_align( center, this->m_model_box,
                                                              side ) };

                g_cfg.m_esp.edited( ).m_anchors[ this->m_grab ] = side;
                g_cfg.m_esp.edited( ).m_aligns[ this->m_grab ]  = align;

                if ( !element_is_bar( static_cast< usize >( this->m_grab ) ) )
                {
                    this->reorder_stack( static_cast< usize >( this->m_grab ), side, align,
                                         center );
                }
            }

            const rect_t rect{ this->m_element_rects[ this->m_grab ] };

            this->m_pending          = this->m_grab;
            this->m_pending_center.x = ( rect.left + rect.right ) / 2;
            this->m_pending_center.y = ( rect.top + rect.bottom ) / 2;
            this->m_grab             = -1;
            return;
        }

        if ( this->m_hovered < 0 || !g_input.pressed( VK_LBUTTON ) )
        {
            return;
        }

        const rect_t rect{ this->m_element_rects[ this->m_hovered ] };

        this->m_grab = this->m_hovered;

        this->m_grab_offset.x = mouse.x - ( rect.left + rect.right ) / 2;
        this->m_grab_offset.y = mouse.y - ( rect.top + rect.bottom ) / 2;
    }

    void update_elements( )
    {
        const u32 delta{ gui::animation_delta( this->m_element_tick ) };

        this->m_hovered = -1;

        if ( !this->grab_model_box( delta ) )
        {
            if ( !g_input.down( VK_LBUTTON ) )
            {
                this->m_grab = -1;
            }
            return;
        }

        const config::esp_profile_t &profile{ g_cfg.m_esp.edited( ) };
        const point_t                mouse{ g_input.mouse( ) };

        for ( usize index{ }; index < config::ESP_ELEMENT_COUNT; ++index )
        {
            const bool enabled{ profile.m_elements[ index ] };

            gui::step_mix( this->m_alpha[ index ], enabled, delta, OVERLAY_FADE_DURATION );

            this->m_element_shown[ index ] = enabled && element_draggable( index );
            this->m_element_laid[ index ]  = element_draggable( index ) &&
                                             ( enabled || this->m_alpha[ index ] );
        }

        this->place_elements( profile, delta, mouse );

        const bool interactive{ g_input.focused( ) && g_gui.can_interact( ) && !g_gui.dragging( ) };

        if ( interactive && this->m_grab < 0 && g_gui.hit_test( this->m_bounds ) )
        {
            // Actual contents win over a neighbour's expanded hit area.
            for ( usize index{ }; index < config::ESP_ELEMENT_COUNT; ++index )
            {
                if ( this->m_element_shown[ index ] &&
                     g_gui.hit_test( this->m_element_rects[ index ] ) )
                {
                    this->m_hovered = static_cast< i32 >( index );
                }
            }

            const bool padded{ this->m_hovered < 0 };
            i32        best_distance{ OVERLAY_HIT_EXPAND * OVERLAY_HIT_EXPAND * 2 + 1 };
            for ( usize index{ }; padded && index < config::ESP_ELEMENT_COUNT; ++index )
            {
                // Find the nearest real rect; equal distances follow the drawing order.
                if ( !this->m_element_shown[ index ] )
                {
                    continue;
                }

                const rect_t rect{ this->m_element_rects[ index ] };
                const i32    dx{ mouse.x < rect.left     ? rect.left - mouse.x
                                 : mouse.x >= rect.right ? mouse.x - rect.right + 1
                                                         : 0 };
                const i32    dy{ mouse.y < rect.top       ? rect.top - mouse.y
                                 : mouse.y >= rect.bottom ? mouse.y - rect.bottom + 1
                                                          : 0 };

                if ( dx > OVERLAY_HIT_EXPAND || dy > OVERLAY_HIT_EXPAND )
                {
                    continue;
                }

                const i32 distance{ dx * dx + dy * dy };
                if ( distance <= best_distance && g_gui.hit_test( rect_t{
                                                      rect.left - OVERLAY_HIT_EXPAND, rect.top - OVERLAY_HIT_EXPAND,
                                                      rect.right + OVERLAY_HIT_EXPAND, rect.bottom + OVERLAY_HIT_EXPAND } ) )
                {
                    best_distance   = distance;
                    this->m_hovered = static_cast< i32 >( index );
                }
            }
        }

        for ( usize index{ }; index < config::ESP_ELEMENT_COUNT; ++index )
        {
            const bool focused{ this->m_grab == static_cast< i32 >( index ) ||
                                this->m_hovered == static_cast< i32 >( index ) };

            gui::step_mix( this->m_glow[ index ], focused, delta, WIDGET_HOVER_DURATION );
        }

        if ( !interactive )
        {
            this->cancel_drag( );
            return;
        }

        this->drive_drag( mouse );
    }

    void draw_element( usize index, u8 alpha ) const
    {
        const config::esp_profile_t &profile{ g_cfg.m_esp.edited( ) };
        const rect_t                 rect{ this->m_element_rects[ index ] };
        const config::esp_anchor_t   anchor{ profile.m_anchors[ index ] };
        const render::text_align     align{ esp_layout_t::align_for( anchor ) };

        const i32 text_x{ align == render::text_align::right  ? rect.right
                          : align == render::text_align::left ? rect.left
                                                              : ( rect.left + rect.right ) / 2 };

        switch ( static_cast< config::esp_element_t >( index ) )
        {
            case config::esp_element_t::health:
                draw_bar( rect, 0.62f + 0.38f * math::sin( this->m_phase * 0.9f ),
                          profile.m_health_style, profile.m_health_color,
                          profile.m_health_low_color, profile.m_health_divisions, alpha );
                break;
            case config::esp_element_t::name:
            {
                // the builder previews the avatar with the local user's own picture
                const usize shape{ g_cfg.m_esp.m_avatars };

                g_render.text( ATLAS_XORSTR( "ESP Name" ).get( ), point_t{ text_x, rect.top },
                               g_gui.color( gui::with_alpha( profile.m_name_color, alpha ) ),
                               player_scene( g_cfg.m_esp.m_edited ).m_name, true, align, 0.0f,
                               shape != render::AVATAR_NONE ? g_steam.local_account_id( ) : 0,
                               static_cast< u32 >( shape ) );

                break;
            }
            case config::esp_element_t::tag:
            {
                const char *label{ nullptr };
                color_t     color{ };
                switch ( g_cfg.m_esp.m_edited )
                {
                    case ( usize )config::esp_target_t::atlas:
                    {
                        label = ATLAS_XORSTR_STATIC( "Atlas User" );
                        color = color_t{ 75, 105, 215 };
                        break;
                    }

                    case ( usize )config::esp_target_t::friends:
                    {
                        label = ATLAS_XORSTR_STATIC( "Friend" );
                        color = color_t{ 100, 215, 110 };
                        break;
                    }

                    default:
                    {
                        bool swap{ math::sin( this->m_phase ) >= 0.0f };
                        label = swap ? ATLAS_XORSTR_STATIC( "Ignored" ) : ATLAS_XORSTR_STATIC( "Cheater" );
                        color = swap ? color_t{ 170, 170, 175 } : color_t{ 255, 100, 100 };
                        break;
                    }
                }

                g_render.text( ATLAS_XORSTR( "ESP Name" ).get( ),
                               point_t{ text_x, rect.top },
                               g_gui.color( gui::with_alpha( color, alpha ) ),
                               label, true, align );
                break;
            }
            case config::esp_element_t::distance:
                g_render.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ text_x, rect.top },
                               g_gui.color( gui::with_alpha( profile.m_distance_color,
                                                             alpha ) ),
                               ATLAS_XORSTR( "[12M]" ).get( ), true, align );
                break;
            case config::esp_element_t::weapon:
                g_render.text( ATLAS_XORSTR( "ESP Small" ).get( ), point_t{ text_x, rect.top },
                               g_gui.color( gui::with_alpha( profile.m_weapon_color, alpha ) ),
                               player_scene( g_cfg.m_esp.m_edited ).m_weapon_name, true,
                               align );
                break;
            default:
            {
                static const sample_t SAMPLES[ FLAG_SAMPLE_COUNT ]{
                    { ATLAS_XORSTR_STATIC( "UBER" ), { 255, 70, 70 } },
                    { ATLAS_XORSTR_STATIC( "KRITZ" ), { 120, 200, 255 } },
                    { ATLAS_XORSTR_STATIC( "BURN" ), { 255, 140, 40 } }
                };

                draw_samples( SAMPLES, FLAG_SAMPLE_COUNT, point_t{ text_x, rect.top },
                              this->m_phase, alpha, align );
                break;
            }
        }
    }

    void draw_skeleton( u8 alpha ) const
    {
        const color_t value{ gui::with_alpha( g_cfg.m_esp.edited( ).m_skeleton_color,
                                              alpha ) };

        const esp_builder::bone_segment_t *segments{ this->m_frame.m_skeleton };
        const i32                          count{ this->m_frame.m_skeleton_count };

        for ( i32 index{ }; index < count; ++index )
        {
            g_render.line( this->model_point( segments[ index ].m_from ),
                           this->model_point( segments[ index ].m_to ),
                           g_gui.color( value ) );
        }
    }

    void draw_elements( ) const
    {
        if ( !this->m_box_valid )
        {
            return;
        }

        const config::esp_profile_t &profile{ g_cfg.m_esp.edited( ) };

        if ( const u8 alpha{ gui::eased_mix(
                 this->m_alpha[ static_cast< usize >( config::esp_element_t::skeleton ) ] ) } )
        {
            this->draw_skeleton( alpha );
        }

        if ( const u8 alpha{ gui::eased_mix(
                 this->m_alpha[ static_cast< usize >( config::esp_element_t::box ) ] ) } )
        {
            draw_outlined_box( this->m_model_box, profile.m_box_color, alpha );
        }

        for ( usize index{ }; index < config::ESP_ELEMENT_COUNT; ++index )
        {
            if ( !this->m_element_laid[ index ] )
            {
                continue;
            }

            if ( const u8 glow{ gui::eased_mix( this->m_glow[ index ] ) } )
            {
                const rect_t rect{ this->m_element_rects[ index ] };

                for ( i32 ring{ }; ring < 4; ++ring )
                {
                    const u8 falloff{ static_cast< u8 >( ( OVERLAY_GLOW_ALPHA >> ring ) *
                                                         glow / 255 ) };

                    if ( !falloff )
                    {
                        break;
                    }

                    g_render.box( rect_t{ rect.left - 2 - ring, rect.top - 2 - ring,
                                          rect.right + 2 + ring, rect.bottom + 2 + ring },
                                  g_gui.color( gui::with_alpha( g_theme.m_accent,
                                                                falloff ) ) );
                }
            }

            this->draw_element( index, gui::eased_mix( this->m_alpha[ index ] ) );
        }
    }

    void update_entity_elements( )
    {
        const u32 delta{ gui::animation_delta( this->m_element_tick ) };

        this->m_hovered = -1;
        this->m_grab    = -1;

        if ( this->m_entity_last != g_cfg.m_esp.m_target_group )
        {
            this->m_entity_last = g_cfg.m_esp.m_target_group;

            for ( u8 &alpha : this->m_entity_alpha )
            {
                alpha = 0;
            }
        }

        if ( !this->grab_model_box( delta ) )
        {
            return;
        }

        const bool  buildings{ showing_buildings( ) };
        const usize item{ g_cfg.m_esp.m_edited };
        const bool *elements{ buildings                       ? g_cfg.m_esp.m_building_elements
                              : item == config::WORLD_PICKUPS ? g_cfg.m_esp.m_pickup_elements
                              : item == config::WORLD_INTEL   ? g_cfg.m_esp.m_intel_elements
                              : item == config::WORLD_NPCS    ? g_cfg.m_esp.m_npc_elements
                                                              : g_cfg.m_esp.m_projectile_elements };
        const usize count{ buildings ? 5u : item == config::WORLD_NPCS ? 3u
                                                                       : 4u };

        for ( usize index{ }; index < ENTITY_ELEMENT_MAX; ++index )
        {
            gui::step_mix( this->m_entity_alpha[ index ], index < count && elements[ index ],
                           delta, OVERLAY_FADE_DURATION );
        }
    }

    void draw_entity_elements( ) const
    {
        if ( !this->m_box_valid )
        {
            return;
        }

        const rect_t box{ this->m_model_box };
        const i32    center_x{ box.left + ( box.right - box.left ) / 2 };
        const bool   buildings{ showing_buildings( ) };

        if ( const u8 alpha{ gui::eased_mix( this->m_entity_alpha[ 0 ] ) } )
        {
            draw_outlined_box( box, color_t{ 255, 255, 255 }, alpha );
        }

        if ( const u8 alpha{ gui::eased_mix( this->m_entity_alpha[ 1 ] ) } )
        {
            g_render.text( ATLAS_XORSTR( "ESP Small" ).get( ),
                           point_t{ center_x, box.top - 12 },
                           g_gui.color( gui::with_alpha( color_t{ 255, 255, 255 }, alpha ) ),
                           entity_scene( ).m_name, true,
                           render::text_align::center );
        }

        if ( const u8 alpha{ gui::eased_mix( this->m_entity_alpha[ 2 ] ) } )
        {
            g_render.text( ATLAS_XORSTR( "ESP Small" ).get( ),
                           point_t{ center_x, box.bottom + 3 },
                           g_gui.color( gui::with_alpha( color_t{ 255, 255, 255 }, alpha ) ),
                           ATLAS_XORSTR( "[12M]" ).get( ), true, render::text_align::center );
        }

        if ( buildings )
        {
            if ( const u8 alpha{ gui::eased_mix( this->m_entity_alpha[ 3 ] ) } )
            {
                const float fill{ 0.62f + 0.38f * math::sin( this->m_phase * 0.9f ) };
                draw_bar( rect_t{ box.left - ESP_BAR_GAP - ESP_BAR_WIDTH, box.top,
                                  box.left - ESP_BAR_GAP, box.bottom },
                          fill, config::HEALTH_STYLE_SOLID, health_tint( fill ), { }, 0, alpha );
            }
        }

        if ( const u8 alpha{ gui::eased_mix( this->m_entity_alpha[ buildings ? 4 : 3 ] ) } )
        {
            static const sample_t BUILDING_SAMPLES[ 2 ]{
                { ATLAS_XORSTR_STATIC( "NO AMMO" ), { 235, 190, 80 } },
                { ATLAS_XORSTR_STATIC( "SAPPED" ), { 110, 200, 230 } }
            };

            static const sample_t PROJECTILE_SAMPLES[ 1 ]{
                { ATLAS_XORSTR_STATIC( "CRIT" ), { 255, 70, 70 } }
            };

            static const sample_t PACK_SAMPLES[ 1 ]{
                { ATLAS_XORSTR_STATIC( "8.2s" ), { 220, 220, 220 } }
            };

            static const sample_t INTEL_SAMPLES[ 1 ]{
                { ATLAS_XORSTR_STATIC( "DROPPED" ), { 255, 220, 60 } }
            };

            const usize     item{ g_cfg.m_esp.m_edited };
            const sample_t *samples{ buildings                       ? BUILDING_SAMPLES
                                     : item == config::WORLD_PICKUPS ? PACK_SAMPLES
                                     : item == config::WORLD_INTEL   ? INTEL_SAMPLES
                                                                     : PROJECTILE_SAMPLES };

            draw_samples( samples, buildings ? 2u : 1u, point_t{ box.right + 4, box.top - 1 },
                          this->m_phase, alpha, render::text_align::left );
        }
    }

    void rotate( )
    {
        if ( !g_input.focused( ) || !g_gui.can_interact( ) || g_gui.dragging( ) )
        {
            this->m_rotating = false;
            return;
        }

        const i32 width{ this->m_bounds.right - this->m_bounds.left };
        const i32 height{ this->m_bounds.bottom - this->m_bounds.top };

        if ( !this->m_rotating && this->m_grab < 0 && this->m_hovered < 0 &&
             g_gui.can_interact( ) && !g_gui.dragging( ) && g_input.pressed( VK_LBUTTON ) &&
             width > 0 && height > 0 &&
             g_input.in_area( this->m_bounds.left, this->m_bounds.top, width - 1,
                              height - 1 ) )
        {
            this->m_rotating = true;
            this->m_anchor_x = g_input.mouse( ).x;
        }

        if ( !this->m_rotating )
        {
            return;
        }

        if ( !g_input.down( VK_LBUTTON ) )
        {
            this->m_rotating = false;
            return;
        }

        const i32 mouse_x{ g_input.mouse( ).x };

        this->m_yaw -= static_cast< float >( mouse_x - this->m_anchor_x ) * ROTATE_SPEED;
        this->m_anchor_x = mouse_x;

        while ( this->m_yaw >= 360.0f )
        {
            this->m_yaw -= 360.0f;
        }

        while ( this->m_yaw < 0.0f )
        {
            this->m_yaw += 360.0f;
        }

        g_esp_builder.set_yaw( this->m_yaw );
    }

    esp_builder::frame_t m_frame{ };
    u64                  m_epoch{ };
    u64                  m_revision{ };
    usize                m_active_group{ config::CHAM_ENTITY_COUNT };
    usize                m_active_target{ config::ESP_TARGET_COUNT };
    usize                m_active_local_model{ };

    float m_yaw{ };
    i32   m_anchor_x{ };
    bool  m_rotating{ };

    u8    m_entity_alpha[ ENTITY_ELEMENT_MAX ]{ };
    usize m_entity_last{ };

    rect_t  m_model_box{ };
    rect_t  m_element_rects[ config::ESP_ELEMENT_COUNT ]{ };
    bool    m_element_shown[ config::ESP_ELEMENT_COUNT ]{ };
    bool    m_element_laid[ config::ESP_ELEMENT_COUNT ]{ };
    u8      m_alpha[ config::ESP_ELEMENT_COUNT ]{ };
    u8      m_glow[ config::ESP_ELEMENT_COUNT ]{ };
    float   m_offset_x[ config::ESP_ELEMENT_COUNT ]{ };
    float   m_offset_y[ config::ESP_ELEMENT_COUNT ]{ };
    u64     m_element_tick{ };
    float   m_phase{ };
    bool    m_box_valid{ };
    i32     m_grab{ -1 };
    i32     m_hovered{ -1 };
    i32     m_pending{ -1 };
    point_t m_pending_center{ };
    point_t m_grab_offset{ };
};

void attach_esp_preview( gui::sub_tab *page )
{
    page->set_panel< esp_preview >( ICON_CROSSHAIR, ATLAS_XORSTR_STATIC( "Preview" ),
                                    PREVIEW_PANEL_WIDTH );
}

void reset_esp_preview( )
{
    ++s_preview_revision;
}
