#pragma once

#include "math/vector.hpp"
#include "util/typedefs.hpp"

struct lua_State;

namespace script
{
    void open_base( lua_State *state );

    void  push_vec3( lua_State *state, const vec3 &value );
    vec3 *check_vec3( lua_State *state, int index );

    color_t check_color( lua_State *state, int index );

    void *new_object( lua_State *state, usize size, const void *key );
    void *check_object( lua_State *state, int index, const void *key, const char *type_name );

    void set_function( lua_State *state, const char *name, int ( *function )( lua_State * ) );
} // namespace script
