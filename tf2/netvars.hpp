#pragma once

#include "sdk.hpp"

// I just checked and TF2 currently ships ~1,842 flattened offsets
// We pick 4096 to keep the load factor under 0.5 for cheap linear dumping (what I do for some stuff on cs2)
inline constexpr usize VARS_CAPACITY{ 4096 };

// This is basically a bit mask used in place of % VARS_CAPACITY when dumping the hash table
// It's somewhat cheaper
inline constexpr u32 VARS_MASK{ static_cast< u32 >( VARS_CAPACITY - 1 ) };

inline constexpr usize VARS_ARRAY_HOOK_CAPACITY{ 64 };

// (table_hash ^ prop_hash) -> (offset, prop)
class netvars
{
public:
    enum class int_kind : u8
    {
        none,
        int8,
        int16,
        int32,
        handle,
    };

    struct entry
    {
        hash_t          m_key{ };
        i32             m_offset{ };
        const RecvProp *m_prop{ };
        int_kind        m_int{ };
    };

    bool load( );

    i32             get_offset( hash_t table, hash_t prop ) const;
    const RecvProp *get_prop( hash_t table, hash_t prop ) const;
    const entry    *get_entry( hash_t table, hash_t prop ) const;
    int_kind        classify( const RecvProp *prop ) const;
    i32             get_datamap_offset( const datamap_t *map, hash_t name ) const;
    typedescription_t *find_datamap_field( const datamap_t *map, hash_t name ) const;

    usize count( ) const
    {
        return this->m_count;
    }

private:
    void dump_recv_table( RecvTable *table, hash_t table_hash, i32 base_offset );
    void insert( hash_t key, i32 offset, const RecvProp *prop );

    const entry *find( hash_t key ) const;

    entry          m_entries[ VARS_CAPACITY ]{ };
    usize          m_count{ };
    RecvVarProxyFn m_int8_proxy{ };
    RecvVarProxyFn m_int16_proxy{ };
    RecvVarProxyFn m_int32_proxy{ };
    RecvVarProxyFn m_handle_proxy{ };
};

extern netvars g_netvars;

// -----------------------------------------------------------------------------
// Got this idea from SEO64, basically RecvProp::m_ProxyFn is the callback Valve
// invokes every time the server pushes a new value for that specific field
// (dt_recv.cpp in the CRecvDecoder path) By swapping the pointer we intercept
// incoming updates before the engine actually writes them to the entity;
// -----------------------------------------------------------------------------
class netvar_hook
{
public:
    bool init( hash_t table, hash_t prop, RecvVarProxyFn replacement );
    void shutdown( );

    void call_original( const CRecvProxyData *data, void *entity, void *out ) const;

    RecvVarProxyFn original( ) const
    {
        return this->m_original;
    }
    const RecvProp *prop( ) const
    {
        return this->m_prop;
    }

    explicit operator bool( ) const
    {
        return this->m_prop != nullptr;
    }

private:
    const RecvProp *m_prop{ };
    RecvVarProxyFn  m_original{ };
};

class netvar_array_hook
{
public:
    struct saved
    {
        RecvProp      *m_prop{ };
        RecvVarProxyFn m_original{ };
    };

    bool init( hash_t table, hash_t prop, RecvVarProxyFn replacement );
    void shutdown( );

    void call_original( usize index, const CRecvProxyData *data, void *entity, void *out ) const;

    usize count( ) const
    {
        return this->m_count;
    }

    i32 index_of( const RecvProp *prop ) const
    {
        for ( usize index{ }; index < this->m_count; ++index )
        {
            if ( this->m_saved[ index ].m_prop == prop )
            {
                return static_cast< i32 >( index );
            }
        }

        return -1;
    }

    explicit operator bool( ) const
    {
        return this->m_count != 0;
    }

private:
    saved m_saved[ VARS_ARRAY_HOOK_CAPACITY ]{ };
    usize m_count{ };
};

#define NETVAR( type, name, table, prop )                                                    \
    type &name( )                                                                            \
    {                                                                                        \
        static const i32 s_off{ g_netvars.get_offset( HASH_CT( table ), HASH_CT( prop ) ) }; \
        return *reinterpret_cast< type * >( reinterpret_cast< u8 * >( this ) + s_off );      \
    }

#define NETVAR_OFFSET( type, name, table, prop, extra )                                             \
    type &name( )                                                                                   \
    {                                                                                               \
        static const i32 s_off{ g_netvars.get_offset( HASH_CT( table ), HASH_CT( prop ) ) };        \
        return *reinterpret_cast< type * >( reinterpret_cast< u8 * >( this ) + s_off + ( extra ) ); \
    }

#define CLASS_VAR( type, name, offset )                                                  \
    type &name( )                                                                        \
    {                                                                                    \
        return *reinterpret_cast< type * >( reinterpret_cast< u8 * >( this ) + offset ); \
    }

#define NETVAR_PTR( type, name, table, prop )                                                \
    type *name( )                                                                            \
    {                                                                                        \
        static const i32 s_off{ g_netvars.get_offset( HASH_CT( table ), HASH_CT( prop ) ) }; \
        return reinterpret_cast< type * >( reinterpret_cast< u8 * >( this ) + s_off );       \
    }

#define DATAMAP_VAR( type, name )                                                                            \
    type &name( )                                                                                            \
    {                                                                                                        \
        static const i32 s_off{ g_netvars.get_datamap_offset( this->GetPredDescMap( ), HASH_CT( #name ) ) }; \
        return *reinterpret_cast< type * >( reinterpret_cast< u8 * >( this ) + s_off );                      \
    }
