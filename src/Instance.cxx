// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "Connection.hxx"
#include "util/DeleteDisposer.hxx"

#include <cstddef>

#include <sys/socket.h>

Instance::Instance(const Config &_config)
	:config(_config)
{
	shutdown_listener.Enable();

	listener.Open(config.listener.Create(SOCK_STREAM).Release());
	listener.ScheduleRead();
}

void
Instance::OnShutdown() noexcept
{
	shutdown_listener.Disable();
	listener.Close();
	connections.clear_and_dispose(DeleteDisposer{});
}
