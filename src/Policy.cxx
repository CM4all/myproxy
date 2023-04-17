// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Policy.hxx"

void
policy_init()
{
}

void
policy_deinit()
{
}

Event::Duration
policy_login(const char *user)
{
	(void)user;
	return {};
}

void
policy_duration(const char *user, Event::Duration duration)
{
	(void)user;
	(void)duration;
}
