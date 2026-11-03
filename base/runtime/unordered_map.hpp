#pragma once

#include <new>

#include "hash_table.hpp"
#include "util/typedefs.hpp"

namespace detail
{
    template< typename key_type_t, typename value_type_t >
    struct map_entry
    {
        key_type_t   key;
        value_type_t value;
    };

    template< typename key_type_t, typename value_type_t >
    struct map_policy
    {
        using key_type   = key_type_t;
        using value_type = value_type_t;
        using slot_type  = map_entry< key_type_t, value_type_t >;

        static const key_type &key_from( const slot_type &slot )
        {
            return slot.key;
        }

        template< typename... args_t >
        static void construct( slot_type *destination, const key_type &key, args_t &&...arguments )
        {
            new ( destination ) slot_type{ key, value_type{ detail::table_forward< args_t >( arguments )... } };
        }
    };
} // namespace detail

namespace atl
{
    template< typename key_type_t, typename value_type_t >
    class unordered_map
    {
    private:
        using policy_type = detail::map_policy< key_type_t, value_type_t >;
        using table_type  = detail::flat_hash_table< policy_type >;

    public:
        using key_type   = key_type_t;
        using value_type = value_type_t;
        using entry_type = detail::map_entry< key_type_t, value_type_t >;
        using size_type  = usize;

        class iterator
        {
        public:
            iterator( ) = default;

            iterator( table_type *table, size_type index ) :
                m_table{ table },
                m_index{ index }
            {
            }

            entry_type &operator*( ) const
            {
                return this->m_table->slot_data( )[ this->m_index ];
            }

            entry_type *operator->( ) const
            {
                return &this->m_table->slot_data( )[ this->m_index ];
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
            table_type *m_table{ };
            size_type   m_index{ };
        };

        class const_iterator
        {
        public:
            const_iterator( ) = default;

            const_iterator( const table_type *table, size_type index ) :
                m_table{ table },
                m_index{ index }
            {
            }

            const entry_type &operator*( ) const
            {
                return this->m_table->slot_data( )[ this->m_index ];
            }

            const entry_type *operator->( ) const
            {
                return &this->m_table->slot_data( )[ this->m_index ];
            }

            const_iterator &operator++( )
            {
                this->m_index = this->m_table->next_occupied( this->m_index + 1 );
                return *this;
            }

            bool operator==( const const_iterator &other ) const
            {
                return this->m_table == other.m_table && this->m_index == other.m_index;
            }

            bool operator!=( const const_iterator &other ) const
            {
                return !( *this == other );
            }

        private:
            const table_type *m_table{ };
            size_type         m_index{ };
        };

        unordered_map( )                                  = default;
        unordered_map( const unordered_map & )            = delete;
        unordered_map &operator=( const unordered_map & ) = delete;
        unordered_map( unordered_map && )                 = default;
        unordered_map &operator=( unordered_map && )      = default;

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

        iterator begin( )
        {
            return iterator{ &this->m_table, this->m_table.next_occupied( 0 ) };
        }

        iterator end( )
        {
            return iterator{ &this->m_table, this->m_table.capacity( ) };
        }

        const_iterator begin( ) const
        {
            return const_iterator{ &this->m_table, this->m_table.next_occupied( 0 ) };
        }

        const_iterator end( ) const
        {
            return const_iterator{ &this->m_table, this->m_table.capacity( ) };
        }

        iterator find( const key_type &key )
        {
            entry_type *slot{ this->m_table.find_slot( key ) };

            if ( !slot )
            {
                return this->end( );
            }

            return iterator{ &this->m_table, static_cast< size_type >( slot - this->m_table.slot_data( ) ) };
        }

        const_iterator find( const key_type &key ) const
        {
            const entry_type *slot{ this->m_table.find_slot( key ) };

            if ( !slot )
            {
                return this->end( );
            }

            return const_iterator{ &this->m_table, static_cast< size_type >( slot - this->m_table.slot_data( ) ) };
        }

        bool contains( const key_type &key ) const
        {
            return this->m_table.find_slot( key ) != nullptr;
        }

        value_type *get( const key_type &key )
        {
            entry_type *slot{ this->m_table.find_slot( key ) };
            return slot ? &slot->value : nullptr;
        }

        const value_type *get( const key_type &key ) const
        {
            const entry_type *slot{ this->m_table.find_slot( key ) };
            return slot ? &slot->value : nullptr;
        }

        value_type *insert( const key_type &key, const value_type &value, bool *inserted_out = nullptr )
        {
            entry_type *slot{ this->m_table.insert( key, inserted_out, value ) };
            return slot ? &slot->value : nullptr;
        }

        value_type *insert( const key_type &key, value_type &&value, bool *inserted_out = nullptr )
        {
            entry_type *slot{ this->m_table.insert( key, inserted_out, detail::table_move( value ) ) };
            return slot ? &slot->value : nullptr;
        }

        value_type *assign( const key_type &key, const value_type &value, bool *inserted_out = nullptr )
        {
            bool        inserted{ false };
            entry_type *slot{ this->m_table.insert( key, &inserted, value ) };

            if ( !slot )
            {
                return nullptr;
            }

            if ( !inserted )
            {
                slot->value = value;
            }

            if ( inserted_out )
            {
                *inserted_out = inserted;
            }

            return &slot->value;
        }

        value_type *assign( const key_type &key, value_type &&value, bool *inserted_out = nullptr )
        {
            bool        inserted{ false };
            entry_type *slot{ this->m_table.insert( key, &inserted, detail::table_move( value ) ) };

            if ( !slot )
            {
                return nullptr;
            }

            if ( !inserted )
            {
                slot->value = detail::table_move( value );
            }

            if ( inserted_out )
            {
                *inserted_out = inserted;
            }

            return &slot->value;
        }

        template< typename... args_t >
        value_type *emplace( const key_type &key, args_t &&...arguments )
        {
            entry_type *slot{ this->m_table.insert( key, nullptr, detail::table_forward< args_t >( arguments )... ) };
            return slot ? &slot->value : nullptr;
        }

        bool erase( const key_type &key )
        {
            return this->m_table.erase( key );
        }

        value_type_t &operator[]( const key_type &key )
        {
            // TODO: ADJUST ME WHEN EMPLACE IS COMPLIANT WITH THE C++ STANDARD.
            if ( this->find( key ) == this->end( ) )
            {
                return *this->emplace( key );
            }

            return *this->get( key );
        }

    private:
        table_type m_table{ };
    };
} // namespace atl
