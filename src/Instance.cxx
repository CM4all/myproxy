/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Connection.hxx"

#include <cstddef>

#include <netdb.h>

Instance::Instance()
{
}

Instance::~Instance() noexcept
{
	event_base_free(event_base);

	if (server_address != NULL)
		freeaddrinfo(server_address);
}
