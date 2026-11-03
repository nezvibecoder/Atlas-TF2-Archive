#pragma once

#include "d3d9.hpp"
#include "font.hpp"
#include "gdi.hpp"
#include "math/vector.hpp"
#include "runtime/array.hpp"
#include "runtime/mutex.hpp"
#include "runtime/obfuscation.hpp"
#include "runtime/vector.hpp"
#include "steam_html.hpp"
#include "util/string.hpp"

#if defined( ATLAS_RENDER_D3D11 )
#include "d3d11.hpp"
#else
#include "overlay.hpp"
#endif

namespace render
{
#if defined( ATLAS_RENDER_D3D11 )
    using texture_t = d3d11::texture;
#else
    using texture_t = d3d9::texture9;
#endif

    inline constexpr usize TEXT_COMMAND_CAPACITY{ 1024 };
    inline constexpr usize FONT_NAME_CAPACITY{ 64 };
    inline constexpr u8    TEXT_ATLAS_FIRST_CHAR{ 32 };
    inline constexpr u8    TEXT_ATLAS_LAST_CHAR{ 126 };
    inline constexpr u32   TEXT_ATLAS_COLUMNS{ 16 };
    inline constexpr u32   TEXT_ATLAS_PADDING{ 2 };
    inline constexpr usize POLYGON_CAPACITY{ 8 };

    struct text_glyph
    {
        float m_u0{ };
        float m_v0{ };
        float m_u1{ };
        float m_v1{ };
        float m_width{ };
        float m_height{ };
        float m_offset_x{ };
        float m_offset_y{ };
        float m_advance{ };
        u32   m_bitmap_width{ };
        u32   m_bitmap_height{ };
        bool  m_visible{ };
    };

    struct icon_atlas_t
    {
        texture_t      m_texture{ };
        u32            m_slot_size{ };
        u32            m_slot_count{ };
        bool           m_ready{ };
    };

    enum : u32
    {
        AVATAR_NONE   = 0,
        AVATAR_SQUARE = 1,
        AVATAR_CIRCLE = 2,
    };

    inline constexpr usize AVATAR_CACHE_SIZE{ 34 };

    struct avatar_entry_t
    {
        u32            m_account{ };
        texture_t      m_texture{ };
        u64            m_last_used{ };    // frame clock of the last draw; a full table recycles the oldest
        u64            m_next_attempt{ }; // frame clock before which steam is not asked again
        bool           m_ready{ };
    };

    struct html_texture_t
    {
        texture_t      m_texture{ };
        u32            m_width{ };
        u32            m_height{ };
        u32            m_generation{ }; // the panel it was filled from; a reopened slot starts over
    };

    struct text_atlas
    {
        texture_t                          m_texture{ };
        atl::array< text_glyph, 256 >      m_glyphs{ };
        fixed_string< FONT_NAME_CAPACITY > m_face{ };
        i32                                m_font_height{ };
        i32                                m_font_weight{ };
        bool                               m_crisp{ };
        u32                                m_width{ };
        u32                                m_height{ };
        u32                                m_line_height{ };
        bool                               m_ready{ };
    };

    struct text_font_t
    {
        fixed_string< FONT_NAME_CAPACITY > m_face{ };
        i32                                m_height{ -static_cast< i32 >( DEFAULT_FONT_PIXEL_SIZE ) };
        i32                                m_weight{ FW_NORMAL };
        bool                               m_crisp{ };
    };

    enum class command_type : u32
    {
        none,
        line,
        circle,
        arc,
        box,
        gradient,
        polygon,
        blur,
        model,
        text,
        icon,
        html,
    };

    enum class text_align : u32
    {
        left,
        center,
        right,
    };

    struct render_command
    {
        command_type                          m_type{ command_type::none };
        vec2                                  m_from{ };
        vec2                                  m_to{ };
        float                                 m_thickness{ 1.0f };
        point_t                               m_position{ };
        rect_t                                m_bounds{ };
        color_t                               m_color{ };
        f32                                   m_radius{ };
        f32                                   m_start{ };
        f32                                   m_sweep{ };
        point_t                               m_transform_origin{ };
        float                                 m_transform_scale{ 1.0f };
        rect_t                                m_clip{ };
        bool                                  m_clipped{ };
        bool                                  m_filled{ };
        bool                                  m_outlined{ };
        text_align                            m_align{ text_align::left };
        float                                 m_crop_top{ };
        u32                                   m_icon_slot{ };      // index within the atlas
        u32                                   m_icon_atlas{ };     // render::icons::atlas_kind_t
        u32                                   m_avatar_account{ }; // steam account id ahead of the text
        u32                                   m_avatar_shape{ };   // AVATAR_SQUARE / AVATAR_CIRCLE
        u32                                   m_avatar_size{ };    // zero uses the font line height
        vec2                                  m_points[ POLYGON_CAPACITY ]{ };
        color_t                               m_point_colors[ POLYGON_CAPACITY ]{ };
        u32                                   m_point_count{ };
        fixed_string< FONT_NAME_CAPACITY >    m_font{ };
        fixed_string< TEXT_COMMAND_CAPACITY > m_text{ };
    };

    struct grade_t
    {
        float m_exposure{ };
        float m_contrast{ 1.0f };
        float m_saturation{ 1.0f };
        float m_vibrance{ };
        float m_shadows{ };
        float m_highlights{ };
        float m_temperature{ };
        float m_tint{ };
        float m_vignette{ };
        float m_bloom{ };
        float m_bloom_threshold{ };
    };

    enum class batch_type : u32
    {
        none,
        lines,
        solid,
        blur,
        model,
        text,
        icon_small,
        icon_large,
        icon_logo,
        icon_vacc,
        avatar,
        html,
    };

    class backend
    {
    public:
        bool init( );
        void shutdown( );

#if defined( ATLAS_RENDER_D3D11 )
        bool start_frame( IDXGISwapChain *swap_chain );
        void release_target( );

        // back buffer size, valid once start_frame succeeded
        u32 width( ) const
        {
            return this->m_width;
        }

        u32 height( ) const
        {
            return this->m_height;
        }
#else
        bool start_frame( void *device );
#endif
        void end_frame( );

        void line( point_t from, point_t to, color_t value, float thickness = 1.0f );
        void line( vec2 from, vec2 to, color_t value, float thickness = 1.0f );
        void circle( vec2 point, color_t col, f32 radius, f32 thickness = 1.0f );
        void circle( point_t point, color_t col, f32 radius, float thickness = 1.0f );
        void arc( vec2 centre, f32 radius, f32 thickness, f32 start, f32 sweep, color_t value );
        void box( rect_t bounds, color_t value, bool filled = false );
        void gradient( rect_t bounds, color_t from, color_t to, bool vertical );
        void polygon( const vec2 *points, const color_t *colors, usize count );
        void blur( rect_t bounds, u8 opacity = 255 );
        void text( const char *font, point_t position, color_t value, string_view text,
                   bool outlined = false, text_align align = text_align::left,
                   float crop_top = 0.0f, u32 avatar_account = 0, u32 avatar_shape = AVATAR_NONE,
                   u32 avatar_size = 0 );
        i32  get_text_width( const char *font, string_view text, text_align align = text_align::left );
        void icon( u32 slot, u32 atlas_kind, point_t position, color_t value,
                   float crop_top = 0.0f, bool outlined = false );
        void html( u32 slot, rect_t bounds, u8 opacity = 255 );
        void set_transform( point_t origin, float scale );
        void reset_transform( );
        void set_clip( rect_t bounds );
        void reset_clip( );

#if !defined( ATLAS_RENDER_D3D11 )
        bool adopt_model_target( u64 serial, u32 width, u32 height,
                                 u32 target_width, u32 target_height );
        void model( rect_t bounds, u8 opacity = 255 );

        bool grade( const grade_t &grade );

        bool model_captured( ) const
        {
            return this->m_model_ready;
        }

        u64 model_capture_serial( ) const
        {
            return this->m_model_ready ? this->m_model_serial : 0;
        }

        void on_present( void *device, i32 status );
        void on_device_lost( );
        void on_device_reset( void *device, i32 status );

        void set_stream_proof( bool enabled );
        bool consume_stream_proof_fallback( u32 *stage, u32 *error );

        void *device( ) const
        {
            return this->m_device.Get( );
        }
#endif

        bool initialized( ) const
        {
            return this->m_initialized;
        }

        bool last_frame_succeeded( ) const
        {
            return this->m_last_frame_succeeded;
        }

    private:
        bool setup_fonts( );
        bool push_command( const render_command &command );
        bool render_frame( );
        bool count_vertices( const atl::vector< render_command > &commands, usize *required ) const;
        bool begin_batch( batch_type type );
        bool flush_batch( );
        bool set_scissor( const rect_t &bounds, bool enabled );
        bool apply_scissor( const render_command &command );
        bool append_command( const render_command &command );
        bool append_line( const render_command &command );
        bool append_box( const render_command &command );
        bool append_gradient( const render_command &command );
        bool append_circle( const render_command &command );
        bool append_arc( const render_command &command );
        bool append_polygon( const render_command &command );
        bool append_blur( const render_command &command );
        bool append_text( const render_command &command );
        bool append_icon( const render_command &command );
        bool append_html( const render_command &command );
        void release_html_textures( );
        bool setup_batch_state( batch_type type ) const;

        const icon_atlas_t &icon_atlas_for( batch_type type ) const;

        // Textures are B8G8R8A8 in memory on both backends, so every upload is a straight row copy.
        bool create_texture( u32 width, u32 height, u32 levels, texture_t &texture ) const;
        bool upload_texture( const texture_t &texture, u32 level, const rect_t &area, const u8 *pixels,
                             usize pitch ) const;

        bool build_icon_atlases( );
        void release_icon_atlases( );
        bool build_icon_atlas( icon_atlas_t &atlas, const u8 *encrypted, u32 size, u64 seed,
                               u32 slot_size );

        avatar_entry_t *ensure_avatar( u32 account );
        bool            fetch_avatar( avatar_entry_t &entry );
        bool            append_avatar( const render_command &command, float left, float top,
                                       float size );
        void            release_avatars( );

        bool        text_atlas_matches( const text_atlas &atlas, const text_font_t &font ) const;
        text_atlas *find_text_atlas( const text_font_t &font );
        text_atlas *select_text_atlas( );
        bool        ensure_text_atlas( const render_command &command, text_atlas **atlas );
        bool        build_text_atlas( text_atlas &atlas, const text_font_t &font );
        bool        build_raster_glyphs( const text_atlas &atlas, HGDIOBJ font, u32 atlas_width,
                                         u32 atlas_height, u32 cell_width, u32 cell_height,
                                         gdi::bitmap_t &bitmap ) const;
        void        release_text_atlas( text_atlas &atlas );
        void        release_text_atlases( );
        bool        font_for( const render_command &command, text_font_t *font ) const;

#if defined( ATLAS_RENDER_D3D11 )
        bool attach( IDXGISwapChain *swap_chain );
        bool create_device_objects( );
        void release_device_objects( );
        bool ensure_target( );
        bool ensure_vertex_capacity( usize count );
        bool setup_render_state( ID3D11RenderTargetView *target, u32 width, u32 height, float step_x, float step_y ) const;
        bool ensure_blur_resources( );
        void release_blur_resources( );
        bool render_blur( ID3D11Texture2D *back_buffer );
        bool render_blur_pass( ID3D11PixelShader *shader, const d3d11::texture &source,
                               const d3d11::texture &destination, float step_x, float step_y );
        bool draw_vertices( const d3d9::overlay_vertex *vertices, u32 count, bool lines );
#else
        bool append_model( const render_command &command );
        bool setup_render_state( ) const;
        bool capture_state( );

        bool ensure_composite_target( u32 width, u32 height );
        void release_composite_target( );
        bool draw_composite( const d3d9::D3DVIEWPORT9 &viewport ) const;
        bool present_composite( const d3d9::D3DVIEWPORT9 &viewport );
        bool ensure_blur_resources( u32 width, u32 height, u32 format, bool resolve );
        void release_blur_resources( );
        bool render_blur( const d3d9::surface9 &source );
        bool render_blur_pass( const d3d9::texture9 &source, const d3d9::surface9 &destination,
                               float direction_x, float direction_y );
        bool ensure_grade_resources( u32 width, u32 height, u32 format );
        void release_grade_resources( );
        bool draw_grade( const d3d9::surface9 &target, const d3d9::D3DVIEWPORT9 &viewport,
                         const grade_t &grade, bool bloom );
        void release_model_resources( );
#endif

        bool                                              m_initialized{ };
        bool                                              m_inside_frame{ };
        bool                                              m_frame_build_failed{ };
        bool                                              m_last_frame_succeeded{ };
        u32                                               m_width{ };
        u32                                               m_height{ };
        bool                                              m_blur_frame_ready{ };
        gdi::api                                          m_gdi{ };
        font_manager                                      m_fonts{ };
        atl::array< text_atlas, MAX_FONT_RESOURCES >      m_text_atlases{ };
        text_atlas                                       *m_active_text_atlas{ };
        usize                                             m_text_atlas_cursor{ };
        atl::array< icon_atlas_t, 4 >                     m_icon_atlases{ };
        atl::array< avatar_entry_t, AVATAR_CACHE_SIZE >   m_avatars{ };
        usize                                             m_avatar_count{ };
        atl::vector< u8 >                                 m_avatar_scratch{ };
        const texture_t                                  *m_avatar_bound{ };
        atl::array< html_texture_t, HTML_PANEL_CAPACITY > m_html_textures{ };
        const texture_t                                  *m_html_bound{ };
        u64                                               m_frame_ms{ };
        atl::vector< render_command >                     m_frame_commands{ };
        atl::vector< d3d9::overlay_vertex >               m_vertices{ };
        point_t                                           m_transform_origin{ };
        float                                             m_transform_scale{ 1.0f };
        rect_t                                            m_clip{ };
        bool                                              m_clip_enabled{ };
        rect_t                                            m_scissor{ };
        bool                                              m_scissor_enabled{ };
        batch_type                                        m_batch_type{ batch_type::none };

#if defined( ATLAS_RENDER_D3D11 )
        IDXGISwapChain          *m_swap_chain{ };
        ID3D11Device            *m_device{ };
        ID3D11DeviceContext     *m_context{ };
        ID3D11RenderTargetView  *m_target{ };
        ID3D11VertexShader      *m_vertex_shader{ };
        ID3D11InputLayout       *m_input_layout{ };
        ID3D11PixelShader       *m_solid_shader{ };
        ID3D11PixelShader       *m_textured_shader{ };
        ID3D11PixelShader       *m_tinted_shader{ };
        ID3D11PixelShader       *m_blur_shader{ };
        ID3D11Buffer            *m_vertex_buffer{ };
        u32                      m_vertex_capacity{ };
        u32                      m_vertex_cursor{ };
        ID3D11Buffer            *m_constant_buffer{ };
        ID3D11BlendState        *m_blend_state{ };
        ID3D11BlendState        *m_opaque_state{ };
        ID3D11RasterizerState   *m_raster_state{ };
        ID3D11DepthStencilState *m_depth_state{ };
        ID3D11SamplerState      *m_point_sampler{ };
        ID3D11SamplerState      *m_linear_sampler{ };
        u32                      m_target_format{ };
        u32                      m_target_samples{ };
        d3d11::texture           m_blur_source{ };
        d3d11::texture           m_blur_texture_a{ };
        d3d11::texture           m_blur_texture_b{ };
        u32                      m_blur_width{ };
        u32                      m_blur_height{ };
#else
        d3d9::device9      m_device{ };
        d3d9::state_block9 m_state_block{ };
        d3d9::texture9     m_composite_texture{ };
        d3d9::surface9     m_composite_surface{ };
        overlay            m_overlay{ };
        bool               m_stream_proof{ };
        bool               m_stream_proof_fallback{ };
        bool               m_stream_proof_fallback_reported{ };
        d3d9::object9      m_blur_shader{ };
        d3d9::texture9     m_blur_texture_a{ };
        d3d9::surface9     m_blur_surface_a{ };
        d3d9::texture9     m_blur_texture_b{ };
        d3d9::surface9     m_blur_surface_b{ };
        d3d9::texture9     m_blur_resolve_texture{ };
        d3d9::surface9     m_blur_resolve_surface{ };
        u32                m_blur_width{ };
        u32                m_blur_height{ };
        u32                m_blur_source_width{ };
        u32                m_blur_source_height{ };
        u32                m_blur_format{ };
        bool               m_blur_resolve{ };
        d3d9::object9      m_grade_shader{ };
        d3d9::texture9     m_grade_texture{ };
        d3d9::surface9     m_grade_surface{ };
        u32                m_grade_width{ };
        u32                m_grade_height{ };
        u32                m_grade_format{ };
        d3d9::texture9     m_model_texture{ };
        vec2               m_model_uv{ };
        u64                m_model_serial{ };
        bool               m_model_ready{ };
#endif
    };

    class draw_list
    {
    public:
        void line( point_t from, point_t to, color_t value, float thickness = 1.0f );
        void line( vec2 from, vec2 to, color_t value, float thickness = 1.0f );
        void circle( vec2 point, color_t col, f32 radius, f32 thickness = 1.0f );
        void circle( point_t point, color_t col, f32 radius, f32 thickness = 1.0f );
        void arc( vec2 centre, f32 radius, f32 thickness, f32 start, f32 sweep, color_t value );
        void box( rect_t bounds, color_t value, bool filled = false );
        void gradient( rect_t bounds, color_t from, color_t to, bool vertical );
        void polygon( const vec2 *points, const color_t *colors, usize count );
        void text( const char *font, point_t position, color_t value, string_view text,
                   bool outlined = false, text_align align = text_align::left,
                   float crop_top = 0.0f, u32 avatar_account = 0, u32 avatar_shape = AVATAR_NONE );
        void icon( u32 slot, u32 atlas_kind, point_t position, color_t value,
                   float crop_top = 0.0f, bool outlined = false );
        void html( u32 slot, rect_t bounds, u8 opacity = 255 );

        bool send( );
        void traverse( );
        void reset( );

    private:
        bool push_command( const render_command &command );
        void dispatch( const render_command &command ) const;

        atl::mutex                    m_mutex{ };
        atl::vector< render_command > m_commands{ };
        atl::vector< render_command > m_safe_commands{ };
        bool                          m_build_failed{ };
        bool                          m_snapshot_valid{ };
    };

#if !defined( ATLAS_RENDER_D3D11 )
    void find_device_methods( void *&present, void *&reset );
#endif
} // namespace render

extern render::backend   g_render;
extern render::draw_list g_draw;
