/*
 * Manage connections from MySQL clients.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_CONNECTION_H
#define MYPROXY_CONNECTION_H

#include "peer.h"

#include <inline/list.h>

#include <stdbool.h>

struct config;

struct connection {
    struct list_head siblings;
    struct instance *instance;

    bool greeting_received, login_received;

    char user[64];

    /**
     * The connection to the client.
     */
    struct peer client;

    /**
     * The connection to the server.
     */
    struct peer server;
};

struct connection *
connection_new(struct instance *instance, int fd);

void
connection_close(struct connection *connection);

#endif
