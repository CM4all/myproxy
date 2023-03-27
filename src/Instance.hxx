/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_INSTANCE_HXX
#define MYPROXY_INSTANCE_HXX

#include <event.h>
#include <stdbool.h>

#include <inline/list.h>

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
