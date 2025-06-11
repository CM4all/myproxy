// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Listener.hxx"
#include "Stats.hxx"
#include "lua/ReloadRunner.hxx"
#include "lua/State.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/net/ServerSocket.hxx"
#include "event/net/PrometheusExporterHandler.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include "event/systemd/Watchdog.hxx"
#endif // HAVE_LIBSYSTEMD

#ifdef ENABLE_CONTROL
#include "event/net/control/Handler.hxx"
#endif

#include <forward_list>

struct SocketConfig;
class PrometheusExporterListener;

class Instance final
	: PrometheusExporterHandler
#ifdef ENABLE_CONTROL
	, BengControl::Handler
#endif
{
	EventLoop event_loop;

	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

	SignalEvent sighup_event;

#ifdef HAVE_LIBSYSTEMD
	Systemd::Watchdog systemd_watchdog{event_loop};
#endif // HAVE_LIBSYSTEMD

	Lua::State lua_state;

	Lua::ReloadRunner reload{lua_state.get()};

	std::forward_list<MyProxyListener> listeners;

#ifdef ENABLE_CONTROL
	std::forward_list<BengControl::Server> control_listeners;
#endif

	std::forward_list<PrometheusExporterListener> prometheus_exporters;

	Stats stats;

public:
	explicit Instance();
	~Instance() noexcept;

	auto &GetEventLoop() noexcept {
		return event_loop;
	}

	lua_State *GetLuaState() const noexcept {
		return lua_state.get();
	}

	auto &GetStats() noexcept {
		return stats;
	}

	void AddListener(UniqueSocketDescriptor &&fd,
			 std::shared_ptr<LuaHandler> &&handler) noexcept;

	void AddListener(SocketAddress address,
			 std::shared_ptr<LuaHandler> handler);

	void AddControlListener(const SocketConfig &config);

	void AddPrometheusListener(SocketAddress address);

	/**
	 * Listen for incoming connections on sockets passed by systemd
	 * (systemd socket activation).
	 */
#ifdef HAVE_LIBSYSTEMD
	void AddSystemdListener(std::shared_ptr<LuaHandler> handler);
#endif

	void Check();

private:
	void OnShutdown() noexcept;
	void OnReload(int) noexcept;

#ifdef ENABLE_CONTROL
	void DisconnectDatabase(std::string_view account) noexcept;

	/* virtual methods from class ControlHandler */
	void OnControlPacket(BengControl::Server &control_server,
			     BengControl::Command command,
			     std::span<const std::byte> payload,
			     std::span<UniqueFileDescriptor> fds,
			     SocketAddress address, int uid) override;

	void OnControlError(std::exception_ptr ep) noexcept override;
#endif // ENABLE_CONTROL

	/* virtual methods from class PrometheusExporterHandler */
	std::string OnPrometheusExporterRequest() override;
	void OnPrometheusExporterError(std::exception_ptr error) noexcept override;
};
