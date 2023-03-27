/*
 * A connection to one peer.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Peer.hxx"
#include "fifo_buffer.h"

size_t
peer_feed(Peer *peer)
{
    size_t length;
    const void *data = fifo_buffer_read(peer->socket.input, &length);
    if (data == NULL)
        return 0;

    size_t nbytes = mysql_reader_feed(&peer->reader, data, length);
    assert(nbytes > 0);
    return nbytes;
}
