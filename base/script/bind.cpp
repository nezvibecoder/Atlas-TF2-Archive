#include "bind.hpp"

#include "script.hpp"

#include "gui/gui.hpp"
#include "gui/notifications.hpp"
#include "gui/theme.hpp"
#include "input/input.hpp"
#include "render/render.hpp"
#include "runtime/hash.hpp"
#include "steam_html.hpp"

#include "libs/lua/lauxlib.h"
#include "libs/lua/lua.h"

namespace script
{
    static char s_vec3_key{ };
    static char s_html_key{ };

    struct html_object_t
    {
        html_handle_t m_handle{ };
        u32           m_width{ };
        u32           m_height{ };
    };

    void set_function( lua_State *state, const char *name, int ( *function )( lua_State * ) )
    {
        lua_pushcfunction( state, function );
        lua_setfield( state, -2, name );
    }

    color_t check_color( lua_State *state, int index )
    {
        const u32 packed{ static_cast< u32 >( luaL_checkinteger( state, index ) ) };
        return color_t{ static_cast< u8 >( packed >> 24 ), static_cast< u8 >( packed >> 16 ),
                        static_cast< u8 >( packed >> 8 ), static_cast< u8 >( packed ) };
    }

    ATLAS_FORCE_INLINE u32 check_channel( lua_State *state, int index, lua_Integer fallback )
    {
        const lua_Integer value{ luaL_optinteger( state, index, fallback ) };
        luaL_argcheck( state, value >= 0 && value <= 255, index, ATLAS_XORSTR( "channel out of range" ).get( ) );
        return static_cast< u32 >( value );
    }

    static int color_pack( lua_State *state )
    {
        const u32 red{ check_channel( state, 1, 0 ) };
        const u32 green{ check_channel( state, 2, 0 ) };
        const u32 blue{ check_channel( state, 3, 0 ) };
        const u32 alpha{ check_channel( state, 4, 255 ) };

        lua_pushinteger( state, static_cast< lua_Integer >( ( red << 24 ) | ( green << 16 ) | ( blue << 8 ) | alpha ) );
        return 1;
    }

    void *new_object( lua_State *state, usize size, const void *key )
    {
        void *data{ lua_newuserdatauv( state, size, 0 ) };
        lua_rawgetp( state, LUA_REGISTRYINDEX, key );
        lua_setmetatable( state, -2 );
        return data;
    }

    void *check_object( lua_State *state, int index, const void *key, const char *type_name )
    {
        void *data{ lua_touserdata( state, index ) };

        if ( data && lua_getmetatable( state, index ) )
        {
            lua_rawgetp( state, LUA_REGISTRYINDEX, key );
            const bool matches{ lua_rawequal( state, -1, -2 ) != 0 };
            lua_pop( state, 2 );

            if ( matches )
            {
                return data;
            }
        }

        luaL_typeerror( state, index, type_name );
        return nullptr;
    }

    vec3 *check_vec3( lua_State *state, int index )
    {
        return static_cast< vec3 * >( check_object( state, index, &s_vec3_key, ATLAS_XORSTR( "vec3" ).get( ) ) );
    }

    void push_vec3( lua_State *state, const vec3 &value )
    {
        *static_cast< vec3 * >( new_object( state, sizeof( vec3 ), &s_vec3_key ) ) = value;
    }

    ATLAS_FORCE_INLINE f32 check_float( lua_State *state, int index )
    {
        return static_cast< f32 >( luaL_checknumber( state, index ) );
    }

    ATLAS_FORCE_INLINE i32 component( lua_State *state, int index )
    {
        usize       length{ };
        const char *key{ lua_type( state, index ) == LUA_TSTRING ? lua_tolstring( state, index, &length ) : nullptr };

        if ( !key || length != 1 || key[ 0 ] < 'x' || key[ 0 ] > 'z' )
        {
            return -1;
        }

        return key[ 0 ] - 'x';
    }

    static int vec3_new( lua_State *state )
    {
        push_vec3( state, vec3{ static_cast< f32 >( luaL_optnumber( state, 1, 0.0 ) ),
                                static_cast< f32 >( luaL_optnumber( state, 2, 0.0 ) ),
                                static_cast< f32 >( luaL_optnumber( state, 3, 0.0 ) ) } );
        return 1;
    }

    static int vec3_index( lua_State *state )
    {
        const vec3 &self{ *check_vec3( state, 1 ) };
        const i32   field{ component( state, 2 ) };

        if ( field >= 0 )
        {
            lua_pushnumber( state, self[ static_cast< usize >( field ) ] );
            return 1;
        }

        lua_pushvalue( state, 2 );
        lua_rawget( state, lua_upvalueindex( 1 ) );
        return 1;
    }

    static int vec3_newindex( lua_State *state )
    {
        vec3     &self{ *check_vec3( state, 1 ) };
        const i32 field{ component( state, 2 ) };
        luaL_argcheck( state, field >= 0, 2, ATLAS_XORSTR( "vec3 only has x, y and z" ).get( ) );

        self[ static_cast< usize >( field ) ] = check_float( state, 3 );
        return 0;
    }

    static int vec3_add( lua_State *state )
    {
        push_vec3( state, *check_vec3( state, 1 ) + *check_vec3( state, 2 ) );
        return 1;
    }

    static int vec3_sub( lua_State *state )
    {
        push_vec3( state, *check_vec3( state, 1 ) - *check_vec3( state, 2 ) );
        return 1;
    }

    static int vec3_mul( lua_State *state )
    {
        if ( lua_type( state, 1 ) == LUA_TNUMBER )
        {
            push_vec3( state, *check_vec3( state, 2 ) * check_float( state, 1 ) );
            return 1;
        }

        const vec3 &left{ *check_vec3( state, 1 ) };

        if ( lua_type( state, 2 ) == LUA_TNUMBER )
        {
            push_vec3( state, left * check_float( state, 2 ) );
            return 1;
        }

        const vec3 &right{ *check_vec3( state, 2 ) };
        push_vec3( state, vec3{ left.x * right.x, left.y * right.y, left.z * right.z } );
        return 1;
    }

    static int vec3_div( lua_State *state )
    {
        push_vec3( state, *check_vec3( state, 1 ) / check_float( state, 2 ) );
        return 1;
    }

    static int vec3_unm( lua_State *state )
    {
        push_vec3( state, -*check_vec3( state, 1 ) );
        return 1;
    }

    static int vec3_eq( lua_State *state )
    {
        lua_pushboolean( state, *check_vec3( state, 1 ) == *check_vec3( state, 2 ) );
        return 1;
    }

    static int vec3_tostring( lua_State *state )
    {
        const vec3 &self{ *check_vec3( state, 1 ) };
        lua_pushfstring( state, ATLAS_XORSTR( "vec3(%f, %f, %f)" ).get( ), static_cast< lua_Number >( self.x ),
                         static_cast< lua_Number >( self.y ), static_cast< lua_Number >( self.z ) );
        return 1;
    }

    static int vec3_length( lua_State *state )
    {
        lua_pushnumber( state, check_vec3( state, 1 )->length( ) );
        return 1;
    }

    static int vec3_length_2d( lua_State *state )
    {
        lua_pushnumber( state, check_vec3( state, 1 )->length2d( ) );
        return 1;
    }

    static int vec3_dot( lua_State *state )
    {
        lua_pushnumber( state, check_vec3( state, 1 )->dot( *check_vec3( state, 2 ) ) );
        return 1;
    }

    static int vec3_cross( lua_State *state )
    {
        push_vec3( state, check_vec3( state, 1 )->cross( *check_vec3( state, 2 ) ) );
        return 1;
    }

    static int vec3_distance( lua_State *state )
    {
        lua_pushnumber( state, check_vec3( state, 1 )->dist_to( *check_vec3( state, 2 ) ) );
        return 1;
    }

    static int vec3_normalized( lua_State *state )
    {
        vec3 copy{ *check_vec3( state, 1 ) };
        copy.normalize( );
        push_vec3( state, copy );
        return 1;
    }

    static int vec3_unpack( lua_State *state )
    {
        const vec3 &self{ *check_vec3( state, 1 ) };
        lua_pushnumber( state, self.x );
        lua_pushnumber( state, self.y );
        lua_pushnumber( state, self.z );
        return 3;
    }

    ATLAS_FORCE_INLINE void open_vec3( lua_State *state )
    {
        lua_newtable( state );
        set_function( state, ATLAS_XORSTR( "__newindex" ).get( ), &vec3_newindex );
        set_function( state, ATLAS_XORSTR( "__add" ).get( ), &vec3_add );
        set_function( state, ATLAS_XORSTR( "__sub" ).get( ), &vec3_sub );
        set_function( state, ATLAS_XORSTR( "__mul" ).get( ), &vec3_mul );
        set_function( state, ATLAS_XORSTR( "__div" ).get( ), &vec3_div );
        set_function( state, ATLAS_XORSTR( "__unm" ).get( ), &vec3_unm );
        set_function( state, ATLAS_XORSTR( "__eq" ).get( ), &vec3_eq );
        set_function( state, ATLAS_XORSTR( "__tostring" ).get( ), &vec3_tostring );

        lua_newtable( state );
        set_function( state, ATLAS_XORSTR( "length" ).get( ), &vec3_length );
        set_function( state, ATLAS_XORSTR( "length_2d" ).get( ), &vec3_length_2d );
        set_function( state, ATLAS_XORSTR( "dot" ).get( ), &vec3_dot );
        set_function( state, ATLAS_XORSTR( "cross" ).get( ), &vec3_cross );
        set_function( state, ATLAS_XORSTR( "distance" ).get( ), &vec3_distance );
        set_function( state, ATLAS_XORSTR( "normalized" ).get( ), &vec3_normalized );
        set_function( state, ATLAS_XORSTR( "unpack" ).get( ), &vec3_unpack );
        lua_pushcclosure( state, &vec3_index, 1 );
        lua_setfield( state, -2, ATLAS_XORSTR( "__index" ).get( ) );

        lua_rawsetp( state, LUA_REGISTRYINDEX, &s_vec3_key );

        lua_pushcfunction( state, &vec3_new );
        lua_setglobal( state, ATLAS_XORSTR( "vec3" ).get( ) );
    }

    ATLAS_FORCE_INLINE void require_paint( lua_State *state )
    {
        if ( g_scripts.current( ) != event_t::paint )
        {
            luaL_error( state, ATLAS_XORSTR( "draw functions only work inside a paint handler" ).get( ) );
        }
    }

    ATLAS_FORCE_INLINE const char *check_font( lua_State *state, int index )
    {
        if ( lua_isnoneornil( state, index ) )
        {
            return ATLAS_XORSTR_STATIC( "ESP Name" );
        }

        switch ( HASH_RT( luaL_checkstring( state, index ) ) )
        {
            case HASH_CT( "esp" ): return ATLAS_XORSTR_STATIC( "ESP Name" );
            case HASH_CT( "esp_small" ): return ATLAS_XORSTR_STATIC( "ESP Small" );
            case HASH_CT( "menu" ): return MENU_FONT( );
            case HASH_CT( "menu_bold" ): return MENU_FONT_BOLD( );
            case HASH_CT( "menu_small" ): return MENU_FONT_TINY( );
            case HASH_CT( "mono" ): return MENU_FONT_MONO( );
            default: break;
        }

        luaL_argerror( state, index, ATLAS_XORSTR( "unknown font" ).get( ) );
        return nullptr;
    }

    ATLAS_FORCE_INLINE render::text_align check_align( lua_State *state, int index )
    {
        if ( lua_isnoneornil( state, index ) )
        {
            return render::text_align::left;
        }

        switch ( HASH_RT( luaL_checkstring( state, index ) ) )
        {
            case HASH_CT( "left" ): return render::text_align::left;
            case HASH_CT( "center" ): return render::text_align::center;
            case HASH_CT( "right" ): return render::text_align::right;
            default: break;
        }

        luaL_argerror( state, index, ATLAS_XORSTR( "align is left, center or right" ).get( ) );
        return render::text_align::left;
    }

    static int draw_line( lua_State *state )
    {
        const vec2    from{ check_float( state, 1 ), check_float( state, 2 ) };
        const vec2    to{ check_float( state, 3 ), check_float( state, 4 ) };
        const color_t value{ check_color( state, 5 ) };
        const f32     thickness{ static_cast< f32 >( luaL_optnumber( state, 6, 1.0 ) ) };

        require_paint( state );
        g_draw.line( from, to, value, thickness );
        return 0;
    }

    ATLAS_FORCE_INLINE void draw_box( lua_State *state, bool filled )
    {
        const i32     x{ static_cast< i32 >( luaL_checknumber( state, 1 ) ) };
        const i32     y{ static_cast< i32 >( luaL_checknumber( state, 2 ) ) };
        const i32     width{ static_cast< i32 >( luaL_checknumber( state, 3 ) ) };
        const i32     height{ static_cast< i32 >( luaL_checknumber( state, 4 ) ) };
        const color_t value{ check_color( state, 5 ) };

        require_paint( state );
        g_draw.box( rect_t{ x, y, x + width, y + height }, value, filled );
    }

    static int draw_rect( lua_State *state )
    {
        draw_box( state, false );
        return 0;
    }

    static int draw_rect_filled( lua_State *state )
    {
        draw_box( state, true );
        return 0;
    }

    static int draw_circle( lua_State *state )
    {
        const vec2    center{ check_float( state, 1 ), check_float( state, 2 ) };
        const f32     radius{ check_float( state, 3 ) };
        const color_t value{ check_color( state, 4 ) };
        const f32     thickness{ static_cast< f32 >( luaL_optnumber( state, 5, 1.0 ) ) };

        require_paint( state );
        g_draw.circle( center, value, radius, thickness );
        return 0;
    }

    static int draw_text( lua_State *state )
    {
        const point_t            position{ static_cast< i32 >( luaL_checknumber( state, 1 ) ),
                                           static_cast< i32 >( luaL_checknumber( state, 2 ) ) };
        const color_t            value{ check_color( state, 3 ) };
        usize                    length{ };
        const char              *text{ luaL_checklstring( state, 4, &length ) };
        const char              *font{ check_font( state, 5 ) };
        const render::text_align align{ check_align( state, 6 ) };
        const bool               outlined{ lua_toboolean( state, 7 ) != 0 };

        require_paint( state );
        g_draw.text( font, position, value, string_view{ text, length }, outlined, align );
        return 0;
    }

    ATLAS_FORCE_INLINE html_object_t &check_html( lua_State *state )
    {
        return *static_cast< html_object_t * >( check_object( state, 1, &s_html_key, ATLAS_XORSTR( "html" ).get( ) ) );
    }

    ATLAS_FORCE_INLINE u32 check_side( lua_State *state, int index )
    {
        const lua_Integer value{ luaL_checkinteger( state, index ) };
        luaL_argcheck( state, value > 0 && value <= HTML_PANEL_MAX_SIDE, index, ATLAS_XORSTR( "size out of range" ).get( ) );
        return static_cast< u32 >( value );
    }

    ATLAS_FORCE_INLINE int open_panel( lua_State *state, bool web )
    {
        const u32   width{ check_side( state, 1 ) };
        const u32   height{ check_side( state, 2 ) };
        usize       length{ };
        const char *source{ luaL_checklstring( state, 3, &length ) };

        // css and l4d2 never hand the surface over
        if ( !g_html.ready( ) )
        {
            return luaL_error( state, ATLAS_XORSTR( "html panels are unavailable" ).get( ) );
        }

        const html_handle_t handle{ g_html.open( width, height, string_view{ source, length }, web ) };

        if ( handle.m_slot >= HTML_PANEL_CAPACITY )
        {
            return luaL_error( state, web ? ATLAS_XORSTR( "could not open the page, the url must be http or https" ).get( )
                                          : ATLAS_XORSTR( "could not open an html panel" ).get( ) );
        }

        *static_cast< html_object_t * >( new_object( state, sizeof( html_object_t ), &s_html_key ) ) = html_object_t{ handle, width, height };
        return 1;
    }

    static int ui_html( lua_State *state )
    {
        return open_panel( state, false );
    }

    static int ui_web( lua_State *state )
    {
        return open_panel( state, true );
    }

    static int html_draw( lua_State *state )
    {
        const html_object_t &self{ check_html( state ) };
        const i32            x{ static_cast< i32 >( luaL_checknumber( state, 2 ) ) };
        const i32            y{ static_cast< i32 >( luaL_checknumber( state, 3 ) ) };
        const lua_Integer    alpha{ luaL_optinteger( state, 4, 255 ) };

        luaL_argcheck( state, alpha >= 0 && alpha <= 255, 4, ATLAS_XORSTR( "alpha out of range" ).get( ) );
        require_paint( state );

        if ( self.m_handle.m_slot < HTML_PANEL_CAPACITY )
        {
            g_draw.html( self.m_handle.m_slot, rect_t{ x, y, x + static_cast< i32 >( self.m_width ), y + static_cast< i32 >( self.m_height ) },
                         static_cast< u8 >( alpha ) );
        }

        return 0;
    }

    static int html_load( lua_State *state )
    {
        const html_object_t &self{ check_html( state ) };
        usize                length{ };
        const char          *source{ luaL_checklstring( state, 2, &length ) };

        lua_pushboolean( state, g_html.load( self.m_handle, string_view{ source, length } ) );
        return 1;
    }

    static int html_send( lua_State *state )
    {
        const html_object_t &self{ check_html( state ) };
        usize                length{ };
        const char          *text{ luaL_checklstring( state, 2, &length ) };

        lua_pushboolean( state, g_html.send( self.m_handle, string_view{ text, length } ) );
        return 1;
    }

    static int html_receive( lua_State *state )
    {
        const html_object_t                &self{ check_html( state ) };
        fixed_string< HTML_MESSAGE_LENGTH > message{ };

        if ( !g_html.receive( self.m_handle, message ) )
        {
            lua_pushnil( state );
            return 1;
        }

        lua_pushlstring( state, message.data( ), message.size( ) );
        return 1;
    }

    static int html_close( lua_State *state )
    {
        html_object_t &self{ check_html( state ) };

        g_html.close( self.m_handle );
        self.m_handle = html_handle_t{ };
        return 0;
    }

    ATLAS_FORCE_INLINE void open_html( lua_State *state )
    {
        lua_newtable( state );
        set_function( state, ATLAS_XORSTR( "__gc" ).get( ), &html_close );

        lua_newtable( state );
        set_function( state, ATLAS_XORSTR( "draw" ).get( ), &html_draw );
        set_function( state, ATLAS_XORSTR( "load" ).get( ), &html_load );
        set_function( state, ATLAS_XORSTR( "send" ).get( ), &html_send );
        set_function( state, ATLAS_XORSTR( "receive" ).get( ), &html_receive );
        set_function( state, ATLAS_XORSTR( "close" ).get( ), &html_close );
        lua_setfield( state, -2, ATLAS_XORSTR( "__index" ).get( ) );

        lua_rawsetp( state, LUA_REGISTRYINDEX, &s_html_key );

        lua_newtable( state );
        set_function( state, ATLAS_XORSTR( "html" ).get( ), &ui_html );
        set_function( state, ATLAS_XORSTR( "web" ).get( ), &ui_web );
        lua_setglobal( state, ATLAS_XORSTR( "ui" ).get( ) );
    }

    static bool is_create_move( event_t evt )
    {
        switch ( evt )
        {
            case event_t::create_move:
            case event_t::create_move_post_pred:
            case event_t::create_move_pre_pred: return true;
            default: return false;
        }
    }

    static int input_is_down( lua_State *state )
    {
        const u32 key{ static_cast< u32 >( luaL_checkinteger( state, 1 ) ) };
        lua_pushboolean( state, is_create_move( g_scripts.current( ) ) ? g_input.down_batch( key ) : g_input.down( key ) );
        return 1;
    }

    static int input_is_pressed( lua_State *state )
    {
        const u32 key{ static_cast< u32 >( luaL_checkinteger( state, 1 ) ) };
        lua_pushboolean( state, is_create_move( g_scripts.current( ) ) ? g_input.pressed_batch( key ) : g_input.pressed( key ) );
        return 1;
    }

    static int input_mouse( lua_State *state )
    {
        const point_t mouse{ g_input.mouse( ) };
        lua_pushinteger( state, mouse.x );
        lua_pushinteger( state, mouse.y );
        return 2;
    }

    static int input_menu_open( lua_State *state )
    {
        lua_pushboolean( state, g_gui.open( ) );
        return 1;
    }

    static int atlas_name( lua_State *state )
    {
        const string_view user{ g_gui.user( ) };
        lua_pushlstring( state, user.data( ), user.size( ) );
        return 1;
    }

    static int atlas_script( lua_State *state )
    {
        const string_view name{ host::name_of( state ) };
        lua_pushlstring( state, name.data( ), name.size( ) );
        return 1;
    }

    static int atlas_notify( lua_State *state )
    {
        const char   *text{ luaL_checkstring( state, 1 ) };
        notify_kind_t kind{ notify_kind_t::information };

        if ( !lua_isnoneornil( state, 2 ) )
        {
            switch ( HASH_RT( luaL_checkstring( state, 2 ) ) )
            {
                case HASH_CT( "info" ): kind = notify_kind_t::information; break;
                case HASH_CT( "success" ): kind = notify_kind_t::success; break;
                case HASH_CT( "warning" ): kind = notify_kind_t::warning; break;
                case HASH_CT( "error" ): kind = notify_kind_t::error; break;
                default: return luaL_argerror( state, 2, ATLAS_XORSTR( "kind is info, success, warning or error" ).get( ) );
            }
        }

        char              title[ NAME_LENGTH + 1 ]{ };
        const string_view name{ host::name_of( state ) };
        str::copy( title, sizeof( title ), name.data( ), name.size( ) );

        push_notification( kind, ICON_CODE, title, text );
        return 0;
    }

    static int atlas_time( lua_State *state )
    {
        lua_pushnumber( state, static_cast< lua_Number >( interrupt_time( ) ) / 1e7 );
        return 1;
    }

    void open_base( lua_State *state )
    {
        open_vec3( state );

        lua_pushcfunction( state, &color_pack );
        lua_setglobal( state, ATLAS_XORSTR( "color" ).get( ) );

        lua_newtable( state );
        set_function( state, ATLAS_XORSTR( "name" ).get( ), &atlas_name );
        set_function( state, ATLAS_XORSTR( "script" ).get( ), &atlas_script );
        set_function( state, ATLAS_XORSTR( "notify" ).get( ), &atlas_notify );
        set_function( state, ATLAS_XORSTR( "time" ).get( ), &atlas_time );
        lua_setglobal( state, ATLAS_XORSTR( "atlas" ).get( ) );

        lua_newtable( state );
        set_function( state, ATLAS_XORSTR( "line" ).get( ), &draw_line );
        set_function( state, ATLAS_XORSTR( "rect" ).get( ), &draw_rect );
        set_function( state, ATLAS_XORSTR( "rect_filled" ).get( ), &draw_rect_filled );
        set_function( state, ATLAS_XORSTR( "circle" ).get( ), &draw_circle );
        set_function( state, ATLAS_XORSTR( "text" ).get( ), &draw_text );
        lua_setglobal( state, ATLAS_XORSTR( "draw" ).get( ) );

        lua_newtable( state );
        set_function( state, ATLAS_XORSTR( "is_down" ).get( ), &input_is_down );
        set_function( state, ATLAS_XORSTR( "is_pressed" ).get( ), &input_is_pressed );
        set_function( state, ATLAS_XORSTR( "mouse" ).get( ), &input_mouse );
        set_function( state, ATLAS_XORSTR( "menu_open" ).get( ), &input_menu_open );
        lua_setglobal( state, ATLAS_XORSTR( "input" ).get( ) );

        open_html( state );
    }
} // namespace script
