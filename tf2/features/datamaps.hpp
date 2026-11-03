#pragma once

#include "link.hpp"

struct data_field_t
{
    fieldtype_t m_type{ };
    i32         m_offset{ };
    i32         m_flags{ };
};

struct named_field_t
{
    const char  *m_id{ };
    data_field_t m_field{ };
};

class datamap_editor
{
public:
    static bool init( );
};

extern datamap_editor g_datamap;
