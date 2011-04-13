/*
 * The policy implementation.  It decides what to do with a request.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_POLICY_H
#define MYPROXY_POLICY_H

/**
 * Global initialization.
 */
void
policy_init(void);

void
policy_deinit(void);

/**
 * A user attempts to log in.
 *
 * @return the number of milliseconds this operation should be delayed
 */
unsigned
policy_login(const char *user);

/**
 * A user sends a SQL statement.
 *
 * @return the number of milliseconds this operation should be delayed
 */
unsigned
policy_execute(const char *user);

/**
 * Submit the time it took the MySQL server to execute a SQL
 * statement.
 *
 * @param duration_ms the duration between the submission of the SQL
 * statement to the server's response
 */
void
policy_duration(const char *user, unsigned duration_ms);

#endif
