#pragma once

#include <new>

#include "hash_table.hpp"
#include "util/typedefs.hpp"

namespace detail
{
    template< typename key_type_t >
    struct set_slot
    {
        key_type_t key;
    };

    template< typename key_type_t >
    struct set_policy
    {
        using key_type  = key_type_t;
        using slot_type = set_slot< key_type_t >;

        static const key_type &key_from( const slot_type &slot )
        {
            return slot.key;
        }

        template< typename... args_t >
        static void construct( slot_type *destination, const key_type &key, args_t &&... )
        {
            new ( destination ) slot_type{ key };
        }
    };
} // namespace detail

namespace atl
{
    template< typename key_type_t >
    class unordered_set
    {
    private:
        using policy_type = detail::set_policy< key_type_t >;
        using table_type  = detail::flat_hash_table< policy_type >;
        using slot_type   = detail::set_slot< key_type_t >;

    public:
        using key_type  = key_type_t;
        using size_type = usize;

        class iterator
        {
        public:
            iterator( ) = default;

            iterator( const table_type *table, size_type index ) :
                m_table{ table },
                m_index{ index }
            {
            }

            const key_type &operator*( ) const
            {
                return this->m_table->slot_data( )[ this->m_index ].key;
            }

            const key_type *operator->( ) const
            {
                return &this->m_table->slot_data( )[ this->m_index ].key;
            }

            iterator &operator++( )
            {
                this->m_index = this->m_table->next_occupied( this->m_index + 1 );
                return *this;
            }

            bool operator==( const iterator &other ) const
            {
                return this->m_table == other.m_table && this->m_index == other.m_index;
            }

            bool operator!=( const iterator &other ) const
            {
                return !( *this == other );
            }

        private:
            const table_type *m_table{ };
            size_type         m_index{ };
        };

        using const_iterator = iterator;

        unordered_set( )                                  = default;
        unordered_set( const unordered_set & )            = delete;
        unordered_set &operator=( const unordered_set & ) = delete;
        unordered_set( unordered_set && )                 = default;
        unordered_set &operator=( unordered_set && )      = default;

        size_type size( ) const
        {
            return this->m_table.size( );
        }

        size_type capacity( ) const
        {
            return this->m_table.capacity( );
        }

        bool empty( ) const
        {
            return this->m_table.empty( );
        }

        bool reserve( size_type count )
        {
            return this->m_table.reserve( count );
        }

        void clear( )
        {
            this->m_table.clear( );
        }

        void reset( )
        {
            this->m_table.reset( );
        }

        iterator begin( ) const
        {
            return iterator{ &this->m_table, this->m_table.next_occupied( 0 ) };
        }

        iterator end( ) const
        {
            return iterator{ &this->m_table, this->m_table.capacity( ) };
        }

        iterator find( const key_type &key ) const
        {
            const slot_type *slot{ this->m_table.find_slot( key ) };

            if ( !slot )
            {
                return this->end( );
            }

            return iterator{ &this->m_table, static_cast< size_type >( slot - this->m_table.slot_data( ) ) };
        }

        bool contains( const key_type &key ) const
        {
            return this->m_table.find_slot( key ) != nullptr;
        }

        const key_type *insert( const key_type &key, bool *inserted_out = nullptr )
        {
            slot_type *slot{ this->m_table.insert( key, inserted_out ) };
            return slot ? &slot->key : nullptr;
        }

        bool erase( const key_type &key )
        {
            return this->m_table.erase( key );
        }

    private:
        table_type m_table{ };
    };
} // namespace atl
