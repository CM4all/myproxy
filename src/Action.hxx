// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Options.hxx"
#include "lua/ValuePtr.hxx"
#include "net/AllocatedSocketAddress.hxx"

#include <string>

struct ErrAction {
	std::string msg;
};

struct ConnectAction {
	// either #address or #cluster is set

	AllocatedSocketAddress address;

	Lua::ValuePtr cluster;

	std::string user, password, database;

	/**
	 * If this is non-empty, then this is used instead of
	 * #password; it contains the the SHA1 of the password
	 * (binary, not hex).
	 */
	std::string password_sha1;

	ConnectOptions options;
};
