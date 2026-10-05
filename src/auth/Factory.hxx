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

	bool strict = true;
};

std::unique_ptr<AuthHandler>
MakeAuthHandler(std::string_view plugin_name, AuthFactoryOptions options) noexcept;

} // namespace Mysql
