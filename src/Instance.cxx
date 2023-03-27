/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"

#include <cstddef>

#include <netdb.h>

void
instance_init(Instance *instance)
{
	instance->event_base = event_init();
	instance->server_address = NULL;

	list_init(&instance->connections);
}

void
instance_deinit(Instance *instance)
{
	event_base_free(instance->event_base);

	if (instance->server_address != NULL)
		freeaddrinfo(instance->server_address);
}

