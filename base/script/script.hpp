#pragma once

#include "runtime/array.hpp"
#include "runtime/mutex.hpp"
#include "runtime/vector.hpp"
#include "util/string.hpp"

struct lua_State;
struct lua_Debug;

namespace script
{
    inline constexpr usize SCRIPT_CAPACITY{ 16 };
    inline constexpr usize NAME_LENGTH{ 31 };
    inline constexpr usize SOURCE_LIMIT{ 3u * 1024u * 1024u };
    inline constexpr usize MEMORY_LIMIT{ 64u * 1024u * 1024u };
    inline constexpr usize FILE_CAPACITY{ 64 };
    inline constexpr usize LOG_CAPACITY{ 256 };
    inline constexpr usize LOG_LINE_LENGTH{ 191 };

    enum class event_t : u8
    {
        paint,
        create_move_pre_pred,
        create_move,
        create_move_post_pred,
        frame_stage,
        level_init,
        level_shutdown,
        unload,
        _COUNT,
    };

    enum class log_kind_t : u8
    {
        print,
        info,
        error,
        _COUNT,
    };

    enum class store_kind_t : u8
    {
        list,
        open,
        save,
        load,
        _COUNT,
    };

    struct log_line_t
    {
        log_kind_t                      m_kind{ };
        fixed_string< LOG_LINE_LENGTH > m_text{ };
    };

    using name_t = fixed_string< NAME_LENGTH >;

    struct store_result_t
    {
        store_kind_t        m_kind{ };
        name_t              m_name{ };
        atl::vector< char > m_source{ };
        bool                m_success{ };
    };
    using push_fn = i32 ( * )( lua_State *state );
    using open_fn = void ( * )( lua_State *state );

    bool valid_name( string_view name );
    bool fetch( string_view name, atl::vector< char > &source );
    u64  interrupt_time( );

    class host
    {
    public:
        void    startup( open_fn open_game );
        void    shutdown( );
        void    service( );
        void    dispatch( event_t event, push_fn push = nullptr );
        bool    call( lua_State *state, i32 arguments );
        void    collect( );
        event_t current( ) const;

        static string_view name_of( lua_State *state );

        template< typename visitor_t >
        void for_each_state( visitor_t &&visitor )
        {
            for ( instance_t &instance : this->m_instances )
            {
                if ( instance.m_state && !instance.m_doomed )
                {
                    visitor( instance.m_state );
                }
            }

            this->collect( );
        }

        void request_run( string_view name, string_view source );
        void request_unload( string_view name );
        void request_unload_all( );

        bool               request_store( store_kind_t kind, string_view name = { }, string_view source = { } );
        bool               poll_store( store_result_t &result );
        const char *const *file_names( ) const;
        usize              file_count( ) const;

        void  log( log_kind_t kind, string_view text );
        void  clear_log( );
        bool  is_loaded( string_view name ) const;
        usize loaded_names( name_t *output, usize capacity ) const;

        template< typename visitor_t >
        void visit_log( visitor_t &&visitor ) const
        {
            atl::scoped_lock lock{ this->m_log_lock };

            for ( usize index{ }; index < this->m_log_count; ++index )
            {
                visitor( this->m_log[ ( this->m_log_head + index ) % LOG_CAPACITY ] );
            }
        }

        u64 log_serial( ) const
        {
            return this->m_log_serial;
        }

        usize log_count( ) const
        {
            return this->m_log_count;
        }

    private:
        struct instance_t
        {
            lua_State *m_state{ };
            void      *m_heap{ };
            usize      m_used{ };
            u64        m_deadline{ };
            u32        m_handlers{ };
            bool       m_doomed{ };
            name_t     m_name{ };
        };

        enum class request_kind_t : u8
        {
            run,
            unload,
            unload_all,
            _COUNT,
        };

        struct request_t
        {
            request_kind_t      m_kind{ };
            name_t              m_name{ };
            atl::vector< char > m_source{ };
        };

        struct store_job_t
        {
            store_kind_t                        m_kind{ };
            name_t                              m_name{ };
            atl::vector< char >                 m_source{ };
            atl::array< name_t, FILE_CAPACITY > m_files{ };
            usize                               m_file_count{ };
            u32                                 m_status{ };
            bool                                m_success{ };
            bool                                m_listed{ };
        };

        static void ATLAS_STDCALL store_worker( void *context );

        static void *allocate( void *user, void *block, usize old_size, usize new_size );
        static void  budget_hook( lua_State *state, lua_Debug *debug );
        static int   setup( lua_State *state );
        static int   on_handler( lua_State *state );
        static int   on_print( lua_State *state );
        static int   on_load( lua_State *state );
        static void  on_warning( void *user, const char *message, int to_continue );

        static instance_t *owner( lua_State *state );

        void push_request( request_t &&request );
        bool load( string_view name, string_view source );
        void run_handlers( instance_t &instance, event_t event, push_fn push );
        void unload( instance_t &instance );
        void close( instance_t &instance );
        void publish_names( );
        void fail( instance_t &instance, const char *message );

        atl::array< instance_t, SCRIPT_CAPACITY > m_instances{ };
        open_fn                                   m_open_game{ };
        event_t                                   m_current{ event_t::_COUNT };

        atl::mutex               m_request_lock{ };
        atl::vector< request_t > m_requests{ };

        mutable atl::mutex                     m_log_lock{ };
        atl::array< log_line_t, LOG_CAPACITY > m_log{ };
        usize                                  m_log_head{ };
        usize                                  m_log_count{ };
        u64                                    m_log_serial{ };

        mutable atl::mutex                    m_names_lock{ };
        atl::array< name_t, SCRIPT_CAPACITY > m_loaded{ };
        usize                                 m_loaded_count{ };

        atl::array< name_t, FILE_CAPACITY >       m_files{ };
        atl::array< const char *, FILE_CAPACITY > m_file_labels{ };
        usize                                     m_file_count{ };

        store_job_t   m_job{ };
        volatile long m_job_state{ };
    };
} // namespace script

extern script::host g_scripts;
