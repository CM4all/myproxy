// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Listener.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/net/ServerSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <forward_list>

struct Config;
struct Connection;

class Instance {
	const Config &config;

	EventLoop event_loop;

	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

	std::forward_list<MyProxyListener> listeners;

public:
	explicit Instance(const Config &config);

	const auto &GetConfig() const noexcept {
		return config;
	}

	auto &GetEventLoop() noexcept {
		return event_loop;
	}

private:
	void OnShutdown() noexcept;
};
