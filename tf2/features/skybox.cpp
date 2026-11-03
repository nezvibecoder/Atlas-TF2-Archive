#include "skybox.hpp"

#include "gui/gui.hpp"
#include "util/string.hpp"

ATLAS_NO_DESTROY skybox g_skybox{ };

static bool is_hdr_variant( const char *name, usize length )
{
    return length > 4 && str::equals( name + length - 4, ATLAS_XORSTR( "_hdr" ).get( ) );
}

// trims the last component, so "...\Team Fortress 2\tf\gameinfo.txt" walks up to the install
// root a component at a time
static bool trim_component( char *path )
{
    usize last{ str::npos };

    for ( usize index{ }; path[ index ]; ++index )
    {
        if ( path[ index ] == '\\' || path[ index ] == '/' )
        {
            last = index;
        }
    }

    if ( last == str::npos )
    {
        return false;
    }

    path[ last ] = '\0';
    return true;
}

static bool append( char *path, usize capacity, const char *component )
{
    return str::append( path, capacity, ATLAS_XORSTR( "\\" ).get( ) ) &&
           str::append( path, capacity, component );
}

// the filesystem keeps the path ID pointer, so it has to outlive the call
#define PACKS_PATH_ID ( ATLAS_XORSTR_STATIC( "ATLAS_SKYBOXES" ) )

void skybox::service( )
{
    if ( !this->m_requested.load( ) || this->m_ready.load( ) )
    {
        return;
    }

    this->m_found = 0;

    if ( g_tf2.m_filesystem && this->install_path( ) )
    {
        this->scan( );
    }
    
    if ( this->m_list && this->m_list->insert_element( ATLAS_XORSTR_STATIC( "Map default" ) ) )
    {
        for ( usize index{ }; index < this->m_found; ++index )
        {
            if ( !this->m_list->insert_element( this->m_names[ index ] ) )
            {
                break;
            }
        }
    }

    this->m_ready.store( true );
}

bool skybox::install_path( )
{
    char root[ 260 ]{ };

    // gameinfo.txt is the one file guaranteed to sit at the top of the mod directory, so it is the
    // anchor the install root is derived from: strip the file, then strip "tf"
    if ( !g_tf2.m_filesystem->RelativePathToFullPath( ATLAS_XORSTR( "gameinfo.txt" ).get( ),
                                                      ATLAS_XORSTR( "MOD" ).get( ), root,
                                                      sizeof( root ) ) )
    {
        return false;
    }

    if ( !trim_component( root ) || !trim_component( root ) )
    {
        return false;
    }

    if ( !append( root, sizeof( root ), ATLAS_XORSTR( "Atlas" ).get( ) ) ||
         !append( root, sizeof( root ), ATLAS_XORSTR( "skyboxes" ).get( ) ) )
    {
        return false;
    }

    g_tf2.m_filesystem->CreateDirHierarchy( root, nullptr );
    g_tf2.m_filesystem->AddSearchPath( root, PACKS_PATH_ID, PATH_ADD_TO_HEAD );
    this->install_packs( root );

    return true;
}

// one folder per skybox
void skybox::install_packs( const char *packs_root )
{
    FileFindHandle_t handle{ };

    const char *found{ g_tf2.m_filesystem->FindFirstEx( ATLAS_XORSTR( "*" ).get( ),
                                                        PACKS_PATH_ID, &handle ) };

    while ( found )
    {
        if ( found[ 0 ] != '.' && g_tf2.m_filesystem->FindIsDirectory( handle ) )
        {
            char pack[ 260 ]{ };

            if ( str::copy( pack, sizeof( pack ), packs_root ) &&
                 append( pack, sizeof( pack ), found ) )
            {
                // ahead of the game's own paths, so a pack can replace a stock sky by name
                g_tf2.m_filesystem->AddSearchPath( pack, ATLAS_XORSTR( "GAME" ).get( ),
                                                   PATH_ADD_TO_HEAD );
            }
        }

        found = g_tf2.m_filesystem->FindNext( handle );
    }

    g_tf2.m_filesystem->FindClose( handle );
}

void skybox::scan( )
{
    FileFindHandle_t handle{ };

    const char *found{ g_tf2.m_filesystem->FindFirstEx(
        ATLAS_XORSTR( "materials/skybox/*bk.vmt" ).get( ), ATLAS_XORSTR( "GAME" ).get( ),
        &handle ) };

    while ( found )
    {
        this->insert( found );
        found = g_tf2.m_filesystem->FindNext( handle );
    }

    g_tf2.m_filesystem->FindClose( handle );
}

void skybox::insert( const char *file_name )
{
    const usize file_length{ str::length( file_name ) };

    if ( file_length <= 6 || this->m_found >= SKYBOX_MAX_SKIES )
    {
        return;
    }

    char name[ SKYBOX_NAME_MAX ]{ };

    if ( !str::copy( name, sizeof( name ), file_name, file_length - 6 ) )
    {
        return;
    }

    if ( is_hdr_variant( name, file_length - 6 ) )
    {
        return;
    }

    usize at{ };
    while ( at < this->m_found )
    {
        const i32 order{ str::compare( this->m_names[ at ], name ) };

        if ( order == 0 )
        {
            return;
        }

        if ( order > 0 )
        {
            break;
        }

        ++at;
    }

    for ( usize index{ this->m_found }; index > at; --index )
    {
        str::copy( this->m_names[ index ], SKYBOX_NAME_MAX, this->m_names[ index - 1 ] );
    }

    str::copy( this->m_names[ at ], SKYBOX_NAME_MAX, name );

    ++this->m_found;
}

const char *skybox::name_for( usize index ) const
{
    if ( index == 0 || index > this->m_found )
    {
        return nullptr;
    }

    return this->m_names[ index - 1 ];
}

void skybox::think( )
{
    if ( !g_tf2.sv_skyname || !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    const char *current{ g_tf2.sv_skyname->GetString( ) };

    if ( !current )
    {
        return;
    }

    if ( !str::equals( current, this->m_applied ) )
    {
        str::copy( this->m_original, sizeof( this->m_original ), current );
    }

    const char *wanted{ this->name_for( g_cfg.m_skybox.m_index ) };

    if ( !wanted )
    {
        if ( this->m_applied[ 0 ] )
        {
            this->apply( this->m_original );
            this->m_applied[ 0 ] = '\0';
        }

        return;
    }

    if ( str::equals( current, wanted ) || g_cfg.m_skybox.m_index == this->m_rejected )
    {
        return;
    }

    if ( !this->precache( wanted ) )
    {
        this->m_rejected = g_cfg.m_skybox.m_index;
        return;
    }

    this->m_rejected = 0;

    this->apply( wanted );
    str::copy( this->m_applied, sizeof( this->m_applied ), wanted );
}

void skybox::apply( const char *name ) const
{
    g_tf2.sv_skyname->SetValue( name );
    g_tf2.m_R_InitSky.call_as< void >( );
}

bool skybox::face( const char *name, const char *side, bool hdr ) const
{
    char path[ SKYBOX_NAME_MAX + 16 ]{ };

    if ( !str::copy( path, sizeof( path ), ATLAS_XORSTR( "skybox/" ).get( ) ) ||
         !str::append( path, sizeof( path ), name ) ||
         ( hdr && !str::append( path, sizeof( path ), ATLAS_XORSTR( "_hdr" ).get( ) ) ) ||
         !str::append( path, sizeof( path ), side ) )
    {
        return false;
    }

    IMaterial *material{ g_tf2.m_material_system->FindMaterial(
        path, ATLAS_XORSTR( "SkyBox textures" ).get( ), false ) };

    if ( !material || material->IsErrorMaterial( ) )
    {
        return false;
    }

    if ( !material->IsPrecached( ) )
    {
        material->Precache( );
    }

    return true;
}

bool skybox::precache( const char *name ) const
{
    static const char *const SIDES[]{ "rt", "bk", "lf", "ft", "up", "dn" };

    bool complete{ true };

    for ( const char *side : SIDES )
    {
        const bool hdr{ this->face( name, side, true ) };

        if ( !this->face( name, side, false ) && !hdr )
        {
            complete = false;
        }
    }

    return complete;
}
