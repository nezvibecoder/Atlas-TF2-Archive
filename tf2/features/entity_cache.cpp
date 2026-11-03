#include "entity_cache.hpp"

ATLAS_NO_DESTROY constinit entity_cache g_entity_cache{ };

static_assert( entity_cache::CAPACITY == NUM_ENT_ENTRIES );
static_assert( ENT_ENTRY_MASK == 0xFFFF && NUM_SERIAL_NUM_SHIFT_BITS == 16 );

usize entity_cache::pointer_bucket( const C_BaseEntity *entity )
{
    uptr value{ reinterpret_cast< uptr >( entity ) >> 4 };
    value ^= value >> 17;
    value *= 0x9E3779B1u;
    return static_cast< usize >( value ) & ( POINTER_CAPACITY - 1 );
}

u8 entity_cache::membership( ETFClassID id, i32 index )
{
    u8 mask{ u8{ 1 } << static_cast< u8 >( group::all ) };
    if ( index > 0 && index < MAX_EDICTS )
    {
        mask |= u8{ 1 } << static_cast< u8 >( group::networked );
    }
    bool visual{ false };
    bool npc{ false };
    switch ( id )
    {
        case ETFClassID::CTFPlayer:
        {
            mask |= u8{ 1 } << static_cast< u8 >( group::players );
            visual = true;
            break;
        }

        case ETFClassID::CObjectSentrygun:
        case ETFClassID::CObjectDispenser:
        case ETFClassID::CObjectTeleporter:
        {
            mask |= u8{ 1 } << static_cast< u8 >( group::buildings );
            visual = true;
            break;
        }

        case ETFClassID::CTFGrenadePipebombProjectile:
        case ETFClassID::CTFStunBall:
        case ETFClassID::CTFProjectile_Cleaver:
        case ETFClassID::CTFProjectile_Rocket:
        case ETFClassID::CTFProjectile_SentryRocket:
        case ETFClassID::CTFProjectile_BallOfFire:
        case ETFClassID::CTFProjectile_EnergyBall:
        case ETFClassID::CTFProjectile_Flare:
        case ETFClassID::CTFProjectile_Arrow:
        case ETFClassID::CTFProjectile_HealingBolt:
        case ETFClassID::CTFProjectile_SpellFireball:
        {
            mask |= u8{ 1 } << static_cast< u8 >( group::projectiles );
            visual = true;
            break;
        }

        case ETFClassID::CTFProjectile_Jar:
        case ETFClassID::CTFProjectile_JarMilk:
        case ETFClassID::CTFProjectile_JarGas:
        case ETFClassID::CTFProjectile_MechanicalArmOrb:
        {
            mask |= u8{ 1 } << static_cast< u8 >( group::projectiles );
            break;
        }

        case ETFClassID::CBaseDoor:
        {
            mask |= u8{ 1 } << static_cast< u8 >( group::doors );
            break;
        }

        case ETFClassID::CCaptureFlag:
        case ETFClassID::CBaseAnimating:
        case ETFClassID::CTFAmmoPack:
        {
            visual = true;
            break;
        }

        case ETFClassID::CEyeballBoss:
        case ETFClassID::CHeadlessHatman:
        case ETFClassID::CMerasmus:
        case ETFClassID::CZombie:
        case ETFClassID::CTFTankBoss:
        {
            visual = true;
            npc    = true;
            break;
        }

        default: break;
    }

    if ( visual )
    {
        mask |= u8{ 1 } << static_cast< u8 >( group::visuals );
    }

    if ( npc )
    {
        mask |= 1 << ( u8 )group::npcs;
    }

    return mask;
}

usize entity_cache::pointer_slot( const C_BaseEntity *entity ) const
{
    usize bucket{ pointer_bucket( entity ) };
    for ( usize checked{ }; checked < POINTER_CAPACITY; ++checked )
    {
        const u16 encoded{ this->m_pointer_slots[ bucket ] };
        if ( !encoded )
        {
            return CAPACITY;
        }
        const usize index{ static_cast< usize >( encoded - 1 ) };
        if ( this->m_slots[ index ].m_entry.m_entity == entity )
        {
            return index;
        }
        bucket = ( bucket + 1 ) & ( POINTER_CAPACITY - 1 );
    }
    return CAPACITY;
}

bool entity_cache::insert_pointer( usize index )
{
    usize bucket{ pointer_bucket( this->m_slots[ index ].m_entry.m_entity ) };
    for ( usize checked{ }; checked < POINTER_CAPACITY; ++checked )
    {
        if ( !this->m_pointer_slots[ bucket ] )
        {
            this->m_pointer_slots[ bucket ] = static_cast< u16 >( index + 1 );
            return true;
        }
        bucket = ( bucket + 1 ) & ( POINTER_CAPACITY - 1 );
    }
    return false;
}

void entity_cache::erase_pointer( const C_BaseEntity *entity )
{
    usize hole{ pointer_bucket( entity ) };
    for ( usize checked{ }; checked < POINTER_CAPACITY; ++checked )
    {
        const u16 encoded{ this->m_pointer_slots[ hole ] };
        if ( !encoded )
        {
            return;
        }
        if ( this->m_slots[ encoded - 1 ].m_entry.m_entity == entity )
        {
            break;
        }
        hole = ( hole + 1 ) & ( POINTER_CAPACITY - 1 );
    }

    usize next{ ( hole + 1 ) & ( POINTER_CAPACITY - 1 ) };
    while ( this->m_pointer_slots[ next ] )
    {
        const u16   encoded{ this->m_pointer_slots[ next ] };
        const usize home{ pointer_bucket( this->m_slots[ encoded - 1 ].m_entry.m_entity ) };
        if ( ( ( next - home ) & ( POINTER_CAPACITY - 1 ) ) >=
             ( ( next - hole ) & ( POINTER_CAPACITY - 1 ) ) )
        {
            this->m_pointer_slots[ hole ] = encoded;
            hole                          = next;
        }
        next = ( next + 1 ) & ( POINTER_CAPACITY - 1 );
    }
    this->m_pointer_slots[ hole ] = 0;
}

void entity_cache::add_groups( usize index, u8 mask )
{
    slot_t &slot{ this->m_slots[ index ] };
    for ( usize kind{ }; kind < GROUP_COUNT; ++kind )
    {
        if ( !( mask & ( u8{ 1 } << kind ) ) || ( slot.m_membership & ( u8{ 1 } << kind ) ) )
        {
            continue;
        }
        group_t &list{ this->m_groups[ kind ] };
        slot.m_positions[ kind ]         = static_cast< u16 >( list.m_count );
        list.m_entries[ list.m_count++ ] = &slot.m_entry;
        list.m_sorted                    = false;
        slot.m_membership |= u8{ 1 } << kind;
    }
}

void entity_cache::remove_groups( usize index )
{
    slot_t &slot{ this->m_slots[ index ] };
    for ( usize kind{ }; kind < GROUP_COUNT; ++kind )
    {
        if ( !( slot.m_membership & ( u8{ 1 } << kind ) ) )
        {
            continue;
        }
        group_t       &list{ this->m_groups[ kind ] };
        const usize    position{ slot.m_positions[ kind ] };
        const entry_t *last{ list.m_entries[ --list.m_count ] };
        if ( position != list.m_count )
        {
            list.m_entries[ position ]                         = last;
            this->m_slots[ last->m_index ].m_positions[ kind ] = static_cast< u16 >( position );
            list.m_sorted                                      = false;
        }
        list.m_entries[ list.m_count ] = nullptr;
    }
    slot.m_membership = 0;
}

void entity_cache::remove_pending( usize index )
{
    slot_t &slot{ this->m_slots[ index ] };
    if ( slot.m_pending_position == NO_POSITION )
    {
        return;
    }
    const usize position{ slot.m_pending_position };
    const u32   last{ this->m_pending[ --this->m_pending_count ] };
    if ( position != this->m_pending_count )
    {
        this->m_pending[ position ]                               = last;
        this->m_slots[ last & ENT_ENTRY_MASK ].m_pending_position = static_cast< u16 >( position );
    }
    this->m_pending[ this->m_pending_count ] = 0;
    slot.m_pending_position                  = NO_POSITION;
}

void entity_cache::erase( usize index )
{
    slot_t       &slot{ this->m_slots[ index ] };
    const entry_t old{ slot.m_entry };
    this->erase_pointer( old.m_entity );
    this->remove_groups( index );
    this->remove_pending( index );
    slot.m_entry = entry_t{ };

    if ( this->m_resource == old.m_handle )
    {
        this->m_resource = INVALID_HANDLE;
        for ( usize at{ }; at < this->m_groups[ 0 ].m_count; ++at )
        {
            const entry_t *entry{ this->m_groups[ 0 ].m_entries[ at ] };
            if ( entry->networked( ) && entry->m_class_id == ETFClassID::CTFPlayerResource &&
                 ( this->m_resource == INVALID_HANDLE || entry->m_index < static_cast< i32 >( this->m_resource & ENT_ENTRY_MASK ) ) )
            {
                this->m_resource = entry->m_handle;
            }
        }
    }
    g_cl.on_entity_deleted( old.m_entity, old.m_handle );
}

void entity_cache::clear( )
{
    group_t &all{ this->m_groups[ static_cast< usize >( group::all ) ] };
    while ( all.m_count )
    {
        this->erase( all.m_entries[ all.m_count - 1 ]->m_index );
    }
    for ( group_t &list : this->m_groups )
    {
        list.m_sorted = true;
    }
    this->m_resource = INVALID_HANDLE;
}

bool entity_cache::owner_thread( ) const
{
    const u32 owner{ __atomic_load_n( &this->m_owner_thread, __ATOMIC_ACQUIRE ) };
    return owner != 0 && owner == static_cast< u32 >( I( GetCurrentThreadId )( ) );
}

void entity_cache::fault( u32 reason )
{
    u32 expected{ };
    __atomic_compare_exchange_n( &this->m_fault, &expected, reason, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED );
}

bool entity_cache::ready( ) const
{
    return this->owner_thread( ) && this->m_attached &&
           __atomic_load_n( &this->m_fault, __ATOMIC_ACQUIRE ) == 0;
}

bool entity_cache::init( )
{
    const u32 thread{ static_cast< u32 >( I( GetCurrentThreadId )( ) ) };
    const u32 owner{ __atomic_load_n( &this->m_owner_thread, __ATOMIC_ACQUIRE ) };
    if ( owner && owner != thread )
    {
        return false;
    }
    if ( this->m_attached )
    {
        return this->ready( );
    }
    if ( !g_tf2.m_entity_list || !g_tf2.m_client ||
         __atomic_load_n( &this->m_fault, __ATOMIC_ACQUIRE ) )
    {
        return false;
    }

    // Class registration names/pointers are static. Class IDs are assigned by
    // the engine later, so their readiness is checked only on the engine thread.
    ClientClass *player_class{ };
    usize        count{ };
    for ( ClientClass *current{ g_tf2.m_client->GetAllClasses( ) };
          current && count++ < 1024; current = current->m_pNext )
    {
        if ( current->m_pNetworkName && str::equals( current->m_pNetworkName, ATLAS_XORSTR( "CTFPlayer" ).get( ) ) )
        {
            player_class = current;
            break;
        }
    }
    if ( !player_class )
    {
        return false;
    }
    this->m_player_class = player_class;
    this->m_list         = g_tf2.m_entity_list;
#if defined( ATLAS_DEV ) && defined( ATLAS_ENTITY_CACHE_VALIDATE )
    this->m_validate = true;
#endif
    __atomic_store_n( &this->m_owner_thread, thread, __ATOMIC_RELEASE );

    this->m_classes_ready = this->m_player_class->GetClassID( ) == ETFClassID::CTFPlayer;
    this->m_attached      = true;
    if ( !this->m_list->AddListenerEntity( this ) )
    {
        // A failed post-registration check must still attempt to remove us.
        if ( this->m_list->RemoveListenerEntity( this ) )
        {
            this->m_attached = false;
        }
        this->fault( 2 );
        return false;
    }

    ++this->m_stats.m_bootstraps;
    usize         seeded{ };
    C_BaseEntity *entity{ this->m_list->NextBaseEntity( nullptr ) };
    while ( entity && seeded < CAPACITY )
    {
        this->add( entity, true );
        ++seeded;
        entity = this->m_list->NextBaseEntity( entity );
    }
    this->m_stats.m_bootstrap_entities += seeded;
    if ( entity )
    {
        this->fault( 3 ); // native iterator cycle / incompatible list contract
    }
    return this->ready( );
}

bool entity_cache::classify( usize index, bool settled )
{
    if ( !this->m_classes_ready )
    {
        return false;
    }
    slot_t      &slot{ this->m_slots[ index ] };
    ClientClass *client_class{ slot.m_entry.m_entity->GetClientClass( ) };
    if ( !client_class )
    {
        return settled && index >= MAX_EDICTS;
    }
    const ETFClassID class_id{ client_class->GetClassID( ) };
    if ( static_cast< i32 >( class_id ) < 0 )
    {
        return false;
    }
    slot.m_entry.m_client_class = client_class;
    slot.m_entry.m_class_id     = class_id;
    this->add_groups( index, membership( slot.m_entry.m_class_id, static_cast< i32 >( index ) ) );
    if ( slot.m_entry.networked( ) && slot.m_entry.m_class_id == ETFClassID::CTFPlayerResource &&
         ( this->m_resource == INVALID_HANDLE || index < ( this->m_resource & ENT_ENTRY_MASK ) ) )
    {
        this->m_resource = slot.m_entry.m_handle;
    }
    return true;
}

void entity_cache::add( C_BaseEntity *entity, bool settled )
{
    if ( !entity )
    {
        return;
    }
    const u32   handle{ static_cast< u32 >( entity->GetRefEHandle( ).GetIndex( ) ) };
    const usize index{ handle & ENT_ENTRY_MASK };
    if ( handle == INVALID_HANDLE || index >= CAPACITY )
    {
        this->fault( 4 ); // native OnAdd must run after SetRefEHandle
        return;
    }

    const usize previous{ this->pointer_slot( entity ) };
    if ( previous < CAPACITY )
    {
        if ( this->m_slots[ previous ].m_entry.m_handle == handle )
        {
            ++this->m_stats.m_duplicate_adds;
            return;
        }
        ++this->m_stats.m_replacements;
        this->erase( previous );
    }
    slot_t &slot{ this->m_slots[ index ] };
    if ( slot.m_entry.m_entity )
    {
        ++this->m_stats.m_replacements;
        this->erase( index );
    }

    slot.m_entry = entry_t{ entity, nullptr, handle, static_cast< ETFClassID >( -1 ), static_cast< i32 >( index ) };
    if ( !this->insert_pointer( index ) )
    {
        slot.m_entry = entry_t{ };
        this->fault( 5 );
        return;
    }
    this->add_groups( index, membership( static_cast< ETFClassID >( -1 ), static_cast< i32 >( index ) ) );

    if ( ( settled || index < MAX_EDICTS ) && this->classify( index, settled ) )
    {
        return;
    }
    slot.m_pending_position                    = static_cast< u16 >( this->m_pending_count );
    this->m_pending[ this->m_pending_count++ ] = handle;
}

void entity_cache::OnEntityCreated( C_BaseEntity *entity )
{
    if ( !this->owner_thread( ) )
    {
        this->fault( 1 );
        return;
    }
    if ( !this->m_attached )
    {
        return;
    }
    ++this->m_stats.m_created;
    if ( !this->m_classes_ready )
    {
        this->m_classes_ready = this->m_player_class->GetClassID( ) == ETFClassID::CTFPlayer;
    }
    this->add( entity, false );
}

void entity_cache::OnEntityDeleted( C_BaseEntity *entity )
{
    if ( !this->owner_thread( ) )
    {
        this->fault( 1 );
        return;
    }
    if ( !this->m_attached )
    {
        return;
    }
    ++this->m_stats.m_deleted;
    const usize index{ this->pointer_slot( entity ) };
    if ( index == CAPACITY )
    {
        ++this->m_stats.m_unknown_removes;
        return;
    }

    this->erase( index );
}

bool entity_cache::update( )
{
    if ( !this->init( ) )
    {
        return false;
    }
    if ( !this->m_classes_ready )
    {
        this->m_classes_ready = this->m_player_class->GetClassID( ) == ETFClassID::CTFPlayer;
    }
    if ( this->m_classes_ready )
    {
        usize at{ };
        while ( at < this->m_pending_count )
        {
            const u32   handle{ this->m_pending[ at ] };
            const usize index{ handle & ENT_ENTRY_MASK };
            const auto *direct{ this->m_list->GetClientEntity( static_cast< i32 >( index ) ) };
            if ( !direct || direct != this->m_slots[ index ].m_entry.m_entity ||
                 static_cast< u32 >( direct->GetRefEHandle( ).GetIndex( ) ) != handle )
            {
                this->fault( 6 );
                break;
            }
            if ( this->classify( index, true ) )
            {
                this->remove_pending( index );
            } else
            {
                this->fault( 7 );
                break;
            }
        }
    }
#if defined( ATLAS_DEV )
    if ( this->m_validate )
    {
        const u64 now{ I( GetTickCount64 )( ) };
        if ( now >= this->m_next_validation )
        {
            this->m_next_validation = now + 5000;
            this->validate( );
        }
    }
#endif
    return this->ready( );
}

bool entity_cache::shutdown( )
{
    if ( !this->m_attached )
    {
        return true;
    }
    if ( !this->owner_thread( ) || !this->m_list->RemoveListenerEntity( this ) )
    {
        return false;
    }
    this->m_attached = false;
    this->clear( );
    return true;
}

void entity_cache::on_level_shutdown( )
{
    if ( this->owner_thread( ) && this->m_attached )
    {
        this->clear( );
        this->m_classes_ready = false;
    }
}

entity_cache::view_t entity_cache::view( group kind )
{
    if ( !this->ready( ) )
    {
        return { };
    }
    const usize id{ static_cast< usize >( kind ) };
    group_t    &list{ this->m_groups[ id ] };
    if ( !list.m_sorted )
    {
        const auto sift{ [ &list ]( usize root, usize count )
        {
            const entry_t *value{ list.m_entries[ root ] };
            for ( usize child{ root * 2 + 1 }; child < count; child = root * 2 + 1 )
            {
                if ( child + 1 < count && list.m_entries[ child ]->m_index < list.m_entries[ child + 1 ]->m_index )
                {
                    ++child;
                }
                if ( value->m_index >= list.m_entries[ child ]->m_index )
                {
                    break;
                }
                list.m_entries[ root ] = list.m_entries[ child ];
                root                   = child;
            }
            list.m_entries[ root ] = value;
        } };
        for ( usize root{ list.m_count / 2 }; root > 0; --root )
        {
            sift( root - 1, list.m_count );
        }
        for ( usize end{ list.m_count }; end > 1; --end )
        {
            const entry_t *last{ list.m_entries[ end - 1 ] };
            list.m_entries[ end - 1 ] = list.m_entries[ 0 ];
            list.m_entries[ 0 ]       = last;
            sift( 0, end - 1 );
        }
        for ( usize at{ }; at < list.m_count; ++at )
        {
            this->m_slots[ list.m_entries[ at ]->m_index ].m_positions[ id ] = static_cast< u16 >( at );
        }
        list.m_sorted = true;
        ++this->m_stats.m_sorts;
    }
    return view_t{ list.m_entries, list.m_count };
}

entity_cache::view_t entity_cache::all( )
{
    return this->view( group::all );
}

entity_cache::view_t entity_cache::networked( )
{
    return this->view( group::networked );
}

entity_cache::view_t entity_cache::players( )
{
    return this->view( group::players );
}

entity_cache::view_t entity_cache::buildings( )
{
    return this->view( group::buildings );
}

entity_cache::view_t entity_cache::projectiles( )
{
    return this->view( group::projectiles );
}

entity_cache::view_t entity_cache::visuals( )
{
    return this->view( group::visuals );
}

entity_cache::view_t entity_cache::doors( )
{
    return this->view( group::doors );
}

entity_cache::view_t entity_cache::npcs( )
{
    return this->view( group::npcs );
}

const entity_cache::entry_t *entity_cache::find( const C_BaseEntity *entity ) const
{
    if ( !entity || !this->ready( ) )
    {
        return nullptr;
    }
    const usize index{ this->pointer_slot( entity ) };
    return index < CAPACITY ? &this->m_slots[ index ].m_entry : nullptr;
}

C_BaseEntity *entity_cache::resolve( u32 handle ) const
{
    const usize index{ handle & ENT_ENTRY_MASK };
    if ( handle == INVALID_HANDLE || index >= CAPACITY || !this->ready( ) )
    {
        return nullptr;
    }
    const entry_t &entry{ this->m_slots[ index ].m_entry };
    return entry.m_entity && entry.m_handle == handle ? entry.m_entity : nullptr;
}

C_TFPlayerResource *entity_cache::player_resource( ) const
{
    return reinterpret_cast< C_TFPlayerResource * >( this->resolve( this->m_resource ) );
}

#if defined( ATLAS_DEV )
void entity_cache::set_validation( bool enabled )
{
    if ( this->owner_thread( ) )
    {
        this->m_validate        = enabled;
        this->m_next_validation = 0;
    }
}

bool entity_cache::validate( )
{
    if ( !this->owner_thread( ) || !this->m_attached )
    {
        return false;
    }
    u64   missing{ }, stale{ }, duplicates{ }, misclassified{ };
    usize direct_count{ };
    u8    seen[ CAPACITY ]{ };

    for ( usize index{ }; index < CAPACITY; ++index )
    {
        auto          *client{ this->m_list->GetClientEntity( static_cast< i32 >( index ) ) };
        C_BaseEntity  *entity{ client ? client->GetBaseEntity( ) : nullptr };
        const slot_t  &slot{ this->m_slots[ index ] };
        const entry_t &entry{ slot.m_entry };
        if ( !entity )
        {
            stale += entry.m_entity != nullptr;
            continue;
        }
        ++direct_count;
        const u32 handle{ static_cast< u32 >( entity->GetRefEHandle( ).GetIndex( ) ) };
        if ( !entry.m_entity )
        {
            ++missing;
            continue;
        }
        if ( entry.m_entity != entity || entry.m_handle != handle || entry.m_index != static_cast< i32 >( index ) )
        {
            ++stale;
            continue;
        }
        if ( this->pointer_slot( entity ) != index )
        {
            ++duplicates;
        }
        if ( slot.m_pending_position == NO_POSITION )
        {
            ClientClass     *client_class{ entity->GetClientClass( ) };
            const ETFClassID class_id{ client_class ? client_class->GetClassID( ) : static_cast< ETFClassID >( -1 ) };
            if ( client_class != entry.m_client_class || class_id != entry.m_class_id ||
                 membership( entry.m_class_id, entry.m_index ) != slot.m_membership )
            {
                ++misclassified;
            }
        }
    }

    for ( usize kind{ }; kind < GROUP_COUNT; ++kind )
    {
        const group_t &list{ this->m_groups[ kind ] };
        for ( usize at{ }; at < list.m_count; ++at )
        {
            const entry_t *entry{ list.m_entries[ at ] };
            if ( !entry || entry->m_index < 0 || entry->m_index >= static_cast< i32 >( CAPACITY ) ||
                 entry != &this->m_slots[ entry->m_index ].m_entry || !entry->m_entity )
            {
                ++stale;
                continue;
            }
            const u8 bit{ static_cast< u8 >( u8{ 1 } << kind ) };
            duplicates += ( seen[ entry->m_index ] & bit ) != 0;
            seen[ entry->m_index ] |= bit;
            if ( this->m_slots[ entry->m_index ].m_positions[ kind ] != at )
            {
                ++misclassified;
            }
        }
    }
    for ( usize index{ }; index < CAPACITY; ++index )
    {
        if ( seen[ index ] != this->m_slots[ index ].m_membership )
        {
            ++misclassified;
        }
    }
    if ( direct_count != this->m_groups[ static_cast< usize >( group::all ) ].m_count &&
         !missing && !stale )
    {
        ++duplicates;
    }
    ++this->m_stats.m_validations;
    this->m_stats.m_missing += missing;
    this->m_stats.m_stale += stale;
    this->m_stats.m_duplicates += duplicates;
    this->m_stats.m_misclassified += misclassified;
    DBGOUT( "entity cache: live=%llu pending=%llu missing=%llu stale=%llu duplicates=%llu classification=%llu",
            static_cast< u64 >( direct_count ), static_cast< u64 >( this->m_pending_count ),
            missing, stale, duplicates, misclassified );
    return !missing && !stale && !duplicates && !misclassified;
}
#endif
