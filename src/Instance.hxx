/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "util/IntrusiveList.hxx"

#include <event.h>

struct addrinfo;
struct Connection;

struct Instance {
	struct event_base *const event_base = event_init();

	struct addrinfo *server_address = nullptr;

	bool should_exit;
	struct event sigterm_event, sigint_event, sigquit_event;

	int listener_socket;
	struct event listener_event;

	IntrusiveList<Connection> connections;

	Instance();
	~Instance() noexcept;
};
