#include "netvars.hpp"
#include "global.hpp"

void netvars::insert( hash_t key, i32 offset, const RecvProp *prop )
{
    u32 slot{ key & VARS_MASK };
    for ( usize step{ }; step < VARS_CAPACITY; ++step )
    {
        entry &current{ this->m_entries[ slot ] };

        if ( current.m_prop == nullptr )
        {
            current.m_key    = key;
            current.m_offset = offset;
            current.m_prop   = prop;
            ++this->m_count;
            return;
        }

        if ( current.m_key == key )
        {
            return;
        }

        slot = ( slot + 1 ) & VARS_MASK;
    }
}

const netvars::entry *netvars::find( hash_t key ) const
{
    u32 slot{ key & VARS_MASK };

    for ( usize step{ }; step < VARS_CAPACITY; ++step )
    {
        const entry &current{ this->m_entries[ slot ] };

        if ( current.m_prop == nullptr )
        {
            return nullptr;
        }

        if ( current.m_key == key )
        {
            return &current;
        }

        slot = ( slot + 1 ) & VARS_MASK;
    }

    return nullptr;
}

i32 netvars::get_offset( hash_t table, hash_t prop ) const
{
    const entry *hit{ this->find( table ^ prop ) };
    if ( !hit )
    {
        DBGOUT( "netvars: no prop %08x in table %08x", static_cast< u32 >( prop ), static_cast< u32 >( table ) );
    }

    return hit ? hit->m_offset : 0;
}

const RecvProp *netvars::get_prop( hash_t table, hash_t prop ) const
{
    const entry *hit{ this->find( table ^ prop ) };
    return hit ? hit->m_prop : nullptr;
}

const netvars::entry *netvars::get_entry( hash_t table, hash_t prop ) const
{
    return this->find( table ^ prop );
}

netvars::int_kind netvars::classify( const RecvProp *prop ) const
{
    if ( prop->m_RecvType != DPT_Int )
    {
        return int_kind::none;
    }

    if ( prop->m_ProxyFn == this->m_int8_proxy )
    {
        return int_kind::int8;
    }

    if ( prop->m_ProxyFn == this->m_int16_proxy )
    {
        return int_kind::int16;
    }

    if ( prop->m_ProxyFn == this->m_int32_proxy )
    {
        return int_kind::int32;
    }

    return prop->m_ProxyFn == this->m_handle_proxy ? int_kind::handle : int_kind::none;
}

i32 netvars::get_datamap_offset( const datamap_t *map, hash_t name ) const
{
    for ( i32 depth{ }; map && depth < 16; map = map->baseMap, ++depth )
    {
        for ( i32 index{ }; index < map->dataNumFields; ++index )
        {
            const typedescription_t &field{ map->dataDesc[ index ] };
            if ( !field.fieldName )
            {
                continue;
            }

            const i32 offset{ field.fieldOffset[ TD_OFFSET_NORMAL ] };

            if ( HASH_RT( field.fieldName ) == name )
            {
                return offset;
            }

            if ( field.fieldType == FIELD_EMBEDDED && field.td )
            {
                if ( const i32 inner{ this->get_datamap_offset( field.td, name ) } )
                {
                    return offset + inner;
                }
            }
        }
    }

    return 0;
}

typedescription_t *netvars::find_datamap_field( const datamap_t *map, hash_t name ) const
{
    for ( i32 depth{ }; map && depth < 16; map = map->baseMap, ++depth )
    {
        for ( i32 index{ }; index < map->dataNumFields; ++index )
        {
            typedescription_t &field{ map->dataDesc[ index ] };
            if ( field.fieldName && HASH_RT( field.fieldName ) == name )
            {
                return &field;
            }

            if ( field.fieldType == FIELD_EMBEDDED && field.td )
            {
                if ( typedescription_t * inner{ this->find_datamap_field( field.td, name ) } )
                {
                    return inner;
                }
            }
        }
    }

    return nullptr;
}

// -----------------------------------------------------------------------------
// Here we basically mirror the recursive descent Valve itself performs when building
// CRecvDecoder (check BuildFlatSendTable in dt.cpp and CRecvDecoder::Init in
// dt_recv.cpp) Every DPT_DataTable child is inlined into its parent by adding
// its m_Offset to the accumulated base; That's how a field like
// C_BaseEntity::m_vecOrigin (declared inside DT_BaseEntity via
// SendPropDataTable -> DT_LocalPlayerExclusive) ends up at a single flat offset
// from the entity pointer;
// -----------------------------------------------------------------------------
void netvars::dump_recv_table( RecvTable *table, hash_t table_hash, i32 base_offset )
{
    if ( !table || !table->m_pProps )
    {
        return;
    }

    for ( i32 index{ }; index < table->m_nProps; ++index )
    {
        RecvProp &prop{ table->m_pProps[ index ] };

        const char *name{ prop.m_pVarName };
        if ( !name )
        {
            continue;
        }

        // Array element properties are named "000", "001", etc..
        // These names are only relevant through the parent array property,
        // so they can be skipped during named lookup;
        if ( str::is_digit( name[ 0 ] ) )
        {
            continue;
        }

        // "baseclass" is the synthetic prop that Valve emits so a derived
        // datatable can chain to its parent (BEGIN_RECV_TABLE - src/game/shared/predictioncopy.cpp)
        // It never carries a real offset for our purposes;
        if ( str::equals( name, ATLAS_XORSTR( "baseclass" ).get( ) ) )
        {
            continue;
        }

        const i32 total_offset{ base_offset + prop.m_Offset };

        if ( prop.m_RecvType == DPT_DataTable && prop.m_pDataTable )
        {
            const hash_t key{ table_hash ^ HASH_RT( name ) };
            this->insert( key, total_offset, &prop );

            RecvTable *child{ prop.m_pDataTable };

            // @ SendPropDataTable can either splice a child table's props into the
            // parent's namespace (Valve uses this for DT_LocalPlayerExclusive etc) or expose it as a nested
            // namespace; When the prop's name equals the child table's
            // m_pNetTableName Valve is treating it as its own namespace, so we
            // switch the hash to the child; Otherwise we keep flattening under
            // the current table so grand-children remain findable at the top-level name
            hash_t child_hash{ table_hash };
            if ( child->m_pNetTableName && str::equals( name, child->m_pNetTableName ) )
            {
                child_hash = HASH_RT( child->m_pNetTableName );
            }

            this->dump_recv_table( child, child_hash, total_offset );
            continue;
        }

        const hash_t key{ table_hash ^ HASH_RT( name ) };
        this->insert( key, total_offset, &prop );
    }
}

// -----------------------------------------------------------------------------
// CHLClient::GetAllClasses returns the head of a singly linked
// list built by every DECLARE_CLIENTCLASS / IMPLEMENT_CLIENTCLASS invocation
// in client.dll (client_class.h - ClientClass constructor pushes
// itself onto g_pClientClassHead) We walk it once at startup.
// -----------------------------------------------------------------------------
bool netvars::load( )
{
    if ( !g_tf2.m_client )
    {
        return false;
    }

    ClientClass *head{ g_tf2.m_client->GetAllClasses( ) };
    if ( !head )
    {
        return false;
    }

    for ( ClientClass *iter{ head }; iter; iter = iter->m_pNext )
    {
        RecvTable *table{ iter->m_pRecvTable };
        if ( !table || !table->m_pNetTableName )
        {
            continue;
        }

        const hash_t table_hash{ HASH_RT( table->m_pNetTableName ) };
        this->dump_recv_table( table, table_hash, 0 );
    }

    const RecvProp *int8{ this->get_prop( HASH_CT( "DT_EconEntity" ), HASH_CT( "m_bInitialized" ) ) };
    const RecvProp *int16{ this->get_prop( HASH_CT( "DT_EconEntity" ), HASH_CT( "m_iItemDefinitionIndex" ) ) };
    const RecvProp *int32{ this->get_prop( HASH_CT( "DT_EconEntity" ), HASH_CT( "m_iEntityLevel" ) ) };
    const RecvProp *handle{ this->get_prop( HASH_CT( "DT_BaseCombatCharacter" ), HASH_CT( "m_hActiveWeapon" ) ) };

    if ( !int8 ||
         !int16 ||
         !int32 ||
         !handle ||
         int8->m_ProxyFn == int16->m_ProxyFn || // a width changed in a patch, the anchors no longer tell sizes apart
         int16->m_ProxyFn == int32->m_ProxyFn ||
         int8->m_ProxyFn == int32->m_ProxyFn )
    {
        return false;
    }

    this->m_int8_proxy   = int8->m_ProxyFn;
    this->m_int16_proxy  = int16->m_ProxyFn;
    this->m_int32_proxy  = int32->m_ProxyFn;
    this->m_handle_proxy = handle->m_ProxyFn;

    for ( entry &current : this->m_entries )
    {
        if ( current.m_prop )
        {
            current.m_int = this->classify( current.m_prop );
        }
    }

    return this->m_count > 0;
}

// -----------------------------------------------------------------------------
// Valve's SendProxy / RecvProxy contract basically says that the proxy is invoked by the
// CRecvDecoder with the decoded network value and a pointer to the destination
// field on the entity (for example RecvProxy_Int32ToInt32 in dt_recv.cpp) By
// pointing m_ProxyFn at our own function we get to observe or rewrite every
// update to that field before it lands on the entity;
// -----------------------------------------------------------------------------
bool netvar_hook::init( hash_t table, hash_t prop, RecvVarProxyFn replacement )
{
    if ( !replacement )
    {
        return false;
    }

    const RecvProp *resolved{ g_netvars.get_prop( table, prop ) };
    if ( !resolved )
    {
        return false;
    }

    RecvProp *writable{ const_cast< RecvProp * >( resolved ) };
    this->m_prop        = resolved;
    this->m_original    = writable->m_ProxyFn;
    writable->m_ProxyFn = replacement;
    return true;
}

void netvar_hook::shutdown( )
{
    if ( !this->m_prop || !this->m_original )
    {
        return;
    }

    RecvProp *writable{ const_cast< RecvProp * >( this->m_prop ) };
    writable->m_ProxyFn = this->m_original;

    this->m_prop     = nullptr;
    this->m_original = nullptr;
}

void netvar_hook::call_original( const CRecvProxyData *data, void *entity, void *out ) const
{
    if ( this->m_original )
    {
        this->m_original( data, entity, out );
    }
}

// -----------------------------------------------------------------------------
// RecvPropArray leaves a chain of per-element proxies
// ("000", "001", ...) inside a synthetic child RecvTable, wrapping the outer prop only isn't
// enough; the engine dispatches each element to its own leaf proxy, so we swap
// every one of them and record the originals for restore/dispatch.
// -----------------------------------------------------------------------------
bool netvar_array_hook::init( hash_t table, hash_t prop, RecvVarProxyFn replacement )
{
    if ( !replacement )
    {
        return false;
    }

    const RecvProp *outer{ g_netvars.get_prop( table, prop ) };
    if ( !outer || outer->m_RecvType != DPT_DataTable || !outer->m_pDataTable )
    {
        return false;
    }

    RecvTable *child{ outer->m_pDataTable };
    if ( !child->m_pProps )
    {
        return false;
    }

    this->m_count = 0;

    for ( i32 index{ }; index < child->m_nProps; ++index )
    {
        RecvProp &leaf{ child->m_pProps[ index ] };

        // Nested datatables inside an array are rare and don't carry the
        // per-element proxy we want
        if ( leaf.m_RecvType == DPT_DataTable )
        {
            continue;
        }

        if ( this->m_count >= VARS_ARRAY_HOOK_CAPACITY )
        {
            break;
        }

        saved &slot{ this->m_saved[ this->m_count ] };
        slot.m_prop     = &leaf;
        slot.m_original = leaf.m_ProxyFn;
        leaf.m_ProxyFn  = replacement;
        ++this->m_count;
    }

    return this->m_count > 0;
}

void netvar_array_hook::shutdown( )
{
    for ( usize index{ }; index < this->m_count; ++index )
    {
        saved &slot{ this->m_saved[ index ] };
        if ( slot.m_prop && slot.m_original )
        {
            slot.m_prop->m_ProxyFn = slot.m_original;
        }

        slot.m_prop     = nullptr;
        slot.m_original = nullptr;
    }

    this->m_count = 0;
}

void netvar_array_hook::call_original( usize index, const CRecvProxyData *data, void *entity, void *out ) const
{
    if ( index >= this->m_count )
    {
        return;
    }

    const saved &slot{ this->m_saved[ index ] };
    if ( slot.m_original )
    {
        slot.m_original( data, entity, out );
    }
}
