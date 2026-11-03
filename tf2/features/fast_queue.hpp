#pragma once

#include "link.hpp"

class fast_queue
{
public:
    i32 think( i32 ping, u32 datacenter );

private:
    u32 roll( u32 minimum, u32 maximum );

    u32 m_random{ };
};

extern fast_queue g_fast_queue;
