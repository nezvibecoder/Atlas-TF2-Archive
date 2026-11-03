#include "script.hpp"

#include "bind.hpp"

#include "gui/notifications.hpp"
#include "gui/theme.hpp"
#include "runtime/hash.hpp"
#include "runtime/memory.hpp"
#include "runtime/thread.hpp"

#if !defined( ATLAS_DISABLE_CLOUD )
#include "api/api.hpp"
#include "util/base64.hpp"
#include "util/json.hpp"
#endif

#include "libs/lua/lauxlib.h"
#include "libs/lua/lua.h"
#include "libs/lua/lualib.h"

ATLAS_NO_DESTROY constinit script::host g_scripts{ };

#if !defined( ATLAS_DISABLE_CLOUD )
extern api g_api;
#endif

namespace script
{
    inline constexpr u64 CALL_BUDGET{ 250ull * 10000ull };

    inline constexpr int HOOK_INTERVAL{ 10000 };

    static char s_handlers_key{ };

    u64 interrupt_time( )
    {
        const volatile u32 *time{ reinterpret_cast< const volatile u32 * >( 0x7FFE0008 ) };

        for ( ;; )
        {
            const u32 high{ time[ 1 ] };
            const u32 low{ time[ 0 ] };

            if ( high == time[ 2 ] )
            {
                return ( static_cast< u64 >( high ) << 32 ) | low;
            }
        }
    }

    static int traceback( lua_State *state )
    {
        const char *message{ lua_tostring( state, 1 ) };

        if ( !message )
        {
            message = luaL_tolstring( state, 1, nullptr );
        }

        luaL_traceback( state, state, message, 1 );
        return 1;
    }

    bool valid_name( string_view name )
    {
        if ( name.empty( ) || name.size( ) > NAME_LENGTH )
        {
            return false;
        }

        for ( const char value : name )
        {
            if ( !( value >= 'a' && value <= 'z' ) && !str::is_digit( value ) && value != '-' && value != '_' )
            {
                return false;
            }
        }

        char stem_text[ 4 ]{ };
        str::copy( stem_text, sizeof( stem_text ), name.data( ), name.size( ) < 3 ? name.size( ) : 3 );

        const hash_t stem{ HASH_RT( stem_text ) };
        const bool   device{ ( name.size( ) == 3 && ( stem == HASH_CT( "con" ) || stem == HASH_CT( "prn" ) ||
                                                      stem == HASH_CT( "aux" ) || stem == HASH_CT( "nul" ) ) ) ||
                             ( name.size( ) == 4 && ( stem == HASH_CT( "com" ) || stem == HASH_CT( "lpt" ) ) &&
                               str::is_digit( name[ 3 ] ) ) };

        return !device;
    }

    ATLAS_FORCE_INLINE bool find_event( const char *name, event_t &event )
    {
        switch ( HASH_RT( name ) )
        {
            case HASH_CT( "paint" ): event = event_t::paint; return true;
            case HASH_CT( "create_move_pre_pred" ): event = event_t::create_move_pre_pred; return true;
            case HASH_CT( "create_move" ): event = event_t::create_move; return true;
            case HASH_CT( "create_move_post_pred" ): event = event_t::create_move_post_pred; return true;
            case HASH_CT( "frame_stage" ): event = event_t::frame_stage; return true;
            case HASH_CT( "level_init" ): event = event_t::level_init; return true;
            case HASH_CT( "level_shutdown" ): event = event_t::level_shutdown; return true;
            case HASH_CT( "unload" ): event = event_t::unload; return true;
            default: return false;
        }
    }

    host::instance_t *host::owner( lua_State *state )
    {
        return *static_cast< instance_t ** >( lua_getextraspace( state ) );
    }

    string_view host::name_of( lua_State *state )
    {
        return owner( state )->m_name.view( );
    }

    void *host::allocate( void *user, void *block, usize old_size, usize new_size )
    {
        instance_t &instance{ *static_cast< instance_t * >( user ) };

        if ( !block )
        {
            old_size = 0;
        }

        if ( !new_size )
        {
            if ( block )
            {
                I( HeapFree )( instance.m_heap, 0, block );
            }

            instance.m_used -= old_size;
            return nullptr;
        }

        if ( new_size > old_size && instance.m_used + ( new_size - old_size ) > MEMORY_LIMIT )
        {
            return nullptr;
        }

        void *moved{ block ? I( HeapReAlloc )( instance.m_heap, 0, block, new_size )
                           : I( HeapAlloc )( instance.m_heap, 0, new_size ) };

        if ( moved )
        {
            instance.m_used = instance.m_used - old_size + new_size;
        }

        return moved;
    }

    void host::budget_hook( lua_State *state, lua_Debug * )
    {
        if ( interrupt_time( ) > owner( state )->m_deadline )
        {
            luaL_error( state, ATLAS_XORSTR( "script exceeded its time budget" ).get( ) );
        }
    }

    int host::on_handler( lua_State *state )
    {
        const char *name{ luaL_checkstring( state, 1 ) };
        luaL_checktype( state, 2, LUA_TFUNCTION );

        event_t event{ };
        if ( !find_event( name, event ) )
        {
            return luaL_error( state, ATLAS_XORSTR( "unknown event '%s'" ).get( ), name );
        }

        lua_rawgetp( state, LUA_REGISTRYINDEX, &s_handlers_key );
        lua_rawgeti( state, -1, static_cast< lua_Integer >( event ) + 1 );
        lua_pushvalue( state, 2 );
        lua_rawseti( state, -2, static_cast< lua_Integer >( lua_rawlen( state, -2 ) ) + 1 );

        owner( state )->m_handlers |= 1u << static_cast< u32 >( event );
        return 0;
    }

    int host::on_print( lua_State *state )
    {
        const int   count{ lua_gettop( state ) };
        luaL_Buffer buffer;
        luaL_buffinit( state, &buffer );

        for ( int index{ 1 }; index <= count; ++index )
        {
            if ( index > 1 )
            {
                luaL_addchar( &buffer, ' ' );
            }

            luaL_tolstring( state, index, nullptr );
            luaL_addvalue( &buffer );
        }

        luaL_pushresult( &buffer );

        usize       length{ };
        const char *text{ lua_tolstring( state, -1, &length ) };
        g_scripts.log( log_kind_t::print, string_view{ text, length } );
        return 0;
    }

    int host::on_load( lua_State *state )
    {
        usize       size{ };
        const char *chunk{ luaL_checklstring( state, 1, &size ) };
        const char *chunk_name{ luaL_optstring( state, 2, chunk ) };

        if ( luaL_loadbufferx( state, chunk, size, chunk_name, "t" ) != LUA_OK )
        {
            lua_pushnil( state );
            lua_insert( state, -2 );
            return 2;
        }

        if ( !lua_isnoneornil( state, 4 ) )
        {
            lua_pushvalue( state, 4 );

            if ( !lua_setupvalue( state, -2, 1 ) )
            {
                lua_pop( state, 1 );
            }
        }

        return 1;
    }

    void host::on_warning( void *, const char *message, int )
    {
        if ( message[ 0 ] != '@' )
        {
            g_scripts.log( log_kind_t::info, message );
        }
    }

    int host::setup( lua_State *state )
    {
        host &self{ *static_cast< host * >( lua_touserdata( state, 1 ) ) };

        luaL_requiref( state, LUA_GNAME, &luaopen_base, 1 );
        luaL_requiref( state, LUA_COLIBNAME, &luaopen_coroutine, 1 );
        luaL_requiref( state, LUA_TABLIBNAME, &luaopen_table, 1 );
        luaL_requiref( state, LUA_STRLIBNAME, &luaopen_string, 1 );
        luaL_requiref( state, LUA_MATHLIBNAME, &luaopen_math, 1 );
        luaL_requiref( state, LUA_UTF8LIBNAME, &luaopen_utf8, 1 );
        lua_settop( state, 1 );

        lua_pushglobaltable( state );
        lua_pushnil( state );
        lua_setfield( state, -2, ATLAS_XORSTR( "dofile" ).get( ) );
        lua_pushnil( state );
        lua_setfield( state, -2, ATLAS_XORSTR( "loadfile" ).get( ) );
        set_function( state, ATLAS_XORSTR( "load" ).get( ), &host::on_load );
        set_function( state, ATLAS_XORSTR( "print" ).get( ), &host::on_print );
        lua_pop( state, 1 );

        lua_createtable( state, static_cast< int >( event_t::_COUNT ), 0 );
        for ( usize event{ }; event < static_cast< usize >( event_t::_COUNT ); ++event )
        {
            lua_newtable( state );
            lua_rawseti( state, -2, static_cast< lua_Integer >( event ) + 1 );
        }
        lua_rawsetp( state, LUA_REGISTRYINDEX, &s_handlers_key );

        open_base( state );

        lua_getglobal( state, ATLAS_XORSTR( "atlas" ).get( ) );
        set_function( state, ATLAS_XORSTR( "on" ).get( ), &host::on_handler );
        lua_pop( state, 1 );

        self.m_open_game( state );
        return 0;
    }

    extern "C" void decrypt_lstr( );

    void host::startup( open_fn open_game )
    {
        decrypt_lstr( );
        this->m_open_game = open_game;
        this->request_store( store_kind_t::list );
    }

    void host::shutdown( )
    {
        for ( instance_t &instance : this->m_instances )
        {
            if ( instance.m_state )
            {
                this->unload( instance );
            }
        }
    }

    event_t host::current( ) const
    {
        return this->m_current;
    }

    void host::push_request( request_t &&request )
    {
        atl::scoped_lock lock{ this->m_request_lock };
        this->m_requests.push_back( static_cast< request_t && >( request ) );
    }

    void host::request_run( string_view name, string_view source )
    {
        request_t request{ request_kind_t::run };
        request.m_name.assign( name );
        request.m_source.assign( source.data( ), source.size( ) < SOURCE_LIMIT ? source.size( ) : SOURCE_LIMIT );
        this->push_request( static_cast< request_t && >( request ) );
    }

    void host::request_unload( string_view name )
    {
        request_t request{ request_kind_t::unload };
        request.m_name.assign( name );
        this->push_request( static_cast< request_t && >( request ) );
    }

    void host::request_unload_all( )
    {
        this->push_request( request_t{ request_kind_t::unload_all } );
    }

    void host::service( )
    {
        atl::vector< request_t > requests{ };

        {
            atl::scoped_lock lock{ this->m_request_lock };
            requests.swap( this->m_requests );
        }

        for ( request_t &request : requests )
        {
            switch ( request.m_kind )
            {
                case request_kind_t::run:
                {
                    this->load( request.m_name.view( ), string_view{ request.m_source.data( ), request.m_source.size( ) } );
                    break;
                }

                case request_kind_t::unload:
                {
                    for ( instance_t &instance : this->m_instances )
                    {
                        if ( instance.m_state && instance.m_name.view( ) == request.m_name.view( ) )
                        {
                            this->unload( instance );
                        }
                    }
                    break;
                }

                case request_kind_t::unload_all: this->shutdown( ); break;
                default: break;
            }
        }
    }

    bool host::load( string_view name, string_view source )
    {
        for ( instance_t &instance : this->m_instances )
        {
            if ( instance.m_state && instance.m_name.view( ) == name )
            {
                this->unload( instance );
            }
        }

        instance_t *slot{ };
        for ( instance_t &instance : this->m_instances )
        {
            if ( !instance.m_state )
            {
                slot = &instance;
                break;
            }
        }

        if ( !slot )
        {
            this->log( log_kind_t::error, ATLAS_XORSTR( "every script slot is in use" ).get( ) );
            return false;
        }

        slot->m_heap = I( HeapCreate )( HEAP_NO_SERIALIZE, 0, 0 );
        slot->m_name.assign( name );

        lua_State *state{ slot->m_heap ? lua_newstate( &host::allocate, slot, luaL_makeseed( nullptr ) ) : nullptr };

        if ( !state )
        {
            this->close( *slot );
            this->log( log_kind_t::error, ATLAS_XORSTR( "could not create a lua state" ).get( ) );
            return false;
        }

        slot->m_state                                               = state;
        *static_cast< instance_t ** >( lua_getextraspace( state ) ) = slot;

        lua_setwarnf( state, &host::on_warning, nullptr );
        lua_sethook( state, &host::budget_hook, LUA_MASKCOUNT, HOOK_INTERVAL );

        lua_pushcfunction( state, &host::setup );
        lua_pushlightuserdata( state, this );

        if ( !this->call( state, 1 ) )
        {
            this->close( *slot );
            return false;
        }

        char chunk_name[ NAME_LENGTH + 8 ]{ '@' };
        str::append( chunk_name, sizeof( chunk_name ), name.data( ), name.size( ) );
        str::append( chunk_name, sizeof( chunk_name ), ATLAS_XORSTR( ".lua" ).get( ) );

        if ( luaL_loadbufferx( state, source.data( ), source.size( ), chunk_name, "t" ) != LUA_OK )
        {
            this->fail( *slot, lua_tostring( state, -1 ) );
            this->close( *slot );
            return false;
        }

        if ( !this->call( state, 0 ) )
        {
            this->close( *slot );
            return false;
        }

        char message[ NAME_LENGTH + 16 ]{ };
        str::copy( message, sizeof( message ), name.data( ), name.size( ) );
        str::append( message, sizeof( message ), ATLAS_XORSTR( " loaded" ).get( ) );
        this->log( log_kind_t::info, message );

        this->publish_names( );
        return true;
    }

    bool host::call( lua_State *state, i32 arguments )
    {
        instance_t &instance{ *owner( state ) };
        const int   base{ lua_gettop( state ) - arguments };

        lua_pushcfunction( state, &traceback );
        lua_insert( state, base );

        instance.m_deadline = interrupt_time( ) + CALL_BUDGET;

        const int status{ lua_pcall( state, arguments, 0, base ) };
        lua_remove( state, base );

        if ( status == LUA_OK )
        {
            return true;
        }

        this->fail( instance, lua_type( state, -1 ) == LUA_TSTRING ? lua_tostring( state, -1 ) : nullptr );
        lua_pop( state, 1 );
        return false;
    }

    void host::fail( instance_t &instance, const char *message )
    {
        char text[ 4096 ]{ };
        str::copy( text, sizeof( text ), instance.m_name.c_str( ) );
        str::append( text, sizeof( text ), ATLAS_XORSTR( ": " ).get( ) );
        str::append( text, sizeof( text ), message ? message : ATLAS_XORSTR( "error object is not a string" ).get( ) );

        this->log( log_kind_t::error, text );

        usize first{ };
        while ( text[ first ] && text[ first ] != '\n' )
        {
            ++first;
        }
        text[ first ] = '\0';

        push_notification( notify_kind_t::error, ICON_CODE, ATLAS_XORSTR( "Lua" ).get( ), text );
        instance.m_doomed = true;
    }

    void host::run_handlers( instance_t &instance, event_t event, push_fn push )
    {
        if ( !( instance.m_handlers & ( 1u << static_cast< u32 >( event ) ) ) )
        {
            return;
        }

        lua_State *state{ instance.m_state };

        lua_rawgetp( state, LUA_REGISTRYINDEX, &s_handlers_key );
        lua_rawgeti( state, -1, static_cast< lua_Integer >( event ) + 1 );

        const lua_Integer count{ static_cast< lua_Integer >( lua_rawlen( state, -1 ) ) };

        for ( lua_Integer index{ 1 }; index <= count; ++index )
        {
            lua_rawgeti( state, -1, index );

            if ( !this->call( state, push ? push( state ) : 0 ) )
            {
                break;
            }
        }

        lua_settop( state, 0 );
    }

    void host::dispatch( event_t event, push_fn push )
    {
        this->m_current = event;

        for ( instance_t &instance : this->m_instances )
        {
            if ( instance.m_state && !instance.m_doomed )
            {
                this->run_handlers( instance, event, push );
            }
        }

        this->m_current = event_t::_COUNT;
        this->collect( );
    }

    void host::collect( )
    {
        for ( instance_t &instance : this->m_instances )
        {
            if ( instance.m_state && instance.m_doomed )
            {
                this->close( instance );
            }
        }
    }

    void host::unload( instance_t &instance )
    {
        this->m_current = event_t::unload;
        this->run_handlers( instance, event_t::unload, nullptr );
        this->m_current = event_t::_COUNT;

        char message[ NAME_LENGTH + 16 ]{ };
        str::copy( message, sizeof( message ), instance.m_name.c_str( ) );
        str::append( message, sizeof( message ), ATLAS_XORSTR( " unloaded" ).get( ) );
        this->log( log_kind_t::info, message );

        this->close( instance );
    }

    void host::close( instance_t &instance )
    {
        if ( instance.m_state )
        {
            instance.m_deadline = interrupt_time( ) + CALL_BUDGET;
            lua_close( instance.m_state );
        }

        if ( instance.m_heap )
        {
            I( HeapDestroy )( instance.m_heap );
        }

        instance = instance_t{ };
        this->publish_names( );
    }

    void host::publish_names( )
    {
        atl::scoped_lock lock{ this->m_names_lock };
        this->m_loaded_count = 0;

        for ( const instance_t &instance : this->m_instances )
        {
            if ( instance.m_state && !instance.m_doomed )
            {
                this->m_loaded[ this->m_loaded_count++ ] = instance.m_name;
            }
        }
    }

    bool host::is_loaded( string_view name ) const
    {
        atl::scoped_lock lock{ this->m_names_lock };

        for ( usize index{ }; index < this->m_loaded_count; ++index )
        {
            if ( this->m_loaded[ index ].view( ) == name )
            {
                return true;
            }
        }

        return false;
    }

    usize host::loaded_names( name_t *output, usize capacity ) const
    {
        atl::scoped_lock lock{ this->m_names_lock };
        usize            count{ };

        for ( ; count < this->m_loaded_count && count < capacity; ++count )
        {
            output[ count ] = this->m_loaded[ count ];
        }

        return count;
    }

    void host::log( log_kind_t kind, string_view text )
    {
        atl::scoped_lock lock{ this->m_log_lock };
        usize            start{ };

        for ( ;; )
        {
            usize end{ start };
            while ( end < text.size( ) && text[ end ] != '\n' )
            {
                ++end;
            }

            const usize slot{ ( this->m_log_head + this->m_log_count ) % LOG_CAPACITY };

            if ( this->m_log_count == LOG_CAPACITY )
            {
                this->m_log_head = ( this->m_log_head + 1 ) % LOG_CAPACITY;
            } else
            {
                ++this->m_log_count;
            }

            log_line_t &line{ this->m_log[ slot ] };
            line.m_kind = kind;
            line.m_text.clear( );

            for ( usize index{ start }; index < end && line.m_text.size( ) < LOG_LINE_LENGTH; ++index )
            {
                const char value{ text[ index ] };

                if ( value == '\t' )
                {
                    line.m_text.append( ATLAS_XORSTR( "    " ).get( ) );
                } else if ( value != '\r' )
                {
                    line.m_text.push_back( value );
                }
            }

            if ( end >= text.size( ) )
            {
                break;
            }

            start = end + 1;
        }

        ++this->m_log_serial;
    }

    void host::clear_log( )
    {
        atl::scoped_lock lock{ this->m_log_lock };
        this->m_log_head  = 0;
        this->m_log_count = 0;
        ++this->m_log_serial;
    }

#if defined( ATLAS_DISABLE_CLOUD )
    inline constexpr usize PATH_CAPACITY{ 1024 };

    ATLAS_FORCE_INLINE usize script_folder( wchar_t ( &path )[ PATH_CAPACITY ] )
    {
        u32 length{ I( GetModuleFileNameW )( nullptr, path, PATH_CAPACITY - 64 ) };

        if ( !length || length >= PATH_CAPACITY - 64 )
        {
            return 0;
        }

        while ( length && path[ length - 1 ] != L'\\' )
        {
            --length;
        }

        path[ length ] = L'\0';
        str::append( path, PATH_CAPACITY, ATLAS_XORWSTR( L"atlas-scripts" ).get( ) );
        I( CreateDirectoryW )( path, nullptr );

        length           = str::length( path );
        path[ length++ ] = L'\\';
        path[ length ]   = L'\0';
        return length;
    }

    ATLAS_FORCE_INLINE bool script_path( wchar_t ( &path )[ PATH_CAPACITY ], string_view name, const wchar_t *extension )
    {
        if ( !valid_name( name ) )
        {
            return false;
        }

        usize length{ script_folder( path ) };

        if ( !length )
        {
            return false;
        }

        for ( const char value : name )
        {
            path[ length++ ] = static_cast< wchar_t >( value );
        }

        path[ length ] = L'\0';
        str::append( path, PATH_CAPACITY, extension );
        return true;
    }

    ATLAS_FORCE_INLINE bool store_ready( )
    {
        return true;
    }

    ATLAS_FORCE_INLINE const char *store_failure( u32, const char *local )
    {
        return local;
    }

    ATLAS_FORCE_INLINE bool store_list( atl::array< name_t, FILE_CAPACITY > &files, usize &count, u32 &status )
    {
        status = 0;
        count  = 0;

        wchar_t pattern[ PATH_CAPACITY ]{ };
        if ( !script_folder( pattern ) )
        {
            return false;
        }

        str::append( pattern, PATH_CAPACITY, ATLAS_XORWSTR( L"*.lua" ).get( ) );

        WIN32_FIND_DATAW found{ };
        void            *search{ I( FindFirstFileW )( pattern, &found ) };

        if ( search == INVALID_HANDLE_VALUE )
        {
            return true;
        }

        do
        {
            const usize length{ str::length( found.cFileName ) };
            if ( length < 5 || !str::ends_with( found.cFileName, length, ATLAS_XORWSTR( L".lua" ).get( ), 4 ) )
            {
                continue;
            }

            char  name[ NAME_LENGTH + 1 ]{ };
            usize size{ };
            bool  ascii{ length - 4 <= NAME_LENGTH };

            for ( ; ascii && size < length - 4; ++size )
            {
                ascii        = found.cFileName[ size ] < 0x80;
                name[ size ] = static_cast< char >( found.cFileName[ size ] );
            }

            if ( !ascii || !valid_name( string_view{ name, size } ) )
            {
                continue;
            }

            usize slot{ count };
            while ( slot && str::compare_ignore_case( files[ slot - 1 ].c_str( ), name ) > 0 )
            {
                files[ slot ] = files[ slot - 1 ];
                --slot;
            }

            files[ slot ].assign( string_view{ name, size } );
            ++count;
        } while ( count < FILE_CAPACITY && I( FindNextFileW )( search, &found ) );

        I( FindClose )( search );
        return true;
    }

    ATLAS_FORCE_INLINE bool store_get( string_view name, atl::vector< char > &source, u32 &status )
    {
        status = 0;

        wchar_t path[ PATH_CAPACITY ]{ };
        if ( !script_path( path, name, ATLAS_XORWSTR( L".lua" ).get( ) ) )
        {
            return false;
        }

        void *file{ I( CreateFileW )( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr ) };
        if ( file == INVALID_HANDLE_VALUE )
        {
            return false;
        }

        LARGE_INTEGER size{ };
        DWORD         read{ };
        const bool    ok{ I( GetFileSizeEx )( file, &size ) && size.QuadPart >= 0 && size.QuadPart <= static_cast< i64 >( SOURCE_LIMIT ) &&
                          source.resize( static_cast< usize >( size.QuadPart ) ) &&
                          I( ReadFile )( file, source.data( ), static_cast< DWORD >( size.QuadPart ), &read, nullptr ) && read == size.QuadPart };

        I( CloseHandle )( file );
        return ok;
    }

    ATLAS_FORCE_INLINE bool store_put( string_view name, string_view source, u32 &status )
    {
        status = 0;

        wchar_t path[ PATH_CAPACITY ]{ };
        wchar_t temporary[ PATH_CAPACITY ]{ };
        if ( source.size( ) > SOURCE_LIMIT || !script_path( path, name, ATLAS_XORWSTR( L".lua" ).get( ) ) ||
             !script_path( temporary, name, ATLAS_XORWSTR( L".tmp" ).get( ) ) )
        {
            return false;
        }

        void *file{ I( CreateFileW )( temporary, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr ) };
        if ( file == INVALID_HANDLE_VALUE )
        {
            return false;
        }

        DWORD written{ };
        bool  ok{ I( WriteFile )( file, source.data( ), static_cast< DWORD >( source.size( ) ), &written, nullptr ) &&
                  written == source.size( ) && I( FlushFileBuffers )( file ) };

        I( CloseHandle )( file );

        ok = ok && I( MoveFileExW )( temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH );
        if ( !ok )
        {
            I( DeleteFileW )( temporary );
        }

        return ok;
    }
#else
    inline constexpr usize URL_CAPACITY{ 64 };
    inline constexpr usize LIST_REPLY_LIMIT{ 64u * 1024u };
    inline constexpr usize SOURCE_REPLY_LIMIT{ ( SOURCE_LIMIT + 2 ) / 3 * 4 + 64 };

    ATLAS_FORCE_INLINE bool store_ready( )
    {
        return g_api.has_config_token( );
    }

    ATLAS_FORCE_INLINE const char *store_failure( u32 status, const char * )
    {
        switch ( status )
        {
            case 0: return ATLAS_XORSTR_STATIC( "Could not reach Cloud." );
            case 401:
            case 403: return ATLAS_XORSTR_STATIC( "Cloud session rejected." );
            case 404: return ATLAS_XORSTR_STATIC( "Script not found in Cloud." );
            case 409: return ATLAS_XORSTR_STATIC( "Cloud already holds 64 scripts." );
            case 413: return ATLAS_XORSTR_STATIC( "This script is too large for Cloud." );
            case 429: return ATLAS_XORSTR_STATIC( "Cloud rate limit reached." );
            default: return ATLAS_XORSTR_STATIC( "Cloud request failed." );
        }
    }

    ATLAS_FORCE_INLINE void script_url( wchar_t ( &url )[ URL_CAPACITY ], string_view name )
    {
        url[ 0 ] = L'\0';
        str::append( url, URL_CAPACITY, ATLAS_XORWSTR( L"/v1/scripts" ).get( ) );

        usize cursor{ str::length( url ) };

        if ( !name.empty( ) )
        {
            url[ cursor++ ] = L'/';

            for ( const char value : name )
            {
                url[ cursor++ ] = static_cast< wchar_t >( value );
            }
        }

        url[ cursor ] = L'\0';
    }

    ATLAS_FORCE_INLINE bool store_list( atl::array< name_t, FILE_CAPACITY > &files, usize &count, u32 &status )
    {
        count = 0;

        wchar_t url[ URL_CAPACITY ]{ };
        script_url( url, { } );

        atl::vector< char > reply{ };
        json::document      document{ };

        if ( !g_api.request( ATLAS_XORWSTR( L"GET" ).get( ), url, { }, reply, LIST_REPLY_LIMIT, status ) || status != 200 ||
             !document.parse( string_view{ reply.data( ), reply.size( ) } ) )
        {
            return false;
        }

        const json::value items{ document.root( )[ ATLAS_XORSTR( "scripts" ).get( ) ] };
        if ( items.type( ) != json::type_t::array )
        {
            return false;
        }

        for ( usize index{ }; index < items.size( ) && count < FILE_CAPACITY; ++index )
        {
            const string_view name{ items[ index ][ ATLAS_XORSTR( "name" ).get( ) ].as< string_view >( ) };

            if ( valid_name( name ) )
            {
                files[ count++ ].assign( name );
            }
        }

        return true;
    }

    ATLAS_FORCE_INLINE bool store_get( string_view name, atl::vector< char > &source, u32 &status )
    {
        wchar_t url[ URL_CAPACITY ]{ };
        script_url( url, name );

        atl::vector< char > reply{ };
        json::document      document{ };
        atl::vector< u8 >   bytes{ };

        return g_api.request( ATLAS_XORWSTR( L"GET" ).get( ), url, { }, reply, SOURCE_REPLY_LIMIT, status ) && status == 200 &&
               document.parse( string_view{ reply.data( ), reply.size( ) } ) &&
               base64::decode( document.root( )[ ATLAS_XORSTR( "data" ).get( ) ].as< string_view >( ), bytes ) &&
               source.assign( reinterpret_cast< const char * >( bytes.data( ) ), bytes.size( ) );
    }

    ATLAS_FORCE_INLINE bool store_put( string_view name, string_view source, u32 &status )
    {
        status = 0;

        if ( source.size( ) > SOURCE_LIMIT )
        {
            status = 413;
            return false;
        }

        atl::vector< char > text{ };
        if ( !base64::append( reinterpret_cast< const u8 * >( source.data( ) ), source.size( ), text ) )
        {
            return false;
        }

        json::writer writer{ };
        writer.begin_object( );
        writer.member( ATLAS_XORSTR( "data" ).get( ), string_view{ text.data( ), text.size( ) } );
        writer.end_object( );

        wchar_t url[ URL_CAPACITY ]{ };
        script_url( url, name );

        atl::vector< char > reply{ };
        return writer.ok( ) && g_api.request( ATLAS_XORWSTR( L"PUT" ).get( ), url, writer.text( ), reply, LIST_REPLY_LIMIT, status ) &&
               status >= 200 && status < 300;
    }
#endif

    bool fetch( string_view name, atl::vector< char > &source )
    {
        u32 status{ };
        return store_ready( ) && store_get( name, source, status );
    }

    void ATLAS_STDCALL host::store_worker( void *context )
    {
        host        &self{ *static_cast< host * >( context ) };
        store_job_t &job{ self.m_job };

        switch ( job.m_kind )
        {
            case store_kind_t::list:
            {
                job.m_success = store_list( job.m_files, job.m_file_count, job.m_status );
                job.m_listed  = job.m_success;
                break;
            }

            case store_kind_t::open:
            case store_kind_t::load: job.m_success = store_get( job.m_name.view( ), job.m_source, job.m_status ); break;

            case store_kind_t::save:
            {
                u32 status{ };
                job.m_success = store_put( job.m_name.view( ), string_view{ job.m_source.data( ), job.m_source.size( ) }, job.m_status );
                job.m_listed  = job.m_success && store_list( job.m_files, job.m_file_count, status );
                break;
            }

            default: break;
        }

        _InterlockedExchange( &self.m_job_state, 2 );
    }

    bool host::request_store( store_kind_t kind, string_view name, string_view source )
    {
        if ( this->m_job_state )
        {
            push_notification( notify_kind_t::warning, ICON_CODE, ATLAS_XORSTR( "Lua" ).get( ),
                               ATLAS_XORSTR( "Scripts are still syncing, try again in a moment" ).get( ) );
            return false;
        }

        if ( !store_ready( ) )
        {
            push_notification( notify_kind_t::warning, ICON_CODE, ATLAS_XORSTR( "Lua" ).get( ), ATLAS_XORSTR( "No Cloud session" ).get( ) );
            return false;
        }

        this->m_job        = store_job_t{ };
        this->m_job.m_kind = kind;
        this->m_job.m_name.assign( name );

        if ( !this->m_job.m_source.assign( source.data( ), source.size( ) ) )
        {
            return false;
        }

        _InterlockedExchange( &this->m_job_state, 1 );

        if ( !atl::create_hidden_thread( &host::store_worker, this ) )
        {
            _InterlockedExchange( &this->m_job_state, 0 );
            push_notification( notify_kind_t::error, ICON_CODE, ATLAS_XORSTR( "Lua" ).get( ),
                               ATLAS_XORSTR( "Could not start the scripts worker" ).get( ) );
            return false;
        }

        return true;
    }

    bool host::poll_store( store_result_t &result )
    {
        if ( _InterlockedCompareExchange( &this->m_job_state, 2, 2 ) != 2 )
        {
            return false;
        }

        store_job_t &job{ this->m_job };

        if ( job.m_listed )
        {
            this->m_file_count = job.m_file_count;

            for ( usize index{ }; index < this->m_file_count; ++index )
            {
                this->m_files[ index ]       = job.m_files[ index ];
                this->m_file_labels[ index ] = this->m_files[ index ].c_str( );
            }
        }

        if ( !job.m_success )
        {
            const char *local{ };
            switch ( job.m_kind )
            {
                case store_kind_t::list: local = ATLAS_XORSTR_STATIC( "Could not read the scripts" ); break;
                case store_kind_t::save: local = ATLAS_XORSTR_STATIC( "Could not save the script" ); break;
                default: local = ATLAS_XORSTR_STATIC( "Could not read the script" ); break;
            }

            push_notification( notify_kind_t::error, ICON_CODE, ATLAS_XORSTR( "Lua" ).get( ), store_failure( job.m_status, local ) );
        } else if ( job.m_kind == store_kind_t::load )
        {
            this->request_run( job.m_name.view( ), string_view{ job.m_source.data( ), job.m_source.size( ) } );
        }

        result.m_kind    = job.m_kind;
        result.m_name    = job.m_name;
        result.m_success = job.m_success;
        result.m_source.swap( job.m_source );

        this->m_job = store_job_t{ };
        _InterlockedExchange( &this->m_job_state, 0 );
        return true;
    }

    const char *const *host::file_names( ) const
    {
        return this->m_file_labels.begin( );
    }

    usize host::file_count( ) const
    {
        return this->m_file_count;
    }
} // namespace script
