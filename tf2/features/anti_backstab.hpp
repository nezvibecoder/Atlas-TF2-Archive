#pragma once

#include "link.hpp"

class anti_backstab
{
public:
    void think( );

private:
    bool should_run( ) const;
    bool find_threat( i32 batch, vec3 &face ) const;
};

extern anti_backstab g_anti_backstab;
