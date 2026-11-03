#pragma once

#include "link.hpp"

enum class party_vacc_request : u8
{
    bullet,
    blast,
    fire,
    _COUNT,
};

enum class party_vacc_result : u8
{
    popped,
    no_charge,
    unavailable,
    _COUNT,
};

class party_network
{
public:
    bool on_party_chat( IGameEvent *event );
    void think( );
    bool has_peer( u32 account ) const;

    void send_vacc_status( u32 target );
    void send_vacc_request( u32 medic, party_vacc_request request );
    void send_vacc_result( u32 patient, party_vacc_result result );

private:
    enum class packet_type : u8
    {
        presence = 5,
        vacc_status,
        vacc_request,
        vacc_result,
    };

    struct peer_t
    {
        u32    m_account{ };
        double m_seen_at{ };
    };

    void send( packet_type type, const u8 *payload, usize size );
    bool touch_peer( u32 account, double now );

    double m_next_send_time{ };
    peer_t m_peers[ MAX_PARTY_SIZE - 1 ]{ };
    bool m_enabled{ };
};

extern party_network g_party_network;
