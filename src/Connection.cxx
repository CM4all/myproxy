// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Config.hxx"
#include "Instance.hxx"
#include "BufferedIO.hxx"
#include "MysqlProtocol.hxx"
#include "clock.h"
#include "Policy.hxx"
#include "net/ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/PrintException.hxx"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>

Connection::~Connection() noexcept = default;

std::pair<PeerHandler::ForwardResult, std::size_t>
Connection::Outgoing::OnPeerForward(std::span<std::byte> src)
{
	return connection.incoming.Forward(src);
}

bool
Connection::Outgoing::OnPeerWrite()
{
	if (connection.delayed)
		/* don't continue reading now */
		return true;

	return connection.incoming.socket.socket.Read(); // TODO return value?
}

void
Connection::Outgoing::OnPeerClosed() noexcept
{
	delete &connection;
}

void
Connection::Outgoing::OnPeerError(std::exception_ptr e) noexcept
{
	PrintException(e);
	delete &connection;
}

std::pair<PeerHandler::ForwardResult, std::size_t>
Connection::OnPeerForward(std::span<std::byte> src)
{
	if (delayed)
		/* don't continue reading now */
		return {PeerHandler::ForwardResult::OK, 0};

	if (!outgoing)
		return {PeerHandler::ForwardResult::OK, 0};

	return outgoing->peer.Forward(src);
}

void
Connection::OnPeerClosed() noexcept
{
	delete this;
}

bool
Connection::OnPeerWrite()
{
	if (!outgoing)
		return true;

	return outgoing->peer.socket.socket.Read(); // TODO return value?
}

void
Connection::OnPeerError(std::exception_ptr e) noexcept
{
	PrintException(e);
	delete this;
}

void
Connection::OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept
{
	assert(!outgoing);

	outgoing.emplace(*this, std::move(fd));
	socket_schedule_read(&outgoing->peer.socket, true);
}

void
Connection::OnSocketConnectError(std::exception_ptr e) noexcept
{
	assert(!outgoing);

	PrintException(e);
	delete this;
}

static bool
connection_login_packet(Connection *connection,
			const char *data, size_t length)
{
	if (length < 33)
		return false;

	const char *user = data + 32;
	const char *user_end = (const char *)
		memchr((const void *)user, 0, data + length - user);
	if (user_end == NULL || user_end == user)
		return false;

	size_t user_length = user_end - user;
	if (user_length >= sizeof(connection->user))
		user_length = sizeof(connection->user) - 1;

	memcpy(connection->user, user, user_length);
	connection->user[user_length] = 0;

	unsigned delay_ms = policy_login(connection->user);
	if (delay_ms > 0)
		connection_delay(connection, delay_ms);

	return true;
}

void
Connection::OnMysqlPacket(unsigned number, size_t length,
			  const void *data, size_t available)
{
	if (Mysql::IsQueryPacket(number, data, length) &&
	    request_time == 0)
		request_time = now_us();

	if (number == 1 && !login_received) {
		login_received =
			connection_login_packet(this, (const char *)data, available);
	}
}

void
Connection::Outgoing::OnMysqlPacket(unsigned number, size_t length,
				    const void *data,
				    [[maybe_unused]] size_t available)
{
	if (!connection.greeting_received && number == 0) {
		connection.greeting_received = true;
	}

	if (Mysql::IsEofPacket(number, data, length) &&
	    connection.login_received &&
	    connection.request_time != 0) {
		uint64_t duration_us = now_us() - connection.request_time;
		policy_duration(connection.user, (unsigned)(duration_us / 1000));
		connection.request_time = 0;
	}
}

Connection::Outgoing::Outgoing(Connection &_connection,
			       UniqueSocketDescriptor fd) noexcept
	:connection(_connection),
	 peer(connection.GetEventLoop(), std::move(fd), *this, *this)
{
}

void
Connection::OnDelayTimer() noexcept
{
	assert(delayed);

	delayed = false;

	incoming.socket.socket.Read();
}

Connection::Connection(Instance &_instance, UniqueSocketDescriptor fd,
		       SocketAddress)
	:instance(&_instance),
	 delay_timer(instance->event_loop, BIND_THIS_METHOD(OnDelayTimer)),
	 incoming(instance->event_loop, std::move(fd), *this, *this),
	 connect(instance->event_loop, *this)
{
	socket_schedule_read(&incoming.socket, false);

	delayed = false;

	greeting_received = false;
	login_received = false;
	request_time = 0;

	// TODO move this call out of the ctor
	connect.Connect(instance->config.server_address, std::chrono::seconds{30});

	socket_schedule_read(&incoming.socket, false);
}

void
connection_delay(Connection *c, unsigned delay_ms)
{
	assert(delay_ms > 0);

	c->delayed = true;

	socket_unschedule_read(&c->incoming.socket);

	c->delay_timer.Schedule(std::chrono::milliseconds{delay_ms});
}
