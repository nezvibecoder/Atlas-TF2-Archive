#include "thread.hpp"

#include "libs/lazy_importer.hpp"
#include "memory.hpp"
#include "util/address.hpp"

namespace
{
    constinit void *g_launch_gadget{ };

#if !defined( __x86_64__ )
    using rtl_capture_context_fn = void( ATLAS_STDCALL * )( CONTEXT *context );
    using rtl_exit_user_thread_fn = void( ATLAS_STDCALL * )( i32 status );

    constinit rtl_capture_context_fn   g_capture_context{ };
    constinit rtl_exit_user_thread_fn  g_exit_user_thread{ };
#endif

    struct hidden_ctx_t
    {
#if defined( __x86_64__ )
        void                           *m_launcher{ };
#else
        CONTEXT                         m_context{ };
        void                           *m_frame[ 4 ]{ };
#endif
        atl::thread_entry_fn            m_entry{ };
        void                           *m_arg{ };
        decltype( &GetCurrentThreadId ) m_get_thread_id{ };
        bool                            m_creator_owns{ };
        void                           *m_thread_id{ };
    };

    void ATLAS_STDCALL hidden_thread_launcher( void *raw_ctx )
    {
        auto *ctx{ static_cast< hidden_ctx_t * >( raw_ctx ) };

        atl::thread_entry_fn entry{ ctx->m_entry };
        void                *arg{ ctx->m_arg };
        const bool           creator_owns{ ctx->m_creator_owns };
        if ( creator_owns )
        {
            const auto get_thread_id{ ctx->m_get_thread_id };
            void      *thread_id{ reinterpret_cast< void * >( static_cast< usize >( get_thread_id( ) ) ) };
            __atomic_store_n( &ctx->m_thread_id, thread_id, __ATOMIC_RELEASE );
        } else
        {
            atl::free( ctx );
        }

        if ( entry != nullptr )
        {
            entry( arg );
        }

#if !defined( __x86_64__ )
        if ( g_exit_user_thread )
        {
            g_exit_user_thread( 0 );
            __builtin_unreachable( );
        }
#endif
    }

    constexpr u32 THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER{ 0x4 };
} // namespace

bool atl::init_hidden_threads( const util::pe_module &ntdll )
{
    if ( g_launch_gadget != nullptr )
    {
        return true;
    }

    if ( ntdll.is_null( ) )
    {
        return false;
    }

#if defined( __x86_64__ )
    const void *text{ };
    usize       text_size{ };
    if ( !ntdll.section( ".text", &text, &text_size ) || !text || text_size < 2 )
    {
        return false;
    }

    const auto *bytes{ static_cast< const u8 * >( text ) };
    for ( usize at{ }; at + 1 < text_size; ++at )
    {
        if ( bytes[ at ] == 0xFF && bytes[ at + 1 ] == 0x21 )
        {
            g_launch_gadget = const_cast< u8 * >( &bytes[ at ] );
            return true;
        }
    }

    return false;
#else
    util::address restore{ ntdll.find_export( "RtlRestoreContext" ) };
    util::address exit_thread{ ntdll.find_export( "RtlExitUserThread" ) };
    util::address capture{ ntdll.find_export( "RtlCaptureContext" ) };

    if ( restore.is_null( ) || exit_thread.is_null( ) || capture.is_null( ) )
    {
        return false;
    }

    g_launch_gadget    = restore.get_as< void * >( );
    g_exit_user_thread = exit_thread.get_as< rtl_exit_user_thread_fn >( );
    g_capture_context  = capture.get_as< rtl_capture_context_fn >( );
    return true;
#endif
}

bool atl::hidden_threads_ready( )
{
    return g_launch_gadget != nullptr;
}

bool atl::create_hidden_thread( thread_entry_fn entry, void *arg,
                                void **out_handle, void **out_thread_id )
{
    if ( out_handle )
    {
        *out_handle = nullptr;
    }
    if ( out_thread_id )
    {
        *out_thread_id = nullptr;
    }

    if ( !entry || !g_launch_gadget )
    {
        return false;
    }

#if !defined( __x86_64__ )
    if ( !g_capture_context || !g_exit_user_thread )
    {
        return false;
    }
#endif

    auto *ctx{ static_cast< hidden_ctx_t * >( atl::malloc( sizeof( hidden_ctx_t ) ) ) };
    if ( !ctx )
    {
        return false;
    }

#if defined( __x86_64__ )
    ctx->m_launcher      = reinterpret_cast< void * >( &hidden_thread_launcher );
#else
    g_capture_context( &ctx->m_context );
    ctx->m_context.ContextFlags = CONTEXT_FULL;
    ctx->m_context.Eip          = reinterpret_cast< u32 >( &hidden_thread_launcher );
    ctx->m_context.Esp          = reinterpret_cast< u32 >( &ctx->m_frame[ 0 ] );
    ctx->m_frame[ 0 ] = reinterpret_cast< void * >( g_exit_user_thread );
    ctx->m_frame[ 1 ] = ctx;
    ctx->m_frame[ 2 ] = nullptr;
    ctx->m_frame[ 3 ] = nullptr;
#endif
    ctx->m_entry         = entry;
    ctx->m_arg           = arg;
    ctx->m_get_thread_id = I( GetCurrentThreadId );
    ctx->m_creator_owns  = out_thread_id != nullptr;
    ctx->m_thread_id     = nullptr;

    void     *handle{ };
    const i32 status{ I( NtCreateThreadEx )( &handle, THREAD_ALL_ACCESS, nullptr, util::get_current_process( ), g_launch_gadget, ctx, THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER, 0, 0, 0, nullptr ) };

    if ( status < 0 )
    {
        atl::free( ctx );
        return false;
    }

    if ( out_thread_id )
    {
        void *thread_id{ };
        u32   spins{ };
        while ( !( thread_id = __atomic_load_n( &ctx->m_thread_id, __ATOMIC_ACQUIRE ) ) )
        {
            if ( spins < 256u )
            {
                ++spins;
                _mm_pause( );
            } else
            {
                I( Sleep )( 1 );
            }
        }

        *out_thread_id = thread_id;
        atl::free( ctx );
    }

    if ( out_handle )
    {
        *out_handle = handle;
    } else
    {
        I( CloseHandle )( handle );
    }

    return true;
}

void *atl::hidden_gadget_address( )
{
    return g_launch_gadget;
}
