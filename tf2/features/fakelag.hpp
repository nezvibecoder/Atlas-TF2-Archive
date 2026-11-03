#include "link.hpp"

class fakelag
{
public:
    void think( );
    bool is_active( );

    void reset( );

private:
    bool should_run( );

    // The initial goal choke.
    i32 m_goal_choke{ 0 };

    // The absolute goal choke after needed additions.
    i32 m_abs_goal_choke{ 0 };

    bool m_is_active{ false };

    config::fakelag_profile_t m_profile;
};

extern fakelag g_fakelag;
