// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Factory.hxx"
#include "ClearPassword.hxx"
#include "Ed25519.hxx"
#include "NativePassword.hxx"
#include "config.h"

#ifdef HAVE_OPENSSL
#include "CachingSha2Password.hxx"
#endif

using std::string_view_literals::operator""sv;

namespace Mysql {

std::unique_ptr<AuthHandler>
MakeAuthHandler(std::string_view plugin_name, bool strict) noexcept
{
	if (plugin_name == "mysql_clear_password"sv)
		return std::make_unique<ClearPassword>();
	else if (plugin_name == "client_ed25519"sv)
		return std::make_unique<ClientEd25519>();
#ifdef HAVE_OPENSSL
	else if (plugin_name == "caching_sha2_password"sv)
		return std::make_unique<CachingSha2Password>();
#endif // HAVE_OPENSSL
	else if (!strict || plugin_name == "mysql_native_password"sv)
		return std::make_unique<NativePassword>();
	else
		return {};
}

} // namespace Mysql
