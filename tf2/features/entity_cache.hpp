#pragma once

#include "link.hpp"

class entity_cache final : public IClientEntityListener
{
public:
    static constexpr usize CAPACITY{ NUM_ENT_ENTRIES };
    static constexpr u32   INVALID_HANDLE{ INVALID_EHANDLE_INDEX };

    struct entry_t
    {
        C_BaseEntity *m_entity{ };
        ClientClass  *m_client_class{ };
        u32           m_handle{ };
        ETFClassID    m_class_id{ };
        i32           m_index{ };

        bool networked( ) const
        {
            return this->m_index > 0 && this->m_index < MAX_EDICTS;
        }
    };

    struct view_t
    {
        const entry_t *const *m_data{ };
        usize                 m_size{ };

        const entry_t *const *begin( ) const
        {
            return this->m_data;
        }
        const entry_t *const *end( ) const
        {
            return this->m_data ? this->m_data + this->m_size : nullptr;
        }
        usize size( ) const
        {
            return this->m_size;
        }
        bool empty( ) const
        {
            return this->m_size == 0;
        }
    };

    struct stats_t
    {
        u64 m_created{ }, m_deleted{ }, m_duplicate_adds{ }, m_unknown_removes{ };
        u64 m_replacements{ }, m_bootstraps{ }, m_bootstrap_entities{ }, m_sorts{ };
        u64 m_validations{ }, m_missing{ }, m_stale{ }, m_duplicates{ }, m_misclassified{ };
    };

    constexpr entity_cache( )                       = default;
    entity_cache( const entity_cache & )            = delete;
    entity_cache &operator=( const entity_cache & ) = delete;

    bool init( );
    bool ready( ) const;
    bool update( );
    bool shutdown( );
    void on_level_shutdown( );

    view_t              all( );
    view_t              networked( );
    view_t              players( );
    view_t              buildings( );
    view_t              projectiles( );
    view_t              visuals( );
    view_t              doors( );
    view_t              npcs( );
    const entry_t      *find( const C_BaseEntity *entity ) const;
    C_BaseEntity       *resolve( u32 handle ) const;
    C_TFPlayerResource *player_resource( ) const;

    void OnEntityCreated( C_BaseEntity *entity ) override;
    void OnEntityDeleted( C_BaseEntity *entity ) override;

#if defined( ATLAS_DEV )
    void           set_validation( bool enabled );
    bool           validate( );
    const stats_t &stats( ) const
    {
        return this->m_stats;
    }
#endif

private:
    enum class group : u8
    {
        all,
        networked,
        players,
        buildings,
        projectiles,
        visuals,
        doors,
        npcs,
        count
    };

    static constexpr usize GROUP_COUNT{ static_cast< usize >( group::count ) };
    static constexpr usize POINTER_CAPACITY{ CAPACITY * 2 };
    static constexpr u16   NO_POSITION{ 0xFFFFu };

    struct slot_t
    {
        entry_t m_entry{ };
        u16     m_positions[ GROUP_COUNT ]{ };
        u16     m_pending_position{ NO_POSITION };
        u8      m_membership{ };
    };

    struct group_t
    {
        const entry_t *m_entries[ CAPACITY ]{ };
        usize          m_count{ };
        bool           m_sorted{ true };
    };

    static usize pointer_bucket( const C_BaseEntity *entity );
    static u8    membership( ETFClassID id, i32 index );
    usize        pointer_slot( const C_BaseEntity *entity ) const;
    bool         insert_pointer( usize index );
    void         erase_pointer( const C_BaseEntity *entity );
    void         add_groups( usize index, u8 mask );
    void         remove_groups( usize index );
    void         remove_pending( usize index );
    void         erase( usize index );
    void         clear( );
    void         add( C_BaseEntity *entity, bool settled );
    bool         classify( usize index, bool settled );
    bool         owner_thread( ) const;
    void         fault( u32 reason );
    view_t       view( group kind );

    CClientEntityList *m_list{ };
    ClientClass       *m_player_class{ };
    slot_t             m_slots[ CAPACITY ]{ };
    group_t            m_groups[ GROUP_COUNT ]{ };
    u16                m_pointer_slots[ POINTER_CAPACITY ]{ }; // slot + 1; zero is empty
    u32                m_pending[ CAPACITY ]{ };
    usize              m_pending_count{ };
    u32                m_resource{ INVALID_HANDLE };
    u32                m_owner_thread{ };
    u32                m_fault{ };
    bool               m_attached{ };
    bool               m_classes_ready{ };
    stats_t            m_stats{ };
#if defined( ATLAS_DEV )
    bool m_validate{ };
    u64  m_next_validation{ };
#endif
};

extern entity_cache g_entity_cache;
