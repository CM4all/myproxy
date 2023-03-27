/*
 * The policy implementation.  It decides what to do with a request.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Policy.hxx"

void
policy_init()
{
}

void
policy_deinit()
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
