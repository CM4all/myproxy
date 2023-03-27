/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <event.h>

#include <inline/list.h>

struct addrinfo;

struct Instance {
	struct event_base *const event_base = event_init();

	struct addrinfo *server_address = nullptr;

	bool should_exit;
	struct event sigterm_event, sigint_event, sigquit_event;

	int listener_socket;
	struct event listener_event;

	struct list_head connections;

	Instance();
	~Instance() noexcept;
};
