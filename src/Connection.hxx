// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Action.hxx"
#include "Peer.hxx"
#include "MysqlHandler.hxx"
#include "lua/Ref.hxx"
#include "lua/Resume.hxx"
#include "lua/Value.hxx"
#include "lua/ValuePtr.hxx"
#include "lua/CoRunner.hxx"
#include "event/DeferEvent.hxx"
#include "event/net/ConnectSocket.hxx"
#include "util/IntrusiveList.hxx"

#include <optional>
#include <string>

/**
 * Manage connections from MySQL clients.
 */
class Connection final
	: public IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK>,
	  Lua::ResumeListener,
	  PeerHandler, MysqlHandler,
	  ConnectSocketHandler
{
	const Lua::ValuePtr handler;

	Lua::Value lua_client;

	/**
	 * The Lua thread which runs the handler coroutine.
	 */
	Lua::CoRunner thread;

	/**
	 * Launches the Lua handler.
	 */
	DeferEvent defer_start_handler;

	/**
	 * Used to insert delay in the connection: it gets fired after the
	 * delay is over.  It re-enables parsing and forwarding client
	 * input.
	 */
	CoarseTimerEvent delay_timer;

	std::string username, auth_response, database;

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
		bool OnPeerWrite() override;
		void OnPeerError(std::exception_ptr e) noexcept override;

		/* virtual methods from MysqlHandler */
		Result OnMysqlPacket(unsigned number, std::span<const std::byte> payload,
				     bool complete) noexcept override;
		std::pair<Result, std::size_t> OnMysqlRaw(std::span<const std::byte> src) noexcept override;
	};

	std::optional<Outgoing> outgoing;

	uint_least8_t handeshake_response_sequence_id;

public:
	Connection(EventLoop &event_loop, Lua::ValuePtr _handler,
		   UniqueSocketDescriptor fd,
		   SocketAddress address);
	~Connection() noexcept;

	auto &GetEventLoop() const noexcept {
		return delay_timer.GetEventLoop();
	}

private:
	bool IsDelayed() const noexcept {
		return delay_timer.IsPending();
	}

	void Delay(Event::Duration duration) noexcept;

	MysqlHandler::Result OnHandshakeResponse(uint_least8_t sequence_id,
						 std::span<const std::byte> payload);

	void OnDeferredStartHandler() noexcept;

	/**
	 * Called when the artificial delay is over, and restarts the
	 * transfer from the client to the server->
	 */
	void OnDelayTimer() noexcept;

	/* virtual methods from PeerSocketHandler */
	void OnPeerClosed() noexcept override;
	bool OnPeerWrite() override;
	void OnPeerError(std::exception_ptr e) noexcept override;

	/* virtual methods from MysqlHandler */
	Result OnMysqlPacket(unsigned number, std::span<const std::byte> payload,
			     bool complete) noexcept override;
	std::pair<Result, std::size_t> OnMysqlRaw(std::span<const std::byte> src) noexcept override;

	/* virtual methods from ConnectSocketHandler */
	void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override;
	void OnSocketConnectError(std::exception_ptr e) noexcept override;

	/* virtual methods from class Lua::ResumeListener */
	void OnLuaFinished(lua_State *L) noexcept override;
	void OnLuaError(lua_State *L, std::exception_ptr e) noexcept override;
};

/**
 * Delay forwarding client input for the specified duration.  Can be
 * used to throttle the connection.
 */
void
connection_delay(Connection *c, unsigned delay_ms);
