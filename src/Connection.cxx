// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Action.hxx"
#include "Cluster.hxx"
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
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lua/CoAwaitable.hxx"
#include "lua/Thread.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/ScopeExit.hxx"
#include "util/SpanCast.hxx"

#include <cassert>
#include <cstring>
#include <stdexcept>

using std::string_view_literals::operator""sv;

Connection::~Connection() noexcept = default;

PeerHandler::WriteResult
Connection::Outgoing::OnPeerWrite()
{
	assert(!connection.IsStale());

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

	switch (connection.incoming.Read()) {
	case BufferedReadResult::OK:
	case BufferedReadResult::BLOCKING:
		break;

	case BufferedReadResult::DISCONNECTED:
	case BufferedReadResult::DESTROYED:
		return WriteResult::CLOSED;
	}

	return connection.got_raw_from_incoming
		? WriteResult::MORE
		: WriteResult::DONE;
}

void
Connection::Outgoing::OnPeerClosed() noexcept
{
	connection.SafeDelete();
}

void
Connection::Outgoing::OnPeerError(std::exception_ptr e) noexcept
{
	fmt::print(stderr, "[{}] {}\n", connection.GetName(), e);
	connection.SafeDelete();
}

void
Connection::OnPeerClosed() noexcept
{
	SafeDelete();
}

PeerHandler::WriteResult
Connection::OnPeerWrite()
{
	assert(!IsStale());

	if (!incoming.handshake) {
		static constexpr std::array<std::byte, 0x15> auth_plugin_data{};

		auto s = Mysql::MakeHandshakeV10(lua_client_ptr->GetServerVersion(),
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

	switch (outgoing->peer.Read()) {
	case BufferedReadResult::OK:
	case BufferedReadResult::BLOCKING:
		break;

	case BufferedReadResult::DISCONNECTED:
	case BufferedReadResult::DESTROYED:
		return WriteResult::CLOSED;
	}

	return got_raw_from_outgoing ? WriteResult::MORE : WriteResult::DONE;
}

void
Connection::OnPeerError(std::exception_ptr e) noexcept
{
	fmt::print(stderr, "[{}] {}\n", GetName(), e);
	SafeDelete();
}

void
Connection::OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept
{
	assert(!outgoing);

	/* disable Nagle's algorithm to reduce latency */
	fd.SetNoDelay();

	outgoing.emplace(*this, std::move(fd));
}

void
Connection::OnSocketConnectError(std::exception_ptr e) noexcept
{
	assert(!outgoing);
	assert(incoming.handshake);
	assert(incoming.handshake_response);
	assert(!incoming.command_phase);

	fmt::print(stderr, "[{}] {}\n", GetName(), e);

	if (incoming.SendErr(2,
			     Mysql::ErrorCode::HANDSHAKE_ERROR, "08S01"sv,
			     "Connection error"sv))
		SafeDelete();
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

	fmt::print("[{}] login user='{}' database='{}'\n", GetName(), packet.user, packet.database);

	user = packet.user;
	auth_response = packet.auth_response;
	database = packet.database;

	incoming.handshake_response = true;

	StartCoroutine(InvokeLuaHandshakeResponse(sequence_id));
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
	fmt::print(stderr, "[{}] Malformed packet from client\n", GetName());
	SafeDelete();
	return Result::CLOSED;
} catch (...) {
	fmt::print(stderr, "[{}] {}\n", GetName(), std::current_exception());
	SafeDelete();
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

static auto
MakeHandshakeResponse41(const Mysql::HandshakePacket &handshake,
			uint8_t sequence_id, uint_least32_t client_flag,
			const ConnectAction &action)
{
	if (!action.password_sha1.empty()) {
		assert(action.password_sha1.size() == SHA1_DIGEST_LENGTH);

		const std::span<const std::byte, SHA1_DIGEST_LENGTH> password_sha1{AsBytes(action.password_sha1)};
		return Mysql::MakeHandshakeResponse41SHA1(handshake, sequence_id, client_flag,
							  action.user, password_sha1,
							  action.database);
	}

	return Mysql::MakeHandshakeResponse41(handshake,
					      sequence_id, client_flag,
					      action.user,
					      action.password,
					      action.database);
}

MysqlHandler::Result
Connection::Outgoing::OnHandshake(uint8_t sequence_id, std::span<const std::byte> payload)
{
	assert(!connection.incoming.command_phase);

	if (!payload.empty() && static_cast<Mysql::Command>(payload.front()) == Mysql::Command::ERR) {
		const auto err = Mysql::ParseErr(payload, peer.capabilities);
		throw FmtRuntimeError("Connection rejected by server: {}",
				      err.error_message);
	}

	const auto packet = Mysql::ParseHandshake(payload);

	connection.lua_client_ptr->SetServerVersion(packet.server_version);

	peer.capabilities = packet.capabilities;

	fmt::print("[{}] handshake server_version='{}'\n",
		   connection.GetName(), packet.server_version);

	const auto &action = *connection.connect_action;

	auto s = MakeHandshakeResponse41(packet, sequence_id + 1,
					 connection.incoming.capabilities,
					 action);
	if (!peer.Send(s.Finish()))
		return Result::CLOSED;

	peer.handshake_response = true;
	return Result::IGNORE;
}

static auto
MakeAuthSwitchResponse(const Mysql::AuthSwitchRequest &auth_switch_request,
		       uint8_t sequence_id,
		       const ConnectAction &action)
{
	return Mysql::MakeAuthSwitchResponse(auth_switch_request, sequence_id,
					     action.password, action.password_sha1);
}

MysqlHandler::Result
Connection::Outgoing::OnAuthSwitchRequest(uint8_t sequence_id, std::span<const std::byte> payload)
{
	assert(!connection.incoming.command_phase);

	const auto packet = Mysql::ParseAuthSwitchRequest(payload);

	const auto &action = *connection.connect_action;

	auto s = MakeAuthSwitchResponse(packet, sequence_id + 1, action);
	if (!peer.Send(s.Finish()))
		return Result::CLOSED;

	return Result::IGNORE;
}

MysqlHandler::Result
Connection::Outgoing::OnMysqlPacket(unsigned number,
				    std::span<const std::byte> payload,
				    [[maybe_unused]] bool complete) noexcept
try {
	assert(connection.incoming.handshake);
	assert(connection.incoming.handshake_response);
	assert(connection.connect_action);

	if (!peer.handshake) {
		peer.handshake = true;
		return OnHandshake(number, payload);
	}

	if (payload.empty())
		throw Mysql::MalformedPacket{};

	const auto cmd = static_cast<Mysql::Command>(payload.front());

	if (!peer.command_phase) {
		assert(!connection.incoming.command_phase);

		switch (cmd) {
		case Mysql::Command::OK:
			peer.command_phase = true;
			connection.incoming.command_phase = true;

			connection.StartCoroutine(connection.InvokeLuaCommandPhase());

			return Result::FORWARD;

		case Mysql::Command::EOF_:
			return OnAuthSwitchRequest(number, payload);

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
	fmt::print(stderr, "[{}] Malformed packet from server\n",
		   connection.GetName());
	connection.SafeDelete();
	return Result::CLOSED;
} catch (const Mysql::ErrPacket &packet) {
	if (!packet.error_message.empty())
		fmt::print(stderr, "[{}] MySQL error: '{}' ({})\n",
			   connection.GetName(),
			   packet.error_message,
			   static_cast<uint_least16_t>(packet.error_code));
	else
		fmt::print(stderr, "[{}] MySQL error: {}\n",
			   connection.GetName(),
			   static_cast<uint_least16_t>(packet.error_code));
	connection.SafeDelete();
	return Result::CLOSED;
} catch (...) {
	fmt::print(stderr, "[{}] {}\n", connection.GetName(), std::current_exception());
	connection.SafeDelete();
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

Connection::Connection(EventLoop &event_loop,
		       std::shared_ptr<LuaHandler> _handler,
		       UniqueSocketDescriptor fd,
		       SocketAddress address)
	:handler(std::move(_handler)),
	 lua_client(handler->GetState()),
	 defer_start_handler(event_loop, BIND_THIS_METHOD(OnDeferredStartHandler)),
	 defer_delete(event_loop, BIND_THIS_METHOD(OnDeferredDelete)),
	 incoming(event_loop, std::move(fd), *this, *this),
	 connect(event_loop, *this)
{
	lua_client_ptr = LClient::New(GetLuaState(), incoming.GetSocket(), address,
				      "5.7.30"sv);
	lua_client.Set(GetLuaState(), Lua::RelativeStackIndex{-1});
	lua_pop(GetLuaState(), 1);

	StartCoroutine(InvokeLuaConnect());
}

std::string_view
Connection::GetName() const noexcept
{
	return lua_client_ptr->GetName();
}

inline void
Connection::StartCoroutine(Co::InvokeTask &&_coroutine) noexcept
{
	assert(!coroutine);
	assert(!defer_start_handler.IsPending());

	coroutine = std::move(_coroutine);
	defer_start_handler.Schedule();
}

inline void
Connection::OnCoroutineComplete(std::exception_ptr error) noexcept
{
	if (error) {
		fmt::print(stderr, "[{}] {}\n", GetName(), error);
		SafeDelete();
	}
}

inline void
Connection::OnDeferredStartHandler() noexcept
{
	assert(coroutine);

	coroutine.Start(BIND_THIS_METHOD(OnCoroutineComplete));
}

inline Co::InvokeTask
Connection::InvokeLuaConnect()
try {
	const auto main_L = GetLuaState();
	const Lua::ScopeCheckStack check_main_stack{main_L};

	Lua::Thread thread{main_L};

	/* create a new thread for the coroutine */
	const auto L = thread.Create(main_L);
	/* pop the new thread from the main stack */
	lua_pop(main_L, 1);

	handler->PushOnConnect(L);

	if (!lua_isnil(L, -1)) {
		lua_client.Push(L);

		co_await Lua::CoAwaitable{thread, L, 1};

		if (lua_isnil(L, -1)) {
			// OK
			// TODO implement more actions
		} else if (auto *err = CheckLuaErrAction(L, -1)) {
			if (incoming.SendErr(0,
					     Mysql::ErrorCode::HANDSHAKE_ERROR, "08S01"sv,
					     err->msg))
				SafeDelete();
			co_return;
		} else
			throw std::invalid_argument{"Bad return value"};
	}

	/* write the handshake */
	incoming.DeferWrite();
} catch (...) {
	fmt::print(stderr, "[{}] {}\n", GetName(), std::current_exception());

	if (incoming.SendErr(0,
			     Mysql::ErrorCode::HANDSHAKE_ERROR, "08S01"sv,
			     "Lua error"sv))
		SafeDelete();
}

inline Co::InvokeTask
Connection::InvokeLuaCommandPhase()
{
	const auto main_L = GetLuaState();
	const Lua::ScopeCheckStack check_main_stack{main_L};

	Lua::Thread thread{main_L};

	/* create a new thread for the coroutine */
	const auto L = thread.Create(main_L);
	/* pop the new thread from the main stack */
	lua_pop(main_L, 1);

	handler->PushOnCommandPhase(L);

	if (!lua_isnil(L, -1)) {
		lua_client.Push(L);

		co_await Lua::CoAwaitable{thread, L, 1};

		if (lua_isnil(L, -1)) {
			// OK
			// TODO implement more actions
		} else
			throw std::invalid_argument{"Bad return value"};
	}

	/* now process postponed packets */
	incoming.DeferRead();
}

inline Co::InvokeTask
Connection::InvokeLuaHandshakeResponse(uint_least8_t sequence_id) noexcept
try {
	const auto main_L = GetLuaState();
	const Lua::ScopeCheckStack check_main_stack{main_L};

	Lua::Thread thread{main_L};

	/* create a new thread for the coroutine */
	const auto L = thread.Create(main_L);
	/* pop the new thread from the main stack */
	lua_pop(main_L, 1);

	handler->PushOnHandshakeResponse(L);

	lua_client.Push(L);

	lua_newtable(L);
	Lua::SetField(L, Lua::RelativeStackIndex{-1}, "user", user);
	Lua::SetField(L, Lua::RelativeStackIndex{-1}, "password", auth_response);

	if (!database.empty())
		Lua::SetField(L, Lua::RelativeStackIndex{-1}, "database", database);

	co_await Lua::CoAwaitable{thread, L, 2};

	if (auto *err = CheckLuaErrAction(L, -1)) {
		incoming.SendErr(sequence_id + 1,
				 Mysql::ErrorCode::HANDSHAKE_ERROR, "08S01"sv,
				 err->msg);
	} else if (auto *c = CheckLuaConnectAction(L, -1)) {
		connect_action = std::move(*c);

		SocketAddress address = connect_action->address;
		if (connect_action->cluster) {
			connect_action->cluster->Push(L);
			AtScopeExit(L) { lua_pop(L, 1); };

			auto &cluster = Cluster::Cast(L, -1);

			/* wait until all nodes have been probed */
			co_await cluster.CoWaitReady();

			address = cluster.Pick(lua_client_ptr->GetAccount());
		}

		/* connect to the outgoing server and perform the
		   handshake to it */
		connect.Connect(address,
				std::chrono::seconds{30});
	} else
		throw std::invalid_argument{"Bad return value"};
} catch (...) {
	fmt::print(stderr, "[{}] {}\n", GetName(), std::current_exception());

	if (incoming.SendErr(sequence_id + 1,
			     Mysql::ErrorCode::HANDSHAKE_ERROR, "08S01"sv,
			     "Lua error"sv))
		SafeDelete();
}
