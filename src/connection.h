/*
 * Manage connections from MySQL clients.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_CONNECTION_H
#define MYPROXY_CONNECTION_H

#include "socket.h"

#include <inline/list.h>

#include <stdbool.h>

struct config;

struct connection {
    struct list_head siblings;
    struct instance *instance;

    /**
     * The connection to the client.
     */
    struct socket client;

    /**
     * The connection to the server.
     */
    struct socket server;
};

struct connection *
connection_new(struct instance *instance, int fd);

void
connection_close(struct connection *connection);

#endif
