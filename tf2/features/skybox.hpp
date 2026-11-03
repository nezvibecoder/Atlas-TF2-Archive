#pragma once

#include "link.hpp"
#include "runtime/atomic.hpp"

inline constexpr usize SKYBOX_MAX_SKIES{ 64 };
inline constexpr usize SKYBOX_NAME_MAX{ 64 };

namespace gui
{
    class dynamic_list_box;
}

class skybox
{
public:
    void bind_list( gui::dynamic_list_box *list )
    {
        this->m_list = list;
    }

    void request( )
    {
        this->m_requested.store( true );
    }

    void service( );
    void think( );

private:
    const char *name_for( usize index ) const;

    bool install_path( );
    void install_packs( const char *packs_root );
    void scan( );
    void insert( const char *file_name );
    void apply( const char *name ) const;
    bool precache( const char *name ) const;
    bool face( const char *name, const char *side, bool hdr ) const;

    char  m_names[ SKYBOX_MAX_SKIES ][ SKYBOX_NAME_MAX ]{ };
    usize m_found{ };

    gui::dynamic_list_box *m_list{ };

    atl::atomic< bool > m_requested{ false };
    atl::atomic< bool > m_ready{ false };

    char m_original[ SKYBOX_NAME_MAX ]{ };
    char m_applied[ SKYBOX_NAME_MAX ]{ };

    usize m_rejected{ };
};

extern skybox g_skybox;
