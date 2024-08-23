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
#include "MysqlForwardPacket.hxx"
#include "auth/Factory.hxx"
#include "auth/Handler.hxx"
#include "Policy.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lua/CoAwaitable.hxx"
#include "lua/Thread.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/ConnectSocket.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/ScopeExit.hxx"
#include "util/SpanCast.hxx"

#include <cassert>
#include <cstring>
#include <stdexcept>

using std::string_view_literals::operator""sv;

/**
 * The capabilities bit mask we send to clients connecting to us.
 */
static constexpr uint_least32_t handshake_capabilities =
	Mysql::CLIENT_MYSQL |
	Mysql::CLIENT_FOUND_ROWS |
	Mysql::CLIENT_LONG_FLAG |
	Mysql::CLIENT_CONNECT_WITH_DB |
	Mysql::CLIENT_PROTOCOL_41 |
	Mysql::CLIENT_IGNORE_SIGPIPE |
	Mysql::CLIENT_TRANSACTIONS |
	Mysql::CLIENT_RESERVED |
	Mysql::CLIENT_SECURE_CONNECTION | // TODO removing this breaks the HandshakeResponse??
	Mysql::CLIENT_MULTI_STATEMENTS |
	Mysql::CLIENT_MULTI_RESULTS |
	Mysql::CLIENT_PS_MULTI_RESULTS |
	Mysql::CLIENT_PLUGIN_AUTH |
	Mysql::CLIENT_SESSION_TRACK |
	Mysql::CLIENT_DEPRECATE_EOF |
	Mysql::CLIENT_REMEMBER_OPTIONS;

inline
Connection::Outgoing::~Outgoing() noexcept = default;

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
	connection.OnOutgoingError("Server closed the connection"sv);
}

void
Connection::Outgoing::OnPeerError(std::exception_ptr e) noexcept
{
	fmt::print(stderr, "[{}] {}\n", connection.GetName(), e);
	connection.OnOutgoingError("Error on connection to server"sv);
}

void
Connection::OnClusterNodeUnavailable() noexcept
{
	fmt::print(stderr, "[{}] Closing because node is unavailable\n", GetName());
	OnOutgoingError("Node is unavailable"sv);
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

		incoming.capabilities = handshake_capabilities;

		auto s = Mysql::MakeHandshakeV10(lua_client_ptr->GetServerVersion(),
						 handshake_capabilities,
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

bool
Connection::OnPeerBroken() noexcept
{
	/* do not log error if the client closes the connection while
	   we're sending to it */
	SafeDelete();
	return false;
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

	++outgoing_stats->n_connects;

	/* disable Nagle's algorithm to reduce latency */
	fd.SetNoDelay();

	outgoing.emplace(*this, *outgoing_stats, std::move(fd));
}

void
Connection::OnSocketConnectError(std::exception_ptr e) noexcept
{
	assert(!outgoing);
	assert(incoming.handshake);
	assert(incoming.handshake_response);
	assert(!incoming.command_phase);

	++outgoing_stats->n_connect_errors;

	fmt::print(stderr, "[{}] {}\n", GetName(), e);

	if (incoming.SendErr(incoming_handshake_response_sequence_id + 1,
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

	++stats.n_client_handshake_responses;

	const auto packet = Mysql::ParseHandshakeResponse(payload);

	incoming.capabilities &= packet.capabilities;

	fmt::print("[{}] login user={:?} database={:?}\n", GetName(), packet.user, packet.database);

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
	++stats.n_client_packets_received;

	if (!incoming.handshake)
		throw SocketProtocolError{"Unexpected client data before handshake"};

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
		++stats.n_client_queries;
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
	++stats.n_client_malformed_packets;
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
	if (result > 0) [[likely]] {
		if (static_cast<std::size_t>(result) < src.size())
			outgoing->peer.ScheduleWrite();

		stats.n_client_bytes_received += result;
		return {RawResult::OK, static_cast<std::size_t>(result)};
	}

	switch (result) {
	case WRITE_BLOCKING:
		return {RawResult::OK, 0U};

	default:
		// TODO
		return {RawResult::CLOSED, 0U};
	}
}

inline Event::Duration
Connection::MaybeFinishQuery() noexcept
{
	return request_time != Event::TimePoint{}
	       ? GetEventLoop().SteadyNow() - std::exchange(request_time, Event::TimePoint{})
	       : Event::Duration{-1};
}

inline MysqlHandler::Result
Connection::Outgoing::OnHandshake(uint_least8_t sequence_id,
				  std::span<const std::byte> payload)
{
	assert(!connection.incoming.command_phase);

	if (!payload.empty() && static_cast<Mysql::Command>(payload.front()) == Mysql::Command::ERR) {
		const auto err = Mysql::ParseErr(payload, peer.capabilities);
		throw FmtRuntimeError("Connection rejected by server: {}",
				      err.error_message);
	}

	const auto packet = Mysql::ParseHandshake(payload);

	connection.lua_client_ptr->SetServerVersion(packet.server_version);

	peer.capabilities = packet.capabilities & connection.incoming.capabilities;

	fmt::print("[{}] handshake server_version={:?}\n",
		   connection.GetName(), packet.server_version);

	auth_handler = Mysql::MakeAuthHandler(packet.auth_plugin_name, false);

	const auto &action = *connection.connect_action;
	const auto response =
		auth_handler->GenerateResponse(action.password,
					       AsBytes(action.password_sha1),
					       AsBytes(packet.auth_plugin_data1),
					       AsBytes(packet.auth_plugin_data2));

	auto s = Mysql::MakeHandshakeResponse41(sequence_id + 1,
						connection.incoming.capabilities,
						action.user,
						ToStringView(response),
						action.database,
						auth_handler->GetName());
	if (!peer.Send(s.Finish()))
		return Result::CLOSED;

	peer.handshake_response = true;
	return Result::IGNORE;
}

inline MysqlHandler::Result
Connection::Outgoing::OnAuthSwitchRequest(uint_least8_t sequence_id,
					  std::span<const std::byte> payload)
{
	assert(!connection.incoming.command_phase);

	const auto packet = Mysql::ParseAuthSwitchRequest(payload);

	auth_handler = Mysql::MakeAuthHandler(packet.auth_plugin_name, true);
	if (!auth_handler)
		throw SocketProtocolError{"Unsupported auth_plugin"};

	const auto &action = *connection.connect_action;
	const auto response =
		auth_handler->GenerateResponse(action.password,
					       AsBytes(action.password_sha1),
					       AsBytes(packet.auth_plugin_data),
					       {});

	Mysql::PacketSerializer s(sequence_id + 1);
	s.WriteN(response);
	if (!peer.Send(s.Finish()))
		return Result::CLOSED;

	return Result::IGNORE;
}

inline void
Connection::Outgoing::OnQueryOk(const Mysql::OkPacket &packet,
				Event::Duration duration) noexcept
{
	++stats.n_queries;
	stats.n_query_warnings += packet.warnings;
	stats.query_wait += duration;

	policy_duration(connection.user.c_str(), duration);
}

inline void
Connection::Outgoing::OnQueryErr(Event::Duration duration) noexcept
{
	++stats.n_queries;
	++stats.n_query_errors;
	stats.query_wait += duration;

	policy_duration(connection.user.c_str(), duration);
}

MysqlHandler::Result
Connection::Outgoing::OnMysqlPacket(unsigned number,
				    std::span<const std::byte> payload,
				    [[maybe_unused]] bool complete) noexcept
try {
	auto &c = connection;
	assert(c.incoming.handshake);
	assert(c.incoming.handshake_response);
	assert(c.connect_action);

	++stats.n_packets_received;

	if (!peer.handshake) {
		peer.handshake = true;
		return OnHandshake(number, payload);
	}

	if (payload.empty())
		throw Mysql::MalformedPacket{};

	const auto cmd = static_cast<Mysql::Command>(payload.front());

	if (!peer.command_phase) {
		assert(!c.incoming.command_phase);

		if (auth_handler) {
			if (const auto new_payload = auth_handler->HandlePacket(payload);
			    new_payload.data() != nullptr) {
				if (!new_payload.empty()) {
					Mysql::PacketSerializer s(number + 1);
					s.WriteN(new_payload);
					if (!peer.Send(s.Finish()))
						return Result::CLOSED;
				}

				return Result::IGNORE;
			}
		}

		switch (cmd) {
		case Mysql::Command::OK:
			++connection.stats.n_client_auth_ok;

			peer.command_phase = true;
			c.incoming.command_phase = true;
			auth_handler.reset();

			c.StartCoroutine(c.InvokeLuaCommandPhase());

			return c.incoming.Send(Mysql::MakeOk(c.incoming_handshake_response_sequence_id + 1,
							     c.incoming.capabilities,
							     Mysql::ParseOk(payload, peer.capabilities)))
				? Result::IGNORE
				: Result::CLOSED;

		case Mysql::Command::EOF_:
			return OnAuthSwitchRequest(number, payload);

		case Mysql::Command::ERR:
			++connection.stats.n_client_auth_err;

			if (c.incoming.Send(Mysql::MakeErr(c.incoming_handshake_response_sequence_id + 1,
							   c.incoming.capabilities,
							   Mysql::ParseErr(payload, peer.capabilities))))
				/* connection can't be reused after an
				   auth error */
				c.SafeDelete();

			return Result::CLOSED;

		default:
			throw SocketProtocolError{"Unexpected server reply to HandshakeResponse"};
		}
	}

	switch (cmd) {
	case Mysql::Command::EOF_:
		if (const auto duration = c.MaybeFinishQuery(); duration.count() >= 0)
			OnQueryOk(Mysql::ParseEof(payload, peer.capabilities),
				  duration);

		break;

	case Mysql::Command::OK:
		if (const auto duration = c.MaybeFinishQuery(); duration.count() >= 0)
			OnQueryOk(Mysql::ParseOk(payload, peer.capabilities),
				  duration);

		break;

	case Mysql::Command::ERR:
		if (const auto duration = c.MaybeFinishQuery(); duration.count() >= 0)
			OnQueryErr(duration);

		break;

	case Mysql::Command::QUERY:
	case Mysql::Command::INIT_DB:
	case Mysql::Command::CHANGE_USER:
	case Mysql::Command::RESET_CONNECTION:
		break;
	}

	return Result::FORWARD;
} catch (Mysql::MalformedPacket) {
	++stats.n_malformed_packets;
	fmt::print(stderr, "[{}] Malformed packet from server\n",
		   connection.GetName());
	connection.OnOutgoingError("Malformed packet from server"sv);
	return Result::CLOSED;
} catch (const SocketProtocolError &e) {
	++stats.n_malformed_packets;
	fmt::print(stderr, "[{}] {}\n", connection.GetName(), e.what());
	connection.OnOutgoingError(e.what());
	return Result::CLOSED;
} catch (...) {
	fmt::print(stderr, "[{}] {}\n", connection.GetName(), std::current_exception());
	connection.OnOutgoingError("Connection error");
	return Result::CLOSED;
}

std::pair<MysqlHandler::RawResult, std::size_t>
Connection::Outgoing::OnMysqlRaw(std::span<const std::byte> src) noexcept
{
	connection.got_raw_from_outgoing = true;

	const auto result = connection.incoming.WriteSome(src);
	if (result > 0) [[likely]] {
		if (static_cast<std::size_t>(result) < src.size())
			connection.incoming.ScheduleWrite();

		stats.n_bytes_received += result;
		return {RawResult::OK, static_cast<std::size_t>(result)};
	}

	switch (result) {
	case WRITE_BLOCKING:
		return {RawResult::OK, 0U};

	default:
		// TODO
		return {RawResult::CLOSED, 0U};
	}
}

Connection::Outgoing::Outgoing(Connection &_connection,
			       NodeStats &_stats,
			       UniqueSocketDescriptor fd) noexcept
	:connection(_connection),
	 stats(_stats),
	 peer(connection.GetEventLoop(), std::move(fd), *this, *this)
{
}

Connection::Connection(EventLoop &event_loop, Stats &_stats,
		       std::shared_ptr<LuaHandler> _handler,
		       UniqueSocketDescriptor fd,
		       SocketAddress address)
	:stats(_stats),
	 handler(std::move(_handler)),
	 auto_close(handler->GetState()),
	 lua_client(handler->GetState()),
	 defer_start_handler(event_loop, BIND_THIS_METHOD(OnDeferredStartHandler)),
	 defer_delete(event_loop, BIND_THIS_METHOD(OnDeferredDelete)),
	 incoming(event_loop, std::move(fd), *this, *this),
	 connect(event_loop, *this)
{
	++stats.n_accepted_connections;

	lua_client_ptr = LClient::New(GetLuaState(), auto_close,
				      incoming.GetSocket(), address,
				      "5.7.30"sv);
	lua_client.Set(GetLuaState(), Lua::RelativeStackIndex{-1});
	lua_pop(GetLuaState(), 1);

	StartCoroutine(InvokeLuaConnect());
}

Connection::~Connection() noexcept = default;

std::string_view
Connection::GetName() const noexcept
{
	return lua_client_ptr->GetName();
}

void
Connection::OnOutgoingError(std::string_view msg) noexcept
{
	if (incoming.SendErr(incoming.command_phase
			     ? 0
			     : incoming_handshake_response_sequence_id + 1,
			     incoming.command_phase
			     ? Mysql::ErrorCode::UNKNOWN_COM_ERROR
			     : Mysql::ErrorCode::HANDSHAKE_ERROR, "08S01"sv,
			     msg))
		SafeDelete();
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
			++stats.n_rejected_connections;

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
	++stats.n_lua_errors;
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
		++stats.n_rejected_connections;

		incoming.SendErr(sequence_id + 1,
				 Mysql::ErrorCode::HANDSHAKE_ERROR, "08S01"sv,
				 err->msg);
		SafeDelete();
	} else if (auto *c = CheckLuaConnectAction(L, -1)) {
		connect_action = std::move(*c);

		SocketAddress address = connect_action->address;
		if (connect_action->cluster) {
			connect_action->cluster->Push(L);
			AtScopeExit(L) { lua_pop(L, 1); };

			auto &cluster = Cluster::Cast(L, -1);

			/* wait until all nodes have been probed */
			co_await cluster.CoWaitReady();

			const auto p = cluster.Pick(lua_client_ptr->GetAccount(), this);
			address = p.first;
			outgoing_stats = &p.second;
		} else {
			outgoing_stats = &stats.GetNode(address);
		}

		incoming_handshake_response_sequence_id = sequence_id;

		/* connect to the outgoing server and perform the
		   handshake to it */
		fmt::print("[{}] connecting to {}\n", GetName(), address);
		connect.Connect(address,
				std::chrono::seconds{30});
	} else
		throw std::invalid_argument{"Bad return value"};
} catch (...) {
	++stats.n_lua_errors;
	fmt::print(stderr, "[{}] {}\n", GetName(), std::current_exception());

	if (incoming.SendErr(sequence_id + 1,
			     Mysql::ErrorCode::HANDSHAKE_ERROR, "08S01"sv,
			     "Lua error"sv))
		SafeDelete();
}
