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
#include "util/SpanCast.hxx"

#include <cassert>
#include <cstring>

Connection::~Connection() noexcept = default;

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
Connection::OnLoginPacket(std::span<const std::byte> payload)
{
	if (payload.size() < 33)
		return false;

	payload = payload.subspan(32);

	auto nul = std::find(payload.begin(), payload.end(), std::byte{});
	if (nul == payload.begin() && nul == payload.end())
		return false;

	user = ToStringView(std::span{payload.begin(), nul});

	const auto delay = policy_login(user.c_str());
	if (delay.count() > 0)
		Delay(delay);

	return true;
}

MysqlHandler::Result
Connection::OnMysqlPacket(unsigned number, std::span<const std::byte> payload,
			  [[maybe_unused]] bool complete) noexcept
{
	if (Mysql::IsQueryPacket(number, payload) &&
	    request_time == Event::TimePoint{})
		request_time = GetEventLoop().SteadyNow();

	if (number == 1 && !login_received) {
		login_received = OnLoginPacket(payload);
	}

	return Result::OK;
}

std::pair<MysqlHandler::Result, std::size_t>
Connection::OnMysqlRaw(std::span<const std::byte> src) noexcept
{
	if (delayed)
		/* don't continue reading now */
		return {Result::OK, 0U};

	if (!outgoing)
		return {Result::OK, 0U};

	const auto result = outgoing->peer.socket.Write(src.data(), src.size());
	if (result > 0) [[likely]]
		return {Result::OK, static_cast<std::size_t>(result)};

	switch (result) {
	case WRITE_BLOCKING:
		return {Result::OK, 0U};

	default:
		// TODO
		return {Result::CLOSED, 0U};
	}
}

MysqlHandler::Result
Connection::Outgoing::OnMysqlPacket(unsigned number, std::span<const std::byte> payload,
				    [[maybe_unused]] bool complete) noexcept
{
	if (!connection.greeting_received && number == 0) {
		connection.greeting_received = true;
	}

	if (Mysql::IsEofPacket(number, payload) &&
	    connection.login_received &&
	    connection.request_time != Event::TimePoint{}) {
		const auto duration = connection.GetEventLoop().SteadyNow() - connection.request_time;
		policy_duration(connection.user.c_str(), duration);
		connection.request_time = Event::TimePoint{};
	}

	return Result::OK;
}

std::pair<MysqlHandler::Result, std::size_t>
Connection::Outgoing::OnMysqlRaw(std::span<const std::byte> src) noexcept
{
	const auto result = connection.incoming.socket.Write(src.data(), src.size());
	if (result > 0) [[likely]]
		return {Result::OK, static_cast<std::size_t>(result)};

	switch (result) {
	case WRITE_BLOCKING:
		return {Result::OK, 0U};

	default:
		// TODO
		return {Result::CLOSED, 0U};
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
