/*
 * A connection to one peer.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "Socket.hxx"
#include "MySQLReader.hxx"

struct Peer {
	Socket socket;

	struct mysql_reader reader;

	Peer(enum socket_state _state,
	     int _fd,
	     void (*read_callback)(int, short, void *),
	     void (*write_callback)(int, short, void *),
	     void *arg) noexcept
		:socket(_state, _fd,
			read_callback, write_callback, arg) {}
};

/**
 * Feed data from the input buffer into the MySQL reader.
 *
 * @return the number of bytes that should be forwarded from the
 * buffer
 */
size_t
peer_feed(Peer *peer);
