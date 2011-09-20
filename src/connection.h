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

    /**
     * Used to insert delay in the connection: it gets fired after the
     * delay is over.  It re-enables parsing and forwarding client
     * input.
     */
    struct event delay_timer;

    bool delayed;

    bool greeting_received, login_received;

    char user[64];

    /**
     * The time stamp of the last request packet [us].
     */
    uint64_t request_time;

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

/**
 * Delay forwarding client input for the specified duration.  Can be
 * used to throttle the connection.
 */
void
connection_delay(struct connection *c, unsigned delay_ms);

#endif
