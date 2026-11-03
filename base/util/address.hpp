#pragma once

#include "typedefs.hpp"

namespace util
{
    class address
    {
    public:
        address( )  = default;
        ~address( ) = default;

        explicit address( void *addr )
        {
            m_addr = reinterpret_cast< uptr >( addr );
        }

        explicit address( const uptr addr ) : m_addr( addr )
        {
        }

        void set( uptr addr )
        {
            m_addr = addr;
        }

        void set( void *addr )
        {
            m_addr = reinterpret_cast< uptr >( addr );
        }

        bool is_null( ) const
        {
            return m_addr == 0x0;
        }

        uptr get_raw( ) const
        {
            return m_addr;
        }

        address deref( )
        {
            m_addr = reinterpret_cast< uptr >( *reinterpret_cast< void ** >( m_addr ) );
            return *this;
        }

        template< typename T >
        T get_as( )
        {
            return reinterpret_cast< T >( m_addr );
        }

        template< typename R = void, typename... args >
        R call_as( args... arguments )
        {
            const auto func{ reinterpret_cast< R ( * )( args... ) >( m_addr ) };
            return func( arguments... );
        }
        
        template< typename R = void, typename... args >
        R call_variadic( const char *format, args... arguments )
        {
            const auto func{ reinterpret_cast< R ( * )( const char *, ... ) >( m_addr ) };
            return func( format, arguments... );
        }

        void rel2abs( const pdiff disp = 3, const pdiff ip = 7 )
        {
            const auto addr = m_addr;
            m_addr          = *reinterpret_cast< i32 * >( addr + disp ) + addr + ip;
        }

    private:
        uptr m_addr;
    };

    template< typename T >
    inline constexpr T call_virtual( void *vmt, u16 idx )
    {
        return ( *static_cast< T ** >( vmt ) )[ idx ];
    }

    inline constexpr void *get_virtual( void *vmt, u16 idx )
    {
        return ( *static_cast< void *** >( vmt ) )[ idx ];
    }

    inline void *get_current_process( )
    {
        return reinterpret_cast< void * >( static_cast< usize >( -1 ) );
    }
} // namespace util
