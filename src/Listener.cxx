// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Listener.hxx"
#include "Instance.hxx"
#include "Connection.hxx"
#include "net/IPv4Address.hxx"
#include "net/SocketError.hxx"

#include <fmt/core.h>

#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

void
Instance::OnListenerReady(unsigned) noexcept
{
	UniqueSocketDescriptor remote_fd{listener.GetSocket().AcceptNonBlock()};
	if (!remote_fd.IsDefined()) {
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			fmt::print(stderr, "accept() failed: %s\n", strerror(errno));
		return;
	}

	if (!remote_fd.SetNoDelay()) {
		fmt::print(stderr, "setsockopt(TCP_NODELAY) failed: %s\n", strerror(errno));
		return;
	}

	Connection *connection = new Connection(*this, std::move(remote_fd));
	connections.push_back(*connection);
}

static UniqueSocketDescriptor
listener_create_socket(int family, int socktype, int protocol,
		       SocketAddress address)
{
	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(family, socktype, protocol))
		throw MakeSocketError("Failed to create socket");

	if (!fd.SetReuseAddress())
		throw MakeSocketError("Failed to set SO_REUSEADDR");

	if (!fd.Bind(address))
		throw MakeSocketError("Failed to bind socket");

	if (!fd.Listen(16))
		throw MakeSocketError("Failed to listen");

	return fd;
}

static void
listener_init_address(Instance *instance,
		      int family, int socktype, int protocol,
		      SocketAddress address)
{
	instance->listener.Open(listener_create_socket(family, socktype, protocol, address).Release());
	instance->listener.ScheduleRead();
}


void
listener_init(Instance *instance, uint16_t port)
{
	assert(port > 0);

	listener_init_address(instance, PF_INET, SOCK_STREAM, 0, IPv4Address{port});
}
