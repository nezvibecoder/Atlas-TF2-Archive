#include "inline_hook.hpp"
#if defined( __x86_64__ )
#include "libs/hde64.hpp"
#else
#include "libs/hde32.hpp"
#endif

namespace atlashook
{
#if defined( __x86_64__ )
    using hde_s = hde64s;
    inline constexpr u32 inline_patch_size{ 14u };
    inline constexpr u32 external_call_size{ 16u };
    inline constexpr u32 external_jump_size{ 14u };
    inline constexpr u32 external_condition_size{ 16u };
    inline constexpr u32 internal_condition_size{ 6u };
    inline constexpr u32 internal_rel_size{ 5u };
#else
    using hde_s = hde32s;
    inline constexpr u32 inline_patch_size{ 5u };
    inline constexpr u32 external_call_size{ 5u };
    inline constexpr u32 external_jump_size{ 5u };
    inline constexpr u32 external_condition_size{ 6u };
    inline constexpr u32 internal_condition_size{ 6u };
    inline constexpr u32 internal_rel_size{ 5u };
#endif

    ATLAS_FORCE_INLINE unsigned int hde_disasm( const void *code, hde_s *hs )
    {
#if defined( __x86_64__ )
        return hde64_disasm( code, hs );
#else
        return hde32_disasm( code, hs );
#endif
    }

    enum class relative_kind : u8
    {
        none,
        call,
        jump,
        condition,
    };

    struct instruction_info
    {
        hde_s         decoded{ };
        u32           source_offset{ };
        u32           trampoline_offset{ };
        u32           output_size{ };
        i64           branch_target{ };
        relative_kind kind{ relative_kind::none };
        bool          internal{ };
    };

    struct memory_basic_information
    {
        void  *base_address{ };
        void  *allocation_base{ };
        u32    allocation_protect{ };
#if defined( __x86_64__ )
        u32    alignment{ };
#endif
        usize  region_size{ };
        u32    state{ };
        u32    protect{ };
        u32    type{ };
    };

    static_assert( sizeof( memory_basic_information ) == sizeof( MEMORY_BASIC_INFORMATION ) );

    bool executable_protect( u32 protect )
    {
        const u32 clean{ protect & ~PAGE_GUARD };
        return clean == PAGE_EXECUTE || clean == PAGE_EXECUTE_READ ||
               clean == PAGE_EXECUTE_READWRITE || clean == PAGE_EXECUTE_WRITECOPY;
    }

    bool committed_range( const void *address, usize size, bool executable )
    {
        if ( !address || !size )
        {
            return false;
        }

        memory_basic_information information{ };
        usize                    returned{ };

        if ( I( NtQueryVirtualMemory )(
                 util::get_current_process( ), const_cast< void * >( address ), 0,
                 &information, sizeof( information ), &returned ) < 0 )
        {
            return false;
        }

        const uptr begin{ reinterpret_cast< uptr >( address ) };
        const uptr region{ reinterpret_cast< uptr >( information.base_address ) };

        if ( information.state != MEM_COMMIT ||
             ( information.protect & ( PAGE_NOACCESS | PAGE_GUARD ) ) != 0 ||
             begin < region || size > information.region_size - ( begin - region ) )
        {
            return false;
        }

        return !executable || executable_protect( information.protect );
    }

    void *follow_existing_hook_chain( void *target )
    {
        u8 *cursor{ static_cast< u8 * >( target ) };
        u8 *visited[ 8 ]{ };

        for ( usize depth{ }; depth < 8; ++depth )
        {
            if ( !committed_range( cursor, 6, true ) )
            {
                return nullptr;
            }

            for ( usize index{ }; index < depth; ++index )
            {
                if ( visited[ index ] == cursor )
                {
                    return nullptr;
                }
            }

            visited[ depth ] = cursor;

            if ( cursor[ 0 ] == 0xE9 )
            {
                const i32 displacement{ *reinterpret_cast< const i32 * >( cursor + 1 ) };
                cursor = cursor + 5 + displacement;
                continue;
            }

            if ( cursor[ 0 ] == 0xFF && cursor[ 1 ] == 0x25 )
            {
                const i32 displacement{ *reinterpret_cast< const i32 * >( cursor + 2 ) };
#if defined( __x86_64__ )
                u8 *slot{ cursor + 6 + displacement };
#else
                u8 *slot{ reinterpret_cast< u8 * >( static_cast< uptr >( static_cast< u32 >( displacement ) ) ) };
#endif

                if ( !committed_range( slot, sizeof( void * ), false ) )
                {
                    return nullptr;
                }

                cursor = *reinterpret_cast< u8 ** >( slot );
                continue;
            }

            return cursor;
        }

        return nullptr;
    }

#if defined( __x86_64__ )
    bool is_rip_relative( const hde_s &instruction )
    {
        if ( ( instruction.flags & F_MODRM ) == 0 || instruction.modrm_mod != 0 ||
             instruction.p_67 )
        {
            return false;
        }

        if ( instruction.modrm_rm == 5 )
        {
            return true;
        }

        return ( instruction.flags & F_SIB ) != 0 && instruction.sib_base == 5;
    }

    bool fits_i32( i64 value )
    {
        return value >= static_cast< i64 >( -2147483647 ) - 1 &&
               value <= static_cast< i64 >( 2147483647 );
    }
#endif

    relative_kind classify_relative( const hde_s &instruction )
    {
        if ( ( instruction.flags & F_RELATIVE ) == 0 )
        {
            return relative_kind::none;
        }

        if ( instruction.opcode == 0xE8 )
        {
            return relative_kind::call;
        }

        if ( instruction.opcode == 0xE9 || instruction.opcode == 0xEB )
        {
            return relative_kind::jump;
        }

        if ( ( instruction.opcode >= 0x70 && instruction.opcode <= 0x7F ) ||
             ( instruction.opcode == 0x0F && instruction.opcode2 >= 0x80 &&
               instruction.opcode2 <= 0x8F ) )
        {
            return relative_kind::condition;
        }

        return relative_kind::none;
    }

    i64 relative_target( const u8 *source, const hde_s &instruction )
    {
        i64 displacement{ };

        if ( ( instruction.flags & F_IMM8 ) != 0 )
        {
            displacement = static_cast< i8 >( source[ instruction.len - 1 ] );
        } else if ( ( instruction.flags & F_IMM32 ) != 0 )
        {
            displacement = *reinterpret_cast< const i32 * >(
                source + instruction.len - sizeof( i32 ) );
        }

        return reinterpret_cast< i64 >( source + instruction.len ) + displacement;
    }

    bool is_instruction_boundary( const instruction_info *instructions, u32 count,
                                  u32 stolen_size, u32 offset )
    {
        if ( offset == stolen_size )
        {
            return true;
        }

        for ( u32 index{ }; index < count; ++index )
        {
            if ( instructions[ index ].source_offset == offset )
            {
                return true;
            }
        }

        return false;
    }

    bool decode_instructions( const void *target, instruction_info *instructions,
                              u32 &instruction_count, u32 &stolen_size,
                              u32 &trampoline_code_size )
    {
        auto *bytes{ reinterpret_cast< const u8 * >( target ) };

        instruction_count    = 0;
        stolen_size          = 0;
        trampoline_code_size = 0;

        while ( stolen_size < inline_patch_size )
        {
            if ( instruction_count >= 16u )
            {
                return false;
            }

            instruction_info &info{ instructions[ instruction_count ] };
            const u32 instruction_size{ hde_disasm(
                bytes + stolen_size, &info.decoded ) };

            if ( !instruction_size || ( info.decoded.flags & F_ERROR ) != 0 ||
                 stolen_size + instruction_size > 64u )
            {
                return false;
            }

            info.source_offset = stolen_size;
            info.kind          = classify_relative( info.decoded );

            if ( ( info.decoded.flags & F_RELATIVE ) != 0 &&
                 ( info.kind == relative_kind::none ||
                   ( info.decoded.flags & ( F_IMM8 | F_IMM32 ) ) == 0 ) )
            {
                return false;
            }

            stolen_size += instruction_size;
            ++instruction_count;
        }

        for ( u32 index{ }; index < instruction_count; ++index )
        {
            instruction_info &info{ instructions[ index ] };

            if ( info.kind != relative_kind::none )
            {
                info.branch_target = relative_target(
                    bytes + info.source_offset, info.decoded );
                const i64 relative_offset{ info.branch_target -
                                           reinterpret_cast< i64 >( bytes ) };
                info.internal = relative_offset >= 0 &&
                                relative_offset <= stolen_size;

                if ( info.internal && !is_instruction_boundary(
                         instructions, instruction_count, stolen_size,
                         static_cast< u32 >( relative_offset ) ) )
                {
                    return false;
                }
            }

            info.trampoline_offset = trampoline_code_size;

            if ( info.kind == relative_kind::none )
            {
                info.output_size = info.decoded.len;
            } else if ( info.internal )
            {
                info.output_size = info.kind == relative_kind::condition
                                       ? internal_condition_size
                                       : internal_rel_size;
            } else if ( info.kind == relative_kind::call )
            {
                info.output_size = external_call_size;
            } else if ( info.kind == relative_kind::jump )
            {
                info.output_size = external_jump_size;
            } else
            {
                info.output_size = external_condition_size;
            }

            trampoline_code_size += info.output_size;
        }

        return true;
    }

    bool bytes_equal( const void *left, const void *right, usize size )
    {
        if ( !left || !right )
        {
            return false;
        }

        auto *left_bytes{ reinterpret_cast< const volatile u8 * >( left ) };
        auto *right_bytes{ reinterpret_cast< const volatile u8 * >( right ) };

        for ( usize index{ }; index < size; ++index )
        {
            if ( left_bytes[ index ] != right_bytes[ index ] )
            {
                return false;
            }
        }

        return true;
    }

    void write_absolute_jump( void *source, void *destination )
    {
        auto *bytes{ reinterpret_cast< u8 * >( source ) };

#if defined( __x86_64__ )
        bytes[ 0 ]                              = 0xFF;
        bytes[ 1 ]                              = 0x25;
        *reinterpret_cast< u32 * >( bytes + 2 ) = 0;
        *reinterpret_cast< u64 * >( bytes + 6 ) = reinterpret_cast< u64 >( destination );
#else
        const u32 displacement{ static_cast< u32 >(
            reinterpret_cast< uptr >( destination ) - reinterpret_cast< uptr >( bytes ) - 5u ) };
        bytes[ 0 ]                              = 0xE9;
        *reinterpret_cast< u32 * >( bytes + 1 ) = displacement;
#endif
    }
    void build_inline_patch( atl::array< u8, 64 > &patch, void *source, void *destination )
    {
        patch.fill( 0x90 );
#if defined( __x86_64__ )
        ATLAS_UNUSED_PARAM( source );
        write_absolute_jump( patch.data( ), destination );
#else
        auto     *bytes{ patch.data( ) };
        const u32 displacement{ static_cast< u32 >(
            reinterpret_cast< uptr >( destination ) - reinterpret_cast< uptr >( source ) - 5u ) };
        bytes[ 0 ]                              = 0xE9;
        *reinterpret_cast< u32 * >( bytes + 1 ) = displacement;
#endif
    }

    bool write_protected_bytes( void *destination, const void *source, usize size )
    {
        u32   old_protect{ };
        void *base{ destination };
        usize region_size{ size };

        if ( I( NtProtectVirtualMemory )( util::get_current_process( ), &base, &region_size, PAGE_EXECUTE_READWRITE,
                                          &old_protect ) < 0 )
        {
            return false;
        }

        atl::memcpy( destination, source, size );

        u32 unused_protect{ };
        base        = destination;
        region_size = size;
        bool restored{ I( NtProtectVirtualMemory )( util::get_current_process( ), &base, &region_size, old_protect,
                                                    &unused_protect ) >= 0 };
        I( NtFlushInstructionCache )( util::get_current_process( ), destination, size );

        return restored && bytes_equal( destination, source, size );
    }

    bool write_inline_patch( void *target, void *destination, usize size )
    {
        atl::array< u8, 64 > patch{ };
        build_inline_patch( patch, target, destination );

        return write_protected_bytes( target, patch.data( ), size );
    }

    bool has_inline_patch( void *target, void *destination, usize size )
    {
        if ( !target || !destination || !size || size > 64u )
        {
            return false;
        }

        atl::array< u8, 64 > patch{ };
        build_inline_patch( patch, target, destination );

        return bytes_equal( target, patch.data( ), size );
    }

    u8 original_byte_mask( usize key, usize index )
    {
        usize mixed{ detail::mix_entropy( key ^ ( index + static_cast< usize >( 0xA24BAED4963EE407ull ) ) ) };
        return static_cast< u8 >( mixed >> ( ( index & 7u ) * 8u ) );
    }

    void crypt_original_bytes( void *bytes, u32 size, usize key )
    {
        auto *data{ reinterpret_cast< u8 * >( bytes ) };

        for ( usize index{ }; index < size; ++index )
        {
            data[ index ] ^= original_byte_mask( key, index );
        }
    }

    usize make_original_key( const void *storage, const void *target, u32 size )
    {
        usize value{ reinterpret_cast< usize >( target ) ^ static_cast< usize >( size ) };
        return detail::pointer_key( storage, value );
    }

    void free_trampoline( void *base );

    void *allocate_trampoline( const void *target, usize size )
    {
        if ( !size )
        {
            return nullptr;
        }

#if defined( __x86_64__ )
        const uptr target_address{ reinterpret_cast< uptr >( target ) };
        const uptr aligned_target{ target_address & ~static_cast< uptr >( 0xFFFFu ) };

        for ( uptr step{ 0x10000u }; step < 0x7FFF0000u; step += 0x10000u )
        {
            for ( u32 direction{ }; direction < 2; ++direction )
            {
                if ( direction == 0 && aligned_target < step )
                {
                    continue;
                }

                const uptr candidate{ direction == 0 ? aligned_target - step
                                                      : aligned_target + step };
                void *base{ reinterpret_cast< void * >( candidate ) };
                usize region_size{ size };

                if ( I( NtAllocateVirtualMemory )(
                         util::get_current_process( ), &base, 0, &region_size,
                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) >= 0 )
                {
                    const i64 distance{ reinterpret_cast< i64 >( base ) -
                                        reinterpret_cast< i64 >( target ) };

                    if ( fits_i32( distance ) )
                    {
                        return base;
                    }

                    free_trampoline( base );
                }
            }
        }

        return nullptr;
#else
        ATLAS_UNUSED_PARAM( target );
        void *base{ nullptr };
        usize region_size{ size };

        if ( I( NtAllocateVirtualMemory )( util::get_current_process( ), &base, 0, &region_size,
                                           MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) >= 0 )
        {
            return base;
        }

        return nullptr;
#endif
    }

    void free_trampoline( void *base )
    {
        if ( !base )
        {
            return;
        }

        usize region_size{ };
        I( NtFreeVirtualMemory )( util::get_current_process( ), &base, &region_size, MEM_RELEASE );
    }

    u32 trampoline_offset_for( const instruction_info *instructions, u32 count,
                               u32 stolen_size, u32 code_size, u32 source_offset )
    {
        if ( source_offset == stolen_size )
        {
            return code_size;
        }

        for ( u32 index{ }; index < count; ++index )
        {
            if ( instructions[ index ].source_offset == source_offset )
            {
                return instructions[ index ].trampoline_offset;
            }
        }

        return static_cast< u32 >( -1 );
    }

    void write_absolute_call( void *destination, u64 target )
    {
        auto *bytes{ static_cast< u8 * >( destination ) };
#if defined( __x86_64__ )
        bytes[ 0 ] = 0xFF;
        bytes[ 1 ] = 0x15;
        *reinterpret_cast< u32 * >( bytes + 2 ) = 2;
        bytes[ 6 ] = 0xEB;
        bytes[ 7 ] = 8;
        *reinterpret_cast< u64 * >( bytes + 8 ) = target;
#else
        const u32 displacement{ static_cast< u32 >(
            static_cast< u32 >( target ) - reinterpret_cast< uptr >( bytes ) - 5u ) };
        bytes[ 0 ]                              = 0xE8;
        *reinterpret_cast< u32 * >( bytes + 1 ) = displacement;
#endif
    }

    bool build_trampoline( const u8 *source, u8 *destination,
                           instruction_info *instructions, u32 instruction_count,
                           u32 stolen_size, u32 code_size )
    {
        for ( u32 index{ }; index < instruction_count; ++index )
        {
            instruction_info &info{ instructions[ index ] };
            const u8 *source_instruction{ source + info.source_offset };
            u8 *output{ destination + info.trampoline_offset };

            if ( info.kind == relative_kind::none )
            {
                detail::copy_bytes( output, source_instruction, info.decoded.len );

#if defined( __x86_64__ )
                if ( is_rip_relative( info.decoded ) )
                {
                    u32 immediate_size{ };

                    if ( ( info.decoded.flags & F_IMM8 ) != 0 )
                    {
                        immediate_size = 1;
                    } else if ( ( info.decoded.flags & F_IMM16 ) != 0 )
                    {
                        immediate_size = 2;
                    } else if ( ( info.decoded.flags & F_IMM32 ) != 0 )
                    {
                        immediate_size = 4;
                    } else if ( ( info.decoded.flags & F_IMM64 ) != 0 )
                    {
                        immediate_size = 8;
                    }

                    if ( info.decoded.len < immediate_size + sizeof( i32 ) )
                    {
                        return false;
                    }

                    const u32 displacement_offset{ static_cast< u32 >(
                        info.decoded.len - immediate_size - sizeof( i32 ) ) };
                    const i32 displacement{ *reinterpret_cast< const i32 * >(
                        source_instruction + displacement_offset ) };
                    const i64 absolute{ reinterpret_cast< i64 >(
                                            source_instruction + info.decoded.len ) +
                                        displacement };
                    const i64 relocated{ absolute - reinterpret_cast< i64 >(
                        output + info.decoded.len ) };

                    if ( !fits_i32( relocated ) )
                    {
                        return false;
                    }

                    *reinterpret_cast< i32 * >( output + displacement_offset ) =
                        static_cast< i32 >( relocated );
                }
#endif

                continue;
            }

            u8 condition{ };
            if ( info.kind == relative_kind::condition )
            {
                condition = info.decoded.opcode == 0x0F
                                ? static_cast< u8 >( info.decoded.opcode2 & 0x0F )
                                : static_cast< u8 >( info.decoded.opcode & 0x0F );
            }

            if ( info.internal )
            {
                const u32 source_target_offset{ static_cast< u32 >(
                    info.branch_target - reinterpret_cast< i64 >( source ) ) };
                const u32 mapped_target{ trampoline_offset_for(
                    instructions, instruction_count, stolen_size, code_size,
                    source_target_offset ) };

                if ( mapped_target == static_cast< u32 >( -1 ) )
                {
                    return false;
                }

                if ( info.kind == relative_kind::condition )
                {
                    output[ 0 ] = 0x0F;
                    output[ 1 ] = static_cast< u8 >( 0x80u | condition );
                    *reinterpret_cast< i32 * >( output + 2 ) = static_cast< i32 >(
                        mapped_target - ( info.trampoline_offset + 6u ) );
                } else
                {
                    output[ 0 ] = info.kind == relative_kind::call ? 0xE8 : 0xE9;
                    *reinterpret_cast< i32 * >( output + 1 ) = static_cast< i32 >(
                        mapped_target - ( info.trampoline_offset + 5u ) );
                }

                continue;
            }

            if ( info.kind == relative_kind::call )
            {
                write_absolute_call( output, static_cast< u64 >( info.branch_target ) );
            } else if ( info.kind == relative_kind::jump )
            {
                write_absolute_jump( output, reinterpret_cast< void * >( info.branch_target ) );
            } else
            {
#if defined( __x86_64__ )
                output[ 0 ] = static_cast< u8 >( 0x70u | ( condition ^ 1u ) );
                output[ 1 ] = 14;
                write_absolute_jump( output + 2,
                                     reinterpret_cast< void * >( info.branch_target ) );
#else
                output[ 0 ] = 0x0F;
                output[ 1 ] = static_cast< u8 >( 0x80u | condition );
                const u32 displacement{ static_cast< u32 >(
                    static_cast< u32 >( info.branch_target ) -
                    reinterpret_cast< uptr >( output + 6 ) ) };
                *reinterpret_cast< u32 * >( output + 2 ) = displacement;
#endif
            }
        }

        write_absolute_jump( destination + code_size,
                             const_cast< u8 * >( source ) + stolen_size );
        return true;
    }

    static inline_hook *s_registered_head{ };

    inline_hook *inline_hook::registered_head( )
    {
        return s_registered_head;
    }

    void inline_hook::register_hook( inline_hook *hook )
    {
        hook->m_registered_next = s_registered_head;
        s_registered_head       = hook;
    }

    void inline_hook::unregister_hook( inline_hook *hook )
    {
        for ( inline_hook **link{ &s_registered_head }; *link; link = &( *link )->m_registered_next )
        {
            if ( *link == hook )
            {
                *link                   = hook->m_registered_next;
                hook->m_registered_next = nullptr;
                return;
            }
        }
    }

    void inline_hook::replace_hook( inline_hook *from, inline_hook *to )
    {
        for ( inline_hook **link{ &s_registered_head }; *link; link = &( *link )->m_registered_next )
        {
            if ( *link == from )
            {
                *link                   = to;
                to->m_registered_next   = from->m_registered_next;
                from->m_registered_next = nullptr;
                return;
            }
        }
    }

    bool inline_hook::restore_original( u8 *buffer, usize buffer_size, usize offset ) const
    {
        if ( !this->m_enabled || !this->m_original_size || !this->m_original_key ||
             offset > buffer_size || this->m_original_size > buffer_size - offset )
        {
            return false;
        }

        atl::array< u8, 64 > patch{ };
        build_inline_patch( patch, this->m_target, this->m_destination );

        if ( !bytes_equal( buffer + offset, patch.data( ), this->m_original_size ) )
        {
            return false;
        }

        this->copy_original( buffer + offset, 0, this->m_original_size );
        return true;
    }

    usize inline_hook::copy_original( u8 *dst, usize offset, usize size ) const
    {
        if ( !dst || !this->m_original_size || !this->m_original_key || offset >= this->m_original_size )
        {
            return 0;
        }

        if ( size > this->m_original_size - offset )
        {
            size = this->m_original_size - offset;
        }

        atl::array< u8, 64 > original{ this->m_original_bytes };
        crypt_original_bytes( original.data( ), this->m_original_size, this->m_original_key );
        detail::copy_bytes( dst, original.data( ) + offset, size );
        return size;
    }

    inline_hook::inline_hook( inline_hook &&other )
    {
        *this = static_cast< inline_hook && >( other );
    }

    inline_hook &inline_hook::operator=( inline_hook &&other )
    {
        if ( this == &other )
        {
            return *this;
        }

        this->reset( );

        this->m_target             = other.m_target;
        this->m_destination        = other.m_destination;
        this->m_trampoline         = other.m_trampoline;
        this->m_trampoline_size    = other.m_trampoline_size;
        this->m_trampoline_protect = other.m_trampoline_protect;
        this->m_original_size      = other.m_original_size;
        this->m_original_key       = other.m_original_key;
        detail::copy_bytes( this->m_original_bytes.data( ), other.m_original_bytes.data( ), this->m_original_bytes.size_bytes( ) );
        this->m_enabled = other.m_enabled;
        this->m_error   = other.m_error;

        replace_hook( &other, this );

        other.m_target      = nullptr;
        other.m_destination = nullptr;
        other.m_trampoline.reset( );
        other.m_trampoline_size    = 0;
        other.m_trampoline_protect = 0;
        other.m_original_bytes.reset( );
        other.m_original_key    = 0;
        other.m_original_size   = 0;
        other.m_registered_next = nullptr;
        other.m_enabled         = false;
        other.m_error           = error::none;

        return *this;
    }

    inline_hook inline_hook::create( void *target, void *destination, flags options )
    {
        inline_hook hook{ };

        target = follow_existing_hook_chain( target );

        hook.m_target      = target;
        hook.m_destination = destination;

        if ( !target || !destination )
        {
            hook.m_error |= error::invalid_argument;
            return hook;
        }

        instruction_info instructions[ 16 ]{ };
        u32 instruction_count{ };
        u32 trampoline_code_size{ };

        if ( !decode_instructions( target, instructions, instruction_count,
                                   hook.m_original_size, trampoline_code_size ) )
        {
            hook.m_error |= error::unsupported;
            return hook;
        }

        hook.m_trampoline_size = trampoline_code_size + external_jump_size;
        detail::copy_bytes( hook.m_original_bytes.data( ), target, hook.m_original_size );

        usize trampoline_size{ hook.m_trampoline_size };
        void *trampoline{ allocate_trampoline( target, trampoline_size ) };

        if ( !trampoline )
        {
            hook.m_error |= error::allocation_failed;
            return hook;
        }

        auto *trampoline_bytes{ reinterpret_cast< u8 * >( trampoline ) };

        if ( !build_trampoline( reinterpret_cast< const u8 * >( target ),
                                trampoline_bytes, instructions, instruction_count,
                                hook.m_original_size, trampoline_code_size ) )
        {
            free_trampoline( trampoline );
            hook.m_error |= error::unsupported;
            return hook;
        }

        I( NtFlushInstructionCache )( util::get_current_process( ), trampoline, trampoline_size );

        u32   writable_protect{ };
        void *trampoline_base{ trampoline };
        usize protected_size{ trampoline_size };

        if ( I( NtProtectVirtualMemory )( util::get_current_process( ), &trampoline_base, &protected_size,
                                          PAGE_EXECUTE_READ, &writable_protect ) < 0 )
        {
            free_trampoline( trampoline );
            hook.m_error |= error::protection_failed;
            return hook;
        }

        hook.m_trampoline         = trampoline;
        hook.m_trampoline_size    = trampoline_size;
        hook.m_trampoline_protect = writable_protect;
        hook.m_original_key       = make_original_key( &hook, target, hook.m_original_size );
        crypt_original_bytes( hook.m_original_bytes.data( ), hook.m_original_size, hook.m_original_key );
        register_hook( &hook );

        if ( ( options & start_disabled ) != 0 )
        {
            hook.m_enabled = false;
            return hook;
        }

        if ( !hook.enable( ) )
        {
            error failure{ hook.m_error };
            hook.reset( );
            hook.m_error = failure;
        }

        return hook;
    }

    bool inline_hook::enable( )
    {
        if ( this->m_enabled )
        {
            return true;
        }

        if ( !this->m_target || !this->m_destination || !this->m_trampoline.get( ) || !this->m_original_size )
        {
            this->m_error |= error::invalid_argument;
            return false;
        }

        if ( !write_inline_patch( this->m_target, this->m_destination, this->m_original_size ) )
        {
            this->m_error |= error::protection_failed;
            return false;
        }

        this->m_enabled = true;
        return true;
    }

    bool inline_hook::disable( )
    {
        if ( !this->m_enabled )
        {
            return true;
        }

        if ( !this->m_target || !this->m_destination || !this->m_original_size || !this->m_original_key )
        {
            this->m_error |= error::invalid_argument;
            return false;
        }

        atl::array< u8, 64 > original_bytes{ this->m_original_bytes };
        crypt_original_bytes( original_bytes.data( ), this->m_original_size, this->m_original_key );

        if ( bytes_equal( this->m_target, original_bytes.data( ), this->m_original_size ) )
        {
            original_bytes.reset( );
            this->m_enabled = false;
            return true;
        }

        if ( !has_inline_patch( this->m_target, this->m_destination, this->m_original_size ) )
        {
            original_bytes.reset( );
            this->m_error |= error::modified;
            return false;
        }

        if ( !write_protected_bytes( this->m_target, original_bytes.data( ), this->m_original_size ) )
        {
            original_bytes.reset( );
            this->m_error |= error::protection_failed;
            return false;
        }

        original_bytes.reset( );
        this->m_enabled = false;
        return true;
    }

    void inline_hook::reset( )
    {
        if ( this->m_enabled && !this->disable( ) )
        {
            return;
        }

        void *trampoline{ this->m_trampoline.get( ) };

        if ( trampoline )
        {
            if ( this->m_trampoline_protect )
            {
                u32   unused_protect{ };
                void *trampoline_base{ trampoline };
                usize trampoline_size{ this->m_trampoline_size };
                I( NtProtectVirtualMemory )( util::get_current_process( ), &trampoline_base, &trampoline_size,
                                             this->m_trampoline_protect, &unused_protect );
            }

            free_trampoline( trampoline );
        }

        unregister_hook( this );

        this->m_target      = nullptr;
        this->m_destination = nullptr;
        this->m_trampoline.reset( );
        this->m_trampoline_size    = 0;
        this->m_trampoline_protect = 0;
        this->m_original_bytes.reset( );
        this->m_original_key    = 0;
        this->m_original_size   = 0;
        this->m_registered_next = nullptr;
        this->m_enabled         = false;
        this->m_error         = error::none;
    }
} // namespace atlashook
