#pragma once

#include "util/pe_module.hpp"
#include "util/typedefs.hpp"

namespace atl
{
    using thread_entry_fn = void ( ATLAS_STDCALL * )( void *arg );
    bool init_hidden_threads( const util::pe_module &ntdll );
    bool hidden_threads_ready( );
    bool create_hidden_thread( thread_entry_fn entry, void *arg,
                               void **out_handle    = nullptr,
                               void **out_thread_id = nullptr );
    void *hidden_gadget_address( );
} // namespace atl
