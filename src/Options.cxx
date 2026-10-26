// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Options.hxx"
#include "OptionsTable.hxx"
#include "config.h"

#ifdef HAVE_OPENSSL
#include "auth/CachingSha2Password.hxx"
#endif

using std::string_view_literals::operator""sv;

void
ClusterOptions::ApplyLuaTable(lua_State *L, int table_idx)
{
	Lua::ApplyOptionsTable(L, table_idx, [this, L](std::string_view key, auto value_idx){
		if (key == "monitoring"sv)
			monitoring = Lua::CheckBool(L, value_idx,
						    "Bad `monitoring` option");
		else if (key == "user"sv)
			check.user = Lua::CheckStringView(L, value_idx,
							  "Bad 'user' value");
		else if (key == "password"sv)
			check.password = Lua::CheckStringView(L, value_idx,
							      "Bad 'password' value");
		else if (key == "allow_clear_password"sv)
			check.allow_clear_password = Lua::CheckBool(L, value_idx,
								    "Bad 'allow_clear_password' value");
		else if (key == "get_server_public_key"sv)
			check.get_server_public_key = Lua::CheckBool(L, value_idx,
								     "Bad 'get_server_public_key' value");
		else if (key == "server_public_key"sv) {
#ifdef HAVE_OPENSSL
			check.server_public_key = Lua::CheckStringView(L, value_idx,
								       "Bad 'server_public_key' value");
			Mysql::CheckCachingSha2PublicKey(check.server_public_key);
#else
			throw Lua::ArgError{"'server_public_key' requires OpenSSL"};
#endif
		} else if (key == "no_read_only"sv)
			check.no_read_only = Lua::CheckBool(L, value_idx,
							    "Bad 'no_read_only' value");
		else if (key == "disconnect_unavailable"sv)
			disconnect_unavailable = Lua::CheckBool(L, value_idx,
								"Bad `disconnect_unavailable` option");
		else
			throw Lua::ArgError{"Unknown option"};
	});

	if (monitoring) {
		if (check.user.empty() && !check.password.empty())
			throw Lua::ArgError{"'password' without 'user'"};

		if (check.user.empty() && check.no_read_only)
			throw Lua::ArgError{"'no_read_only' without 'user'"};
	} else {
		if (!check.user.empty())
			throw Lua::ArgError{"'user' without 'monitoring'"};

		if (!check.password.empty())
			throw Lua::ArgError{"'password' without 'monitoring'"};

		if (check.no_read_only)
			throw Lua::ArgError{"'no_read_only' without 'monitoring'"};

		if (disconnect_unavailable)
			throw Lua::ArgError{"'disconnect_unavailable' without 'monitoring'"};
	}
}

void
ConnectOptions::ApplyLuaTable(lua_State *L, int table_idx)
{
	Lua::ApplyOptionsTable(L, table_idx, [this, L](std::string_view key, auto value_idx){
		if (key == "read_only"sv)
			read_only = Lua::CheckBool(L, value_idx,
						   "Bad 'read_only' value");
		else
			throw Lua::ArgError{"Unknown option"};
	});
}
