// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Connection.hxx"
#include "util/DeleteDisposer.hxx"

#include <cstddef>

Instance::Instance(const Config &_config)
	:config(_config)
{
	shutdown_listener.Enable();
}

void
Instance::OnShutdown() noexcept
{
	shutdown_listener.Disable();
	listener.Close();
	connections.clear_and_dispose(DeleteDisposer{});
}
