// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Socket.hxx"
#include "MysqlReader.hxx"

/*
 * A connection to one peer.
 */
struct Peer {
	Socket socket;

	MysqlReader reader;

	Peer(enum socket_state _state,
	     UniqueSocketDescriptor _fd,
	     void (*read_callback)(int, short, void *),
	     void (*write_callback)(int, short, void *),
	     void *arg,
	     const MysqlHandler &_handler, void *_ctx) noexcept
		:socket(_state, std::move(_fd),
			read_callback, write_callback, arg),
		 reader(_handler, _ctx) {}
};

/**
 * Feed data from the input buffer into the MySQL reader.
 *
 * @return the number of bytes that should be forwarded from the
 * buffer
 */
size_t
peer_feed(Peer *peer);
