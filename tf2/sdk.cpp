#include "sdk.hpp"

#include "global.hpp"

const char *vgui::IPanel::GetName( VPANEL vguiPanel )
{
    return util::call_virtual< const char *( * )( void *, VPANEL ) >(
        this, g_tf2.m_vgui_panel_get_name )( this, vguiPanel );
}

bool CEngineVGui::IsGameUIVisible( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_engine_vgui_is_game_ui_visible )( this );
}

vec3 IPhysicsCollision::CollideGetExtent( const CPhysCollide *collide, const vec3 &origin, const vec3 &angles, const vec3 &direction )
{
    vec3 result{ }; // sneaky fucker
    util::call_virtual< vec3 *( * )( void *, vec3 *, const CPhysCollide *, const vec3 &, const vec3 &, const vec3 & ) >(
        this, g_tf2.m_physics_collision_get_extent )( this, &result, collide, origin, angles, direction );
    return result;
}

vcollide_t *CModelInfoClient::GetVCollide( const model_t *model )
{
    return util::call_virtual< vcollide_t *( * )( void *, const model_t * ) >(
        this, g_tf2.m_model_info_get_vcollide_model )( this, model );
}

void CEngineTrace::GetBrushesInAABB( const vec3 &mins, const vec3 &maxs, CUtlVector< i32 > *brushes, i32 mask )
{
    util::call_virtual< void ( * )( void *, const vec3 &, const vec3 &, CUtlVector< i32 > *, i32 ) >(
        this, g_tf2.m_engine_trace_get_brushes )( this, mins, maxs, brushes, mask );
}

bool CEngineTrace::GetBrushInfo( i32 brush, CUtlVector< brush_plane_t > *planes, i32 *contents )
{
    return util::call_virtual< bool ( * )( void *, i32, CUtlVector< brush_plane_t > *, i32 * ) >(
        this, g_tf2.m_engine_trace_get_brush_info )( this, brush, planes, contents );
}

CPhysCollide *CEngineTrace::GetCollidableFromDisplacementsInAABB( const vec3 &mins, const vec3 &maxs )
{
    return util::call_virtual< CPhysCollide *( * )( void *, const vec3 &, const vec3 & ) >(
        this, g_tf2.m_engine_trace_get_displacements )( this, mins, maxs );
}

void CEngineTrace::EnumerateEntities( const vec3 &mins, const vec3 &maxs, IEntityEnumerator *enumerator )
{
    util::call_virtual< void ( * )( void *, const vec3 &, const vec3 &, IEntityEnumerator * ) >(
        this, g_tf2.m_engine_trace_enumerate_entities )( this, mins, maxs, enumerator );
}

ICollideable *CEngineTrace::GetCollideable( IHandleEntity *entity )
{
    return util::call_virtual< ICollideable *( * )( void *, IHandleEntity * ) >(
        this, g_tf2.m_engine_trace_get_collideable )( this, entity );
}

void CEngineTrace::ClipRayToEntity( const Ray_t &ray, u32 mask, IHandleEntity *entity, trace_t *trace )
{
    util::call_virtual< void ( * )( void *, const Ray_t &, u32, IHandleEntity *, trace_t * ) >(
        this, g_tf2.m_engine_trace_clip_entity )( this, ray, mask, entity, trace );
}

const model_t *ICollideable::GetCollisionModel( )
{
    return util::call_virtual< const model_t *( * )( void * ) >(
        this, g_tf2.m_collideable_get_model )( this );
}

const vec3 &ICollideable::GetCollisionOrigin( )
{
    return util::call_virtual< const vec3 &( * )( void * ) >(
        this, g_tf2.m_collideable_get_origin )( this );
}

const vec3 &ICollideable::GetCollisionAngles( )
{
    return util::call_virtual< const vec3 &( * )( void * ) >(
        this, g_tf2.m_collideable_get_angles )( this );
}

const matrix3x4 &ICollideable::CollisionToWorldTransform( )
{
    return util::call_virtual< const matrix3x4 &( * )( void * ) >(
        this, g_tf2.m_collideable_get_transform )( this );
}

i32 ICollideable::GetSolid( )
{
    return util::call_virtual< i32 ( * )( void * ) >(
        this, g_tf2.m_collideable_get_solid )( this );
}

i32 ICollideable::GetSolidFlags( )
{
    return util::call_virtual< i32 ( * )( void * ) >(
        this, g_tf2.m_collideable_get_solid_flags )( this );
}

CPhysConvex *IPhysicsCollision::ConvexFromPlanes( float *planes, i32 count, float merge_distance )
{
    return util::call_virtual< CPhysConvex *( * )( void *, float *, i32, float ) >(
        this, g_tf2.m_physics_collision_convex_from_planes )( this, planes, count, merge_distance );
}

void IPhysicsCollision::ConvexFree( CPhysConvex *convex )
{
    util::call_virtual< void ( * )( void *, CPhysConvex * ) >(
        this, g_tf2.m_physics_collision_convex_free )( this, convex );
}

CPhysCollide *IPhysicsCollision::ConvertConvexToCollide( CPhysConvex **convexes, i32 count )
{
    return util::call_virtual< CPhysCollide *( * )( void *, CPhysConvex **, i32 ) >(
        this, g_tf2.m_physics_collision_convert_convex )( this, convexes, count );
}

void IPhysicsCollision::DestroyCollide( CPhysCollide *collide )
{
    util::call_virtual< void ( * )( void *, CPhysCollide * ) >(
        this, g_tf2.m_physics_collision_destroy_collide )( this, collide );
}

i32 IPhysicsCollision::GetConvexesUsedInCollideable( const CPhysCollide *collide, CPhysConvex **convexes, i32 capacity )
{
    return util::call_virtual< i32 ( * )( void *, const CPhysCollide *, CPhysConvex **, i32 ) >(
        this, g_tf2.m_physics_collision_get_convexes )( this, collide, convexes, capacity );
}

CPolyhedron *IPhysicsCollision::PolyhedronFromConvex( CPhysConvex *convex, bool temporary )
{
    return util::call_virtual< CPolyhedron *( * )( void *, CPhysConvex *, bool ) >(
        this, g_tf2.m_physics_collision_polyhedron )( this, convex, temporary );
}

i32 IPhysicsCollision::CreateDebugMesh( const CPhysCollide *collide, vec3 **vertices )
{
    return util::call_virtual< i32 ( * )( void *, const CPhysCollide *, vec3 ** ) >(
        this, g_tf2.m_physics_collision_create_mesh )( this, collide, vertices );
}

void IPhysicsCollision::DestroyDebugMesh( i32 count, vec3 *vertices )
{
    util::call_virtual< void ( * )( void *, i32, vec3 * ) >(
        this, g_tf2.m_physics_collision_destroy_mesh )( this, count, vertices );
}

void IPhysicsCollision::TraceCollide( const vec3 &start, const vec3 &end, const CPhysCollide *sweep, const vec3 &angles,
                                      const CPhysCollide *collide, const vec3 &origin, const vec3 &collide_angles, trace_t *trace )
{
    util::call_virtual< void ( * )( void *, const vec3 &, const vec3 &, const CPhysCollide *, const vec3 &, const CPhysCollide *, const vec3 &, const vec3 &, trace_t * ) >(
        this, g_tf2.m_physics_collision_trace_collide )( this, start, end, sweep, angles, collide, origin, collide_angles, trace );
}

static ATLAS_FORCE_INLINE u32 low_bits( u32 bits )
{
    return ( 1u << bits ) - 1u;
}

void bf_write::StartWriting( void *data, i32 bytes, i32 start_bit, i32 max_bits )
{
    if ( bytes % 4 != 0 || ( reinterpret_cast< uptr >( data ) & 3 ) != 0 )
    {
        return;
    }

    m_pData      = static_cast< unsigned long * >( data );
    m_nDataBytes = bytes;

    if ( max_bits == -1 )
    {
        m_nDataBits = bytes << 3;
    } else if ( max_bits <= bytes * 8 )
    {
        m_nDataBits = max_bits;
    }

    m_iCurBit   = start_bit;
    m_bOverflow = false;
}

void bf_write::WriteUBitLong( u32 data, i32 numbits )
{
    if ( GetNumBitsLeft( ) < numbits )
    {
        m_iCurBit = m_nDataBits;
        SetOverflowFlag( );
        return;
    }

    const i32 cur_bit_masked{ m_iCurBit & 31 };
    const i32 dword_index{ m_iCurBit >> 5 };

    m_iCurBit += numbits;

    unsigned long *out{ &m_pData[ dword_index ] };

    data = _rotl( data, cur_bit_masked );

    const u32 top{ 1u << ( numbits - 1 ) };
    const u32 mask1{ ( top * 2u - 1u ) << cur_bit_masked };
    const u32 mask2{ ( top - 1u ) >> ( 31 - cur_bit_masked ) };

    const i32 next{ static_cast< i32 >( mask2 & 1u ) };

    u32 dword1{ static_cast< u32 >( out[ 0 ] ) };
    u32 dword2{ static_cast< u32 >( out[ next ] ) };

    dword1 ^= mask1 & ( data ^ dword1 );
    dword2 ^= mask2 & ( data ^ dword2 );

    out[ next ] = dword2;
    out[ 0 ]    = dword1;
}

bool bf_write::WriteBits( const void *in, i32 bits )
{
    const u8 *data{ static_cast< const u8 * >( in ) };
    i32       bits_left{ bits };

    if ( m_iCurBit + bits > m_nDataBits )
    {
        SetOverflowFlag( );
        return false;
    }

    while ( ( reinterpret_cast< uptr >( data ) & 3 ) != 0 && bits_left >= 8 )
    {
        WriteUBitLong( *data, 8 );
        ++data;
        bits_left -= 8;
    }

    if ( bits_left >= 32 && ( m_iCurBit & 7 ) == 0 )
    {
        const i32 bytes{ bits_left >> 3 };

        atl::memcpy( reinterpret_cast< u8 * >( m_pData ) + ( m_iCurBit >> 3 ), data,
                     static_cast< usize >( bytes ) );

        data += bytes;
        m_iCurBit += bytes << 3;
        bits_left -= bytes << 3;
    }

    if ( bits_left >= 32 )
    {
        const u32 bits_right{ static_cast< u32 >( m_iCurBit & 31 ) };
        const u32 shift_back{ 32u - bits_right };
        const u32 mask_left{ low_bits( bits_right ) };
        const u32 mask_right{ ~low_bits( bits_right ) };

        unsigned long *out{ &m_pData[ m_iCurBit >> 5 ] };

        while ( bits_left >= 32 )
        {
            u32 cur{ };
            atl::memcpy( &cur, data, sizeof( cur ) );
            data += sizeof( u32 );

            *out &= mask_left;
            *out |= cur << bits_right;

            ++out;

            if ( shift_back < 32u )
            {
                cur >>= shift_back;
                *out &= mask_right;
                *out |= cur;
            }

            bits_left -= 32;
            m_iCurBit += 32;
        }
    }

    while ( bits_left >= 8 )
    {
        WriteUBitLong( *data, 8 );
        ++data;
        bits_left -= 8;
    }

    if ( bits_left )
    {
        WriteUBitLong( *data, bits_left );
    }

    return !IsOverflowed( );
}

bool bf_write::WriteString( const char *value )
{
    if ( value )
    {
        while ( *value )
        {
            WriteUBitLong( static_cast< u8 >( *value++ ), 8 );
        }
    }

    WriteUBitLong( 0, 8 );
    return !IsOverflowed( );
}

bool NET_StringCmd::WriteToBuffer( bf_write &buffer )
{
    buffer.WriteUBitLong( GetType( ), NETMSG_TYPE_BITS );
    return buffer.WriteString( m_szCommand );
}

bool NET_SetConVar::WriteToBuffer( bf_write &buffer )
{
    buffer.WriteUBitLong( GetType( ), NETMSG_TYPE_BITS );
    buffer.WriteUBitLong( 1, 8 );
    buffer.WriteString( m_name );
    buffer.WriteString( m_value );
    return !buffer.IsOverflowed( );
}

u32 bf_read::ReadUBitLong( i32 numbits )
{
    if ( m_iCurBit + numbits > m_nDataBits )
    {
        m_bOverflow = true;
        return 0;
    }

    // the engine assembles least significant bit first inside little endian dwords
    u32 value{ };

    for ( i32 index{ }; index < numbits; ++index )
    {
        const i32 bit{ m_iCurBit + index };
        value |= ( ( m_pData[ bit >> 5 ] >> ( bit & 31 ) ) & 1u ) << index;
    }

    m_iCurBit += numbits;
    return value;
}

usize bf_read::ReadString( char *out, usize capacity, bool line )
{
    usize length{ };

    while ( !m_bOverflow )
    {
        const char value{ static_cast< char >( ReadUBitLong( 8 ) ) };

        if ( value == '\0' || ( line && value == '\n' ) )
        {
            break;
        }

        if ( length + 1 < capacity )
        {
            out[ length++ ] = value;
        }
    }

    out[ length ] = '\0';
    return length;
}

u32 bf_read::ReadOneBit( )
{
    return ReadUBitLong( 1 );
}

// seven payload bits per byte with the high bit continuing, five bytes at most
u32 bf_read::ReadVarInt32( )
{
    u32 value{ };

    for ( i32 shift{ }; shift < 35; shift += 7 )
    {
        const u32 byte{ ReadUBitLong( 8 ) };
        value |= ( byte & 0x7fu ) << shift;

        if ( m_bOverflow || !( byte & 0x80u ) )
        {
            break;
        }
    }

    return value;
}

void bf_read::Seek( i32 bit )
{
    if ( bit < 0 || bit > m_nDataBits )
    {
        m_bOverflow = true;
        m_iCurBit   = m_nDataBits;
        return;
    }

    m_iCurBit = bit;
}

void bf_write::WriteWord( i32 value )
{
    WriteUBitLong( static_cast< u32 >( value ), 16 );
}

void bf_write::WriteLong( i32 value )
{
    WriteUBitLong( static_cast< u32 >( value ), 32 );
}

ETFClassID ClientClass::GetClassID( )
{
    return static_cast< ETFClassID >( m_ClassID );
}

ClientClass *CHLClient::GetAllClasses( )
{
    return util::call_virtual< ClientClass *( * )( void * ) >(
        this, g_tf2.m_chl_client_get_all_classes )( this );
}

bool CHLClient::GetPlayerView( CViewSetup &playerView )
{
    return util::call_virtual< bool ( * )( void *, CViewSetup & ) >(
        this, g_tf2.m_chl_client_get_player_view )( this, playerView );
}

void CHLClient::CreateMove( int sequence_number, float input_sample_frametime, bool active )
{
    util::call_virtual< void ( * )( void *, int, float, bool ) >(
        this, g_tf2.m_chl_client_create_move )( this, sequence_number, input_sample_frametime, active );
}

bool CHLClient::WriteUsercmdDeltaToBuffer( bf_write *buf, int from, int to, bool isnewcommand )
{
    return util::call_virtual< bool ( * )( void *, bf_write *, int, int, bool ) >(
        this, g_tf2.m_chl_client_write_usercmd_delta_to_buffer )( this, buf, from, to, isnewcommand );
}

void CInput::CAM_ToThirdPerson( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_c_input_cam_to_third_person )( this );
}

void CInput::CAM_ToFirstPerson( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_c_input_cam_to_first_person )( this );
}

bool CInput::CAM_IsThirdPerson( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_c_input_cam_is_third_person )( this );
}

IClientEntity *CClientEntityList::GetClientEntity( int num )
{
    return util::call_virtual< IClientEntity *( * )( void *, int ) >(
        this, g_tf2.m_client_entity_list_get_client_entity )( this, num );
}

IClientEntity *CClientEntityList::GetClientEntityFromHandle( CBaseHandle num )
{
    if ( !num.IsValid( ) || num.GetEntryIndex( ) >= NUM_ENT_ENTRIES )
    {
        return nullptr;
    }

    return util::call_virtual< IClientEntity *( * )( void *, CBaseHandle ) >(
        this, g_tf2.m_client_entity_list_get_client_entity_from_handle )( this, num );
}

int CClientEntityList::GetHighestEntityIndex( )
{
    return util::call_virtual< int ( * )( void * ) >(
        this, g_tf2.m_client_entity_list_get_highest_entity_index )( this );
}

CUtlVector< IClientEntityListener * > &CClientEntityList::EntityListeners( )
{
    return *reinterpret_cast< CUtlVector< IClientEntityListener * > * >(
        reinterpret_cast< u8 * >( this ) + sizeof( void * ) );
}

bool CClientEntityList::AddListenerEntity( IClientEntityListener *listener )
{
    if ( !listener )
    {
        return false;
    }

    g_tf2.m_CClientEntityList_AddListenerEntity.call_as< void >( g_tf2.m_entity_list_internal, listener );

    const auto &listeners{ this->EntityListeners( ) };
    for ( i32 index{ }; index < listeners.m_Size; ++index )
    {
        if ( listeners.m_Memory.m_pMemory[ index ] == listener )
        {
            return true;
        }
    }
    return false;
}

bool CClientEntityList::RemoveListenerEntity( IClientEntityListener *listener )
{
    if ( !listener )
    {
        return false;
    }

    auto &listeners{ this->EntityListeners( ) };
    for ( i32 index{ }; index < listeners.m_Size; )
    {
        if ( listeners.m_Memory.m_pMemory[ index ] != listener )
        {
            ++index;
            continue;
        }
        --listeners.m_Size;
        for ( i32 next{ index }; next < listeners.m_Size; ++next )
        {
            listeners.m_Memory.m_pMemory[ next ] = listeners.m_Memory.m_pMemory[ next + 1 ];
        }
        listeners.m_Memory.m_pMemory[ listeners.m_Size ] = nullptr;
    }
    return true;
}

C_BaseEntity *CClientEntityList::NextBaseEntity( C_BaseEntity *previous )
{
    return g_tf2.m_CClientEntityList_NextBaseEntity.call_as< C_BaseEntity * >( g_tf2.m_entity_list_internal, previous );
}

CBaseHandle::CBaseHandle( const CBaseHandle &other )
{
    m_Index = other.m_Index;
}

CBaseHandle::CBaseHandle( unsigned long value )
{
    m_Index = value;
}

CBaseHandle::CBaseHandle( int iEntry, int iSerialNumber )
{
    Init( iEntry, iSerialNumber );
}

void CBaseHandle::Init( int iEntry, int iSerialNumber )
{
    m_Index = ( static_cast< u32 >( iEntry ) & ENT_ENTRY_MASK ) |
              ( static_cast< u32 >( iSerialNumber ) << NUM_SERIAL_NUM_SHIFT_BITS );
}

void CBaseHandle::Term( )
{
    m_Index = INVALID_EHANDLE_INDEX;
}

bool CBaseHandle::IsValid( ) const
{
    return m_Index != INVALID_EHANDLE_INDEX;
}

int CBaseHandle::GetEntryIndex( ) const
{
    return IsValid( ) ? static_cast< int >( m_Index & ENT_ENTRY_MASK ) : NUM_ENT_ENTRIES - 1;
}

int CBaseHandle::GetSerialNumber( ) const
{
    return static_cast< int >( m_Index >> NUM_SERIAL_NUM_SHIFT_BITS );
}

int CBaseHandle::ToInt( ) const
{
    return static_cast< int >( m_Index );
}

unsigned long CBaseHandle::GetIndex( ) const
{
    return m_Index;
}

bool CBaseHandle::operator==( const CBaseHandle &other ) const
{
    return m_Index == other.m_Index;
}

bool CBaseHandle::operator!=( const CBaseHandle &other ) const
{
    return m_Index != other.m_Index;
}

bool CBaseHandle::operator<( const CBaseHandle &other ) const
{
    return m_Index < other.m_Index;
}

bool CBaseHandle::operator==( const IHandleEntity *pEnt ) const
{
    return Get( ) == pEnt;
}

bool CBaseHandle::operator!=( const IHandleEntity *pEnt ) const
{
    return Get( ) != pEnt;
}

bool CBaseHandle::operator<( const IHandleEntity *pEnt ) const
{
    return reinterpret_cast< uptr >( Get( ) ) < reinterpret_cast< uptr >( pEnt );
}

const CBaseHandle &CBaseHandle::operator=( const IHandleEntity *pEntity )
{
    return Set( pEntity );
}

const CBaseHandle &CBaseHandle::Set( const IHandleEntity *entity )
{
    m_Index = entity ? entity->GetRefEHandle( ).GetIndex( ) : INVALID_EHANDLE_INDEX;
    return *this;
}

IHandleEntity *CBaseHandle::Get( ) const
{
    if ( m_Index == INVALID_EHANDLE_INDEX || !g_tf2.m_entity_list )
    {
        return nullptr;
    }

    return g_tf2.m_entity_list->GetClientEntityFromHandle( *this );
}

int CEngineClient::GetLocalPlayer( )
{
    return util::call_virtual< int ( * )( void * ) >(
        this, g_tf2.m_engine_client_get_local_player )( this );
}

double CEngineClient::Plat_FloatTime( )
{
    return util::call_virtual< float ( * )( void * ) >(
        this, g_tf2.m_engine_client_plat_float_time )( this );
}

bool CEngineClient::GetPlayerInfo( int ent_num, player_info_t *pinfo )
{
    return util::call_virtual< bool ( * )( void *, int, player_info_t * ) >(
        this, g_tf2.m_engine_client_get_player_info )( this, ent_num, pinfo );
}

const char *CModelInfoClient::GetModelName( const model_t *model )
{
    return util::call_virtual< const char *( * )( void *, const model_t * ) >(
        this, g_tf2.m_model_info_get_model_name )( this, model );
}

studiohdr_t *CModelInfoClient::GetStudiomodel( const model_t *model )
{
    return util::call_virtual< studiohdr_t *( * )( void *, const model_t * ) >(
        this, g_tf2.m_model_info_get_studio_model )( this, model );
}

MDLHandle_t CModelInfoClient::GetCacheHandle( const model_t *model )
{
    return util::call_virtual< MDLHandle_t ( * )( void *, const model_t * ) >(
        this, g_tf2.m_model_info_get_cache_handle )( this, model );
}

MDLHandle_t IMDLCache::FindMDL( const char *path )
{
    return util::call_virtual< MDLHandle_t ( * )( void *, const char * ) >(
        this, g_tf2.m_mdl_cache_find_mdl )( this, path );
}

studiohdr_t *IMDLCache::GetStudioHdr( MDLHandle_t handle )
{
    return util::call_virtual< studiohdr_t *( * )( void *, MDLHandle_t ) >(
        this, g_tf2.m_mdl_cache_get_studio_hdr )( this, handle );
}

studiohwdata_t *IMDLCache::GetHardwareData( MDLHandle_t handle )
{
    return util::call_virtual< studiohwdata_t *( * )( void *, MDLHandle_t ) >(
        this, g_tf2.m_mdl_cache_get_hardware_data )( this, handle );
}

void IMDLCache::BeginLock( )
{
    util::call_virtual< void ( * )( void * ) >( this, g_tf2.m_mdl_cache_begin_lock )( this );
}

void IMDLCache::EndLock( )
{
    util::call_virtual< void ( * )( void * ) >( this, g_tf2.m_mdl_cache_end_lock )( this );
}

void IStaticPropMgr::CreateVPhysicsRepresentations( IPhysicsEnvironment *environment, IVPhysicsKeyHandler *defaults, void *game_data )
{
    util::call_virtual< void ( * )( void *, IPhysicsEnvironment *, IVPhysicsKeyHandler *, void * ) >(
        this, g_tf2.m_static_prop_mgr_create_vphysics_representations )( this, environment, defaults, game_data );
}

bool IStaticPropMgr::IsStaticProp( IHandleEntity *pHandleEntity ) const
{
    return IsStaticProp( pHandleEntity->GetRefEHandle( ) );
}

bool IStaticPropMgr::IsStaticProp( CBaseHandle handle ) const
{
    // die
    auto _this = const_cast< IStaticPropMgr * >( this );
    return util::call_virtual< bool ( * )( void *, const CBaseHandle * ) >(
        _this, g_tf2.m_static_prop_mgr_is_static_prop_cbasehandle )( _this, &handle );
}

void IStaticPropMgr::GetAllStaticPropsInAABB( const vec3 &mins, const vec3 &maxs, CUtlVector< ICollideable * > *out )
{
    util::call_virtual< void ( * )( void *, const vec3 &, const vec3 &, CUtlVector< ICollideable * > * ) >(
        this, g_tf2.m_static_prop_mgr_get_all_in_aabb )( this, mins, maxs, out );
}

int CEngineClient::GetPlayerForUserID( int userid )
{
    return util::call_virtual< int ( * )( void *, int ) >(
        this, g_tf2.m_engine_client_get_player_for_user_id )( this, userid );
}

bool CEngineClient::IsInGame( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_engine_client_is_in_game )( this );
}

bool CEngineClient::IsConnected( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_engine_client_is_connected )( this );
}

bool CEngineClient::IsPlayingDemo( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_engine_client_is_playing_demo )( this );
}

void CEngineClient::GetScreenSize( int &width, int &height )
{
    util::call_virtual< void ( * )( void *, int &, int & ) >(
        this, g_tf2.m_engine_client_get_screen_size )( this, width, height );
}

void CEngineClient::GetViewAngles( vec3 &va )
{
    util::call_virtual< void ( * )( void *, vec3 & ) >(
        this, g_tf2.m_engine_client_get_view_angles )( this, va );
}

void CEngineClient::SetViewAngles( vec3 &va )
{
    util::call_virtual< void ( * )( void *, vec3 & ) >(
        this, g_tf2.m_engine_client_set_view_angles )( this, va );
}

int CEngineClient::GetMaxClients( )
{
    return util::call_virtual< int ( * )( void * ) >(
        this, g_tf2.m_engine_client_get_max_clients )( this );
}

CNetChannel *CEngineClient::GetNetChannelInfo( )
{
    return util::call_virtual< CNetChannel *( * )( void * ) >(
        this, g_tf2.m_engine_client_get_net_channel_info )( this );
}

void CEngineClient::FireEvents( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_engine_client_fire_events )( this );
}

void CEngineClient::ClientCmd_Unrestricted( const char *input )
{
    util::call_virtual< void ( * )( void *, const char * ) >(
        this, g_tf2.m_engine_client_client_cmd_unrestricted )( this, input );
}

void CEngineClient::ServerCmdKeyValues( KeyValues *pKeyValues )
{
    util::call_virtual< void ( * )( void *, KeyValues * ) >(
        this, g_tf2.m_engine_client_server_cmd_key_values )( this, pKeyValues );
}

const char *CEngineClient::GetMapEntitiesString( )
{
    return util::call_virtual< const char *( * )( void * ) >(
        this, g_tf2.m_engine_client_get_map_entities_string )( this );
}

bool CEngineClient::IsTakingScreenshot( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_engine_client_is_taking_screenshot )( this );
}

void CUniformRandomStream::SetSeed( int seed )
{
    util::call_virtual< void ( * )( void *, int ) >( this, 0 )( this, seed );
}

float CUniformRandomStream::RandomFloat( float minimum, float maximum )
{
    return util::call_virtual< float ( * )( void *, float, float ) >( this, 1 )( this, minimum, maximum );
}

int CUniformRandomStream::RandomInt( int minimum, int maximum )
{
    return util::call_virtual< int ( * )( void *, int, int ) >( this, 2 )( this, minimum, maximum );
}

#if defined( ATLAS_DEV )
edict_t *IVEngineServer::PEntityOfEntIndex( i32 index )
{
    return util::call_virtual< edict_t *( * )( void *, i32 ) >( this, 19 )( this, index );
}

void CBaseAnimating::GetBoneTransform( i32 bone, matrix3x4 &bone_to_world )
{
    util::call_virtual< void ( * )( void *, i32, matrix3x4 & ) >( this, 210 )( this, bone, bone_to_world );
}
#endif // ATLAS_DEV

CRC32_t CUserCmd::GetChecksum( ) const
{
    return g_tf2.m_CUserCmd_GetChecksum.call_as< CRC32_t >( this );
}

void CUserCmd::Reset( )
{
    command_number     = 0;
    tick_count         = 0;
    viewangles.x       = 0.f;
    viewangles.y       = 0.f;
    viewangles.z       = 0.f;
    forwardmove        = 0.0f;
    sidemove           = 0.0f;
    upmove             = 0.0f;
    buttons            = 0;
    impulse            = 0;
    weaponselect       = 0;
    weaponsubtype      = 0;
    random_seed        = 0;
    server_random_seed = 0;
    mousedx            = 0;
    mousedy            = 0;
}

void CMatSystemSurface::UnlockCursor( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_mat_system_surface_unlock_cursor )( this );
}

void CMatSystemSurface::SetCursorAlwaysVisible( bool visible )
{
    util::call_virtual< void ( * )( void *, bool ) >(
        this, g_tf2.m_mat_system_surface_set_cursor_always_visible )( this, visible );
}

void CMatSystemSurface::PlaySound( const char *file_name )
{
    util::call_virtual< void ( * )( void *, const char * ) >(
        this, g_tf2.m_mat_system_surface_play_sound )( this, file_name );
}

void CMoveHelperClient::ProcessImpacts( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_move_helper_client_process_impacts )( this );
}

void CMoveHelperClient::SetHost( C_BasePlayer *host )
{
    util::call_virtual< void ( * )( void *, C_BasePlayer * ) >(
        this, g_tf2.m_move_helper_client_set_host )( this, host );
}

void CMoveHelperClient::ResetTouchList( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_move_helper_client_reset_touch_list )( this );
}

void CPrediction::GetLocalViewAngles( vec3 &angles )
{
    util::call_virtual< void ( * )( void *, vec3 & ) >(
        this, g_tf2.m_prediction_get_local_view_angles )( this, angles );
}

void CPrediction::SetLocalViewAngles( vec3 &angles )
{
    util::call_virtual< void ( * )( void *, vec3 & ) >(
        this, g_tf2.m_prediction_set_local_view_angles )( this, angles );
}

void CPrediction::RunCommand( C_BasePlayer *player, CUserCmd *ucmd, CMoveHelperClient *moveHelper )
{
    util::call_virtual< void ( * )( void *, C_BasePlayer *, CUserCmd *, CMoveHelperClient * ) >(
        this, g_tf2.m_prediction_run_command )( this, player, ucmd, moveHelper );
}

void CPrediction::SetupMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveHelperClient *moveHelper, CMoveData *move )
{
    util::call_virtual< void ( * )( void *, C_BasePlayer *, CUserCmd *, CMoveHelperClient *, CMoveData * ) >(
        this, g_tf2.m_prediction_setup_move )(
        this,
        player,
        ucmd,
        moveHelper,
        move );
}

void CPrediction::FinishMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
    util::call_virtual< void ( * )( void *, C_BasePlayer *, CUserCmd *, CMoveData * ) >(
        this, g_tf2.m_prediction_finish_move )( this, player, ucmd, move );
}

int CPredictionCopy::TransferData( const char *operation, int entindex, datamap_t *dmap )
{
    return g_tf2.m_PredCopy_TransferData.call_as< int, void *, const char *, int, datamap_t * >(
        this, operation, entindex, dmap );
}

void CVRenderView::GetMatricesForView(
    const CViewSetup &view,
    VMatrix          *pWorldToView,
    VMatrix          *pViewToProjection,
    VMatrix          *pWorldToProjection,
    VMatrix          *pWorldToPixels )
{
    util::call_virtual< void ( * )( void *, const CViewSetup &, VMatrix *, VMatrix *, VMatrix *, VMatrix * ) >(
        this, g_tf2.m_render_view_get_matrices_for_view )(
        this,
        view,
        pWorldToView,
        pViewToProjection,
        pWorldToProjection,
        pWorldToPixels );
}

void CStudioRender::SetColorModulation( const float *color )
{
    util::call_virtual< void ( * )( void *, const float * ) >(
        this, g_tf2.m_studio_render_set_color_modulation )( this, color );
}

void CStudioRender::SetAlphaModulation( const float alpha )
{
    util::call_virtual< void ( * )( void *, const float ) >(
        this, g_tf2.m_studio_render_set_alpha_modulation )( this, alpha );
}

void CStudioRender::ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType )
{
    util::call_virtual< void ( * )( void *, IMaterial *, OverrideType_t ) >(
        this, g_tf2.m_studio_render_forced_material_override )( this, newMaterial, nOverrideType );
}

IMaterial *CMaterialSystem::CreateMaterial( const char *pMaterialName, KeyValues *pVMTKeyValues )
{
    return util::call_virtual< IMaterial *( * )( void *, const char *, KeyValues * ) >(
        this, g_tf2.m_material_system_create_material )( this, pMaterialName, pVMTKeyValues );
}

IMaterial *CMaterialSystem::FindMaterial( const char *pMaterialName, const char *pTextureGroupName, bool complain, const char *pComplainPrefix )
{
    return util::call_virtual< IMaterial *( * )( void *, const char *, const char *, bool, const char * ) >(
        this, g_tf2.m_material_system_find_material )( this, pMaterialName, pTextureGroupName, complain, pComplainPrefix );
}

IMatRenderContext *CMaterialSystem::GetRenderContext( )
{
    return util::call_virtual< IMatRenderContext *( * )( void * ) >(
        this, g_tf2.m_material_system_get_render_context )( this );
}

MaterialHandle_t CMaterialSystem::FirstMaterial( )
{
    return util::call_virtual< MaterialHandle_t ( * )( void * ) >(
        this, g_tf2.m_material_system_first_material )( this );
}

MaterialHandle_t CMaterialSystem::NextMaterial( MaterialHandle_t handle )
{
    return util::call_virtual< MaterialHandle_t ( * )( void *, MaterialHandle_t ) >(
        this, g_tf2.m_material_system_next_material )( this, handle );
}

MaterialHandle_t CMaterialSystem::InvalidMaterial( )
{
    return util::call_virtual< MaterialHandle_t ( * )( void * ) >(
        this, g_tf2.m_material_system_invalid_material )( this );
}

IMaterial *CMaterialSystem::GetMaterial( MaterialHandle_t handle )
{
    return util::call_virtual< IMaterial *( * )( void *, MaterialHandle_t ) >(
        this, g_tf2.m_material_system_get_material )( this, handle );
}

void IMatRenderContext::DepthRange( float zNear, float zFar )
{
    util::call_virtual< void ( * )( void *, float, float ) >(
        this, g_tf2.m_render_context_depth_range )( this, zNear, zFar );
}

void IMatRenderContext::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil )
{
    util::call_virtual< void ( * )( void *, bool, bool, bool ) >(
        this, g_tf2.m_render_context_clear_buffers )( this, bClearColor, bClearDepth, bClearStencil );
}

void IMatRenderContext::SetStencilEnable( bool onoff )
{
    util::call_virtual< void ( * )( void *, bool ) >(
        this, g_tf2.m_render_context_set_stencil_enable )( this, onoff );
}

void IMatRenderContext::SetStencilFailOperation( StencilOperation_t op )
{
    util::call_virtual< void ( * )( void *, StencilOperation_t ) >(
        this, g_tf2.m_render_context_set_stencil_fail_operation )( this, op );
}

void IMatRenderContext::SetStencilZFailOperation( StencilOperation_t op )
{
    util::call_virtual< void ( * )( void *, StencilOperation_t ) >(
        this, g_tf2.m_render_context_set_stencil_zfail_operation )( this, op );
}

void IMatRenderContext::SetStencilPassOperation( StencilOperation_t op )
{
    util::call_virtual< void ( * )( void *, StencilOperation_t ) >(
        this, g_tf2.m_render_context_set_stencil_pass_operation )( this, op );
}

void IMatRenderContext::SetStencilCompareFunction( StencilComparisonFunction_t fn )
{
    util::call_virtual< void ( * )( void *, StencilComparisonFunction_t ) >(
        this, g_tf2.m_render_context_set_stencil_compare_function )( this, fn );
}

void IMatRenderContext::SetStencilReferenceValue( int ref )
{
    util::call_virtual< void ( * )( void *, int ) >(
        this, g_tf2.m_render_context_set_stencil_reference_value )( this, ref );
}

void IMatRenderContext::SetStencilTestMask( u32 msk )
{
    util::call_virtual< void ( * )( void *, u32 ) >(
        this, g_tf2.m_render_context_set_stencil_test_mask )( this, msk );
}

void IMatRenderContext::SetStencilWriteMask( u32 msk )
{
    util::call_virtual< void ( * )( void *, u32 ) >(
        this, g_tf2.m_render_context_set_stencil_write_mask )( this, msk );
}

void IMatRenderContext::SetRenderTarget( ITexture *texture )
{
    util::call_virtual< void ( * )( void *, ITexture * ) >(
        this, g_tf2.m_render_context_set_render_target )( this, texture );
}

void IMatRenderContext::Viewport( int x, int y, int width, int height )
{
    util::call_virtual< void ( * )( void *, int, int, int, int ) >(
        this, g_tf2.m_render_context_viewport )( this, x, y, width, height );
}

void IMatRenderContext::CullMode( MaterialCullMode_t mode )
{
    util::call_virtual< void ( * )( void *, MaterialCullMode_t ) >(
        this, g_tf2.m_render_context_cull_mode )( this, mode );
}

void IMatRenderContext::ClearColor4ub( u8 r, u8 g, u8 b, u8 a )
{
    util::call_virtual< void ( * )( void *, u8, u8, u8, u8 ) >(
        this, g_tf2.m_render_context_clear_color4ub )( this, r, g, b, a );
}

void IMatRenderContext::DrawScreenSpaceRectangle(
    IMaterial *material,
    int        dest_x,
    int        dest_y,
    int        width,
    int        height,
    float      src_x0,
    float      src_y0,
    float      src_x1,
    float      src_y1,
    int        src_width,
    int        src_height )
{
    util::call_virtual< void ( * )( void *, IMaterial *, int, int, int, int, float, float, float, float, int, int, void *, int, int ) >(
        this, g_tf2.m_render_context_draw_screen_space_rectangle )(
        this, material, dest_x, dest_y, width, height, src_x0, src_y0, src_x1, src_y1, src_width, src_height, nullptr, 1, 1 );
}

void IMatRenderContext::PushRenderTargetAndViewport( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_render_context_push_render_target_and_viewport )( this );
}

void IMatRenderContext::PopRenderTargetAndViewport( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_render_context_pop_render_target_and_viewport )( this );
}

void CMeshDX8::HandleLateCreation( )
{
    util::call_virtual< void ( * )( void * ) >( this, g_tf2.m_mesh_handle_late_creation )( this );
}

void ICallQueue::QueueFunctor( CFunctor *functor )
{
    functor->AddRef( );
    util::call_virtual< void ( * )( void *, CFunctor * ) >(
        this, g_tf2.m_call_queue_queue_functor_internal )( this, functor );
}

ICallQueue *IMatRenderContext::GetCallQueue( )
{
    return util::call_virtual< ICallQueue *( * )( void * ) >(
        this, g_tf2.m_render_context_get_call_queue )( this );
}

void IMatRenderContext::GetMatrix( i32 mode, VMatrix *matrix )
{
    util::call_virtual< void ( * )( void *, i32, VMatrix * ) >(
        this, g_tf2.m_render_context_get_matrix )( this, mode, matrix );
}

void IMatRenderContext::Release( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_render_context_release )( this );
}

void IMatRenderContext::FogMode( MaterialFogMode_t mode )
{
    util::call_virtual< void ( * )( void *, MaterialFogMode_t ) >(
        this, g_tf2.m_render_context_fog_mode )( this, mode );
}

void IMatRenderContext::FogStart( float start )
{
    util::call_virtual< void ( * )( void *, float ) >(
        this, g_tf2.m_render_context_fog_start )( this, start );
}

void IMatRenderContext::FogEnd( float end )
{
    util::call_virtual< void ( * )( void *, float ) >(
        this, g_tf2.m_render_context_fog_end )( this, end );
}

void IMatRenderContext::FogColor3fv( const float *rgb )
{
    util::call_virtual< void ( * )( void *, const float * ) >(
        this, g_tf2.m_render_context_fog_color3fv )( this, rgb );
}

void IMatRenderContext::FogMaxDensity( float max_density )
{
    util::call_virtual< void ( * )( void *, float ) >(
        this, g_tf2.m_render_context_fog_max_density )( this, max_density );
}

const char *IMaterial::GetName( )
{
    return util::call_virtual< const char *( * )( void * ) >(
        this, g_tf2.m_material_get_name )( this );
}

const char *IMaterial::GetTextureGroupName( )
{
    return util::call_virtual< const char *( * )( void * ) >(
        this, g_tf2.m_material_get_texture_group_name )( this );
}

void IMaterial::ColorModulate( float r, float g, float b )
{
    util::call_virtual< void ( * )( void *, float, float, float ) >(
        this, g_tf2.m_material_color_modulate )( this, r, g, b );
}

void IMaterial::IncrementReferenceCount( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_material_increment_reference_count )( this );
}

void CVRenderView::Push3DView( const CViewSetup &view, i32 flags, ITexture *render_target, VPlane *frustum )
{
    util::call_virtual< void ( * )( void *, const CViewSetup &, i32, ITexture *, VPlane * ) >(
        this, g_tf2.m_render_view_push_3d_view )( this, view, flags, render_target, frustum );
}

void CVRenderView::PopView( VPlane *frustum )
{
    util::call_virtual< void ( * )( void *, VPlane * ) >(
        this, g_tf2.m_render_view_pop_view )( this, frustum );
}

void CVRenderView::SetBlend( float blend )
{
    util::call_virtual< void ( * )( void *, float ) >(
        this, g_tf2.m_render_view_set_blend )( this, blend );
}

i32 CModelInfoClient::GetModelIndex( const char *name )
{
    return util::call_virtual< i32 ( * )( void *, const char * ) >(
        this, g_tf2.m_model_info_get_model_index )( this, name );
}

vcollide_t *CModelInfoClient::GetVCollide( i32 model_index )
{
    return util::call_virtual< vcollide_t *( * )( void *, i32 ) >(
        this, g_tf2.m_model_info_get_vcollide )( this, model_index );
}

i32 CModelInfoClient::GetModelType( const model_t *model )
{
    return util::call_virtual< i32 ( * )( void *, const model_t * ) >(
        this, g_tf2.m_model_info_get_model_type )( this, model );
}

const model_t *CModelInfoClient::GetModel( i32 model_index )
{
    return util::call_virtual< const model_t *( * )( void *, i32 ) >(
        this, g_tf2.m_model_info_get_model )( this, model_index );
}

void CModelInfoClient::GetModelBounds( const model_t *model, vec3 &mins, vec3 &maxs )
{
    util::call_virtual< void ( * )( void *, const model_t *, vec3 *, vec3 * ) >(
        this, g_tf2.m_model_info_get_model_bounds )( this, model, &mins, &maxs );
}

CPhysCollide *CModelInfoClient::GetCollideForVirtualTerrain( i32 index )
{
    return util::call_virtual< CPhysCollide *( * )( void *, i32 ) >(
        this, g_tf2.m_model_info_get_collide_for_virtual_terrain )( this, index );
}

// NOTE: Hoku; I could not be fucked to sig trace filters for this, so I'm rebuilding them.
inline C_BaseEntity *EntityFromEntityHandle( IHandleEntity *pHandleEntity )
{
    IClientUnknown *pUnk = ( IClientUnknown * )pHandleEntity;

    // FIXME: Hoku; The server code does this, but the client code doesn't. Remove if this causes issues!
    if ( g_tf2.m_static_prop_mgr->IsStaticProp( pHandleEntity ) )
    {
        return nullptr;
    }

    return pUnk->GetBaseEntity( );
}

//-----------------------------------------------------------------------------
// A standard filter to be applied to just about everything.
//-----------------------------------------------------------------------------
bool StandardFilterRules( IHandleEntity *pHandleEntity, int fContentsMask )
{
    C_BaseEntity *pCollide{ EntityFromEntityHandle( pHandleEntity ) };

    // Static prop case...
    if ( !pCollide )
    {
        return true;
    }

    SolidType_t    solid{ pCollide->GetSolid( ) };
    const model_t *pModel{ pCollide->GetModel( ) };

    if ( ( g_tf2.m_model_info->GetModelType( pModel ) != mod_brush ) || ( solid != SOLID_BSP && solid != SOLID_VPHYSICS ) )
    {
        if ( ( fContentsMask & CONTENTS_MONSTER ) == 0 )
        {
            return false;
        }
    }

    // This code is used to cull out tests against see-thru entities
    if ( !( fContentsMask & CONTENTS_WINDOW ) && pCollide->IsTransparent( ) )
    {
        return false;
    }

    if ( !( fContentsMask & CONTENTS_MOVEABLE ) && ( pCollide->m_MoveType( ) == MOVETYPE_PUSH ) )
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
//
// Shared client/server trace filter code
//
//-----------------------------------------------------------------------------
bool PassServerEntityFilter( const IHandleEntity *pTouch, const IHandleEntity *pPass )
{
    if ( !pPass )
        return true;

    if ( pTouch == pPass )
        return false;

    C_BaseEntity *pEntTouch{ EntityFromEntityHandle( const_cast< IHandleEntity * >( pTouch ) ) };
    C_BaseEntity *pEntPass{ EntityFromEntityHandle( const_cast< IHandleEntity * >( pPass ) ) };
    if ( !pEntTouch || !pEntPass )
        return true;

    // don't clip against own missiles
    if ( pEntTouch->m_hOwnerEntity( ).Get( ) == pEntPass )
        return false;

    // don't clip against owner
    if ( pEntPass->m_hOwnerEntity( ).Get( ) == pEntTouch )
        return false;

    return true;
}

// @ CTFGameRules::ShouldCollide & CGameRules::ShouldCollide;
static bool TFGameRules_ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
    if ( collisionGroup0 > collisionGroup1 )
    {
        const int swap{ collisionGroup0 };
        collisionGroup0 = collisionGroup1;
        collisionGroup1 = swap;
    }

    if ( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT && collisionGroup1 == COLLISION_GROUP_WEAPON )
    {
        return false;
    }

    if ( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT && collisionGroup1 == COLLISION_GROUP_PROJECTILE )
    {
        return false;
    }

    const bool rocket_like{ collisionGroup1 == TFCOLLISION_GROUP_ROCKETS ||
                            collisionGroup1 == TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS };

    if ( collisionGroup0 == COLLISION_GROUP_PLAYER && rocket_like )
    {
        return true;
    }

    if ( rocket_like &&
         ( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT ||
           collisionGroup0 == COLLISION_GROUP_WEAPON ||
           collisionGroup0 == COLLISION_GROUP_PROJECTILE ||
           collisionGroup0 == TF_COLLISIONGROUP_GRENADES ) )
    {
        return false;
    }

    if ( collisionGroup1 == TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS &&
         ( collisionGroup0 == TFCOLLISION_GROUP_ROCKETS ||
           collisionGroup0 == TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS ) )
    {
        return false;
    }

    if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT ) &&
         collisionGroup1 == TF_COLLISIONGROUP_GRENADES )
    {
        return false;
    }

    if ( collisionGroup1 == TFCOLLISION_GROUP_RESPAWNROOMS )
    {
        return collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT;
    }

    if ( collisionGroup0 == TF_COLLISIONGROUP_GRENADES && collisionGroup1 == TF_COLLISIONGROUP_GRENADES )
    {
        return false;
    }

    if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT ) &&
         collisionGroup1 == TFCOLLISION_GROUP_COMBATOBJECT )
    {
        return false;
    }

    if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT ) &&
         collisionGroup1 == TFCOLLISION_GROUP_TANK )
    {
        return false;
    }

    // @ CGameRules::ShouldCollide
    if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT ) &&
         collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
    {
        return false;
    }

    if ( collisionGroup0 == COLLISION_GROUP_DEBRIS && collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
    {
        return true;
    }

    if ( collisionGroup0 == COLLISION_GROUP_IN_VEHICLE || collisionGroup1 == COLLISION_GROUP_IN_VEHICLE )
    {
        return false;
    }

    if ( collisionGroup1 == COLLISION_GROUP_DOOR_BLOCKER && collisionGroup0 != COLLISION_GROUP_NPC )
    {
        return false;
    }

    if ( collisionGroup0 == COLLISION_GROUP_PLAYER && collisionGroup1 == COLLISION_GROUP_PASSABLE_DOOR )
    {
        return false;
    }

    if ( collisionGroup0 == COLLISION_GROUP_DEBRIS || collisionGroup0 == COLLISION_GROUP_DEBRIS_TRIGGER )
    {
        return false;
    }

    if ( ( collisionGroup0 == COLLISION_GROUP_DISSOLVING || collisionGroup1 == COLLISION_GROUP_DISSOLVING ) &&
         collisionGroup0 != COLLISION_GROUP_NONE )
    {
        return false;
    }

    if ( collisionGroup0 == COLLISION_GROUP_INTERACTIVE_DEBRIS &&
         ( collisionGroup1 == COLLISION_GROUP_INTERACTIVE_DEBRIS ||
           collisionGroup1 == COLLISION_GROUP_PLAYER_MOVEMENT ||
           collisionGroup1 == COLLISION_GROUP_PLAYER ) )
    {
        return false;
    }

    if ( collisionGroup0 == COLLISION_GROUP_BREAKABLE_GLASS && collisionGroup1 == COLLISION_GROUP_BREAKABLE_GLASS )
    {
        return false;
    }

    if ( collisionGroup1 == COLLISION_GROUP_INTERACTIVE && collisionGroup0 != COLLISION_GROUP_NONE )
    {
        return false;
    }

    if ( collisionGroup1 == COLLISION_GROUP_PROJECTILE &&
         ( collisionGroup0 == COLLISION_GROUP_DEBRIS ||
           collisionGroup0 == COLLISION_GROUP_WEAPON ||
           collisionGroup0 == COLLISION_GROUP_PROJECTILE ) )
    {
        return false;
    }

    if ( collisionGroup1 == COLLISION_GROUP_WEAPON &&
         ( collisionGroup0 == COLLISION_GROUP_VEHICLE ||
           collisionGroup0 == COLLISION_GROUP_PLAYER ||
           collisionGroup0 == COLLISION_GROUP_NPC ) )
    {
        return false;
    }

    if ( collisionGroup0 == COLLISION_GROUP_VEHICLE_CLIP || collisionGroup1 == COLLISION_GROUP_VEHICLE_CLIP )
    {
        return collisionGroup0 == COLLISION_GROUP_VEHICLE;
    }

    return true;
}

CTraceFilterSimple::CTraceFilterSimple(
    const IHandleEntity *passentity,
    int                  collisionGroup,
    ShouldHitFunc_t      pExtraShouldHitCheckFn ) :
    m_pPassEnt{ passentity },
    m_collisionGroup{ collisionGroup },
    m_pExtraShouldHitCheckFunction{ pExtraShouldHitCheckFn }
{
}

bool CTraceFilterSimple::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
    if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
    {
        return false;
    }

    if ( m_pPassEnt )
    {
        if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
        {
            return false;
        }
    }

    // Don't test if the game code tells us we should ignore this collision...
    auto *pEntity{ ( C_BaseEntity * )g_tf2.m_entity_list->GetClientEntityFromHandle( pHandleEntity->GetRefEHandle( ) ) };
    if ( !pEntity )
        return false;

    if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
    {
        return false;
    }

    if ( TFGameRules( ) && !TFGameRules_ShouldCollide( m_collisionGroup, pEntity->m_CollisionGroup( ) ) )
    {
        return false;
    }

    if ( m_pExtraShouldHitCheckFunction )
    {
        if ( !m_pExtraShouldHitCheckFunction( pHandleEntity, contentsMask ) )
        {
            return false;
        }
    }

    return true;
}

void CTraceFilterSimple::SetPassEntity( const IHandleEntity *pPassEntity )
{
    m_pPassEnt = pPassEntity;
}

void CTraceFilterSimple::SetCollisionGroup( int iCollisionGroup )
{
    m_collisionGroup = iCollisionGroup;
}

const IHandleEntity *CTraceFilterSimple::GetPassEntity( )
{
    return m_pPassEnt;
}

CMedigunFilter::CMedigunFilter( C_BaseEntity *pShooter ) : CTraceFilterSimple( pShooter, COLLISION_GROUP_WEAPON )
{
    m_pShooter = pShooter;
}

bool CMedigunFilter::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
    // If it hit an edict that isn't the target and is on our team, then the ray is blocked.
    C_BaseEntity *pEnt{ static_cast< C_BaseEntity * >( pHandleEntity ) };

    // Ignore collisions with the shooter
    if ( pEnt == m_pShooter )
    {
        return false;
    }

    if ( pEnt->m_iTeamNum( ) == m_pShooter->m_iTeamNum( ) )
    {
        return false;
    }

    return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}

bool CTraceFilterProjectileRedirect::ShouldHitEntity( IHandleEntity *pEntity, int contentsMask )
{
    C_BaseEntity *entity{ EntityFromEntityHandle( pEntity ) };
    if ( entity && entity == m_pSkipTarget )
    {
        return false;
    }

    if ( entity && entity->m_iTeamNum( ) == m_ignore_team )
    {
        switch ( entity->GetClientClass( )->GetClassID( ) )
        {
            case ETFClassID::CTFPlayer:
            case ETFClassID::CTFMedigunShield:
            case ETFClassID::CTFReviveMarker: return false;
            default: break;
        }
    }

    return CTraceFilterSimple::ShouldHitEntity( pEntity, contentsMask );
}

bool CTraceFilterHitscan::ShouldHitEntity( IHandleEntity *pEntity, int contentsMask )
{
    if ( !pEntity || pEntity == m_pSkip || pEntity == m_pSkipTarget )
        return false;

    const auto base_local = g_tf2.m_entity_list->GetClientEntity( g_tf2.m_engine->GetLocalPlayer( ) );
    if ( !base_local )
    {
        return false;
    }

    const auto local_player  = base_local->GetBaseEntity( )->As< C_TFPlayer * >( );
    const auto active_weapon = local_player->m_hActiveWeapon( ).Get( );
    const auto local_weapon  = active_weapon ? active_weapon->As< C_TFWeaponBase * >( ) : nullptr;

    const auto client_entity = static_cast< IClientEntity * >( pEntity );
    if ( !client_entity )
    {
        return false;
    }

    const auto base_entity = client_entity->GetBaseEntity( );
    if ( !base_entity )
    {
        return false;
    }

    const auto client_class = base_entity->GetClientClass( );
    if ( !client_class )
    {
        return false;
    }

    switch ( client_class->GetClassID( ) )
    {
        case ETFClassID::CFuncAreaPortalWindow:
        case ETFClassID::CFuncRespawnRoomVisualizer:
        case ETFClassID::CSniperDot:
        case ETFClassID::CTFAmmoPack: return false;
        case ETFClassID::CTFMedigunShield:
        {
            if ( base_entity->m_iTeamNum( ) == local_player->m_iTeamNum( ) )
            {
                return false;
            }

            break;
        }

        case ETFClassID::CTFPlayer:
        case ETFClassID::CObjectSentrygun:
        case ETFClassID::CObjectTeleporter:
        case ETFClassID::CObjectDispenser:
        {
            if ( !local_weapon )
            {
                break;
            }

            switch ( local_weapon->GetWeaponID( ) )
            {
                case TF_WEAPON_SNIPERRIFLE:
                case TF_WEAPON_SNIPERRIFLE_CLASSIC:
                case TF_WEAPON_SNIPERRIFLE_DECAP:
                {
                    if ( local_player->m_iTeamNum( ) == base_entity->m_iTeamNum( ) )
                    {
                        return false;
                    }

                    break;
                }
            }
        }
        default: break;
    }

    return true;
}

TraceType_t CTraceFilterHitscan::GetTraceType( ) const
{
    return CTraceFilter::GetTraceType( );
}

bool CTraceFilterProjectile::ShouldHitEntity( IHandleEntity *pEntity, int contentsMask )
{
    if ( !pEntity || pEntity == m_pSkip )
    {
        return false;
    }

    const auto client_entity{ g_tf2.m_entity_list->GetClientEntityFromHandle( pEntity->GetRefEHandle( ) ) };
    if ( !client_entity )
    {
        return false;
    }

    const auto base_entity{ client_entity->GetBaseEntity( ) };
    if ( !base_entity || base_entity == m_pSkip || base_entity == m_pSkipTarget )
    {
        return false;
    }

    const auto client_class{ client_entity->GetClientClass( ) };
    if ( !client_class )
    {
        return false;
    }

    if ( base_entity->GetSolid( ) == SOLID_NONE ||
         ( base_entity->m_usSolidFlags( ) & FSOLID_NOT_SOLID ) != 0 )
    {
        return false;
    }

    // ask the entity itself the same question the server asks;
    if ( !base_entity->ShouldCollide( COLLISION_GROUP_PROJECTILE, contentsMask ) )
    {
        return false;
    }

    switch ( client_class->GetClassID( ) )
    {
        case ETFClassID::CTFProjectile_Rocket:
        case ETFClassID::CTFProjectile_SentryRocket:
        case ETFClassID::CTFProjectile_Arrow:
        case ETFClassID::CTFProjectile_HealingBolt:
        case ETFClassID::CTFProjectile_Flare:
        case ETFClassID::CTFProjectile_EnergyBall:
        case ETFClassID::CTFProjectile_EnergyRing:
        case ETFClassID::CTFGrenadePipebombProjectile:
        case ETFClassID::CTFProjectile_Jar:
        case ETFClassID::CTFProjectile_JarMilk:
        case ETFClassID::CTFProjectile_Cleaver:
        case ETFClassID::CTFProjectile_BallOfFire: return false;
        case ETFClassID::CTFPlayer: return m_players_block && base_entity->m_iTeamNum( ) != m_skip_team;
        default: return true;
    }
}

TraceType_t CTraceFilterProjectile::GetTraceType( ) const
{
    return m_entities_only ? TRACE_ENTITIES_ONLY : TRACE_EVERYTHING;
}

bool CTraceFilterWorld::ShouldHitEntity( IHandleEntity *pEntity, int contentsMask )
{
    const auto client_entity = g_tf2.m_entity_list->GetClientEntityFromHandle( pEntity->GetRefEHandle( ) );
    if ( !client_entity )
    {
        return false;
    }

    const auto base_entity = client_entity->GetBaseEntity( );
    if ( !base_entity )
    {
        return false;
    }

    if ( base_entity == m_pSkip )
    {
        return false;
    }

    const auto cc = client_entity->GetClientClass( );
    if ( !cc )
    {
        return false;
    }

    switch ( cc->GetClassID( ) )
    {
        case ETFClassID::CObjectCartDispenser:
        case ETFClassID::CBaseDoor:
        case ETFClassID::CPhysicsProp:
        case ETFClassID::CDynamicProp:
        case ETFClassID::CBaseEntity:
        case ETFClassID::CFuncTrackTrain: return true;
        default: return false;
    }
}

TraceType_t CTraceFilterWorld::GetTraceType( ) const
{
    return CTraceFilter::GetTraceType( );
}

void IFileSystem::AddSearchPath( const char *path, const char *path_id, i32 add_type )
{
    util::call_virtual< void ( * )( void *, const char *, const char *, i32 ) >(
        this, g_tf2.m_filesystem_add_search_path )( this, path, path_id, add_type );
}

const char *IFileSystem::RelativePathToFullPath(
    const char *file_name,
    const char *path_id,
    char       *destination,
    i32         capacity )
{
    return util::call_virtual< const char *( * )( void *, const char *, const char *, char *, i32, i32, void * ) >(
        this, g_tf2.m_filesystem_relative_path_to_full_path )( this, file_name, path_id,
                                                               destination, capacity, 0, nullptr );
}

void IFileSystem::CreateDirHierarchy( const char *path, const char *path_id )
{
    util::call_virtual< void ( * )( void *, const char *, const char * ) >(
        this, g_tf2.m_filesystem_create_dir_hierarchy )( this, path, path_id );
}

const char *IFileSystem::FindFirstEx( const char *wild_card, const char *path_id,
                                      FileFindHandle_t *handle )
{
    return util::call_virtual< const char *( * )( void *, const char *, const char *, FileFindHandle_t * ) >(
        this, g_tf2.m_filesystem_find_first_ex )( this, wild_card, path_id, handle );
}

const char *IFileSystem::FindNext( FileFindHandle_t handle )
{
    return util::call_virtual< const char *( * )( void *, FileFindHandle_t ) >(
        this, g_tf2.m_filesystem_find_next )( this, handle );
}

bool IFileSystem::FindIsDirectory( FileFindHandle_t handle )
{
    return util::call_virtual< bool ( * )( void *, FileFindHandle_t ) >(
        this, g_tf2.m_filesystem_find_is_directory )( this, handle );
}

void IFileSystem::FindClose( FileFindHandle_t handle )
{
    util::call_virtual< void ( * )( void *, FileFindHandle_t ) >(
        this, g_tf2.m_filesystem_find_close )( this, handle );
}

void *IFileSystem::base( )
{
    return reinterpret_cast< u8 * >( this ) + sizeof( void * );
}

FileHandle_t IFileSystem::Open( const char *file_name, const char *options, const char *path_id )
{
    void *self{ this->base( ) };

    return util::call_virtual< FileHandle_t ( * )( void *, const char *, const char *, const char * ) >(
        self, g_tf2.m_filesystem_open )( self, file_name, options, path_id );
}

u32 IFileSystem::Size( FileHandle_t file )
{
    void *self{ this->base( ) };

    return util::call_virtual< u32 ( * )( void *, FileHandle_t ) >(
        self, g_tf2.m_filesystem_size )( self, file );
}

i32 IFileSystem::Read( void *destination, i32 size, FileHandle_t file )
{
    void *self{ this->base( ) };

    return util::call_virtual< i32 ( * )( void *, void *, i32, FileHandle_t ) >(
        self, g_tf2.m_filesystem_read )( self, destination, size, file );
}

void IFileSystem::Close( FileHandle_t file )
{
    void *self{ this->base( ) };

    util::call_virtual< void ( * )( void *, FileHandle_t ) >(
        self, g_tf2.m_filesystem_close )( self, file );
}

Beam_t *IViewRenderBeams::CreateBeamPoints( BeamInfo_t &info )
{
    return util::call_virtual< Beam_t *( * )( void *, BeamInfo_t & ) >(
        this, g_tf2.m_view_render_beams_create_beam_points )( this, info );
}

void IViewRenderBeams::DrawBeam( Beam_t *beam )
{
    util::call_virtual< void ( * )( void *, Beam_t * ) >(
        this, g_tf2.m_view_render_beams_draw_beam )( this, beam );
}

void CEngineTrace::TraceRay( const Ray_t &ray, u32 fMask, ITraceFilter *pTraceFilter, trace_t *pTrace )
{
    util::call_virtual< void ( * )( void *, const Ray_t &ray, u32, ITraceFilter *, trace_t * ) >(
        this, g_tf2.m_engine_trace_trace_ray )( this, ray, fMask, pTraceFilter, pTrace );
}

void IMatRenderContext::BindLocalCubemap( ITexture *texture )
{
    util::call_virtual< void ( * )( void *, ITexture * ) >(
        this, g_tf2.m_render_context_bind_local_cubemap )( this, texture );
}

void IMatRenderContext::SetAmbientLight( float r, float g, float b )
{
    util::call_virtual< void ( * )( void *, float, float, float ) >(
        this, g_tf2.m_render_context_set_ambient_light )( this, r, g, b );
}

void IMatRenderContext::SetAmbientLightCube( const float ( *cube )[ 4 ] )
{
    util::call_virtual< void ( * )( void *, const float ( * )[ 4 ] ) >(
        this, g_tf2.m_render_context_set_ambient_light_cube )( this, cube );
}

void IMatRenderContext::SetLightingOrigin( vec3 origin )
{
    util::call_virtual< void ( * )( void *, vec3 ) >(
        this, g_tf2.m_render_context_set_lighting_origin )( this, origin );
}

ITexture *CMaterialSystem::FindTexture( const char *pTextureName, const char *pTextureGroupName, bool complain )
{
    return util::call_virtual< ITexture *( * )( void *, const char *, const char *, bool ) >(
        this, g_tf2.m_material_system_find_texture )( this, pTextureName, pTextureGroupName, complain );
}

void CStudioRender::SetLocalLights( i32 count, const void *lights )
{
    util::call_virtual< void ( * )( void *, i32, const void * ) >(
        this, g_tf2.m_studio_render_set_local_lights )( this, count, lights );
}

void CStudioRender::SetAmbientLightColors( const vec3 *colors )
{
    util::call_virtual< void ( * )( void *, const vec3 * ) >(
        this, g_tf2.m_studio_render_set_ambient_light_colors )( this, colors );
}

void IVModelRender::SuppressEngineLighting( bool suppress )
{
    util::call_virtual< void ( * )( void *, bool ) >(
        this, g_tf2.m_model_render_suppress_engine_lighting )( this, suppress );
}

i32 INetworkStringTable::AddString( bool isServer, const char *value, i32 length, const void *userdata )
{
    return util::call_virtual< i32 ( * )( void *, bool, const char *, i32, const void * ) >(
        this, g_tf2.m_string_table_add_string )( this, isServer, value, length, userdata );
}

i32 INetworkStringTable::FindStringIndex( const char *value )
{
    return util::call_virtual< i32 ( * )( void *, const char * ) >(
        this, g_tf2.m_string_table_find_string_index )( this, value );
}

void INetworkStringTableContainer::RemoveAllTables( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_string_table_container_remove_all_tables )( this );
}

INetworkStringTable *INetworkStringTableContainer::FindTable( const char *name )
{
    return util::call_virtual< INetworkStringTable *( * )( void *, const char * ) >(
        this, g_tf2.m_string_table_container_find_table )( this, name );
}

INetworkStringTable *INetworkStringTableContainer::CreateStringTableEx(
    const char *name,
    i32         max_entries,
    i32         user_data_fixed_size,
    i32         user_data_network_bits,
    bool        is_filenames )
{
    return util::call_virtual< INetworkStringTable *( * )( void *, const char *, i32, i32, i32, bool ) >(
        this, g_tf2.m_string_table_container_create_string_table_ex )( this, name, max_entries, user_data_fixed_size,
                                                                       user_data_network_bits, is_filenames );
}

model_t *IModelLoader::GetModelForName( const char *name, i32 type )
{
    return util::call_virtual< model_t *( * )( void *, const char *, i32 ) >(
        this, g_tf2.m_model_loader_get_model_for_name )( this, name, type );
}

float CVRenderView::GetBlend( )
{
    return util::call_virtual< float ( * )( void * ) >(
        this, g_tf2.m_render_view_get_blend )( this );
}

void CVRenderView::SetColorModulation( const float *color )
{
    util::call_virtual< void ( * )( void *, const float * ) >(
        this, g_tf2.m_render_view_set_color_modulation )( this, color );
}

void CVRenderView::GetColorModulation( float *color )
{
    util::call_virtual< void ( * )( void *, float * ) >(
        this, g_tf2.m_render_view_get_color_modulation )( this, color );
}

void IMaterial::SetMaterialVarFlag( MaterialVarFlags_t flag, bool on )
{
    util::call_virtual< void ( * )( void *, MaterialVarFlags_t, bool ) >(
        this, g_tf2.m_material_set_material_var_flag )( this, flag, on );
}

void IMaterial::DecrementReferenceCount( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_material_decrement_reference_count )( this );
}

void IMaterial::DeleteIfUnreferenced( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_material_delete_if_unreferenced )( this );
}

IMaterialVar *IMaterial::FindVar( const char *name, bool *found, bool complain )
{
    return util::call_virtual< IMaterialVar *( * )( void *, const char *, bool *, bool ) >(
        this, g_tf2.m_material_find_var )( this, name, found, complain );
}

bool IMaterial::IsPrecached( )
{
    return util::call_virtual< bool ( * )( void * ) >( this, g_tf2.m_material_is_precached )( this );
}

void IMaterial::Precache( )
{
    util::call_virtual< void ( * )( void * ) >( this, g_tf2.m_material_precache )( this );
}

bool IMaterial::IsErrorMaterial( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_material_is_error_material )( this );
}

void IMaterialVar::SetFloatValue( float value )
{
    util::call_virtual< void ( * )( void *, float ) >(
        this, g_tf2.m_material_var_set_float_value )( this, value );
}

void IMaterialVar::SetVecValue( float x, float y, float z )
{
    util::call_virtual< void ( * )( void *, float, float, float ) >(
        this, g_tf2.m_material_var_set_vec_value )( this, x, y, z );
}

void ITexture::IncrementReferenceCount( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_texture_increment_reference_count )( this );
}

void ITexture::DecrementReferenceCount( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_texture_decrement_reference_count )( this );
}

void ITexture::DeleteIfUnreferenced( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_texture_delete_if_unreferenced )( this );
}

ITexture *CMaterialSystem::CreateNamedRenderTargetTextureEx( const char *pRTName, int w, int h, RenderTargetSizeMode_t sizeMode,
                                                             ImageFormat format, MaterialRenderTargetDepth_t depth,
                                                             u32 textureFlags, u32 renderTargetFlags )
{
    return util::call_virtual< ITexture *( * )( void *, const char *, int, int, RenderTargetSizeMode_t, ImageFormat,
                                                MaterialRenderTargetDepth_t, u32, u32 ) >(
        this, g_tf2.m_material_system_create_named_render_target_texture_ex )(
        this, pRTName, w, h, sizeMode, format, depth, textureFlags, renderTargetFlags );
}

void *IKeyValuesSystem::AllocKeyValuesMemory( int size )
{
    return util::call_virtual< void *( * )( IKeyValuesSystem *, int ) >(
        this, g_tf2.m_key_values_system_alloc_key_values_memory )( this, size );
}

KeyValues::KeyValues( const char *name )
{
    g_tf2.m_KeyValues_ctor.call_as< void >( this, name );
}

void KeyValues::SetString( const char *keyName, const char *value )
{
    g_tf2.m_KeyValues_SetString.call_as< void >( this, keyName, value );
}

void KeyValues::SetInt( const char *keyName, int value )
{
    g_tf2.m_KeyValues_SetInt.call_as< void >( this, keyName, value );
}

void *KeyValues::operator new( usize iAllocSize )
{
    return g_tf2.m_kv_system->AllocKeyValuesMemory( ( i32 )iAllocSize );
}

void *IMemAlloc::Alloc( size_t nSize )
{
    return util::call_virtual< void *( * )( void *, size_t ) >(
        this, g_tf2.m_mem_alloc_alloc )( this, nSize );
}

void *IMemAlloc::Realloc( void *mem, size_t size )
{
    return util::call_virtual< void *( * )( void *, void *, size_t ) >(
        this, g_tf2.m_mem_alloc_realloc )( this, mem, size );
}

void *IMemAlloc::Free( void *mem )
{
    return util::call_virtual< void *( * )( void *, void * ) >(
        this, g_tf2.m_mem_alloc_free )( this, mem );
}

template< typename T >
void CUtlVector< T >::Purge( )
{
    if ( m_Memory.m_pMemory )
    {
        g_tf2.m_alloc->Free( m_Memory.m_pMemory );
    }
    m_Memory.m_pMemory = m_pElements = nullptr;
    m_Memory.m_nAllocationCount = m_Memory.m_nGrowSize = m_Size = 0;
}

template struct CUtlVector< i32 >;
template struct CUtlVector< brush_plane_t >;
template struct CUtlVector< ICollideable * >;

bool CNetChannel::IsLoopback( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_net_channel_is_loopback )( this );
}

float CNetChannel::GetLatency( int flow )
{
    return util::call_virtual< float ( * )( void *, int ) >(
        this, g_tf2.m_net_channel_get_latency )( this, flow );
}

bool CNetChannel::IsTimingOut( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_net_channel_is_timing_out )( this );
}

bool CNetChannel::CanPacket( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_net_channel_can_packet )( this );
}

void CNetChannel::SetChoked( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_net_channel_set_choked )( this );
}

int CNetChannel::SendDatagram( bf_write *data )
{
    return util::call_virtual< int ( * )( void *, bf_write * ) >(
        this, g_tf2.m_net_channel_send_datagram )( this, data );
}

bool CNetChannel::SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice )
{
    return util::call_virtual< bool ( * )( void *, INetMessage &, bool, bool ) >(
        this, g_tf2.m_net_channel_send_net_msg )( this, msg, bForceReliable, bVoice );
}

bool CClientState::IsConnected( ) const
{
    return m_nSignonState >= SIGNONSTATE_CONNECTED;
}

bool CClientState::IsActive( ) const
{
    return m_nSignonState == SIGNONSTATE_FULL;
}

bool CClientState::IsPaused( ) const
{
    return g_tf2.m_CClientState_IsPaused_fn.call_as< bool, const CClientState * >( this );
}

void CClientState::ForceFullUpdate( )
{
    g_tf2.m_CClientState_ForceFullUpdate_fn.call_as< void, CClientState * >( this );
}

bool CLC_Move::WriteToBuffer( bf_write &buffer )
{
    buffer.WriteUBitLong( GetType( ), NETMSG_TYPE_BITS );

    m_nLength = m_DataOut.GetNumBitsWritten( );

    buffer.WriteUBitLong( m_nNewCommands, NUM_NEW_COMMAND_BITS );
    buffer.WriteUBitLong( m_nBackupCommands, NUM_BACKUP_COMMAND_BITS );
    buffer.WriteWord( m_nLength );

    return buffer.WriteBits( m_DataOut.GetData( ), m_nLength );
}

bool CLC_Move::ReadFromBuffer( bf_read & )
{
    return false;
}

bool NET_Tick::WriteToBuffer( bf_write &buffer )
{
    buffer.WriteUBitLong( GetType( ), NETMSG_TYPE_BITS );
    buffer.WriteLong( m_nTick );
    buffer.WriteUBitLong( math::clamp( static_cast< int >( NET_TICK_SCALEUP * m_flHostFrameTime ), 0, 65535 ), 16 );
    buffer.WriteUBitLong( math::clamp( static_cast< int >( NET_TICK_SCALEUP * m_flHostFrameTimeStdDeviation ), 0, 65535 ), 16 );

    return !buffer.IsOverflowed( );
}

bool NET_Tick::ReadFromBuffer( bf_read & )
{
    return false;
}

ConVar *ICvar::FindVar( const char *var_name )
{
    return util::call_virtual< ConVar *( * )( void *, const char * ) >(
        this, g_tf2.m_cvar_find_var )( this, var_name );
}

ConCommandBase *ICvar::GetCommands( )
{
    return util::call_virtual< ConCommandBase *( * )( void * ) >(
        this, g_tf2.m_cvar_get_commands )( this );
}

void IConVar::SetValue( const char *pValue )
{
    util::call_virtual< void ( * )( void *, const char * ) >(
        this, g_tf2.m_convar_set_value_chr )( this, pValue );
}

void IConVar::SetValue( float flValue )
{
    util::call_virtual< void ( * )( void *, float ) >(
        this, g_tf2.m_convar_set_value_fl )( this, flValue );
}

void IConVar::SetValue( int nValue )
{
    util::call_virtual< void ( * )( void *, int ) >(
        this, g_tf2.m_convar_set_value_int )( this, nValue );
}

const char *IConVar::GetName( ) const
{
    auto dummy = const_cast< IConVar * >( this ); // fuck off
    return util::call_virtual< const char *( * )( void * ) >(
        dummy, g_tf2.m_convar_get_name )( dummy );
}

bool IConVar::IsFlagSet( int nFlag ) const
{
    auto dummy = const_cast< IConVar * >( this ); // fuck off
    return util::call_virtual< bool ( * )( void *, int ) >(
        dummy, g_tf2.m_convar_is_flag_set )( dummy, nFlag );
}

int ConVar::GetInt( ) const
{
    return m_nValue;
}

bool ConVar::GetBool( ) const
{
    return !!GetInt( );
}

float ConVar::GetFloat( ) const
{
    return m_fValue;
}

const char *ConVar::GetString( ) const
{
    return m_pszString;
}

bool IDemoRecorder::IsRecording( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_demo_recorder_is_recording )( this );
}

bool IDemoRecorder::RecordUserInput( int command_number )
{
    return util::call_virtual< bool ( * )( void *, int ) >(
        this, g_tf2.m_demo_recorder_record_user_input )( this, command_number );
}

bool IDemoPlayer::IsPlayingBack( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_demo_player_is_playing_back )( this );
}

IPhysicsEnvironment *IPhysics::CreateEnvironment( )
{
    return util::call_virtual< IPhysicsEnvironment *( * )( void * ) >(
        this, g_tf2.m_physics_create_environment )( this );
}

void IPhysics::DestroyEnvironment( IPhysicsEnvironment *environment )
{
    util::call_virtual< void ( * )( void *, IPhysicsEnvironment * ) >(
        this, g_tf2.m_physics_destroy_environment )( this, environment );
}

i32 IPhysicsSurfaceProps::GetSurfaceIndex( const char *name ) const
{
    return util::call_virtual< i32 ( * )( const void *, const char * ) >(
        const_cast< IPhysicsSurfaceProps * >( this ), g_tf2.m_physics_surface_props_get_surface_index )( this, name );
}

surfacedata_t *IPhysicsSurfaceProps::GetSurfaceData( i32 surface_data_index )
{
    return util::call_virtual< surfacedata_t *( * )( void *, i32 ) >(
        this, g_tf2.m_physics_surface_props_get_surface_data )( this, surface_data_index );
}

void IPhysicsSurfaceProps::GetPhysicsProperties( i32 surface_data_index, f32 *density, f32 *thickness,
                                                 f32 *friction, f32 *elasticity ) const
{
    util::call_virtual< void ( * )( const void *, i32, f32 *, f32 *, f32 *, f32 * ) >(
        const_cast< IPhysicsSurfaceProps * >( this ), g_tf2.m_physics_surface_props_get_physics_properties )(
        this, surface_data_index, density, thickness, friction, elasticity );
}

void IPhysicsEnvironment::SetGravity( const vec3 &gravityVector )
{
    util::call_virtual< void ( * )( void *, const vec3 & ) >(
        this, g_tf2.m_physics_env_set_gravity )( this, gravityVector );
}

void IPhysicsEnvironment::SetAirDensity( float density )
{
    util::call_virtual< void ( * )( void *, float ) >(
        this, g_tf2.m_physics_env_set_air_density )( this, density );
}

IPhysicsObject *IPhysicsEnvironment::CreatePolyObject(
    const CPhysCollide *pCollisionModel,
    int                 materialIndex,
    const vec3         &position,
    const vec3         &angles,
    objectparams_t     *pParams )
{
    return util::call_virtual< IPhysicsObject *( * )( void *, const CPhysCollide *, int, const vec3 &, const vec3 &, objectparams_t * ) >(
        this, g_tf2.m_physics_env_create_poly_object )( this, pCollisionModel, materialIndex, position, angles, pParams );
}

IPhysicsObject *IPhysicsEnvironment::CreatePolyObjectStatic(
    const CPhysCollide *pCollisionModel,
    int                 materialIndex,
    const vec3         &position,
    const vec3         &angles,
    objectparams_t     *pParams )
{
    return util::call_virtual< IPhysicsObject *( * )( void *, const CPhysCollide *, int, const vec3 &, const vec3 &, objectparams_t * ) >(
        this, g_tf2.m_physics_env_create_poly_object_static )( this, pCollisionModel, materialIndex, position, angles, pParams );
}

void IPhysicsEnvironment::DestroyObject( IPhysicsObject *object )
{
    util::call_virtual< void ( * )( void *, IPhysicsObject * ) >(
        this, g_tf2.m_physics_env_destroy_object )( this, object );
}

void IPhysicsEnvironment::Simulate( float deltaTime )
{
    util::call_virtual< void ( * )( void *, float ) >(
        this, g_tf2.m_physics_env_simulate )( this, deltaTime );
}

void IPhysicsEnvironment::SetSimulationTimestep( float timestep )
{
    util::call_virtual< void ( * )( void *, float ) >(
        this, g_tf2.m_physics_env_set_simulation_timestep )( this, timestep );
}

void IPhysicsEnvironment::ResetSimulationClock( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_physics_env_reset_simulation_clock )( this );
}

void IPhysicsEnvironment::SetCollisionEventHandler( IPhysicsCollisionEvent *pCollisionEvents )
{
    util::call_virtual< void ( * )( void *, IPhysicsCollisionEvent * ) >(
        this, g_tf2.m_physics_env_set_collision_event_handler )( this, pCollisionEvents );
}

void IPhysicsCollisionData::GetSurfaceNormal( vec3 &out )
{
    util::call_virtual< void ( * )( void *, vec3 & ) >(
        this, g_tf2.m_physics_collision_data_get_surface_normal )( this, out );
}

void IPhysicsCollisionData::GetContactPoint( vec3 &out )
{
    util::call_virtual< void ( * )( void *, vec3 & ) >(
        this, g_tf2.m_physics_collision_data_get_contact_point )( this, out );
}

void IPhysicsEnvironment::SetPerformanceSettings( const physics_performanceparams_t *pSettings )
{
    util::call_virtual< void ( * )( void *, const physics_performanceparams_t * ) >(
        this, g_tf2.m_physics_env_set_performance_settings )( this, pSettings );
}

bool IPhysicsObject::IsAsleep( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_physics_object_is_asleep )( this );
}

void IPhysicsObject::Wake( )
{
    util::call_virtual< void ( * )( void * ) >(
        this, g_tf2.m_physics_object_wake )( this );
}

void IPhysicsObject::SetDragCoefficient( float *pDrag, float *pAngularDrag )
{
    util::call_virtual< void ( * )( void *, float *, float * ) >(
        this, g_tf2.m_physics_object_set_drag_coefficient )( this, pDrag, pAngularDrag );
}

void IPhysicsObject::SetPosition( const vec3 &worldPosition, const vec3 &angles, bool isTeleport )
{
    util::call_virtual< void ( * )( void *, const vec3 &, const vec3 &, bool ) >(
        this, g_tf2.m_physics_object_set_position )( this, worldPosition, angles, isTeleport );
}

void IPhysicsObject::GetPosition( vec3 *worldPosition, vec3 *angles )
{
    util::call_virtual< void ( * )( void *, vec3 *, vec3 * ) >(
        this, g_tf2.m_physics_object_get_position )( this, worldPosition, angles );
}

void IPhysicsObject::SetVelocity( const vec3 *velocity, const AngularImpulse *angularVelocity )
{
    util::call_virtual< void ( * )( void *, const vec3 *, const AngularImpulse * ) >(
        this, g_tf2.m_physics_object_set_velocity )( this, velocity, angularVelocity );
}

void IPhysicsObject::GetVelocity( vec3 *velocity, AngularImpulse *angularVelocity )
{
    util::call_virtual< void ( * )( void *, vec3 *, AngularImpulse * ) >(
        this, g_tf2.m_physics_object_get_velocity )( this, velocity, angularVelocity );
}

void IPhysicsCollision::CollideGetAABB( vec3 *mins, vec3 *maxs, const CPhysCollide *collide, const vec3 &origin, const vec3 &angles )
{
    util::call_virtual< void ( * )( void *, vec3 *, vec3 *, const CPhysCollide *, const vec3 &, const vec3 & ) >(
        this, g_tf2.m_physics_collision_collide_get_aabb )( this, mins, maxs, collide, origin, angles );
}

CPhysCollide *IPhysicsCollision::BBoxToCollide( const vec3 &mins, const vec3 &maxs )
{
    return util::call_virtual< CPhysCollide *( * )( void *, const vec3 &, const vec3 & ) >(
        this, g_tf2.m_physics_collision_bbox_to_collide )( this, mins, maxs );
}

void C_TFPartyClient::SendPartyChat( const char *msg )
{
    return g_tf2.m_C_TFPartyClient_SendToPartyChat.call_as< void >( this, msg );
}

i32 CTFGSLobby::GetNumMembers( )
{
    return util::call_virtual< i32 ( * )( void * ) >( this, g_tf2.m_lobby_get_num_members )( this );
}

CTFLobbyPlayerProto *CTFGSLobby::GetMemberDetails( i32 index )
{
    void *details[ 2 ]{ };
    util::call_virtual< void **( * )( void *, void **, i32 ) >(
        this, g_tf2.m_lobby_get_member_details )( this, details, index );
    return static_cast< CTFLobbyPlayerProto * >( details[ 1 ] );
}

CTFGSLobby *CTFGCClientSystem::GetLobby( )
{
    return g_tf2.m_CTFGCClientSystem_GetLobby.call_as< CTFGSLobby * >( this );
}

i32 IPlayerGroup::GetNumMembers( )
{
    return util::call_virtual< i32 ( * )( void * ) >( this, g_tf2.m_player_group_get_num_members )( this );
}

u32 IPlayerGroup::GetMemberAccount( i32 index )
{
    // CSteamID has constructors, so the game returns it through the hidden pointer in rdx
    steamid_t steamid{ };
    util::call_virtual< steamid_t *( * )( void *, steamid_t *, i32 ) >(
        this, g_tf2.m_player_group_get_member )( this, &steamid, index );
    return steamid.m_steamid.m_comp.account_id;
}

void CEconNotification::Destroy( )
{
    util::call_virtual< void ( * )( void *, u32 ) >(
        this, g_tf2.m_econ_notification_destructor )( this, 1 );
}
