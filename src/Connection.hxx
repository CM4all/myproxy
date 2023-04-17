// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Action.hxx"
#include "Peer.hxx"
#include "MysqlHandler.hxx"
#include "lua/Value.hxx"
#include "co/InvokeTask.hxx"
#include "event/DeferEvent.hxx"
#include "event/net/ConnectSocket.hxx"
#include "util/IntrusiveList.hxx"

#include <memory>
#include <optional>
#include <string>

class LuaHandler;
class LClient;

/**
 * Manage connections from MySQL clients.
 */
class Connection final
	: public IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK>,
	  PeerHandler, MysqlHandler,
	  ConnectSocketHandler
{
	const std::shared_ptr<LuaHandler> handler;

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

	std::string user, auth_response, database;

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
	struct Outgoing final : PeerHandler, MysqlHandler {
		Connection &connection;

		Peer peer;

		Outgoing(Connection &_connection,
			 UniqueSocketDescriptor fd) noexcept;

		Result OnHandshake(std::span<const std::byte> payload);

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

	bool got_raw_from_incoming, got_raw_from_outgoing;

public:
	Connection(EventLoop &event_loop,
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

	bool IsDelayed() const noexcept {
		return coroutine;
	}

	Co::InvokeTask InvokeLuaConnect();
	Co::InvokeTask InvokeLuaHandshakeResponse(uint_least8_t sequence_id) noexcept;

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

	/* virtual methods from PeerSocketHandler */
	void OnPeerClosed() noexcept override;
	WriteResult OnPeerWrite() override;
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
