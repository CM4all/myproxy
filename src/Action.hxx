// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"

#include <string>

struct ErrAction {
	std::string msg;
};

struct ConnectAction {
	AllocatedSocketAddress address;

	std::string user, password, database;

	/**
	 * If this is non-empty, then this is used instead of
	 * #password; it contains the the SHA1 of the password
	 * (binary, not hex).
	 */
	std::string password_sha1;
};
