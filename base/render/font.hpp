#pragma once

#include "gdi.hpp"
#include "runtime/array.hpp"
#include "util/string.hpp"

namespace render
{
    inline constexpr usize MAX_FONT_RESOURCES{ 24 };
    inline constexpr u32   DEFAULT_FONT_PIXEL_SIZE{ 15 };

    struct font_resource_t
    {
        const char *m_name{ };
        const char *m_face{ };
        const u8   *m_data{ };
        u32         m_size{ };
        u32         m_pixel_size{ DEFAULT_FONT_PIXEL_SIZE };
        i32         m_weight{ FW_NORMAL };
        bool        m_crisp{ };
        void       *m_handle{ };
        u32         m_font_count{ };
    };

    class font_manager
    {
    public:
        bool init( gdi::api *api );
        void shutdown( );

        bool                   register_memory( const char *name, const char *face, const u8 *data, u32 size,
                                                u32 pixel_size = DEFAULT_FONT_PIXEL_SIZE, i32 weight = FW_NORMAL,
                                                bool crisp = false );
        bool                   register_system( const char *name, const char *face,
                                                u32  pixel_size = DEFAULT_FONT_PIXEL_SIZE,
                                                i32  weight     = FW_NORMAL,
                                                bool crisp      = false );
        const font_resource_t *font( const char *name ) const;
        const char            *face( const char *name ) const;

        bool ready( ) const
        {
            return this->m_api && this->m_api->FontReady( );
        }

        bool system_ready( ) const
        {
            return this->m_api && this->m_api->Ready( );
        }

    private:
        font_resource_t       *find( const char *name );
        const font_resource_t *find( const char *name ) const;

        gdi::api                                         *m_api{ };
        atl::array< font_resource_t, MAX_FONT_RESOURCES > m_resources{ };
        usize                                             m_resource_count{ };
    };
} // namespace render
