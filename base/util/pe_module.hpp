#pragma once

#include <intrin.h>

#include "address.hpp"
#include "string.hpp"

#include "windefs.hpp"

namespace util
{
    class pe_module
    {
    public:
        pe_module( )  = default;
        ~pe_module( ) = default;
        explicit pe_module( void *module_base );

        bool                is_null( ) const;
        bool                is_valid( ) const;
        bool                contains( const void *address, usize size ) const;
        bool                is_executable( const void *address ) const;
        bool                section( const char *name, const void **data, usize *size ) const;
#if defined( ATLAS_DISABLE_CLOUD )
        address             find_pattern( const char *pattern ) const;
#endif
        address             find_export( const char *func_name ) const;
        address             find_export_fuzzy( const char *func_name ) const; // This func "fuzzily" walks the export list and finds a rough match of an export. Useful for mangled functions.
        address             find_interface( const char *interface_name ) const;
        IMAGE_DOS_HEADER   *get_dos_header( ) const;
        IMAGE_NT_HEADERS *get_nt_header( ) const;

    private:
        void *m_module_base{ };
        typedef void *( *CreateInterfaceFn )( const char *name, int *retcode );
        mutable CreateInterfaceFn m_create_interface_fn{ };
    };

    inline pe_module find_module( const wchar_t *name )
    {
        if ( !name )
        {
            return pe_module( nullptr );
        }

        auto *process_environment_block{ li::detail::peb( ) };

        // BUGBUG: Might crash here.
        if ( !process_environment_block || !process_environment_block->Ldr )
        {
            return pe_module( nullptr );
        }

        LIST_ENTRY *head{ &process_environment_block->Ldr->InMemoryOrderModuleList };

        for ( LIST_ENTRY *entry{ head->Flink }; entry != head; entry = entry->Flink )
        {
            auto *module{ reinterpret_cast< LDR_DATA_TABLE_ENTRY * >( reinterpret_cast< u8 * >( entry ) - sizeof( LIST_ENTRY ) ) };

            if ( !module->DllBase || !module->BaseDllName.Buffer )
            {
                continue;
            }

            if ( str::equals_ignore_case( module->BaseDllName.Buffer, name ) )
            {
                return util::pe_module( module->DllBase );
            }
        }

        return pe_module( nullptr );
    }

    pe_module find_module( const void *address );

    struct memory_basic_information
    {
        void  *base_address{ };
        void  *allocation_base{ };
        u32    allocation_protect{ };
        u32    alignment{ };
        usize  region_size{ };
        u32    state{ };
        u32    protect{ };
        u32    type{ };
    };

    static_assert( sizeof( memory_basic_information ) == sizeof( MEMORY_BASIC_INFORMATION ) );

    inline bool is_executable_address( const void *address )
    {
        if ( !address )
        {
            return false;
        }

        pe_module image{ find_module( address ) };
        if ( image.is_executable( address ) )
        {
            return true;
        }

        memory_basic_information information{ };
        usize                    returned{ };

        if ( I( NtQueryVirtualMemory )( util::get_current_process( ), const_cast< void * >( address ), 0,
                                        &information, sizeof( information ), &returned ) < 0 ||
             information.state != MEM_COMMIT ||
             ( information.protect & ( PAGE_GUARD | PAGE_NOACCESS ) ) != 0 )
        {
            return false;
        }

        const u32 protect{ information.protect & 0xFFu };
        return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
               protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
    }
} // namespace util
