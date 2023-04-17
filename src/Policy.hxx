// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * The policy implementation.  It decides what to do with a request.
 */

#pragma once

#include "event/Chrono.hxx"

/**
 * Global initialization.
 */
void
policy_init();

void
policy_deinit();

/**
 * A user attempts to log in.
 *
 * @return the number of milliseconds this operation should be delayed
 */
Event::Duration
policy_login(const char *user);

/**
 * Submit the time it took the MySQL server to execute a SQL
 * statement.
 *
 * @param duration_ms the duration between the submission of the SQL
 * statement to the server's response
 */
void
policy_duration(const char *user, Event::Duration duration);
