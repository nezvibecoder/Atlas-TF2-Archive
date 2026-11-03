#pragma once

#include "link.hpp"

inline constexpr usize MAX_COMPRESSED_PROPS{ 64 };
inline constexpr usize MAX_COMPRESSED_CLASSES{ 4 };
inline constexpr usize COMPRESSION_BUILD_ATTEMPTS{ 32 };
inline constexpr usize MAX_PROP_EXCLUDES{ 96 };

inline constexpr usize COMPRESSION_RING_SIZE{ 64 };
inline constexpr i32   COMPRESSION_RING_MASK{ static_cast< i32 >( COMPRESSION_RING_SIZE ) - 1 };

enum class prop_scope : u8
{
    any = 0,
    local,
    non_local,
};

enum class prop_encoding : u8
{
    none = 0,
    fixed_point,
    normal,
    scaled,
};

struct quantiser_t
{
    prop_encoding m_encoding{ prop_encoding::none };

    i32 m_denominator{ }; // fixed_point

    float m_low{ }; // scaled
    float m_high{ };
    float m_scale{ };
    float m_decode_scale{ }; // (2^bits - 1) / range
    bool  m_wraps{ };

    i32  encode( float value ) const;
    i32  invert( float decoded ) const;
    bool holds( float candidate, float decoded ) const;
};

struct compressed_prop_t
{
    const char  *m_name{ };
    i32          m_offset{ };
    SendPropType m_type{ DPT_Float };
    prop_scope   m_scope{ prop_scope::any };
    quantiser_t  m_quantiser{ };
};

struct prop_exclude_t
{
    const char *m_table{ };
    const char *m_prop{ };
};

struct exclude_list_t
{
    prop_exclude_t m_items[ MAX_PROP_EXCLUDES ]{ };
    usize          m_count{ };

    bool contains( const char *table, const char *prop ) const;
};

struct compressed_class_t
{
    const char       *m_name{ };
    usize             m_count{ };
    compressed_prop_t m_props[ MAX_COMPRESSED_PROPS ]{ };
};

class netvar_compression
{
public:
    void on_run_simulation( C_BasePlayer *player, i32 command );
    void on_packet_start( i32 acked_command );

    void restore( );
    void clear( );

    bool available( ) const
    {
        return this->m_available;
    }

    usize corrections( ) const
    {
        return this->m_total_corrections;
    }

private:
    struct snapshot_t
    {
        i32  m_command{ -1 };
        vec3 m_values[ MAX_COMPRESSED_PROPS ]{ };
    };

    bool                      ensure_built( );
    bool                      build_class( ClientClass *client_class, compressed_class_t &out, bool verbose );
    void                      collect( RecvTable *recv, SendTable *send, compressed_class_t &out, const exclude_list_t &excluded,
                                       i32 base_offset, i32 depth, prop_scope scope );
    const compressed_class_t *model_for( const ClientClass *client_class ) const;

    compressed_class_t m_classes[ MAX_COMPRESSED_CLASSES ]{ };
    usize              m_class_count{ };

    snapshot_t m_ring[ COMPRESSION_RING_SIZE ]{ };

    usize m_attempts{ };
    bool  m_available{ };

    i32  m_acked{ -1 };
    bool m_armed{ };

    usize m_total_corrections{ };
    usize m_logged{ };
    u64   m_logged_mask[ 3 ]{ };
    usize m_window_mark{ };
    float m_window{ };
};

extern netvar_compression g_compression;
