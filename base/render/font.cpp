#include "font.hpp"

namespace render
{

    bool font_manager::init( gdi::api *api )
    {
        if ( !api || !api->Ready( ) )
        {
            return false;
        }

        this->m_api = api;
        return true;
    }

    void font_manager::shutdown( )
    {
        if ( this->m_api )
        {
            for ( usize index{ }; index < this->m_resource_count; ++index )
            {
                if ( this->m_resources[ index ].m_handle )
                {
                    this->m_api->RemoveFontMemResourceEx( this->m_resources[ index ].m_handle );
                }
            }
        }

        this->m_resources.reset( );

        this->m_resource_count = 0;
        this->m_api            = nullptr;
    }

    bool font_manager::register_memory( const char *name, const char *face, const u8 *data, u32 size,
                                        u32 pixel_size, i32 weight, bool crisp )
    {
        if ( !name || !face || !data || !size || !pixel_size || !this->ready( ) )
        {
            return false;
        }

        if ( const font_resource_t *existing{ this->find( name ) } )
        {
            return existing->m_handle != nullptr;
        }

        if ( this->m_resource_count >= this->m_resources.size( ) )
        {
            return false;
        }

        void *handle{ };
        u32   font_count{ };

        if ( !this->m_api->AddFontMemResourceEx( data, size, &handle, &font_count ) )
        {
            return false;
        }

        this->m_resources[ this->m_resource_count ] =
            font_resource_t{ name, face, data, size, pixel_size, weight, crisp, handle, font_count };
        ++this->m_resource_count;
        return true;
    }

    bool font_manager::register_system( const char *name, const char *face,
                                        u32 pixel_size, i32 weight, bool crisp )
    {
        if ( !name || !face || !pixel_size || !this->system_ready( ) )
        {
            return false;
        }

        if ( const font_resource_t *existing{ this->find( name ) } )
        {
            return !existing->m_data && !existing->m_size;
        }

        if ( this->m_resource_count >= this->m_resources.size( ) )
        {
            return false;
        }

        this->m_resources[ this->m_resource_count ] =
            font_resource_t{ name, face, nullptr, 0, pixel_size, weight, crisp, nullptr, 0 };
        ++this->m_resource_count;
        return true;
    }

    const font_resource_t *font_manager::font( const char *name ) const
    {
        return this->find( name );
    }

    const char *font_manager::face( const char *name ) const
    {
        const font_resource_t *resource{ this->find( name ) };
        return resource ? resource->m_face : nullptr;
    }

    font_resource_t *font_manager::find( const char *name )
    {
        if ( !name )
        {
            return nullptr;
        }

        for ( font_resource_t &resource : this->m_resources )
        {
            if ( resource.m_name && str::equals( resource.m_name, name ) )
            {
                return &resource;
            }
        }

        return nullptr;
    }

    const font_resource_t *font_manager::find( const char *name ) const
    {
        if ( !name )
        {
            return nullptr;
        }

        for ( const font_resource_t &resource : this->m_resources )
        {
            if ( resource.m_name && str::equals( resource.m_name, name ) )
            {
                return &resource;
            }
        }

        return nullptr;
    }
} // namespace render
