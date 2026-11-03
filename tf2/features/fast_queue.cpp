#include "fast_queue.hpp"

ATLAS_NO_DESTROY fast_queue g_fast_queue{ };

constexpr u32 datacenter_id( const char *code )
{
    return static_cast< u32 >( static_cast< u8 >( code[ 0 ] ) ) << 16 |
           static_cast< u32 >( static_cast< u8 >( code[ 1 ] ) ) << 8 |
           static_cast< u32 >( static_cast< u8 >( code[ 2 ] ) ) |
           static_cast< u32 >( static_cast< u8 >( code[ 3 ] ) ) << 24;
}

struct datacenter_t
{
    u32 m_id{ };
    u8  m_region{ };
};

constexpr datacenter_t DATACENTERS[]{
    { datacenter_id( "ams" ), config::QUEUE_REGION_NETHERLANDS },
    { datacenter_id( "fra" ), config::QUEUE_REGION_GERMANY },
    { datacenter_id( "fsn" ), config::QUEUE_REGION_GERMANY },
    { datacenter_id( "lhr" ), config::QUEUE_REGION_UNITED_KINGDOM },
    { datacenter_id( "par" ), config::QUEUE_REGION_FRANCE },
    { datacenter_id( "mad" ), config::QUEUE_REGION_SPAIN },
    { datacenter_id( "sto" ), config::QUEUE_REGION_SWEDEN },
    { datacenter_id( "sto2" ), config::QUEUE_REGION_SWEDEN },
    { datacenter_id( "hel" ), config::QUEUE_REGION_FINLAND },
    { datacenter_id( "helm" ), config::QUEUE_REGION_FINLAND },
    { datacenter_id( "vie" ), config::QUEUE_REGION_AUSTRIA },
    { datacenter_id( "waw" ), config::QUEUE_REGION_POLAND },

    { datacenter_id( "bom" ), config::QUEUE_REGION_INDIA },
    { datacenter_id( "bom2" ), config::QUEUE_REGION_INDIA },
    { datacenter_id( "maa" ), config::QUEUE_REGION_INDIA },
    { datacenter_id( "maa2" ), config::QUEUE_REGION_INDIA },
    { datacenter_id( "dxb" ), config::QUEUE_REGION_UNITED_ARAB_EMIRATES },
    { datacenter_id( "hkg" ), config::QUEUE_REGION_HONG_KONG },
    { datacenter_id( "tyo" ), config::QUEUE_REGION_JAPAN },
    { datacenter_id( "tyo1" ), config::QUEUE_REGION_JAPAN },
    { datacenter_id( "tyo2" ), config::QUEUE_REGION_JAPAN },
    { datacenter_id( "tyo3" ), config::QUEUE_REGION_JAPAN },
    { datacenter_id( "seo" ), config::QUEUE_REGION_SOUTH_KOREA },
    { datacenter_id( "sgp" ), config::QUEUE_REGION_SINGAPORE },
    { datacenter_id( "gum" ), config::QUEUE_REGION_GUAM },

    { datacenter_id( "iad" ), config::QUEUE_REGION_VIRGINIA },
    { datacenter_id( "atl" ), config::QUEUE_REGION_GEORGIA },
    { datacenter_id( "ord" ), config::QUEUE_REGION_ILLINOIS },
    { datacenter_id( "dfw" ), config::QUEUE_REGION_TEXAS },
    { datacenter_id( "lax" ), config::QUEUE_REGION_CALIFORNIA },
    { datacenter_id( "sea" ), config::QUEUE_REGION_WASHINGTON },
    { datacenter_id( "eat" ), config::QUEUE_REGION_WASHINGTON },
    { datacenter_id( "mwh" ), config::QUEUE_REGION_WASHINGTON },

    { datacenter_id( "gru" ), config::QUEUE_REGION_BRAZIL },
    { datacenter_id( "lim" ), config::QUEUE_REGION_PERU },
    { datacenter_id( "scl" ), config::QUEUE_REGION_CHILE },
    { datacenter_id( "eze" ), config::QUEUE_REGION_ARGENTINA },

    { datacenter_id( "jnb" ), config::QUEUE_REGION_SOUTH_AFRICA },
    { datacenter_id( "syd" ), config::QUEUE_REGION_AUSTRALIA },
};

u32 fast_queue::roll( u32 minimum, u32 maximum )
{
    if ( !this->m_random )
    {
        this->m_random = static_cast< u32 >( I( GetTickCount64 )( ) ) | 1u;
    }

    this->m_random ^= this->m_random << 13;
    this->m_random ^= this->m_random >> 17;
    this->m_random ^= this->m_random << 5;

    return minimum + this->m_random % ( maximum - minimum + 1 );
}

i32 fast_queue::think( i32 ping, u32 datacenter )
{
    const config::queue_t &queue{ g_cfg.m_queue };
    if ( !queue.m_enabled || ping < 0 )
    {
        return ping;
    }

    for ( const datacenter_t &entry : DATACENTERS )
    {
        if ( entry.m_id != datacenter )
        {
            continue;
        }

        if ( !queue.m_desired[ entry.m_region ] )
        {
            return static_cast< i32 >( this->roll( 700, 1250 ) );
        }

        break;
    }

    if ( ping > queue.m_max_ping )
    {
        return ping;
    }

    const i32 faked{ static_cast< i32 >( this->roll( 5, 20 ) ) };
    return ping < faked ? ping : faked;
}
