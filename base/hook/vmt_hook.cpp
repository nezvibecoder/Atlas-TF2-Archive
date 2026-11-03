#include "vmt_hook.hpp"

namespace atlashook
{
    bool protect_vmt_range( void *address, usize size, u32 protect, u32 *old_protect )
    {
        if ( !address || !size || !old_protect )
        {
            return false;
        }

        void *base{ address };
        usize region_size{ size };
        return I( NtProtectVirtualMemory )( util::get_current_process( ), &base, &region_size, protect, old_protect ) >= 0;
    }

    bool write_vmt_entry( void **entry, void *value )
    {
        if ( !entry || !value )
        {
            return false;
        }

        u32   old_protect{ };
        void *base{ entry };
        usize size{ sizeof( void * ) };

        if ( I( NtProtectVirtualMemory )( util::get_current_process( ), &base, &size, PAGE_READWRITE, &old_protect ) < 0 )
        {
            return false;
        }

        *entry = value;

        u32 unused_protect{ };
        base = entry;
        size = sizeof( void * );
        bool restored{
            I( NtProtectVirtualMemory )( util::get_current_process( ), &base, &size, old_protect, &unused_protect ) >= 0
        };
        return restored && *entry == value;
    }

    bool protect_shadow_vmt( void **shadow_vmt, u32 method_count, u32 protect )
    {
        u32 old_protect{ };
        return protect_vmt_range( shadow_vmt, static_cast< usize >( method_count ) * sizeof( void * ), protect,
                                  &old_protect );
    }

    bool write_shadow_vmt_entry( void **shadow_vmt, u32 method_count, void **entry, void *value )
    {
        if ( !shadow_vmt || !method_count || !entry || !value )
        {
            return false;
        }

        usize entry_address{ reinterpret_cast< usize >( entry ) };
        usize shadow_begin{ reinterpret_cast< usize >( shadow_vmt ) };
        usize shadow_end{ shadow_begin + static_cast< usize >( method_count ) * sizeof( void * ) };

        if ( entry_address < shadow_begin || entry_address >= shadow_end )
        {
            return false;
        }

        u32   old_protect{ };
        usize size{ static_cast< usize >( method_count ) * sizeof( void * ) };

        if ( !protect_vmt_range( shadow_vmt, size, PAGE_READWRITE, &old_protect ) )
        {
            return false;
        }

        *entry = value;

        u32  unused_protect{ };
        bool restored{ protect_vmt_range( shadow_vmt, size, old_protect, &unused_protect ) };
        return restored && *entry == value;
    }

    vmt_method_hook::vmt_method_hook( vmt_method_hook &&other )
    {
        *this = static_cast< vmt_method_hook && >( other );
    }

    vmt_method_hook &vmt_method_hook::operator=( vmt_method_hook &&other )
    {
        if ( this == &other )
        {
            return *this;
        }

        this->reset( );

        this->m_entry       = other.m_entry;
        this->m_original    = other.m_original;
        this->m_destination = other.m_destination;
        this->m_enabled     = other.m_enabled;
        this->m_error       = other.m_error;

        other.m_entry = nullptr;
        other.m_original.reset( );
        other.m_destination = nullptr;
        other.m_enabled     = false;
        other.m_error       = error::none;

        return *this;
    }

    vmt_method_hook vmt_method_hook::create( void *object, u32 index, void *destination )
    {
        vmt_method_hook hook{ };

        if ( !object || !destination )
        {
            hook.m_error |= error::invalid_argument;
            return hook;
        }

        void **vmt{ *reinterpret_cast< void *** >( object ) };

        if ( !vmt )
        {
            hook.m_error |= error::invalid_argument;
            return hook;
        }

        hook.m_entry       = &vmt[ index ];
        hook.m_original    = vmt[ index ];
        hook.m_destination = destination;

        if ( !hook.m_original.get( ) )
        {
            hook.m_error |= error::invalid_argument;
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

    bool vmt_method_hook::enable( )
    {
        if ( this->m_enabled )
        {
            return true;
        }

        if ( !this->m_entry || !this->m_original.get( ) || !this->m_destination )
        {
            this->m_error |= error::invalid_argument;
            return false;
        }

        void *original{ this->m_original.get( ) };

        if ( *this->m_entry == this->m_destination )
        {
            this->m_enabled = true;
            return true;
        }

        if ( *this->m_entry != original )
        {
            this->m_error |= error::modified;
            return false;
        }

        if ( !write_vmt_entry( this->m_entry, this->m_destination ) )
        {
            this->m_error |= error::protection_failed;
            return false;
        }

        this->m_enabled = true;
        return true;
    }

    bool vmt_method_hook::disable( )
    {
        if ( !this->m_enabled )
        {
            return true;
        }

        void *original{ this->m_original.get( ) };

        if ( !this->m_entry || !original )
        {
            this->m_error |= error::invalid_argument;
            return false;
        }

        if ( *this->m_entry == original )
        {
            this->m_enabled = false;
            return true;
        }

        if ( *this->m_entry != this->m_destination )
        {
            this->m_error |= error::modified;
            return false;
        }

        if ( !write_vmt_entry( this->m_entry, original ) )
        {
            this->m_error |= error::protection_failed;
            return false;
        }

        this->m_enabled = false;
        return true;
    }

    void vmt_method_hook::reset( )
    {
        if ( this->m_enabled && !this->disable( ) )
        {
            return;
        }

        this->m_entry = nullptr;
        this->m_original.reset( );
        this->m_destination = nullptr;
        this->m_enabled     = false;
        this->m_error       = error::none;
    }

    vm_hook::vm_hook( vm_hook &&other )
    {
        *this = static_cast< vm_hook && >( other );
    }

    vm_hook &vm_hook::operator=( vm_hook &&other )
    {
        if ( this == &other )
        {
            return *this;
        }

        this->reset( );

        this->m_owner       = other.m_owner;
        this->m_entry       = other.m_entry;
        this->m_original    = other.m_original;
        this->m_destination = other.m_destination;
        this->m_error       = other.m_error;

        other.m_owner = nullptr;
        other.m_entry = nullptr;
        other.m_original.reset( );
        other.m_destination = nullptr;
        other.m_error       = error::none;

        return *this;
    }

    void vm_hook::reset( )
    {
        void *original{ this->m_original.get( ) };

        if ( original && this->m_owner && this->m_owner->m_shadow_vmt && this->m_entry &&
             *this->m_entry == this->m_destination )
        {
            if ( !write_shadow_vmt_entry( this->m_owner->m_shadow_vmt, this->m_owner->m_method_count, this->m_entry,
                                          original ) )
            {
                this->m_error |= error::protection_failed;
                return;
            }
        } else if ( original && this->m_entry && *this->m_entry != original )
        {
            this->m_error |= error::modified;
            return;
        }

        this->m_owner = nullptr;
        this->m_entry = nullptr;
        this->m_original.reset( );
        this->m_destination = nullptr;
        this->m_error       = error::none;
    }

    vmt_hook::vmt_hook( vmt_hook &&other )
    {
        *this = static_cast< vmt_hook && >( other );
    }

    vmt_hook &vmt_hook::operator=( vmt_hook &&other )
    {
        if ( this == &other )
        {
            return *this;
        }

        this->reset( );

        this->m_allocation   = static_cast< allocation && >( other.m_allocation );
        this->m_original_vmt = other.m_original_vmt;
        this->m_shadow_vmt   = other.m_shadow_vmt;
        this->m_method_count = other.m_method_count;
        this->m_objects      = static_cast< atl::vector< applied_object > && >( other.m_objects );
        this->m_error        = other.m_error;

        other.m_original_vmt.reset( );
        other.m_shadow_vmt   = nullptr;
        other.m_method_count = 0;
        other.m_error        = error::none;

        return *this;
    }

    vmt_hook vmt_hook::create( void *object, u32 method_count )
    {
        vmt_hook hook{ };

        if ( !object || !method_count )
        {
            hook.m_error |= error::invalid_argument;
            return hook;
        }

        void **original_vmt{ *reinterpret_cast< void *** >( object ) };

        if ( !original_vmt )
        {
            hook.m_error |= error::invalid_argument;
            return hook;
        }

        usize allocation_size{ static_cast< usize >( method_count ) * sizeof( void * ) };

        if ( !hook.m_allocation.create( allocation_size ) )
        {
            hook.m_error |= error::allocation_failed;
            return hook;
        }

        hook.m_original_vmt = original_vmt;
        hook.m_shadow_vmt   = reinterpret_cast< void ** >( hook.m_allocation.data( ) );
        hook.m_method_count = method_count;
        hook.m_error        = error::none;

        detail::copy_bytes( hook.m_shadow_vmt, original_vmt, allocation_size );

        if ( !protect_shadow_vmt( hook.m_shadow_vmt, method_count, PAGE_READONLY ) )
        {
            hook.m_error |= error::protection_failed;
            error failure{ hook.m_error };
            hook.reset( );
            hook.m_error = failure;
            return hook;
        }

        if ( !hook.apply( object ) )
        {
            error failure{ hook.m_error };
            hook.reset( );
            hook.m_error = failure;
        }

        return hook;
    }

    void vmt_hook::reset( )
    {
        for ( usize index{ }; index < this->m_objects.size( ); ++index )
        {
            void  *object{ this->m_objects[ index ].object };
            void **original_vmt{ this->m_objects[ index ].original_vmt };

            if ( !object || !original_vmt )
            {
                continue;
            }

            auto *object_vmt{ reinterpret_cast< void *** >( object ) };

            if ( *object_vmt == this->m_shadow_vmt )
            {
                write_vmt_entry( reinterpret_cast< void ** >( object_vmt ), original_vmt );
            }
        }

        this->m_objects.reset( );

        this->m_allocation.reset( );
        this->m_original_vmt.reset( );
        this->m_shadow_vmt   = nullptr;
        this->m_method_count = 0;
        this->m_error        = error::none;
    }

    bool vmt_hook::apply( void *object )
    {
        if ( !object || !this->m_shadow_vmt )
        {
            this->m_error |= error::invalid_argument;
            return false;
        }

        for ( usize index{ }; index < this->m_objects.size( ); ++index )
        {
            if ( this->m_objects[ index ].object == object )
            {
                this->m_error = error::none;
                return true;
            }
        }

        auto  *object_vmt{ reinterpret_cast< void *** >( object ) };
        void **original_vmt{ *object_vmt };

        if ( !original_vmt )
        {
            this->m_error |= error::invalid_argument;
            return false;
        }

        applied_object applied{ object, original_vmt };

        if ( !this->m_objects.push_back( applied ) )
        {
            this->m_error |= error::allocation_failed;
            return false;
        }

        if ( !write_vmt_entry( reinterpret_cast< void ** >( object_vmt ), this->m_shadow_vmt ) )
        {
            this->m_objects.erase( this->m_objects.size( ) - 1u );
            this->m_error |= error::protection_failed;
            return false;
        }

        this->m_error = error::none;
        return true;
    }

    void vmt_hook::remove( void *object )
    {
        if ( !object )
        {
            return;
        }

        for ( usize index{ }; index < this->m_objects.size( ); ++index )
        {
            if ( this->m_objects[ index ].object != object )
            {
                continue;
            }

            auto *object_vmt{ reinterpret_cast< void *** >( object ) };

            if ( *object_vmt == this->m_shadow_vmt )
            {
                write_vmt_entry( reinterpret_cast< void ** >( object_vmt ), this->m_objects[ index ].original_vmt );
            }

            this->m_objects.erase( index );
            return;
        }
    }

    vm_hook vmt_hook::hook_method( u32 index, void *destination )
    {
        vm_hook hook{ };

        void **original_vmt{ this->m_original_vmt.get( ) };

        if ( !this->m_shadow_vmt || !original_vmt || !destination )
        {
            hook.m_error |= error::invalid_argument;
            return hook;
        }

        if ( index >= this->m_method_count )
        {
            hook.m_error |= error::bad_index;
            return hook;
        }

        if ( this->m_shadow_vmt[ index ] != original_vmt[ index ] )
        {
            hook.m_error |= error::already_hooked;
            return hook;
        }

        hook.m_owner       = this;
        hook.m_entry       = &this->m_shadow_vmt[ index ];
        hook.m_original    = original_vmt[ index ];
        hook.m_destination = destination;
        hook.m_error       = error::none;

        if ( !write_shadow_vmt_entry( this->m_shadow_vmt, this->m_method_count, hook.m_entry, destination ) )
        {
            hook.reset( );
            hook.m_error |= error::protection_failed;
            return hook;
        }

        return hook;
    }
} // namespace atlashook
