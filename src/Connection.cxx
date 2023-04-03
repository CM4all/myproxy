// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Action.hxx"
#include "LAction.hxx"
#include "LClient.hxx"
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
#include <stdexcept>

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
		throw std::runtime_error{"Unexpected client data before handshake"};

	if (!incoming.handshake_response)
		return OnHandshakeResponse(number, payload);

	if (!outgoing || !outgoing->peer.command_phase)
		return Result::BLOCKING;

	assert(incoming.command_phase);

	if (payload.empty())
		throw Mysql::MalformedPacket{};

	const auto cmd = static_cast<Mysql::Command>(payload.front());

	if (cmd == Mysql::Command::QUERY &&
	    request_time == Event::TimePoint{})
		request_time = GetEventLoop().SteadyNow();

	return Result::OK;
} catch (Mysql::MalformedPacket) {
	fmt::print(stderr, "Malformed packet from client\n");
	delete this;
	return Result::CLOSED;
} catch (...) {
	PrintException(std::current_exception());
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

MysqlHandler::Result
Connection::Outgoing::OnHandshake(std::span<const std::byte> payload)
{
	const auto packet = Mysql::ParseHandshake(payload);

	peer.capabilities = packet.capabilities;

	fmt::print("handshake server_version='{}'\n", packet.server_version);

	const auto &action = *connection.connect_action;

	// TODO implement "mysql_native_password"
	auto s = Mysql::MakeHandshakeResponse41(action.username,
						action.password,
						action.database,
						"mysql_clear_password"sv);
	if (!peer.Send(s.Finish()))
		return Result::CLOSED;

	peer.handshake_response = true;
	return Result::IGNORE;
}

MysqlHandler::Result
Connection::Outgoing::OnMysqlPacket([[maybe_unused]] unsigned number,
				    std::span<const std::byte> payload,
				    [[maybe_unused]] bool complete) noexcept
try {
	assert(connection.incoming.handshake);
	assert(connection.incoming.handshake_response);
	assert(connection.connect_action);

	if (!peer.handshake) {
		peer.handshake = true;
		return OnHandshake(payload);
	}

	if (payload.empty())
		throw Mysql::MalformedPacket{};

	const auto cmd = static_cast<Mysql::Command>(payload.front());

	if (!peer.command_phase) {
		if (cmd == Mysql::Command::OK || cmd == Mysql::Command::EOF_) {
			peer.command_phase = true;

			/* now process postponed packets */
			connection.incoming.socket.DeferRead();

			return Result::IGNORE;
		} else if (cmd == Mysql::Command::ERR) {
			throw Mysql::ParseErr(payload,
					      connection.incoming.capabilities);
		} else
			throw std::runtime_error{"Unexpected server reply to HandshakeResponse"};
	}

	if (cmd == Mysql::Command::EOF_ &&
	    connection.request_time != Event::TimePoint{}) {
		const auto duration = connection.GetEventLoop().SteadyNow() - connection.request_time;
		policy_duration(connection.username.c_str(), duration);
		connection.request_time = Event::TimePoint{};
	}

	return Result::OK;
} catch (Mysql::MalformedPacket) {
	fmt::print(stderr, "Malformed packet from server\n");
	delete &connection;
	return Result::CLOSED;
} catch (const Mysql::ErrPacket &packet) {
	if (!packet.error_message.empty())
		fmt::print(stderr, "MySQL error: '{}' ({})\n",
			   packet.error_message,
			   static_cast<uint_least16_t>(packet.error_code));
	else
		fmt::print(stderr, "MySQL error: {}\n",
			   static_cast<uint_least16_t>(packet.error_code));
	delete &connection;
	return Result::CLOSED;
} catch (...) {
	PrintException(std::current_exception());
	delete this;
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
	 lua_client(handler->GetState()),
	 thread(handler->GetState()),
	 defer_start_handler(event_loop, BIND_THIS_METHOD(OnDeferredStartHandler)),
	 delay_timer(event_loop, BIND_THIS_METHOD(OnDelayTimer)),
	 incoming(event_loop, std::move(fd), *this, *this),
	 connect(event_loop, *this)
{
	NewLuaClient(lua_client.GetState(), incoming.socket.GetSocket(), address);
	lua_client.Set(lua_client.GetState(), Lua::RelativeStackIndex{-1});
	lua_pop(lua_client.GetState(), 1);

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

	lua_client.Push(L);

	lua_newtable(L);
	Lua::SetField(L, Lua::RelativeStackIndex{-1}, "username", username);
	Lua::SetField(L, Lua::RelativeStackIndex{-1}, "password", auth_response);
	Lua::SetField(L, Lua::RelativeStackIndex{-1}, "database", database);

	Lua::Resume(L, 2);
}

void
Connection::OnLuaFinished(lua_State *L) noexcept
try {
	if (auto *err = CheckLuaErrAction(L, -1)) {
		incoming.SendErr(handeshake_response_sequence_id + 1,
				 Mysql::ErrorCode::HANDSHAKE_ERROR, "08S01"sv,
				 err->msg);
	} else if (auto *c = CheckLuaConnectAction(L, -1)) {
		connect_action = std::move(*c);

		/* TODO postpone the "OK" until we have received "OK"
		   from the real server */
		if (!incoming.SendOk(handeshake_response_sequence_id + 1))
			return;

		incoming.command_phase = true;

		/* connect to the outgoing server and perform the
		   handshake to it */
		connect.Connect(connect_action->address,
				std::chrono::seconds{30});
	} else
		throw std::invalid_argument{"Bad return value"};
} catch (...) {
	OnLuaError(L, std::current_exception());
}

void
Connection::OnLuaError(lua_State *L, std::exception_ptr e) noexcept
{
	(void)L;

	PrintException(e);

	if (incoming.SendErr(handeshake_response_sequence_id + 1,
			     Mysql::ErrorCode::HANDSHAKE_ERROR, "08S01"sv,
			     "Lua error"sv))
	    delete this;
}
