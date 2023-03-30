// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "LConnection.hxx"
#include "Config.hxx"
#include "Instance.hxx"
#include "MysqlProtocol.hxx"
#include "MysqlParser.hxx"
#include "MysqlDeserializer.hxx"
#include "MysqlSerializer.hxx"
#include "MysqlMakePacket.hxx"
#include "Policy.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/PrintException.hxx"

#include <fmt/core.h>

#include <cassert>
#include <cstring>

using std::string_view_literals::operator""sv;

Connection::~Connection() noexcept
{
	thread.Cancel();
}

bool
Connection::Outgoing::OnPeerWrite()
{
	if (connection.IsDelayed())
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
	if (!incoming.handshake) {
		static constexpr std::array<std::byte, 0x15> auth_plugin_data{};

		auto s = Mysql::MakeHandshakeV10("5.7.30"sv,
						 "mysql_clear_password"sv,
						 auth_plugin_data);
		if (!incoming.Send(s.Finish()))
			return false;

		incoming.handshake = true;
		return true;
	}

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

inline MysqlHandler::Result
Connection::OnHandshakeResponse(uint_least8_t sequence_id,
				std::span<const std::byte> payload)
{
	assert(incoming.handshake);
	assert(!incoming.handshake_response);
	assert(!incoming.command_phase);
	assert(!outgoing);

	const auto packet = Mysql::ParseHandshakeResponse(payload);

	incoming.capabilities = packet.capabilities;

	fmt::print("login username='{}' database='{}'\n", packet.username, packet.database);

	username = packet.username;
	auth_response = packet.auth_response;
	database = packet.database;

	const auto delay = policy_login(username.c_str());
	if (delay.count() > 0)
		Delay(delay);

	incoming.handshake_response = true;

	handeshake_response_sequence_id = sequence_id;
	defer_start_handler.Schedule();
	return Result::IGNORE;
}

MysqlHandler::Result
Connection::OnMysqlPacket(unsigned number, std::span<const std::byte> payload,
			  [[maybe_unused]] bool complete) noexcept
try {
	if (!incoming.handshake)
		throw Mysql::MalformedPacket{};

	if (!incoming.handshake_response)
		return OnHandshakeResponse(number, payload);

	if (!outgoing || !outgoing->peer.command_phase)
		return Result::BLOCKING;

	if (Mysql::IsQueryPacket(number, payload) &&
	    request_time == Event::TimePoint{})
		request_time = GetEventLoop().SteadyNow();

	return Result::OK;
} catch (Mysql::MalformedPacket) {
	delete this;
	return Result::CLOSED;
}

std::pair<MysqlHandler::Result, std::size_t>
Connection::OnMysqlRaw(std::span<const std::byte> src) noexcept
{
	if (IsDelayed())
		/* don't continue reading now */
		return {Result::BLOCKING, 0U};

	if (!outgoing)
		return {Result::BLOCKING, 0U};

	const auto result = outgoing->peer.socket.Write(src.data(), src.size());
	if (result > 0) [[likely]]
		return {Result::OK, static_cast<std::size_t>(result)};

	switch (result) {
	case WRITE_BLOCKING:
		return {Result::BLOCKING, 0U};

	default:
		// TODO
		return {Result::CLOSED, 0U};
	}
}

bool
Connection::MaybeSendHandshakeResponse() noexcept
{
	assert(incoming.handshake);
	assert(incoming.handshake_response);

	if (!outgoing || !outgoing->peer.handshake)
		return true;

	assert(!outgoing->peer.handshake_response);

	// TODO implement "mysql_native_password"
	auto s = Mysql::MakeHandshakeResponse41(username, auth_response,
						database,
						"mysql_clear_password"sv);
	if (!outgoing->peer.Send(s.Finish()))
		return false;

	outgoing->peer.handshake_response = true;
	return true;
}

void
Connection::Outgoing::OnHandshake(std::span<const std::byte> payload)
{
	const auto packet = Mysql::ParseHandshake(payload);

	peer.capabilities = packet.capabilities;

	fmt::print("handshake server_version='{}'\n", packet.server_version);
}

MysqlHandler::Result
Connection::Outgoing::OnMysqlPacket([[maybe_unused]] unsigned number,
				    std::span<const std::byte> payload,
				    [[maybe_unused]] bool complete) noexcept
try {
	if (!peer.handshake) {
		peer.handshake = true;
		OnHandshake(payload);
		connection.incoming.handshake = true;

		if (!connection.MaybeSendHandshakeResponse())
			return Result::CLOSED;

		return Result::IGNORE;
	}

	if (!peer.command_phase) {
		if (Mysql::IsOkPacket(payload)) {
			peer.command_phase = true;

			/* now process postponed packets */
			connection.incoming.socket.DeferRead();

			return Result::IGNORE;
		} else if (Mysql::IsErrPacket(payload)) {
			// TODO extract message
			throw Mysql::MalformedPacket{};
		} else
			throw Mysql::MalformedPacket{};
	}

	if (Mysql::IsEofPacket(payload) &&
	    connection.request_time != Event::TimePoint{}) {
		const auto duration = connection.GetEventLoop().SteadyNow() - connection.request_time;
		policy_duration(connection.username.c_str(), duration);
		connection.request_time = Event::TimePoint{};
	}

	return Result::OK;
} catch (Mysql::MalformedPacket) {
	delete &connection;
	return Result::CLOSED;
}

std::pair<MysqlHandler::Result, std::size_t>
Connection::Outgoing::OnMysqlRaw(std::span<const std::byte> src) noexcept
{
	const auto result = connection.incoming.socket.Write(src.data(), src.size());
	if (result > 0) [[likely]]
		return {Result::OK, static_cast<std::size_t>(result)};

	switch (result) {
	case WRITE_BLOCKING:
		return {Result::BLOCKING, 0U};

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

inline void
Connection::OnDelayTimer() noexcept
{
	incoming.socket.Read();
}

Connection::Connection(EventLoop &event_loop, Lua::ValuePtr _handler,
		       UniqueSocketDescriptor fd,
		       SocketAddress address)
	:handler(std::move(_handler)),
	 lua_connection(handler->GetState()),
	 thread(handler->GetState()),
	 defer_start_handler(event_loop, BIND_THIS_METHOD(OnDeferredStartHandler)),
	 delay_timer(event_loop, BIND_THIS_METHOD(OnDelayTimer)),
	 incoming(event_loop, std::move(fd), *this, *this),
	 connect(event_loop, *this)
{
	NewLuaConnection(lua_connection.GetState(), incoming.socket.GetSocket(),
			 address);
	lua_connection.Set(lua_connection.GetState(), Lua::RelativeStackIndex{-1});
	lua_pop(lua_connection.GetState(), 1);

	/* write the handshake */
	incoming.socket.DeferWrite();
}

void
Connection::Delay(Event::Duration duration) noexcept
{
	incoming.socket.UnscheduleRead();

	delay_timer.Schedule(duration);
}

inline void
Connection::OnDeferredStartHandler() noexcept
{
	/* create a new thread for the handler coroutine */
	const auto L = thread.CreateThread(*this);

	handler->Push(L);

	lua_connection.Push(L);

	lua_newtable(L);
	Lua::SetField(L, Lua::RelativeStackIndex{-1}, "username", username);
	Lua::SetField(L, Lua::RelativeStackIndex{-1}, "database", database);

	Lua::Resume(L, 2);
}

void
Connection::OnLuaFinished(lua_State *L) noexcept
try {
	if (!incoming.SendOk(handeshake_response_sequence_id + 1))
		return;

	incoming.command_phase = true;

	/* connect to the outgoing server and perform the handshake to
	   it */
	connect.Connect(Lua::ToSocketAddress(L, -1, 3306),
			std::chrono::seconds{30});
} catch (...) {
	OnLuaError(L, std::current_exception());
}

void
Connection::OnLuaError(lua_State *L, std::exception_ptr e) noexcept
{
	PrintException(e);
	(void)L;
	delete this;
}
