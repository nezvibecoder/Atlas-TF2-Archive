#pragma once

#include "link.hpp"

struct datamap_copy_t
{
    u8   *m_data{ };
    usize m_size{ };

    bool ensure( usize needed );
    void release( );
};

class engine_prediction
{
public:
    bool start( i32 sequence_number );
    bool begin_weapon( );
    bool sync_weapon_input( );
    void end( );
    void post_think( );
    void shutdown( );
    void update( );
    void on_post_data_update( );
    i32  network_command( ) const;
    i32  network_tickbase( ) const;

    bool  m_is_active{ false };
    float m_weapon_fire_delay{ 0.0f };

private:
    bool save_data_maps( );
    void restore_data_maps( );

    bool m_in_prediction{ };
    bool m_first_time_predicted{ };
    i32  m_network_command{ -1 };
    i32  m_network_tickbase{ };
    bool m_weapon_entered{ };
    i32  m_previous_buttons{ };
    vec3 m_local_view_angles{ };

    enum masked_field_t
    {
        MASK_INSPECT_TIME,
        MASK_MODEL_INDEX,
        MASK_WEAPON_IDLE,
        MASK_VIEWMODEL_SEQUENCE,
        MASK_VIEWMODEL_PARITY,
        MASK_VIEWMODEL_PLAYBACK,
        MASK_COUNT
    };

    typedescription_t *m_masked_fields[ MASK_COUNT ]{ };

    globals_backup_t m_backup{ };
    datamap_copy_t   m_player_buffer{ };
    datamap_copy_t   m_weapon_buffer{ };
    datamap_copy_t   m_viewmodel_buffers[ MAX_VIEWMODELS ]{ };

    C_TFPlayer     *m_player{ };
    C_TFWeaponBase *m_weapon{ };
    C_BaseEntity   *m_viewmodels[ MAX_VIEWMODELS ]{ };

};

extern engine_prediction g_prediction;
