// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "MysqlReader.hxx"
#include "event/net/BufferedSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <cstdint>
#include <string_view>

namespace Mysql {
enum class ErrorCode : uint_least16_t;
class PacketSerializer;
}

class PeerHandler {
public:
	enum class WriteResult {
		/**
		 * Done writing; there is no more data to be written.
		 */
		DONE,

		/**
		 * The handler wants to write more.
		 */
		MORE,

		CLOSED,
	};

	virtual void OnPeerClosed() noexcept = 0;
	virtual WriteResult OnPeerWrite() = 0;
	virtual void OnPeerError(std::exception_ptr e) noexcept = 0;
};

/*
 * A connection to one peer.
 */
class Peer final : BufferedSocketHandler {
	BufferedSocket socket;

	MysqlReader reader;

	PeerHandler &handler;

public:
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
		socket.Destroy();
	}

	void Close() noexcept {
		socket.Close();
	}

	SocketDescriptor GetSocket() const noexcept {
		return socket.GetSocket();
	}

	BufferedReadResult Read() noexcept {
		return socket.Read();
	}

	void DeferRead() noexcept {
		socket.DeferRead();
	}

	void UnscheduleRead() noexcept {
		socket.UnscheduleRead();
	}

	void DeferWrite() noexcept {
		socket.DeferWrite();
	}

	ssize_t WriteSome(std::span<const std::byte> src) noexcept {
		return socket.Write(src.data(), src.size());
	}

	bool Send(std::span<const std::byte> src) noexcept;
	bool Send(Mysql::PacketSerializer &&s) noexcept;

	bool SendOk(uint8_t sequence_id) noexcept;

	bool SendErr(uint_least8_t sequence_id, Mysql::ErrorCode error_code,
		     std::string_view sql_state, std::string_view msg) noexcept;

	auto Flush() noexcept {
		return reader.Flush(socket);
	}

private:
	/* virtual methods from BufferedSocketHandler */
	BufferedResult OnBufferedData() override;
	bool OnBufferedClosed() noexcept override;
	bool OnBufferedWrite() override;
	void OnBufferedError(std::exception_ptr e) noexcept override;
};
