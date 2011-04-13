/*
 * The policy implementation.  It decides what to do with a request.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "policy.h"

void
policy_init(void)
{
}

void
policy_deinit(void)
{
}

unsigned
policy_login(const char *user)
{
    (void)user;
    return 0;
}

unsigned
policy_execute(const char *user)
{
    (void)user;
    return 0;
}

void
policy_duration(const char *user, unsigned duration_ms)
{
    (void)user;
    (void)duration_ms;
}
