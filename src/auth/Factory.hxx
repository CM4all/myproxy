// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <memory>
#include <string_view>

namespace Mysql {

class AuthHandler;

struct AuthFactoryOptions {
	/**
	 * Is the auth method "mysql_clear_password" allowed?  This
	 * means that we tell the backend server the clear-text
	 * password.
	 */
	bool allow_clear_password = true;

	/**
	 * Is "caching_sha2_password" allowed to request the server's
	 * RSA public key for full authentication?  This key is not
	 * authenticated, which means that an attacker answering for
	 * the server can obtain the clear-text password.
	 */
	bool get_server_public_key = false;

	/**
	 * The trusted RSA public key (PEM) of the server for
	 * "caching_sha2_password" full authentication; if not empty,
	 * it is used instead of requesting the key from the server.
	 * The referenced memory must remain valid as long as the
	 * #AuthHandler is used.
	 */
	std::string_view server_public_key;

	bool strict = true;
};

std::unique_ptr<AuthHandler>
MakeAuthHandler(std::string_view plugin_name, AuthFactoryOptions options) noexcept;

} // namespace Mysql
