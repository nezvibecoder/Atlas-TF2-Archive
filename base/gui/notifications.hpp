#pragma once

#include "util/typedefs.hpp"

enum class notify_kind_t : u8
{
    success,
    information,
    warning,
    error,
};

void push_notification( notify_kind_t kind, const char *title, const char *message );
void push_notification( notify_kind_t kind, const char *icon, const char *title,
                        const char *message );

void draw_notifications( i32 screen_width, i32 screen_height );
