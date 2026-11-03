#pragma once

#include "util/json.hpp"

namespace configs
{
    inline constexpr usize max_configs{ 32 };
    inline constexpr usize max_bytes{ 256 * 1024 };

    enum class action : u8
    {
        list,
        load,
        save,
        remove,
        activate,
        _COUNT,
    };

    bool               valid_name( string_view name );
    bool               transfer( action operation, const char *name, string_view input, atl::vector< char > &output );
    void               startup( );
    bool               request( action operation, string_view name );
    bool               poll( bool &loaded );
    const char *const *names( );
    usize              count( );
    bool               has_name( usize index );
    usize              loaded_index( );
} // namespace configs
