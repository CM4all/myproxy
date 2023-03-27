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
};

/**
 * Feed data from the input buffer into the MySQL reader.
 *
 * @return the number of bytes that should be forwarded from the
 * buffer
 */
size_t
peer_feed(Peer *peer);
