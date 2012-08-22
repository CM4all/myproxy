/*
 * A connection to one peer.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_PEER_HXX
#define MYPROXY_PEER_HXX

#include "Socket.hxx"
#include "MySQLReader.hxx"

struct peer {
    struct socket socket;

    struct mysql_reader reader;
};

/**
 * Feed data from the input buffer into the MySQL reader.
 *
 * @return the number of bytes that should be forwarded from the
 * buffer
 */
size_t
peer_feed(struct peer *peer);

#endif
