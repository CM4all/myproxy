/*
 * A connection to one peer.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_PEER_H
#define MYPROXY_PEER_H

#include "socket.h"

struct peer {
    struct socket socket;
};

#endif
