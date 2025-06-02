// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Options.hxx"
#include "OptionsTable.hxx"

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
		else if (key == "no_read_only"sv)
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
