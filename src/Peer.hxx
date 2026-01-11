// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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

	/**
	 * Writing to the socket has failed.
	 *
	 * @return true to forward the error condition OnPeerError(),
	 * false if this method has destroyed the #Peer
	 */
	virtual bool OnPeerBroken() noexcept {
		return true;
	}

	virtual void OnPeerError(std::exception_ptr e) noexcept = 0;
};

/**
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
	     UniqueSocketDescriptor &&fd,
	     PeerHandler &_handler,
	     MysqlHandler &_mysql_handler) noexcept
		:socket(event_loop),
		 reader(_mysql_handler),
		 handler(_handler)
	{
		socket.Init(fd.Release(), FD_TCP, std::chrono::minutes{1}, *this);
		socket.ScheduleRead();
	}

	void Close() noexcept {
		socket.Close();
		socket.Destroy();
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

	void ScheduleWrite() noexcept {
		socket.ScheduleWrite();
	}

	void DeferWrite() noexcept {
		socket.DeferWrite();
	}

	/**
	 * Send data, allowing partial writes.
	 *
	 * @return the number of bytes sent; 0 if nothing could be
	 * sent; #WRITE_DESTROYED if there was an error and the #Peer
	 * instance has been destroyed inside this method
	 */
	ssize_t SendSome(std::span<const std::byte> src) noexcept;

	/**
	 * Send data.  A partial write is considered an error that
	 * will cause the #Peer instance to be destroyed.
	 *
	 * @return true on success, false on error (the #Peer instance
	 * has been destroyed inside this method)
	 */
	bool Send(std::span<const std::byte> src) noexcept;

	/**
	 * Finish the specified packet and send it.
	 */
	bool Send(Mysql::PacketSerializer &&s) noexcept;

	/**
	 * Shortcut that sends an "OK" packet.
	 */
	bool SendOk(uint_least8_t sequence_id) noexcept;

	/**
	 * Shortcut that sends an "ERR" packet.
	 */
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
	enum write_result OnBufferedBroken() override;
	void OnBufferedError(std::exception_ptr e) noexcept override;
};
