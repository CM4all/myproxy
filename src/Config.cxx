/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Config.hxx"

#include <netdb.h>

Config::~Config() noexcept
{
	if (server_address != nullptr)
		freeaddrinfo(server_address);
}
