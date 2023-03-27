/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "util/IntrusiveList.hxx"

#include <event.h>

struct Config;
struct Connection;

struct Instance {
	const Config &config;

	struct event_base *const event_base = event_init();

	bool should_exit;
	struct event sigterm_event, sigint_event, sigquit_event;

	int listener_socket;
	struct event listener_event;

	IntrusiveList<Connection> connections;

	explicit Instance(const Config &config);
	~Instance() noexcept;
};
