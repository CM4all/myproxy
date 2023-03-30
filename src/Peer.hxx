// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "MysqlReader.hxx"
#include "event/net/BufferedSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"

namespace Mysql { class PacketSerializer; }

class PeerHandler {
public:
	enum ForwardResult {
		OK,
		ERROR,
		CLOSED,
	};

	virtual void OnPeerClosed() noexcept = 0;
	virtual bool OnPeerWrite() = 0;
	virtual void OnPeerError(std::exception_ptr e) noexcept = 0;
};

/*
 * A connection to one peer.
 */
struct Peer final : BufferedSocketHandler {
	BufferedSocket socket;

	MysqlReader reader;

	PeerHandler &handler;

	uint_least32_t capabilities;

	bool handshake = false, handshake_response = false, command_phase = false;

	Peer(EventLoop &event_loop,
	     UniqueSocketDescriptor fd,
	     PeerHandler &_handler,
	     MysqlHandler &_mysql_handler) noexcept
		:socket(event_loop),
		 reader(_mysql_handler),
		 handler(_handler)
	{
		socket.Init(fd.Release(), FD_TCP, std::chrono::minutes{1}, *this);
		socket.ScheduleRead();
	}

	~Peer() noexcept {
		socket.Close();
	}

	bool Send(std::span<const std::byte> src) noexcept;
	bool Send(Mysql::PacketSerializer &&s) noexcept;

	bool SendOk(uint8_t sequence_id) noexcept;

	bool SendErr(uint_least8_t sequence_id,
		     uint_least16_t error_code,
		     std::string_view sql_state, std::string_view msg) noexcept;

private:
	/* virtual methods from BufferedSocketHandler */
	BufferedResult OnBufferedData() override;
	bool OnBufferedClosed() noexcept override;
	bool OnBufferedWrite() override;
	void OnBufferedError(std::exception_ptr e) noexcept override;
};
