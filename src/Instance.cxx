// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "Connection.hxx"
#include "net/SocketConfig.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <cstddef>

#include <sys/socket.h>

Instance::Instance(const Config &_config)
	:config(_config),
	 lua_state(luaL_newstate())
{
	shutdown_listener.Enable();
}

inline void
Instance::AddListener(UniqueSocketDescriptor &&fd,
		      Lua::ValuePtr &&handler) noexcept
{
	listeners.emplace_front(event_loop, event_loop, std::move(handler));
	listeners.front().Listen(std::move(fd));
}

static UniqueSocketDescriptor
MakeListener(SocketAddress address)
{
	constexpr int socktype = SOCK_STREAM;

	SocketConfig config{address};
	config.mode = 0666;

	/* we want to receive the client's UID */
	config.pass_cred = true;

	config.listen = 64;

	return config.Create(socktype);
}

void
Instance::AddListener(SocketAddress address,
		      Lua::ValuePtr &&handler) noexcept
{
	AddListener(MakeListener(address), std::move(handler));
}

void
Instance::Check()
{
	if (listeners.empty())
		throw std::runtime_error("No listeners configured");
}

void
Instance::OnShutdown() noexcept
{
	shutdown_listener.Disable();
	listeners.clear();

	/* TODO this is currently necessary because the Pg::Stock
	   instances created by Lua code don't call Shutdown() */
	event_loop.Break();
}
