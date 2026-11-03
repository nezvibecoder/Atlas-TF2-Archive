#pragma once

#include "runtime/atomic.hpp"
#include "util/typedefs.hpp"

enum class load_stage_t : i32
{
    render,
    modules,
    interfaces,
    exports,
    signatures,
    indices,
    convars,
    netvars,
    datamaps,
    features,
    hooks,
    count,
};

class loading
{
public:
    void enter( load_stage_t stage );
    void draw( );

    bool released( ) const
    {
        return this->m_released;
    }

private:
    atl::atomic< i32 > m_stage{ };
    u64                m_started{ };
    u64                m_finished_at{ };
    u64                m_tick{ };
    i32                m_caption_width{ };
    float              m_fill{ };
    u8                 m_shown{ };
    bool               m_released{ };
};

extern loading g_loading;
