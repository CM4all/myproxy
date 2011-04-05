/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_INSTANCE_H
#define MYPROXY_INSTANCE_H

#include <inline/list.h>

#include <event.h>
#include <stdbool.h>

struct addrinfo;

struct instance {
    struct event_base *event_base;

    struct addrinfo *server_address;

    bool should_exit;
    struct event sigterm_event, sigint_event, sigquit_event;

    int listener_socket;
    struct event listener_event;

    struct list_head connections;
};

void
instance_init(struct instance *instance);

void
instance_deinit(struct instance *instance);

#endif
