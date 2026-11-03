#include "arena.hpp"

namespace gui
{
    arena g_arena{ };

    bool arena::reserve( usize bytes )
    {
        if ( this->m_base )
        {
            return true; // already standing: registration may run more than once
        }

        if ( !bytes )
        {
            return false;
        }

        auto *base{ static_cast< u8 * >( atl::malloc( bytes ) ) };

        if ( !base )
        {
            return false;
        }

        this->m_base   = base;
        this->m_size   = bytes;
        this->m_cursor = 0;
        return true;
    }

    void *arena::take( usize bytes, usize alignment )
    {
        if ( !this->m_base || !bytes || !alignment )
        {
            return nullptr;
        }

        // alignment is always alignof( type ), so always a power of two
        const usize mask{ alignment - 1u };
        const usize aligned{ ( this->m_cursor + mask ) & ~mask };

        // @ the second half of this also catches the wrap from aligning near the end
        if ( aligned > this->m_size || bytes > this->m_size - aligned )
        {
            return nullptr; // exhausted: the caller drops the node instead of scribbling
        }

        this->m_cursor = aligned + bytes;
        return this->m_base + aligned;
    }

    void arena::release( )
    {
        if ( !this->m_base )
        {
            return;
        }

        atl::free( this->m_base );

        this->m_base   = nullptr;
        this->m_size   = 0;
        this->m_cursor = 0;
    }
} // namespace gui
