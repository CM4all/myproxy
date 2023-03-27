// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "MysqlReader.hxx"
#include "event/net/BufferedSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"

class PeerHandler {
public:
	enum ForwardResult {
		OK,
		ERROR,
		CLOSED,
	};

	virtual std::pair<ForwardResult, std::size_t> OnPeerForward(std::span<std::byte> src) = 0;
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

	std::pair<PeerHandler::ForwardResult, std::size_t> Forward(std::span<std::byte> src) noexcept;

private:
	/* virtual methods from BufferedSocketHandler */
	BufferedResult OnBufferedData() override;
	bool OnBufferedClosed() noexcept override;
	bool OnBufferedWrite() override;
	void OnBufferedError(std::exception_ptr e) noexcept override;
};
