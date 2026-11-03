#pragma once

static int lua_crt_errno{ };
#define errno lua_crt_errno
