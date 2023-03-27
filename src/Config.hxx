/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

struct Config {
	struct addrinfo *server_address = nullptr;

	~Config() noexcept;
};
