// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Action.hxx"
#include "Peer.hxx"
#include "MysqlHandler.hxx"
#include "NodeObserver.hxx"
#include "lua/AutoCloseList.hxx"
#include "lua/Value.hxx"
#include "co/InvokeTask.hxx"
#include "event/DeferEvent.hxx"
#include "event/net/ConnectSocket.hxx"
#include "util/IntrusiveList.hxx"

#include <memory>
#include <optional>
#include <string>

struct Stats;
struct NodeStats;
class LuaHandler;
class LClient;

namespace Mysql {
class AuthHandler;
struct OkPacket;
}

/**
 * Manage connections from MySQL clients.
 */
class Connection final
	: public IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK>,
	  ClusterNodeObserver,
	  PeerHandler, MysqlHandler,
	  ConnectSocketHandler
{
	Stats &stats;
	NodeStats *outgoing_stats;

	const std::shared_ptr<LuaHandler> handler;

	Lua::AutoCloseList auto_close;

	Lua::Value lua_client;

	LClient *lua_client_ptr;

	/**
	 * The C++20 coroutine that currently executes the handler.
	 */
	Co::InvokeTask coroutine;

	/**
	 * Launches the Lua handler.
	 */
	DeferEvent defer_start_handler;

	/**
	 * Move the destruction into a safe stack frame.
	 */
	DeferEvent defer_delete;

	std::string user, password, database;

	/**
	 * The time stamp of the last request packet [us].
	 */
	Event::TimePoint request_time;

	/**
	 * The connection to the client.
	 */
	Peer incoming;

	std::optional<ConnectAction> connect_action;

	ConnectSocket connect;

	/**
	 * The connection to the server.
	 */
	class Outgoing final : PeerHandler, MysqlHandler {
		Connection &connection;

		NodeStats &stats;

	public:
		Peer peer;

	private:
		std::unique_ptr<Mysql::AuthHandler> auth_handler;

	public:
		Outgoing(Connection &_connection,
			 NodeStats &_stats,
			 UniqueSocketDescriptor fd) noexcept;
		~Outgoing() noexcept;

	private:
		Result OnHandshake(uint_least8_t sequence_id,
				   std::span<const std::byte> payload);

		Result OnAuthSwitchRequest(uint_least8_t sequence_id,
					   std::span<const std::byte> payload);

		void OnQueryOk(const Mysql::OkPacket &packet,
			       Event::Duration duration) noexcept;

		void OnQueryErr(Event::Duration duration) noexcept;

		/* virtual methods from PeerSocketHandler */
		void OnPeerClosed() noexcept override;
		WriteResult OnPeerWrite() override;
		void OnPeerError(std::exception_ptr e) noexcept override;

		/* virtual methods from MysqlHandler */
		Result OnMysqlPacket(unsigned number, std::span<const std::byte> payload,
				     bool complete) noexcept override;
		std::pair<RawResult, std::size_t> OnMysqlRaw(std::span<const std::byte> src) noexcept override;
	};

	std::optional<Outgoing> outgoing;

	/**
	 * The sequence_id of the HandshakeResponse packet received on
	 * the incoming connection.  This will be used to translate
	 * forwarded responses.
	 */
	uint_least8_t incoming_handshake_response_sequence_id;

	bool got_raw_from_incoming, got_raw_from_outgoing;

public:
	Connection(EventLoop &event_loop, Stats &_stats,
		   std::shared_ptr<LuaHandler> _handler,
		   UniqueSocketDescriptor fd,
		   SocketAddress address);
	~Connection() noexcept;

	[[gnu::const]]
	auto &GetEventLoop() const noexcept {
		return defer_start_handler.GetEventLoop();
	}

	[[gnu::const]]
	auto *GetLuaState() const noexcept {
		return lua_client.GetState();
	}

	[[gnu::pure]]
	std::string_view GetName() const noexcept;

private:
	bool IsStale() const noexcept {
		return defer_delete.IsPending();
	}

	void SafeDelete() noexcept {
		defer_start_handler.Cancel();
		incoming.Close();

		if (connect.IsPending())
			connect.Cancel();

		defer_delete.Schedule();
		outgoing.reset();
	}

	/**
	 * The outgoing connection has failed.  Send an error to the
	 * incoming client and close the connection.  After returning,
	 * the #Connection object has been destroyed.
	 */
	void OnOutgoingError(std::string_view msg) noexcept;

	bool IsDelayed() const noexcept {
		return coroutine;
	}

	Co::InvokeTask InvokeLuaConnect();
	Co::InvokeTask InvokeLuaHandshakeResponse(uint_least8_t sequence_id) noexcept;
	Co::InvokeTask InvokeLuaCommandPhase();

	Result OnHandshakeResponse(uint_least8_t sequence_id,
				   std::span<const std::byte> payload);

	Result OnInitDb(uint_least8_t sequence_id,
			std::span<const std::byte> payload);

	Result OnChangeUser(uint_least8_t sequence_id,
			    std::span<const std::byte> payload);

	void OnDeferredDelete() noexcept {
		delete this;
	}

	void OnDeferredStartHandler() noexcept;

	void OnCoroutineComplete(std::exception_ptr error) noexcept;
	void StartCoroutine(Co::InvokeTask &&_coroutine) noexcept;

	/**
	 * Finish the stopwatch for the current query and return its
	 * duration.  If no query was in progress, return a negative
	 * duration.
	 */
	Event::Duration MaybeFinishQuery() noexcept;

	/* virtual methods from ClusterNodeObserver */
	void OnClusterNodeUnavailable() noexcept override;

	/* virtual methods from PeerSocketHandler */
	void OnPeerClosed() noexcept override;
	WriteResult OnPeerWrite() override;
	bool OnPeerBroken() noexcept override;
	void OnPeerError(std::exception_ptr e) noexcept override;

	/* virtual methods from MysqlHandler */
	Result OnMysqlPacket(unsigned number, std::span<const std::byte> payload,
			     bool complete) noexcept override;
	std::pair<RawResult, std::size_t> OnMysqlRaw(std::span<const std::byte> src) noexcept override;

	/* virtual methods from ConnectSocketHandler */
	void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override;
	void OnSocketConnectError(std::exception_ptr e) noexcept override;
};

/**
 * Delay forwarding client input for the specified duration.  Can be
 * used to throttle the connection.
 */
void
connection_delay(Connection *c, unsigned delay_ms);
