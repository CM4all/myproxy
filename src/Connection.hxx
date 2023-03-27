/*
 * Manage connections from MySQL clients.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Peer.hxx"
#include "util/IntrusiveList.hxx"

struct Instance;

struct Connection : IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK> {
	Instance *const instance;

	/**
	 * Used to insert delay in the connection: it gets fired after the
	 * delay is over.  It re-enables parsing and forwarding client
	 * input.
	 */
	struct event delay_timer;

	bool delayed;

	bool greeting_received, login_received;

	char user[64];

	/**
	 * The time stamp of the last request packet [us].
	 */
	uint64_t request_time;

	/**
	 * The connection to the client.
	 */
	Peer client;

	/**
	 * The connection to the server.
	 */
	Peer server;

	Connection(Instance &_instance, int fd);
	~Connection() noexcept;
};

/**
 * Delay forwarding client input for the specified duration.  Can be
 * used to throttle the connection.
 */
void
connection_delay(Connection *c, unsigned delay_ms);
