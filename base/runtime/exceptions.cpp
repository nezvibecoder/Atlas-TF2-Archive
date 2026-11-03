#include "exceptions.hpp"

#include "util/address.hpp"
#include "util/typedefs.hpp"

#include <dbghelp.h>

struct temp_module_info_t
{
    uptr           m_base_addr{ };
    const wchar_t *m_dll_name{ };
    u16            m_dll_name_length{ };
    uptr           module_size{ };
    bool           m_is_own{ };
};

struct memory_basic_information
{
    void *base_address{ };
    void *allocation_base{ };
    u32   allocation_protect{ };
#if defined( __x86_64__ )
    u32 alignment{ };
#endif
    usize region_size{ };
    u32   state{ };
    u32   protect{ };
    u32   type{ };
};

static_assert( sizeof( memory_basic_information ) == sizeof( MEMORY_BASIC_INFORMATION ) );

namespace exceptions
{
    static uptr own_module_base{ };
    static uptr own_module_end{ };

    static bool query_allocation_range( uptr addr, uptr &base, uptr &end )
    {
        if ( !addr )
        {
            return false;
        }

        memory_basic_information information{ };
        usize                    returned{ };
        if ( I( NtQueryVirtualMemory )(
                 util::get_current_process( ), reinterpret_cast< void * >( addr ), 0,
                 &information, sizeof( information ), &returned ) < 0 ||
             !information.allocation_base )
        {
            return false;
        }

        const auto allocation_base{ reinterpret_cast< uptr >( information.allocation_base ) };
        auto       cursor{ allocation_base };
        auto       allocation_end{ allocation_base };

        for ( ;; )
        {
            memory_basic_information region{ };
            returned = 0;
            if ( I( NtQueryVirtualMemory )( util::get_current_process( ), reinterpret_cast< void * >( cursor ), 0, &region, sizeof( region ), &returned ) < 0 ||
                 ( uptr )region.allocation_base != allocation_base ||
                 !region.region_size )
            {
                break;
            }

            const uptr region_base{ reinterpret_cast< uptr >( region.base_address ) };
            if ( region_base > static_cast< uptr >( -1 ) - region.region_size )
            {
                break;
            }

            const uptr region_end{ region_base + region.region_size };
            if ( region_end <= cursor )
            {
                break;
            }

            allocation_end = region_end;
            cursor         = region_end;
        }

        if ( allocation_end <= allocation_base )
        {
            return false;
        }

        base = allocation_base;
        end  = allocation_end;
        return true;
    }

    // Load in dbghelp so we can use the functions needed from it...
    bool load_dbghelp_module( )
    {
        return I( LoadLibraryExA )( ATLAS_XORSTR( "dbghelp.dll" ).get( ), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 ) != NULL;
    }

    bool initialize_own_module( )
    {
        if ( own_module_base && own_module_end > own_module_base )
        {
            return true;
        }

        uptr base{ };
        uptr end{ };
        if ( !query_allocation_range( reinterpret_cast< uptr >( &initialize_own_module ), base, end ) )
        {
            return false;
        }

        own_module_base = base;
        own_module_end  = end;

        return true;
    }

    static void copy_module_name( const temp_module_info_t &module_info, char *out, usize out_size )
    {
        if ( !out || !out_size )
        {
            return;
        }

        out[ 0 ] = '\0';

        if ( module_info.m_is_own )
        {
            atl::format( out, out_size, ATLAS_XORSTR( "%s" ).get( ), ATLAS_XORSTR( "this" ).get( ) );
            return;
        }

        if ( !module_info.m_dll_name || !module_info.m_dll_name_length || out_size <= 1 )
        {
            return;
        }

        const auto converted{ I( WideCharToMultiByte )(
            CP_UTF8, 0,
            module_info.m_dll_name, static_cast< i32 >( module_info.m_dll_name_length ),
            out, static_cast< i32 >( out_size - 1 ),
            nullptr, nullptr ) };

        if ( converted > 0 )
        {
            out[ converted ] = '\0';
        }
    }

    // Since modules are mapped dynamically in process memory, where they actually land up in the heap is never consistent.
    // This also affects the exception addresses, as they're printed out from process base address + the offset to the data it's holding.
    // This function attempts to resolve where the address actually is in a mapped module's space.
    bool get_module_info( uptr addr, temp_module_info_t &module_info )
    {
        if ( !addr )
        {
            return false;
        }

        // The exception happened because of us... drats.
        if ( own_module_base && addr >= own_module_base && addr < own_module_end )
        {
            module_info.m_base_addr = own_module_base;
            module_info.module_size = own_module_end - own_module_base;
            module_info.m_is_own    = true;
            return true;
        }

        auto *process_environment_block{ li::detail::peb( ) };

        if ( !process_environment_block || !process_environment_block->Ldr )
        {
            return false;
        }

        // Better doing this than to call CreateToolhelp32...
        LIST_ENTRY *head{ &process_environment_block->Ldr->InMemoryOrderModuleList };
        for ( LIST_ENTRY *entry{ head->Flink }; entry && entry != head; entry = entry->Flink )
        {
            auto *module{ reinterpret_cast< LDR_DATA_TABLE_ENTRY * >( reinterpret_cast< u8 * >( entry ) - sizeof( LIST_ENTRY ) ) };
            if ( !module->DllBase || !module->BaseDllName.Buffer )
            {
                continue;
            }

            const auto base_addr{ ( uptr )module->DllBase };
            const auto size_of_image{ static_cast< uptr >( module->SizeOfImage ) };
            if ( !size_of_image || addr < base_addr || addr - base_addr >= size_of_image )
            {
                continue;
            }

            module_info.m_base_addr       = base_addr;
            module_info.module_size       = size_of_image;
            module_info.m_dll_name        = module->BaseDllName.Buffer;
            module_info.m_dll_name_length = static_cast< u16 >( module->BaseDllName.Length / sizeof( wchar_t ) );
            module_info.m_is_own          = false;
            return true;
        }

        return false;
    }

    static uptr get_allocation_base( uptr addr )
    {
        if ( !addr )
        {
            return 0;
        }

        memory_basic_information information{ };
        usize                    returned{ };
        if ( I( NtQueryVirtualMemory )(
                 util::get_current_process( ), reinterpret_cast< void * >( addr ), 0,
                 &information, sizeof( information ), &returned ) < 0 )
        {
            return 0;
        }

        return reinterpret_cast< uptr >( information.allocation_base );
    }

    // NOTE: Hoku - 9/17/26; Removed the casts to u64 as these addresses will already be u64s (we're on 64 bit using uptrs, they'll be defined as u64 already...)
    ATLAS_FORCE_INLINE void write_register_row(
        const char *first_register,
        uptr        first_register_addr,
        const char *second_register,
        uptr        second_register_addr,
        char       *buf_out,
        usize       buf_size )
    {
        temp_module_info_t mod1{ };
        temp_module_info_t mod2{ };

        // Print the register rows out. If the register address landed somewhere inside a module it will get identified here.
        if ( get_module_info( first_register_addr, mod1 ) )
        {
            char module_name[ MAX_PATH ]{ };
            copy_module_name( mod1, module_name, sizeof( module_name ) );
            atl::format_append( buf_out, buf_size, ATLAS_XORSTR( "%s: %s+0x%X    " ).get( ), first_register, module_name, first_register_addr - mod1.m_base_addr );
        } else
        {
            atl::format_append( buf_out, buf_size, ATLAS_XORSTR( "%s: 0x%X    " ).get( ), first_register, first_register_addr );
        }

        if ( get_module_info( second_register_addr, mod2 ) )
        {
            char module_name[ MAX_PATH ]{ };
            copy_module_name( mod2, module_name, sizeof( module_name ) );
            atl::format_append( buf_out, buf_size, ATLAS_XORSTR( "%s: %s+0x%X\n" ).get( ), second_register, module_name, second_register_addr - mod2.m_base_addr );
        } else
        {
            atl::format_append( buf_out, buf_size, ATLAS_XORSTR( "%s: 0x%X\n" ).get( ), second_register, second_register_addr );
        }
    }

    ATLAS_FORCE_INLINE void write_register_single( const char *reg, uptr reg_addr, char *buf_out, usize buf_size )
    {
        temp_module_info_t mod1{ };
        if ( get_module_info( reg_addr, mod1 ) )
        {
            char module_name[ MAX_PATH ]{ };
            copy_module_name( mod1, module_name, sizeof( module_name ) );
            atl::format_append( buf_out, buf_size, ATLAS_XORSTR( "%s: %s+0x%X\n\n" ).get( ), reg, module_name, reg_addr - mod1.m_base_addr );
        } else
        {
            atl::format_append( buf_out, buf_size, ATLAS_XORSTR( "%s: 0x%X\n\n" ).get( ), reg, reg_addr );
        }
    }

    // Yea this fucking sucks.
    // __builtin_return_address only allows a constant expression going in to the parameter, so we can't do this recursively the nice way.
    ATLAS_FORCE_INLINE void *get_return_address( u32 input_level )
    {
        switch ( input_level )
        {
            case 0: return __builtin_return_address( 0 );
            case 1: return __builtin_return_address( 1 );
            case 2: return __builtin_return_address( 2 );
            case 3: return __builtin_return_address( 3 );
            case 4: return __builtin_return_address( 4 );
            case 5: return __builtin_return_address( 5 );
            case 6: return __builtin_return_address( 6 );
            case 7: return __builtin_return_address( 7 );
            case 8: return __builtin_return_address( 8 );
            case 9: return __builtin_return_address( 9 );
            case 10: return __builtin_return_address( 10 );
            case 11: return __builtin_return_address( 11 );
            case 12: return __builtin_return_address( 12 );
            case 13: return __builtin_return_address( 13 );
            case 14: return __builtin_return_address( 14 );
            case 15: return __builtin_return_address( 15 );
            case 16: return __builtin_return_address( 16 );
            case 17: return __builtin_return_address( 17 );
            case 18: return __builtin_return_address( 18 );
            case 19: return __builtin_return_address( 19 );
            case 20: return __builtin_return_address( 20 );
            case 21: return __builtin_return_address( 21 );
            case 22: return __builtin_return_address( 22 );
            case 23: return __builtin_return_address( 23 );
            case 24: return __builtin_return_address( 24 );
            case 25: return __builtin_return_address( 25 );
            case 26: return __builtin_return_address( 26 );
            case 27: return __builtin_return_address( 27 );
            case 28: return __builtin_return_address( 28 );
            case 29: return __builtin_return_address( 29 );
            case 30: return __builtin_return_address( 30 );
            case 31: return __builtin_return_address( 31 );
            case 32: return __builtin_return_address( 32 );
            default: return nullptr; // If the callstack ever somehow gets to more than 32 called functions, I'd personally be surprised. But the rest of the info is probably useless.
        }
    }

    ATLAS_FORCE_INLINE void print_callstack( char *buf, usize buf_size )
    {
        u32 i{ 1 };

        atl::format_append( buf, buf_size, ATLAS_XORSTR( "CALLSTACK:\n" ).get( ) );

        auto ret_addr{ get_return_address( i ) };
        while ( ret_addr != nullptr )
        {
            auto addr{ ( uptr )ret_addr };

            temp_module_info_t mod{ };
            if ( get_module_info( addr, mod ) )
            {
                char module_name[ MAX_PATH ]{ };
                copy_module_name( mod, module_name, sizeof( module_name ) );

                // Add a space before the number if we're printing the first 9 functions of the callstack.
                // ... because it looks fancier...
                // i'm gay.
                if ( i < 9 )
                {
                    atl::format_append( buf, buf_size, ATLAS_XORSTR( " %i.    %s+0x%X\n" ).get( ), i, module_name, addr - mod.m_base_addr );
                } else
                {
                    atl::format_append( buf, buf_size, ATLAS_XORSTR( "%i.    %s+0x%X\n" ).get( ), i, module_name, addr - mod.m_base_addr );
                }
            } else
            {
                // Add a space before the number if we're printing the first 9 functions of the callstack.
                // ... because it looks fancier...
                // i'm gay.
                if ( i < 9 )
                {
                    atl::format_append( buf, buf_size, ATLAS_XORSTR( " %i.    0x%X\n" ).get( ), i, addr );
                } else
                {
                    atl::format_append( buf, buf_size, ATLAS_XORSTR( "%i.    0x%X\n" ).get( ), i, addr );
                }
            }

            i++;
            ret_addr = get_return_address( i );
        }

        atl::format_append( buf, buf_size, ATLAS_XORSTR( "\n\n" ).get( ) );
    }

    long on_caught_exception( PEXCEPTION_POINTERS info )
    {
        const auto code{ info->ExceptionRecord->ExceptionCode };
        switch ( code )
        {
            case STATUS_ACCESS_VIOLATION:
            case STATUS_STACK_OVERFLOW:
            case STATUS_HEAP_CORRUPTION:
            case STATUS_ARRAY_BOUNDS_EXCEEDED:
            case STATUS_ILLEGAL_INSTRUCTION: break;
            default: return EXCEPTION_CONTINUE_SEARCH;
        }

        const auto ctxrec{ info->ContextRecord };
        const auto exception_address{ ( uptr )info->ExceptionRecord->ExceptionAddress };

        static char buf[ 2048 ]{ };

        temp_module_info_t module_info{ };
        if ( get_module_info( exception_address, module_info ) )
        {
            char module_name[ MAX_PATH ]{ };
            copy_module_name( module_info, module_name, sizeof( module_name ) );
            atl::format( buf, sizeof( buf ), ATLAS_XORSTR( "Atlas has crashed.\n\nCode: 0x%X at %s+0x%X\n" ).get( ), code, module_name, exception_address - module_info.m_base_addr );
        } else
        {
            atl::format( buf, sizeof( buf ), ATLAS_XORSTR( "Atlas has crashed.\n\nCode: 0x%X at 0x%X\n" ).get( ), code, exception_address );
        }

#if defined( __x86_64__ )
        write_register_row( ATLAS_XORSTR( "RAX" ).get( ), ctxrec->Rax, ATLAS_XORSTR( "RBX" ).get( ), ctxrec->Rbx, buf, sizeof( buf ) );
        write_register_row( ATLAS_XORSTR( "RCX" ).get( ), ctxrec->Rcx, ATLAS_XORSTR( "RDX" ).get( ), ctxrec->Rdx, buf, sizeof( buf ) );
        write_register_row( ATLAS_XORSTR( "RSP" ).get( ), ctxrec->Rsp, ATLAS_XORSTR( "RBP" ).get( ), ctxrec->Rbp, buf, sizeof( buf ) );
        write_register_row( ATLAS_XORSTR( "RSI" ).get( ), ctxrec->Rsi, ATLAS_XORSTR( "RDI" ).get( ), ctxrec->Rdi, buf, sizeof( buf ) );
        write_register_single( ATLAS_XORSTR( "RIP" ).get( ), ctxrec->Rip, buf, sizeof( buf ) );
#else
        write_register_row( ATLAS_XORSTR( "EAX" ).get( ), ctxrec->Eax, ATLAS_XORSTR( "EBX" ).get( ), ctxrec->Ebx, buf, sizeof( buf ) );
        write_register_row( ATLAS_XORSTR( "ECX" ).get( ), ctxrec->Ecx, ATLAS_XORSTR( "EDX" ).get( ), ctxrec->Edx, buf, sizeof( buf ) );
        write_register_row( ATLAS_XORSTR( "ESP" ).get( ), ctxrec->Esp, ATLAS_XORSTR( "EBP" ).get( ), ctxrec->Ebp, buf, sizeof( buf ) );
        write_register_row( ATLAS_XORSTR( "ESI" ).get( ), ctxrec->Esi, ATLAS_XORSTR( "EDI" ).get( ), ctxrec->Edi, buf, sizeof( buf ) );
        write_register_single( ATLAS_XORSTR( "EIP" ).get( ), ctxrec->Eip, buf, sizeof( buf ) );
#endif

        // print_callstack( buf, sizeof( buf ) );

        atl::format_append( buf, sizeof( buf ), ATLAS_XORSTR( "Report this crash in #bugs-and-crashes using CTRL+C.\n\nBuild: " CUR_COMMIT_HASH ).get( ) );

        I( MessageBoxA )( nullptr, buf, ATLAS_XORSTR( "atlas-error" ).get( ), MB_ICONERROR );

        return EXCEPTION_CONTINUE_SEARCH;
    }
} // namespace exceptions
