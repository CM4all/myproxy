// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Action.hxx"
#include "LAction.hxx"
#include "LClient.hxx"
#include "LHandler.hxx"
#include "Config.hxx"
#include "Instance.hxx"
#include "MysqlProtocol.hxx"
#include "MysqlParser.hxx"
#include "MysqlDeserializer.hxx"
#include "MysqlSerializer.hxx"
#include "MysqlMakePacket.hxx"
#include "MysqlAuth.hxx"
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

PeerHandler::WriteResult
Connection::Outgoing::OnPeerWrite()
{
	switch (connection.incoming.Flush()) {
	case MysqlReader::FlushResult::DRAINED:
		break;

	case MysqlReader::FlushResult::BLOCKING:
		return WriteResult::MORE;

	case MysqlReader::FlushResult::MORE:
		return WriteResult::DONE;

	case MysqlReader::FlushResult::CLOSED:
		return WriteResult::CLOSED;
	}

	if (connection.IsDelayed())
		/* don't continue reading now */
		return WriteResult::DONE;

	connection.got_raw_from_incoming = false;

	if (!connection.incoming.Read())
		return WriteResult::CLOSED;

	return connection.got_raw_from_incoming
		? WriteResult::MORE
		: WriteResult::DONE;
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

PeerHandler::WriteResult
Connection::OnPeerWrite()
{
	if (!incoming.handshake) {
		static constexpr std::array<std::byte, 0x15> auth_plugin_data{};

		auto s = Mysql::MakeHandshakeV10("5.7.30"sv,
						 "mysql_clear_password"sv,
						 auth_plugin_data);
		if (!incoming.Send(s.Finish()))
			return WriteResult::CLOSED;

		incoming.handshake = true;
		return WriteResult::DONE;
	}

	if (!outgoing)
		return WriteResult::DONE;

	switch (outgoing->peer.Flush()) {
	case MysqlReader::FlushResult::DRAINED:
		break;

	case MysqlReader::FlushResult::BLOCKING:
		return WriteResult::MORE;

	case MysqlReader::FlushResult::MORE:
		return WriteResult::DONE;

	case MysqlReader::FlushResult::CLOSED:
		return WriteResult::CLOSED;
	}

	got_raw_from_outgoing = false;

	if (!outgoing->peer.Read())
		return WriteResult::CLOSED;

	return got_raw_from_outgoing ? WriteResult::MORE : WriteResult::DONE;
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

	fmt::print("login user='{}' database='{}'\n", packet.user, packet.database);

	user = packet.user;
	auth_response = packet.auth_response;
	database = packet.database;

	const auto delay = policy_login(user.c_str());
	if (delay.count() > 0)
		Delay(delay);

	incoming.handshake_response = true;

	handeshake_response_sequence_id = sequence_id;
	defer_start_handler.Schedule();
	return Result::IGNORE;
}

inline MysqlHandler::Result
Connection::OnInitDb(uint_least8_t sequence_id,
		     std::span<const std::byte> payload)
{
	assert(incoming.command_phase);
	assert(outgoing);
	assert(outgoing->peer.command_phase);

	const auto packet = Mysql::ParseInitDb(payload);

	if (packet.database == database) {
		/* no-op */

		return incoming.SendOk(sequence_id + 1)
			? Result::IGNORE
			: Result::CLOSED;
	}

	// TODO invoke Lua callback for the decision

	return Result::FORWARD;
}

inline MysqlHandler::Result
Connection::OnChangeUser(uint_least8_t sequence_id,
			 std::span<const std::byte> payload)
{
	assert(incoming.command_phase);
	assert(outgoing);
	assert(outgoing->peer.command_phase);

	const auto packet = Mysql::ParseChangeUser(payload, incoming.capabilities);

	if (packet.user == user && packet.database == database) {
		/* no change: translate to RESET_CONNECTION */

		auto s = Mysql::MakeResetConnection(sequence_id);
		if (!outgoing->peer.Send(s.Finish()))
			return Result::CLOSED;

		return Result::IGNORE;
	}

	return Result::FORWARD;
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

	switch (cmd) {
	case Mysql::Command::OK:
	case Mysql::Command::EOF_:
	case Mysql::Command::ERR:
	case Mysql::Command::RESET_CONNECTION:
		break;

	case Mysql::Command::QUERY:
		if (request_time == Event::TimePoint{})
			request_time = GetEventLoop().SteadyNow();
		break;

	case Mysql::Command::INIT_DB:
		return OnInitDb(number, payload);

	case Mysql::Command::CHANGE_USER:
		return OnChangeUser(number, payload);
	}

	return Result::FORWARD;
} catch (Mysql::MalformedPacket) {
	fmt::print(stderr, "Malformed packet from client\n");
	delete this;
	return Result::CLOSED;
} catch (...) {
	PrintException(std::current_exception());
	delete this;
	return Result::CLOSED;
}

std::pair<MysqlHandler::RawResult, std::size_t>
Connection::OnMysqlRaw(std::span<const std::byte> src) noexcept
{
	if (IsDelayed())
		/* don't continue reading now */
		return {RawResult::OK, 0U};

	if (!outgoing)
		return {RawResult::OK, 0U};

	got_raw_from_incoming = true;

	const auto result = outgoing->peer.WriteSome(src);
	if (result > 0) [[likely]]
		return {RawResult::OK, static_cast<std::size_t>(result)};

	switch (result) {
	case WRITE_BLOCKING:
		return {RawResult::OK, 0U};

	default:
		// TODO
		return {RawResult::CLOSED, 0U};
	}
}

MysqlHandler::Result
Connection::Outgoing::OnHandshake(std::span<const std::byte> payload)
{
	assert(!connection.incoming.command_phase);

	const auto packet = Mysql::ParseHandshake(payload);

	peer.capabilities = packet.capabilities;

	fmt::print("handshake server_version='{}'\n", packet.server_version);

	const auto &action = *connection.connect_action;

	auto s = Mysql::MakeHandshakeResponse41(packet, connection.incoming.capabilities,
						action.user,
						action.password,
						action.database);
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
		assert(!connection.incoming.command_phase);

		switch (cmd) {
		case Mysql::Command::OK:
		case Mysql::Command::EOF_:
			peer.command_phase = true;
			connection.incoming.command_phase = true;

			/* now process postponed packets */
			connection.incoming.DeferRead();

			return Result::FORWARD;

		case Mysql::Command::ERR:
			return Result::FORWARD;

		default:
			throw std::runtime_error{"Unexpected server reply to HandshakeResponse"};
		}
	}

	switch (cmd) {
	case Mysql::Command::EOF_:
		if (connection.request_time != Event::TimePoint{}) {
			const auto duration = connection.GetEventLoop().SteadyNow() - connection.request_time;
			policy_duration(connection.user.c_str(), duration);
			connection.request_time = Event::TimePoint{};
		}

		break;

	case Mysql::Command::OK:
	case Mysql::Command::ERR:
	case Mysql::Command::QUERY:
	case Mysql::Command::INIT_DB:
	case Mysql::Command::CHANGE_USER:
	case Mysql::Command::RESET_CONNECTION:
		break;
	}

	return Result::FORWARD;
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
	delete &connection;
	return Result::CLOSED;
}

std::pair<MysqlHandler::RawResult, std::size_t>
Connection::Outgoing::OnMysqlRaw(std::span<const std::byte> src) noexcept
{
	connection.got_raw_from_outgoing = true;

	const auto result = connection.incoming.WriteSome(src);
	if (result > 0) [[likely]]
		return {RawResult::OK, static_cast<std::size_t>(result)};

	switch (result) {
	case WRITE_BLOCKING:
		return {RawResult::OK, 0U};

	default:
		// TODO
		return {RawResult::CLOSED, 0U};
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
	incoming.Read();
}

Connection::Connection(EventLoop &event_loop,
		       std::shared_ptr<LuaHandler> _handler,
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
	NewLuaClient(GetLuaState(), incoming.GetSocket(), address);
	lua_client.Set(GetLuaState(), Lua::RelativeStackIndex{-1});
	lua_pop(GetLuaState(), 1);

	/* write the handshake */
	incoming.DeferWrite();
}

void
Connection::Delay(Event::Duration duration) noexcept
{
	incoming.UnscheduleRead();

	delay_timer.Schedule(duration);
}

inline void
Connection::OnDeferredStartHandler() noexcept
{
	/* create a new thread for the handler coroutine */
	const auto L = thread.CreateThread(*this);

	handler->PushOnHandshakeResponse(L);

	lua_client.Push(L);

	lua_newtable(L);
	Lua::SetField(L, Lua::RelativeStackIndex{-1}, "user", user);
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
