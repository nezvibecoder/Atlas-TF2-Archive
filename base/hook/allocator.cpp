#include "allocator.hpp"

namespace atlashook
{
    allocation::allocation( allocation &&other )
    {
        *this = static_cast< allocation && >( other );
    }

    allocation &allocation::operator=( allocation &&other )
    {
        if ( this == &other )
        {
            return *this;
        }

        this->reset( );

        this->m_data = other.m_data;
        this->m_size = other.m_size;

        other.m_data = nullptr;
        other.m_size = 0;

        return *this;
    }

    bool allocation::create( usize size )
    {
        this->reset( );

        if ( !size )
        {
            return false;
        }

        void *base{ };
        usize region_size{ size };

        // if we want this to actually be heavy in the protection context I won't use the IMemAlloc for some stuff of ours;
        if ( I( NtAllocateVirtualMemory )( util::get_current_process( ), &base, 0, &region_size,
                                           MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) < 0 )
        {
            return false;
        }

        this->m_data = reinterpret_cast< u8 * >( base );
        this->m_size = region_size;
        return true;
    }

    void allocation::reset( )
    {
        if ( !this->m_data )
        {
            this->m_size = 0;
            return;
        }

        void *base{ this->m_data };
        usize region_size{ };

        I( NtFreeVirtualMemory )( util::get_current_process( ), &base, &region_size, MEM_RELEASE );

        this->m_data = nullptr;
        this->m_size = 0;
    }
} // namespace atlashook
