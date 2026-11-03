#pragma once

#include "utility.hpp"

namespace atlashook
{
    class inline_hook
    {
    public:
        enum flags : u32
        {
            default_flags  = 0,
            start_disabled = 1 << 0,
        };

        inline_hook( )                                = default;
        inline_hook( const inline_hook & )            = delete;
        inline_hook &operator=( const inline_hook & ) = delete;
        inline_hook( inline_hook &&other );
        inline_hook &operator=( inline_hook &&other );
        ~inline_hook( ) = default;

        static inline_hook create( void *target, void *destination, flags options = default_flags );

        bool enable( );
        bool disable( );
        void reset( );

        template< typename type >
        ATLAS_FORCE_INLINE type original( ) const
        {
            return reinterpret_cast< type >( this->m_trampoline.get( ) );
        }

        template< typename ret = void, typename... args >
        ATLAS_FORCE_INLINE ret call( args... arguments ) const
        {
            using function_t = ret ( * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        template< typename ret = void, typename... args >
        ATLAS_FORCE_INLINE ret ccall( args... arguments ) const
        {
            using function_t = ret( ATLAS_CDECL * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        template< typename ret = void, typename... args >
        ATLAS_FORCE_INLINE ret thiscall( args... arguments ) const
        {
            using function_t = ret( ATLAS_THISCALL * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        template< typename ret = void, typename... args >
        ATLAS_FORCE_INLINE ret stdcall( args... arguments ) const
        {
            using function_t = ret( ATLAS_STDCALL * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        template< typename ret = void, typename... args >
        ATLAS_FORCE_INLINE ret fastcall( args... arguments ) const
        {
            using function_t = ret( ATLAS_FASTCALL * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        static inline_hook *registered_head( );

        ATLAS_FORCE_INLINE inline_hook *registered_next( ) const
        {
            return this->m_registered_next;
        }

        bool restore_original( u8 *buffer, usize buffer_size, usize offset ) const;

        usize copy_original( u8 *dst, usize offset, usize size ) const;

        ATLAS_FORCE_INLINE usize original_size( ) const
        {
            return this->m_original_size;
        }

        ATLAS_FORCE_INLINE void *target( ) const
        {
            return this->m_target;
        }

        ATLAS_FORCE_INLINE void *destination( ) const
        {
            return this->m_destination;
        }

        ATLAS_FORCE_INLINE bool enabled( ) const
        {
            return this->m_enabled;
        }

        ATLAS_FORCE_INLINE error last_error( ) const
        {
            return this->m_error;
        }

        explicit ATLAS_FORCE_INLINE operator bool( ) const
        {
            return this->m_target && this->m_destination && this->m_trampoline.get( ) && this->m_error == error::none;
        }

    private:
        static void register_hook( inline_hook *hook );
        static void unregister_hook( inline_hook *hook );
        static void replace_hook( inline_hook *from, inline_hook *to );

        void                             *m_target{ };
        void                             *m_destination{ };
        detail::encoded_pointer< void * > m_trampoline{ };
        usize                             m_trampoline_size{ };
        u32                               m_trampoline_protect{ };
        atl::array< u8, 64 >              m_original_bytes{ };
        usize                             m_original_key{ };
        u32                               m_original_size{ };
        inline_hook                      *m_registered_next{ };
        bool                              m_enabled{ };
        error                             m_error{ error::none };
    };
} // namespace atlashook
