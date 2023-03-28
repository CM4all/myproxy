// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Config.hxx"
#include "Instance.hxx"
#include "BufferedIO.hxx"
#include "MysqlProtocol.hxx"
#include "Policy.hxx"
#include "net/ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/PrintException.hxx"

#include <cassert>
#include <cstring>

Connection::~Connection() noexcept = default;

std::pair<PeerHandler::ForwardResult, std::size_t>
Connection::Outgoing::OnPeerForward(std::span<const std::byte> src)
{
	return connection.incoming.Forward(src);
}

bool
Connection::Outgoing::OnPeerWrite()
{
	if (connection.delayed)
		/* don't continue reading now */
		return true;

	return connection.incoming.socket.Read(); // TODO return value?
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
Connection::OnPeerForward(std::span<const std::byte> src)
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

	return outgoing->peer.socket.Read(); // TODO return value?
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
}

void
Connection::OnSocketConnectError(std::exception_ptr e) noexcept
{
	assert(!outgoing);

	PrintException(e);
	delete this;
}

bool
Connection::OnLoginPacket(const char *data, size_t length)
{
	if (length < 33)
		return false;

	const char *user_ = data + 32;
	const char *user_end = (const char *)
		memchr((const void *)user_, 0, data + length - user_);
	if (user_end == NULL || user_end == user_)
		return false;

	size_t user_length = user_end - user_;
	user = std::string_view{user_, user_length};

	const auto delay = policy_login(user.c_str());
	if (delay.count() > 0)
		Delay(delay);

	return true;
}

void
Connection::OnMysqlPacket(unsigned number, size_t length,
			  const void *data, size_t available)
{
	if (Mysql::IsQueryPacket(number, data, length) &&
	    request_time == Event::TimePoint{})
		request_time = GetEventLoop().SteadyNow();

	if (number == 1 && !login_received) {
		login_received = OnLoginPacket((const char *)data, available);
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
	    connection.request_time != Event::TimePoint{}) {
		const auto duration = connection.GetEventLoop().SteadyNow() - connection.request_time;
		policy_duration(connection.user.c_str(), duration);
		connection.request_time = Event::TimePoint{};
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

	incoming.socket.Read();
}

Connection::Connection(Instance &_instance, UniqueSocketDescriptor fd,
		       SocketAddress)
	:instance(&_instance),
	 delay_timer(instance->event_loop, BIND_THIS_METHOD(OnDelayTimer)),
	 incoming(instance->event_loop, std::move(fd), *this, *this),
	 connect(instance->event_loop, *this)
{
	// TODO move this call out of the ctor
	connect.Connect(instance->config.server_address, std::chrono::seconds{30});
}

void
Connection::Delay(Event::Duration duration) noexcept
{
	delayed = true;

	incoming.socket.UnscheduleRead();

	delay_timer.Schedule(duration);
}
