/*
 * Global declarations.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/AllocatedSocketAddress.hxx"

struct Config {
	AllocatedSocketAddress server_address;
};
