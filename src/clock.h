/*
 * The monotonic clock.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_CLOCK_H
#define MYPROXY_CLOCK_H

#include <stdint.h>
#include <time.h>

/**
 * Returns the current monotonic time stamp in microseconds.
 */
static uint64_t
now_us(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) < 0)
        return 0;

    return t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

#endif
