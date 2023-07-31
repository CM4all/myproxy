// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Listener.hxx"
#include "lua/State.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/net/ServerSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "config.h"

#include <forward_list>

class Instance {
	EventLoop event_loop;

	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

	Lua::State lua_state;

	std::forward_list<MyProxyListener> listeners;

public:
	explicit Instance();

	auto &GetEventLoop() noexcept {
		return event_loop;
	}

	lua_State *GetLuaState() const noexcept {
		return lua_state.get();
	}

	void AddListener(UniqueSocketDescriptor &&fd,
			 std::shared_ptr<LuaHandler> &&handler) noexcept;

	void AddListener(SocketAddress address,
			 std::shared_ptr<LuaHandler> handler) noexcept;

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
};
