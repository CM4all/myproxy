/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"

#include <stddef.h>
#include <netdb.h>

void
instance_init(struct instance *instance)
{
    instance->event_base = event_init();
    instance->server_address = NULL;

    list_init(&instance->connections);
}

void
instance_deinit(struct instance *instance)
{
    event_base_free(instance->event_base);

    if (instance->server_address != NULL)
        freeaddrinfo(instance->server_address);
}

