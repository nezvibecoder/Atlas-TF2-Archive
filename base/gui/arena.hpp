#pragma once

#include <new>

#include "runtime/memory.hpp"
#include "util/typedefs.hpp"

namespace gui
{
    // @ every operator new here lands in NtAllocateVirtualMemory with MEM_RESERVE, so a tree
    // of ~150 small nodes would burn ~150 separate 64KB reservations. the tree is built once
    // at startup and lives until shutdown, so one reservation with a bump cursor is both
    // cheaper and simpler than freeing nodes we were never going to free anyway.
    inline constexpr usize ARENA_DEFAULT_SIZE{ 2u * 1024u * 1024u };

    class arena
    {
    public:
        bool  reserve( usize bytes = ARENA_DEFAULT_SIZE );
        void *take( usize bytes, usize alignment );
        void  release( );

        bool ready( ) const
        {
            return this->m_base != nullptr;
        }

        usize used( ) const
        {
            return this->m_cursor;
        }

        usize size( ) const
        {
            return this->m_size;
        }

    private:
        u8   *m_base{ };
        usize m_size{ };
        usize m_cursor{ };
    };

    extern arena g_arena;
    template< typename type_t, typename... args_t >
    type_t *make( args_t &&...arguments )
    {
        void *slot{ g_arena.take( sizeof( type_t ), alignof( type_t ) ) };

        if ( !slot )
        {
            return nullptr;
        }

        return new ( slot ) type_t( static_cast< args_t && >( arguments )... );
    }
} // namespace gui
