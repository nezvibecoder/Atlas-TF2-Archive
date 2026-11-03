#include "config_storage.hpp"

#include "api/api.hpp"

extern api g_api;

bool configs::valid_name( string_view name )
{
    if ( name.empty( ) || name.size( ) > 32 )
    {
        return false;
    }

    for ( usize i{ }; i < name.size( ); ++i )
    {
        const char c{ name.data( )[ i ] };
        if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '-' || c == '_' ) )
        {
            return false;
        }
    }

    if ( name == "con" || name == "prn" || name == "aux" || name == "nul" ||
         ( name.size( ) == 4 && ( name.substr( 0, 3 ) == "com" || name.substr( 0, 3 ) == "lpt" ) &&
           name.data( )[ 3 ] >= '1' && name.data( )[ 3 ] <= '9' ) )
    {
        return false;
    }

    return true;
}

#if defined( ATLAS_DISABLE_CLOUD )
static bool config_path( wchar_t ( &path )[ 1024 ], const char *name, const wchar_t *extension )
{
    u32 length{ I( GetModuleFileNameW )( nullptr, path, 900 ) };
    if ( !length || length >= 900 )
    {
        return false;
    }

    while ( length && path[ length - 1 ] != L'\\' )
    {
        --length;
    }

    const wchar_t folder[]{ L"atlas-configs" };
    for ( wchar_t c : folder )
    {
        path[ length++ ] = c;
    }

    I( CreateDirectoryW )( path, nullptr );

    path[ length - 1 ] = L'\\';
    while ( *name )
    {
        path[ length++ ] = static_cast< wchar_t >( *name++ );
    }

    do
    {
        path[ length++ ] = *extension;
    } while ( *extension++ );

    return true;
}

static bool config_read( const char *name, const wchar_t *extension, atl::vector< char > &output )
{
    wchar_t path[ 1024 ]{ };
    if ( !config_path( path, name, extension ) )
    {
        return false;
    }

    void *file{ I( CreateFileW )( path, 0x80000000u, 1, nullptr, 3, 0x80, nullptr ) };
    if ( file == reinterpret_cast< void * >( -1 ) )
    {
        return false;
    }

    LARGE_INTEGER size{ };
    DWORD         read{ };
    const bool    ok{ I( GetFileSizeEx )( file, &size ) && size.QuadPart >= 0 && size.QuadPart <= configs::max_bytes &&
                      output.resize( static_cast< usize >( size.QuadPart ) ) &&
                      I( ReadFile )( file, output.data( ), static_cast< u32 >( size.QuadPart ), &read, nullptr ) && read == size.QuadPart };
    I( CloseHandle )( file );
    return ok;
}

static bool config_write( const char *name, const wchar_t *extension, string_view input )
{
    wchar_t path[ 1024 ]{ };
    wchar_t temporary[ 1024 ]{ };
    if ( !config_path( path, name, extension ) || !config_path( temporary, name, L".tmp" ) )
    {
        return false;
    }

    void *file{ I( CreateFileW )( temporary, 0x40000000u, 0, nullptr, 2, 0x80, nullptr ) };
    if ( file == reinterpret_cast< void * >( -1 ) )
    {
        return false;
    }

    DWORD written{ };
    bool  ok{ I( WriteFile )( file, input.data( ), static_cast< u32 >( input.size( ) ), &written, nullptr ) &&
              written == input.size( ) && I( FlushFileBuffers )( file ) };

    I( CloseHandle )( file );

    ok = ok && I( MoveFileExW )( temporary, path, 0x9 );
    if ( !ok )
    {
        I( DeleteFileW )( temporary );
    }

    return ok;
}

bool configs::transfer( action operation, const char *name, string_view input, atl::vector< char > &output )
{
    output.clear( );
    if ( operation != action::list && !valid_name( name ) )
    {
        return false;
    }

    if ( operation == action::load )
    {
        return config_read( name, L".json", output );
    }

    if ( operation == action::save )
    {
        atl::vector< char > listing{ };
        json::document      document{ };
        if ( !transfer( action::list, "", { }, listing ) ||
             !document.parse( string_view{ listing.data( ), listing.size( ) } ) )
        {
            return false;
        }

        const json::value items{ document.root( )[ "configs" ] };
        bool              existing{ false };
        for ( usize i{ }; i < items.size( ); ++i )
        {
            existing |= items[ i ][ "name" ].as< string_view >( ) == name;
        }

        if ( !existing && items.size( ) >= max_configs )
        {
            return false;
        }

        return input.size( ) <= max_bytes && config_write( name, L".json", input ) &&
               config_write( "last", L".txt", name );
    }
    if ( operation == action::activate )
    {
        return config_write( "last", L".txt", name );
    }

    wchar_t path[ 1024 ]{ };
    if ( operation == action::remove )
    {
        if ( !config_path( path, name, L".json" ) || !I( DeleteFileW )( path ) )
        {
            return false;
        }

        atl::vector< char > last{ };
        if ( config_read( "last", L".txt", last ) && string_view{ last.data( ), last.size( ) } == name )
        {
            return config_write( "last", L".txt", { } );
        }

        return true;
    }
    if ( !config_path( path, "*", L".json" ) )
    {
        return false;
    }

    json::writer writer{ };
    writer.begin_object( );

    atl::vector< char > last{ };
    config_read( "last", L".txt", last );

    writer.member( "last_used", string_view{ last.data( ), last.size( ) } );
    writer.key( "configs" );
    writer.begin_array( );

    WIN32_FIND_DATAW data{ };
    void            *search{ I( FindFirstFileW )( path, &data ) };
    if ( search == reinterpret_cast< void * >( -1 ) && I( GetLastError )( ) != 2 )
    {
        return false;
    }

    if ( search != reinterpret_cast< void * >( -1 ) )
    {
        usize count{ };
        do
        {
            char  entry[ 38 ]{ };
            usize length{ };
            while ( length < 37 && data.cFileName[ length ] && data.cFileName[ length ] <= 127 )
            {
                entry[ length ] = static_cast< char >( data.cFileName[ length ] );
                ++length;
            }

            if ( ( data.dwFileAttributes & 0x10 ) || length < 6 || length > 37 || !str::equals( entry + length - 5, ".json" ) )
            {
                continue;
            }

            entry[ length - 5 ] = 0;

            if ( !valid_name( entry ) )
            {
                continue;
            }

            writer.begin_object( );
            writer.member( "name", string_view{ entry } );
            writer.end_object( );
            ++count;
        } while ( count < max_configs && I( FindNextFileW )( search, &data ) );
        I( FindClose )( search );
    }
    writer.end_array( );
    writer.end_object( );
    if ( !writer.ok( ) || !output.resize( writer.text( ).size( ) ) )
    {
        return false;
    }
    for ( usize i{ }; i < output.size( ); ++i )
    {
        output[ i ] = writer.text( ).data( )[ i ];
    }
    return true;
}
#else
bool configs::transfer( action operation, const char *name, string_view input, atl::vector< char > &output )
{
    output.clear( );
    if ( !g_api.has_config_token( ) || input.size( ) > max_bytes ||
         ( operation != action::list && !valid_name( name ) ) )
    {
        return false;
    }
    wchar_t path[ 96 ]{ L"/v1/configs" };
    usize   length{ 11 };
    if ( operation != action::list )
    {
        path[ length++ ] = L'/';
        for ( const char *c{ name }; *c; ++c )
        {
            path[ length++ ] = *c;
        }
    }
    if ( operation == action::activate )
    {
        for ( wchar_t c : L"/activate" )
        {
            path[ length++ ] = c;
        }
    }
    const wchar_t *verb{ };
    switch ( operation )
    {
        case action::save: verb = ATLAS_XORWSTR_STATIC( L"PUT" ); break;
        case action::remove: verb = ATLAS_XORWSTR_STATIC( L"DELETE" ); break;
        case action::activate: verb = ATLAS_XORWSTR_STATIC( L"POST" ); break;
        default: verb = ATLAS_XORWSTR_STATIC( L"GET" );
    }

    u32 status{ };
    return g_api.request( verb, path, input, output, max_bytes, status ) && status >= 200 && status < 300;
}
#endif
