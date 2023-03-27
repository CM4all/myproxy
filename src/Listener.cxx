// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Connection.hxx"

#include <fmt/core.h>

#include <errno.h>
#include <string.h>

void
Instance::OnListenerReady(unsigned) noexcept
{
	UniqueSocketDescriptor remote_fd{listener.GetSocket().AcceptNonBlock()};
	if (!remote_fd.IsDefined()) {
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			fmt::print(stderr, "accept() failed: %s\n", strerror(errno));
		return;
	}

	Connection *connection = new Connection(*this, std::move(remote_fd));
	connections.push_back(*connection);
}
