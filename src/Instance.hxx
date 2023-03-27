// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Listener.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/net/ServerSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"

struct Config;
struct Connection;

struct Instance {
	const Config &config;

	EventLoop event_loop;

	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

	MyProxyListener listener;

	explicit Instance(const Config &config);

private:
	void OnShutdown() noexcept;
};
