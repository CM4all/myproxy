// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Peer.hxx"

size_t
peer_feed(Peer *peer)
{
	const auto r = peer->socket.input.Read();
	if (r.empty())
		return 0;

	size_t nbytes = mysql_reader_feed(&peer->reader, r.data(), r.size());
	assert(nbytes > 0);
	return nbytes;
}
