#include "datamaps.hpp"

ATLAS_NO_DESTROY datamap_editor g_datamap{ };

static i32 get_size( fieldtype_t type )
{
    switch ( type )
    {
        case FIELD_BOOLEAN: return 1;
        case FIELD_FLOAT:
        case FIELD_INTEGER:
        default: return 4;
    }
}

bool initialize_datamap( const named_field_t *fields, usize index_count, datamap_t *datamap )
{
    if ( !datamap || !fields )
    {
        return false;
    }

    atl::vector< typedescription_t > fields_to_copy{ };

    for ( usize i{ 0 }; i < index_count; ++i )
    {
        auto entry = &fields[ i ];
        auto temp  = fields_to_copy.emplace_back( );

        temp->fieldType                       = entry->m_field.m_type;
        temp->fieldName                       = entry->m_id;
        temp->fieldOffset[ TD_OFFSET_NORMAL ] = entry->m_field.m_offset;
        temp->fieldSize                       = 1;
        temp->flags                           = entry->m_field.m_flags;
        temp->fieldSizeInBytes                = get_size( temp->fieldType );
        temp->fieldTolerance                  = 0.f; // ?
    }

    const auto num_fields = datamap->dataNumFields;
    const auto desc       = ( typedescription_t * )g_tf2.m_alloc->Alloc( ( num_fields + index_count ) * sizeof( typedescription_t ) );
    if ( !desc )
    {
        return false;
    }

    if ( num_fields > 0 )
    {
        atl::memcpy( desc, datamap->dataDesc, num_fields * sizeof( typedescription_t ) );
    }

    for ( usize i = 0; i < fields_to_copy.size( ); ++i )
    {
        atl::memcpy( &desc[ num_fields + i ], &fields_to_copy[ i ], sizeof( typedescription_t ) );
    }

    datamap->dataDesc      = desc;
    datamap->dataNumFields = num_fields + index_count;

#if defined( ATLAS_DEV )
    DBGOUT( "Datamap: 0x%x", ( uptr )datamap );
    DBGOUT( "dataNumFields: %i, old: %i", datamap->dataNumFields, num_fields );

    for ( auto i = 0; i < datamap->dataNumFields; ++i )
    {
        const auto cur = &datamap->dataDesc[ i ];
        if ( !cur )
        {
            continue;
        }

        DBGOUT( "%s %i %i %i", cur->fieldName, ( i32 )cur->fieldType, cur->fieldSize, cur->fieldSizeInBytes );
    }
#endif

    datamap->packed_offsets_computed = false;

    return true;
}

bool datamap_editor::init( )
{
    const named_field_t weapon_base_fields[]{
        named_field_t( ATLAS_XORSTR_STATIC( "m_flCritTime" ), data_field_t( FIELD_FLOAT, 4020, FTYPEDESC_NOERRORCHECK ) ),
        named_field_t( ATLAS_XORSTR_STATIC( "m_flLastRapidFireCritCheckTime" ), data_field_t( FIELD_FLOAT, 4036, FTYPEDESC_NOERRORCHECK ) ),
        named_field_t( ATLAS_XORSTR_STATIC( "m_iLastCritCheckFrame" ), data_field_t( FIELD_INTEGER, 4028, FTYPEDESC_NOERRORCHECK ) ),
        named_field_t( ATLAS_XORSTR_STATIC( "m_flCritTokenBucket" ), data_field_t( FIELD_FLOAT, 3732, FTYPEDESC_NOERRORCHECK ) ),
        named_field_t( ATLAS_XORSTR_STATIC( "m_nCritChecks" ), data_field_t( FIELD_INTEGER, 3736, FTYPEDESC_NOERRORCHECK ) ),
        named_field_t( ATLAS_XORSTR_STATIC( "m_nCritSeedRequests" ), data_field_t( FIELD_INTEGER, 3740, FTYPEDESC_NOERRORCHECK ) ),
        named_field_t( ATLAS_XORSTR_STATIC( "m_iWeaponMode" ), data_field_t( FIELD_INTEGER, 3972, FTYPEDESC_NOERRORCHECK ) ),
    };

    const named_field_t medigun_fields[]{
        { ATLAS_XORSTR_STATIC( "m_nChargeResistType" ), { FIELD_INTEGER, 4304, FTYPEDESC_INSENDTABLE } },
    };

    const named_field_t minigun_fields[]{
        { ATLAS_XORSTR_STATIC( "m_bCritShot" ), { FIELD_BOOLEAN, 4268, FTYPEDESC_INSENDTABLE } },
        { ATLAS_XORSTR_STATIC( "m_flStartedFiringAt" ), { FIELD_FLOAT, 4276, FTYPEDESC_NOERRORCHECK } },
        { ATLAS_XORSTR_STATIC( "m_flStartedWindUpAt" ), { FIELD_FLOAT, 4280, FTYPEDESC_NOERRORCHECK } },
    };

    const named_field_t flamethrower_fields[]{
        { ATLAS_XORSTR_STATIC( "m_flMinPrimaryAttackBurstTime" ), { FIELD_FLOAT, 4308, FTYPEDESC_NOERRORCHECK } },
    };

    if ( !initialize_datamap( weapon_base_fields, sizeof( weapon_base_fields ) / sizeof( weapon_base_fields[ 0 ] ), g_tf2.m_C_TFWeaponBase_datamap ) ||
         !initialize_datamap( medigun_fields, sizeof( medigun_fields ) / sizeof( medigun_fields[ 0 ] ), g_tf2.m_C_WeaponMedigun_datamap ) ||
         !initialize_datamap( minigun_fields, sizeof( minigun_fields ) / sizeof( minigun_fields[ 0 ] ), g_tf2.m_C_TFMinigun_datamap ) ||
         !initialize_datamap( flamethrower_fields, sizeof( flamethrower_fields ) / sizeof( flamethrower_fields[ 0 ] ), g_tf2.m_C_TFFlameThrower_datamap ) )
    {
        return false;
    }

    return true;
}
