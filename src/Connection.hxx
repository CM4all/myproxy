// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Peer.hxx"
#include "MysqlHandler.hxx"
#include "event/net/ConnectSocket.hxx"
#include "util/IntrusiveList.hxx"

#include <optional>
#include <string>

struct Instance;

/**
 * Manage connections from MySQL clients.
 */
struct Connection final
	: IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK>,
	  PeerHandler, MysqlHandler,
	  ConnectSocketHandler
{
	Instance *const instance;

	/**
	 * Used to insert delay in the connection: it gets fired after the
	 * delay is over.  It re-enables parsing and forwarding client
	 * input.
	 */
	CoarseTimerEvent delay_timer;

	bool delayed = false;

	bool greeting_received = false, login_received = false;

	std::string user;

	/**
	 * The time stamp of the last request packet [us].
	 */
	Event::TimePoint request_time;

	/**
	 * The connection to the client.
	 */
	Peer incoming;

	ConnectSocket connect;

	/**
	 * The connection to the server.
	 */
	struct Outgoing final : PeerHandler, MysqlHandler {
		Connection &connection;

		Peer peer;

		Outgoing(Connection &_connection,
			 UniqueSocketDescriptor fd) noexcept;

		/* virtual methods from PeerSocketHandler */
		std::pair<ForwardResult, std::size_t> OnPeerForward(std::span<const std::byte> src) override;
		void OnPeerClosed() noexcept override;
		bool OnPeerWrite() override;
		void OnPeerError(std::exception_ptr e) noexcept override;

		/* virtual methods from MysqlHandler */
		void OnMysqlPacket(unsigned number, size_t length,
				   const void *data, size_t available) override;
	};

	std::optional<Outgoing> outgoing;

	Connection(Instance &_instance, UniqueSocketDescriptor fd,
		   SocketAddress address);
	~Connection() noexcept;

	auto &GetEventLoop() const noexcept {
		return delay_timer.GetEventLoop();
	}

	void Delay(Event::Duration duration) noexcept;

private:
	bool OnLoginPacket(const char *data, size_t length);

	/**
	 * Called when the artificial delay is over, and restarts the
	 * transfer from the client to the server->
	 */
	void OnDelayTimer() noexcept;

	/* virtual methods from PeerSocketHandler */
	std::pair<ForwardResult, std::size_t> OnPeerForward(std::span<const std::byte> src) override;
	void OnPeerClosed() noexcept override;
	bool OnPeerWrite() override;
	void OnPeerError(std::exception_ptr e) noexcept override;

	/* virtual methods from MysqlHandler */
	void OnMysqlPacket(unsigned number, size_t length,
			   const void *data, size_t available) override;

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
