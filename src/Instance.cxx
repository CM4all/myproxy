// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "Connection.hxx"
#include "event/net/PrometheusExporterListener.hxx"
#include "net/SocketConfig.hxx"
#include "system/Error.hxx"

extern "C" {
#include <lauxlib.h>
}

#ifdef HAVE_LIBSYSTEMD
#include "AsyncResolver.hxx"
#include <systemd/sd-daemon.h>
#endif

#include <cstddef>

#include <signal.h>
#include <sys/socket.h>

Instance::Instance()
	:sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 lua_state(luaL_newstate())
{
	shutdown_listener.Enable();
	sighup_event.Enable();
}

Instance::~Instance() noexcept = default;

inline void
Instance::AddListener(UniqueSocketDescriptor &&fd,
		      std::shared_ptr<LuaHandler> &&handler) noexcept
{
	listeners.emplace_front(event_loop, event_loop, stats, std::move(handler));
	listeners.front().Listen(std::move(fd));
}

static UniqueSocketDescriptor
MakeListener(SocketAddress address)
{
	constexpr int socktype = SOCK_STREAM;

	const SocketConfig config{
		.bind_address = AllocatedSocketAddress{address},
		.listen = 64,
		.mode = 0666,

		/* we want to receive the client's UID */
		.pass_cred = true,
	};

	return config.Create(socktype);
}

void
Instance::AddListener(SocketAddress address,
		      std::shared_ptr<LuaHandler> handler)
{
	AddListener(MakeListener(address), std::move(handler));
}

#ifdef HAVE_LIBSYSTEMD

void
Instance::AddSystemdListener(std::shared_ptr<LuaHandler> handler)
{
	int n = sd_listen_fds(true);
	if (n < 0)
		throw MakeErrno("sd_listen_fds() failed");

	if (n == 0)
		throw std::runtime_error{"No systemd socket"};

	for (unsigned i = 0; i < unsigned(n); ++i)
		AddListener(UniqueSocketDescriptor(SD_LISTEN_FDS_START + i),
			    std::shared_ptr<LuaHandler>{handler});
}

#endif // HAVE_LIBSYSTEMD

void
Instance::AddPrometheusListener(SocketAddress address)
{
	const SocketConfig config{
		.bind_address = AllocatedSocketAddress{address},
		.listen = 16,
		.tcp_defer_accept = 10,
		.mode = 0666,
		.tcp_no_delay = true,
	};

	PrometheusExporterHandler &handler = *this;
	prometheus_exporters.emplace_front(event_loop,
					   config.Create(SOCK_STREAM),
					   handler);
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
	sighup_event.Disable();

	listeners.clear();
	prometheus_exporters.clear();

#ifdef HAVE_LIBSYSTEMD
	systemd_watchdog.Disable();
#endif // HAVE_LIBSYSTEMD

	/* TODO this is currently necessary because the Pg::Stock
	   instances created by Lua code don't call Shutdown() */
	event_loop.Break();
}

void
Instance::OnReload(int) noexcept
{
	reload.Start();
}
