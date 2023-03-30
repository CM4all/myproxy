// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "Connection.hxx"

#include <cstddef>

#include <sys/socket.h>

Instance::Instance(const Config &_config)
	:config(_config)
{
	shutdown_listener.Enable();

	listeners.emplace_front(event_loop, *this);
	listeners.front().Listen(config.listener.Create(SOCK_STREAM));
}

void
Instance::OnShutdown() noexcept
{
	shutdown_listener.Disable();
	listeners.clear();
}
