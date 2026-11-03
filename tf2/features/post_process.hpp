#pragma once

#include "link.hpp"

class post_process
{
public:
    void grade( ) const;
    void begin_view( );
    void end_view( );

private:
    f32  m_blur_enabled{ };
    f32  m_blur_strength{ };
    bool m_blur_overridden{ };
};

extern post_process g_post_process;
