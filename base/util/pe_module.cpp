#include "pe_module.hpp"

#include "libs/lazy_importer.hpp"
#include "math/math.hpp"
#include "runtime/vector.hpp"
#include "util/string.hpp"

struct memory_basic_information
{
    void *base_address{ };
    void *allocation_base{ };
    u32   allocation_protect{ };
#if defined( __x86_64__ )
    u32   alignment{ };
#endif
    usize region_size{ };
    u32   state{ };
    u32   protect{ };
    u32   type{ };
};

static_assert( sizeof( memory_basic_information ) == sizeof( MEMORY_BASIC_INFORMATION ) );

util::pe_module::pe_module( void *module_base ) :
    m_module_base{ module_base }
{
}

bool util::pe_module::is_null( ) const
{
    return this->m_module_base == nullptr;
}

bool util::pe_module::is_valid( ) const
{
    const IMAGE_NT_HEADERS *nt{ this->get_nt_header( ) };
    return nt &&
           nt->FileHeader.SizeOfOptionalHeader >= sizeof( IMAGE_OPTIONAL_HEADER ) &&
           nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR_MAGIC &&
           nt->OptionalHeader.SizeOfImage;
}

inline bool is_hex( char value )
{
    return ( value >= '0' && value <= '9' ) || ( value >= 'A' && value <= 'F' ) ||
           ( value >= 'a' && value <= 'f' );
}

inline u8 hex_value( char value )
{
    if ( value >= '0' && value <= '9' )
    {
        return static_cast< u8 >( value - '0' );
    }

    if ( value >= 'A' && value <= 'F' )
    {
        return static_cast< u8 >( value - 'A' + 10 );
    }

    return static_cast< u8 >( value - 'a' + 10 );
}

#if defined( ATLAS_DISABLE_CLOUD )

// from glibc...
unsigned long __strtoul( const char *nptr, char **endptr, int base )
{
    const char   *s;
    unsigned long acc, cutoff;
    int           c;
    int           neg, any, cutlim;
    /*
     * See strtol for comments as to the logic used.
     */
    s = nptr;

    do
    {
        c = ( unsigned char )*s++;
    } while ( str::is_space( c ) );

    if ( c == '-' )
    {
        neg = 1;
        c   = *s++;
    } else
    {
        neg = 0;
        if ( c == '+' )
        {
            c = *s++;
        }
    }

    if ( ( base == 0 || base == 16 ) &&
         c == '0' && ( *s == 'x' || *s == 'X' ) )
    {
        c = s[ 1 ];
        s += 2;
        base = 16;
    }

    if ( base == 0 )
    {
        base = c == '0' ? 8 : 10;
    }

    cutoff = ULONG_MAX / ( unsigned long )base;
    cutlim = ULONG_MAX % ( unsigned long )base;
    for ( acc = 0, any = 0;; c = ( unsigned char )*s++ )
    {
        if ( str::is_digit( c ) )
        {
            c -= '0';
        } else if ( str::is_alpha( c ) )
        {
            c -= str::is_upper( c ) ? 'A' - 10 : 'a' - 10;
        } else
        {
            break;
        }

        if ( c >= base )
        {
            break;
        }

        if ( any < 0 )
        {
            continue;
        }

        if ( acc > cutoff || ( acc == cutoff && c > cutlim ) )
        {
            any = -1;
            acc = ULONG_MAX;
        } else
        {
            any = 1;
            acc *= ( unsigned long )base;
            acc += c;
        }
    }

    if ( neg && any > 0 )
    {
        acc = -acc;
    }

    if ( endptr != 0 )
    {
        *endptr = ( char * )( any ? s - 1 : nptr );
    }

    return ( acc );
}

inline atl::vector< i32 > sig2byte( char *pattern )
{
    auto        bytes = atl::vector< i32 >{ };
    const auto  start = pattern;
    const char *end   = pattern + str::strlen( pattern );

    for ( auto *cur = start; cur < end; ++cur )
    {
        if ( *cur == '?' )
        {
            ++cur;

            if ( *cur == '?' )
            {
                ++cur;
            }

            bytes.push_back( -1 );
        } else
        {
            bytes.push_back( ( i32 )__strtoul( cur, &cur, 16 ) );
        }
    }

    return bytes;
}

util::address util::pe_module::find_pattern( const char *pattern ) const
{
    if ( !pattern )
    {
        return address{ nullptr };
    }

    IMAGE_NT_HEADERS *nt_header{ this->get_nt_header( ) };
    if ( !nt_header )
    {
        return address{ nullptr };
    }

    const u32 size_of_image{ nt_header->OptionalHeader.SizeOfImage };

    // Either we have a faulty image or our structs were corrupted/overwritten.
    if ( !size_of_image )
    {
        return address{ nullptr };
    }

    atl::vector< i32 > pattern_to_scan{ sig2byte( const_cast< char * >( pattern ) ) };

    u8         *bytes_to_scan{ static_cast< u8 * >( this->m_module_base ) };
    const usize pattern_size{ pattern_to_scan.size( ) };
    const i32  *pattern_data{ pattern_to_scan.data( ) };

    if ( !pattern_size || pattern_size > size_of_image )
    {
        return address{ nullptr };
    }

    for ( usize index{ }; index <= size_of_image - pattern_size; ++index )
    {
        bool found{ true };

        for ( usize pattern_index{ }; pattern_index < pattern_size; ++pattern_index )
        {
            if ( bytes_to_scan[ index + pattern_index ] != pattern_data[ pattern_index ] && pattern_data[ pattern_index ] != -1 )
            {
                found = false;
                break;
            }
        }

        if ( found )
        {
            return address{ &bytes_to_scan[ index ] };
        }
    }

    return address{ nullptr };
}
#endif // ATLAS_DISABLE_CLOUD


bool util::pe_module::contains( const void *address, usize size ) const
{
    const IMAGE_NT_HEADERS *nt{ this->get_nt_header( ) };

    if ( !nt || !address || !size )
    {
        return false;
    }

    const uptr base{ reinterpret_cast< uptr >( this->m_module_base ) };
    const uptr value{ reinterpret_cast< uptr >( address ) };
    return value >= base && value - base <= nt->OptionalHeader.SizeOfImage &&
           size <= nt->OptionalHeader.SizeOfImage - ( value - base );
}

bool util::pe_module::is_executable( const void *address ) const
{
    IMAGE_NT_HEADERS *nt{ this->get_nt_header( ) };

    if ( !nt || !this->contains( address, 1 ) )
    {
        return false;
    }

    auto      *section{ reinterpret_cast< IMAGE_SECTION_HEADER * >(
        reinterpret_cast< u8 * >( &nt->OptionalHeader ) + nt->FileHeader.SizeOfOptionalHeader ) };
    const uptr rva{ reinterpret_cast< uptr >( address ) -
                    reinterpret_cast< uptr >( this->m_module_base ) };

    for ( u16 index{ }; index < nt->FileHeader.NumberOfSections; ++index )
    {
        const u32 size{ section[ index ].Misc.VirtualSize ? section[ index ].Misc.VirtualSize
                                                          : section[ index ].SizeOfRawData };

        if ( rva >= section[ index ].VirtualAddress &&
             rva - section[ index ].VirtualAddress < size )
        {
            return ( section[ index ].Characteristics & IMAGE_SCN_MEM_EXECUTE ) != 0;
        }
    }

    return false;
}

util::pe_module util::find_module( const void *address )
{
    if ( !address )
    {
        return pe_module{ nullptr };
    }

    auto *process_environment_block{ li::detail::peb( ) };

    if ( process_environment_block && process_environment_block->Ldr )
    {
        LIST_ENTRY *head{ &process_environment_block->Ldr->InMemoryOrderModuleList };

        for ( LIST_ENTRY *entry{ head->Flink }; entry && entry != head; entry = entry->Flink )
        {
            auto     *loaded{ reinterpret_cast< LDR_DATA_TABLE_ENTRY * >(
                reinterpret_cast< u8 * >( entry ) - sizeof( LIST_ENTRY ) ) };
            pe_module image{ loaded ? loaded->DllBase : nullptr };

            if ( image.contains( address, 1 ) )
            {
                return image;
            }
        }
    }

    memory_basic_information information{ };
    usize                    returned{ };

    if ( I( NtQueryVirtualMemory )(
             util::get_current_process( ), const_cast< void * >( address ), 0,
             &information, sizeof( information ), &returned ) < 0 ||
         !information.allocation_base )
    {
        return pe_module{ nullptr };
    }

    pe_module image{ information.allocation_base };
    return image.contains( address, 1 ) ? image : pe_module{ nullptr };
}

bool util::pe_module::section( const char *name, const void **data, usize *size ) const
{
    IMAGE_NT_HEADERS *nt{ this->get_nt_header( ) };

    if ( !nt || !name || !data || !size )
    {
        return false;
    }

    *data = nullptr;
    *size = 0;

    auto       *headers{ reinterpret_cast< IMAGE_SECTION_HEADER * >(
        reinterpret_cast< u8 * >( &nt->OptionalHeader ) + nt->FileHeader.SizeOfOptionalHeader ) };
    const usize name_length{ str::length( name ) };

    if ( !name_length || name_length > sizeof( headers[ 0 ].Name ) )
    {
        return false;
    }

    for ( u16 index{ }; index < nt->FileHeader.NumberOfSections; ++index )
    {
        const IMAGE_SECTION_HEADER &header{ headers[ index ] };
        const usize                 section_name_size{ str::bounded_length(
            reinterpret_cast< const char * >( header.Name ), sizeof( header.Name ) ) };
        const u32                   section_size{ header.Misc.VirtualSize ? header.Misc.VirtualSize : header.SizeOfRawData };

        if ( section_name_size != name_length ||
             !str::equals( reinterpret_cast< const char * >( header.Name ), section_name_size,
                           name, name_length ) ||
             !section_size || header.VirtualAddress > nt->OptionalHeader.SizeOfImage ||
             section_size > nt->OptionalHeader.SizeOfImage - header.VirtualAddress )
        {
            continue;
        }

        *data = static_cast< const u8 * >( this->m_module_base ) + header.VirtualAddress;
        *size = section_size;
        return true;
    }

    return false;
}

util::address util::pe_module::find_export( const char *func_name ) const
{
    if ( !func_name )
    {
        return address{ nullptr };
    }

    const IMAGE_NT_HEADERS *nt{ this->get_nt_header( ) };
    if ( !nt || nt->FileHeader.SizeOfOptionalHeader < FIELD_OFFSET( IMAGE_OPTIONAL_HEADER, DataDirectory[ 1 ] ) )
    {
        return address{ nullptr };
    }

    const u32   image_size{ nt->OptionalHeader.SizeOfImage };
    const auto *directories{ nt->OptionalHeader.DataDirectory };

    if ( !directories ||
         !directories[ 0 ].VirtualAddress ||
         directories[ 0 ].VirtualAddress >= image_size ||
         directories[ 0 ].Size > image_size - directories[ 0 ].VirtualAddress ||
         directories[ 0 ].Size < sizeof( IMAGE_EXPORT_DIRECTORY ) )
    {
        return address{ nullptr };
    }

    const auto range_valid = [ image_size ]( u32 rva, usize size )
    {
        return rva && rva < image_size && size <= image_size - rva;
    };

    u8         *base{ static_cast< u8 * >( this->m_module_base ) };
    const auto *exports{ reinterpret_cast< IMAGE_EXPORT_DIRECTORY * >( base + directories[ 0 ].VirtualAddress ) };

    if ( !range_valid( exports->AddressOfNames, static_cast< usize >( exports->NumberOfNames ) * sizeof( u32 ) ) ||
         !range_valid( exports->AddressOfNameOrdinals, static_cast< usize >( exports->NumberOfNames ) * sizeof( u16 ) ) ||
         !range_valid( exports->AddressOfFunctions, static_cast< usize >( exports->NumberOfFunctions ) * sizeof( u32 ) ) )
    {
        return address{ nullptr };
    }

    const auto *names{ reinterpret_cast< u32 * >( base + exports->AddressOfNames ) };
    const auto *ordinals{ reinterpret_cast< u16 * >( base + exports->AddressOfNameOrdinals ) };
    const auto *functions{ reinterpret_cast< u32 * >( base + exports->AddressOfFunctions ) };

    for ( u32 index{ }; index < exports->NumberOfNames; ++index )
    {
        if ( !range_valid( names[ index ], 1 ) )
        {
            continue;
        }

        const char *export_name{ reinterpret_cast< char * >( base + names[ index ] ) };
        const usize export_name_capacity{ image_size - names[ index ] };
        const usize export_name_size{ str::bounded_length( export_name, export_name_capacity ) };

        if ( export_name_size == export_name_capacity ||
             !str::equals( export_name, export_name_size, func_name, str::length( func_name ) ) )
        {
            continue;
        }

        const u16 ordinal{ ordinals[ index ] };

        if ( ordinal >= exports->NumberOfFunctions )
        {
            return address{ nullptr };
        }

        const u32 function_rva{ functions[ ordinal ] };
        const u32 export_begin{ directories[ 0 ].VirtualAddress };
        const u32 export_end{ export_begin + directories[ 0 ].Size };

        if ( !range_valid( function_rva, 1 ) ||
             ( function_rva >= export_begin && function_rva < export_end ) )
        {
            return address{ nullptr };
        }

        return address{ base + function_rva };
    }

    return address{ nullptr };
}

util::address util::pe_module::find_export_fuzzy( const char *func_name ) const
{
    if ( !func_name )
    {
        return address{ nullptr };
    }

    const IMAGE_NT_HEADERS *nt{ this->get_nt_header( ) };
    if ( !nt || nt->FileHeader.SizeOfOptionalHeader < FIELD_OFFSET( IMAGE_OPTIONAL_HEADER, DataDirectory[ 1 ] ) )
    {
        return address{ nullptr };
    }

    const u32   image_size{ nt->OptionalHeader.SizeOfImage };
    const auto *directories{ nt->OptionalHeader.DataDirectory };

    if ( !directories ||
         !directories[ 0 ].VirtualAddress ||
         directories[ 0 ].VirtualAddress >= image_size ||
         directories[ 0 ].Size > image_size - directories[ 0 ].VirtualAddress ||
         directories[ 0 ].Size < sizeof( IMAGE_EXPORT_DIRECTORY ) )
    {
        return address{ nullptr };
    }

    const auto range_valid = [ image_size ]( u32 rva, usize size )
    {
        return rva && rva < image_size && size <= image_size - rva;
    };

    u8         *base{ static_cast< u8 * >( this->m_module_base ) };
    const auto *exports{ reinterpret_cast< IMAGE_EXPORT_DIRECTORY * >( base + directories[ 0 ].VirtualAddress ) };

    if ( !range_valid( exports->AddressOfNames, static_cast< usize >( exports->NumberOfNames ) * sizeof( u32 ) ) ||
         !range_valid( exports->AddressOfNameOrdinals, static_cast< usize >( exports->NumberOfNames ) * sizeof( u16 ) ) ||
         !range_valid( exports->AddressOfFunctions, static_cast< usize >( exports->NumberOfFunctions ) * sizeof( u32 ) ) )
    {
        return address{ nullptr };
    }

    const auto *names{ reinterpret_cast< u32 * >( base + exports->AddressOfNames ) };
    const auto *ordinals{ reinterpret_cast< u16 * >( base + exports->AddressOfNameOrdinals ) };
    const auto *functions{ reinterpret_cast< u32 * >( base + exports->AddressOfFunctions ) };

    for ( u32 index{ }; index < exports->NumberOfNames; ++index )
    {
        if ( !range_valid( names[ index ], 1 ) )
        {
            continue;
        }

        atl::string_view export_name{ reinterpret_cast< char * >( base + names[ index ] ) };
        const usize      export_name_capacity{ image_size - names[ index ] };
        const usize      export_name_size{ math::smaller( export_name.size( ), export_name_capacity ) };

        if ( export_name_size == export_name_capacity || export_name.contains( func_name ) )
        {
            continue;
        }

        const u16 ordinal{ ordinals[ index ] };

        if ( ordinal >= exports->NumberOfFunctions )
        {
            return address{ nullptr };
        }

        const u32 function_rva{ functions[ ordinal ] };
        const u32 export_begin{ directories[ 0 ].VirtualAddress };
        const u32 export_end{ export_begin + directories[ 0 ].Size };

        if ( !range_valid( function_rva, 1 ) ||
             ( function_rva >= export_begin && function_rva < export_end ) )
        {
            return address{ nullptr };
        }

        return address{ base + function_rva };
    }

    return address{ nullptr };
}

util::address util::pe_module::find_interface( const char *interface_name ) const
{
    if ( !interface_name )
    {
        return address{ nullptr };
    }

    if ( !this->m_create_interface_fn )
    {
        this->m_create_interface_fn = this->find_export( ATLAS_XORSTR( "CreateInterface" ).get( ) ).get_as< CreateInterfaceFn >( );
    }

    if ( !this->m_create_interface_fn )
    {
        return address{ nullptr };
    }

    return address{ this->m_create_interface_fn( interface_name, nullptr ) };
}

IMAGE_DOS_HEADER *util::pe_module::get_dos_header( ) const
{
    if ( !this->m_module_base )
    {
        return nullptr;
    }

    auto *dos{ static_cast< IMAGE_DOS_HEADER * >( this->m_module_base ) };
    if ( dos->e_magic != IMAGE_DOS_SIGNATURE )
    {
        return nullptr;
    }

    return dos;
}

IMAGE_NT_HEADERS *util::pe_module::get_nt_header( ) const
{
    const IMAGE_DOS_HEADER *dos{ this->get_dos_header( ) };
    if ( !dos )
    {
        return nullptr;
    }

    if ( dos->e_lfanew <= 0 )
    {
        return nullptr;
    }

    auto *nt{ reinterpret_cast< IMAGE_NT_HEADERS * >( reinterpret_cast< uptr >( dos ) + static_cast< uptr >( dos->e_lfanew ) ) };
    if ( nt->Signature != IMAGE_NT_SIGNATURE )
    {
        return nullptr;
    }

    return nt;
}
