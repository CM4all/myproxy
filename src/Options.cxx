// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Options.hxx"
#include "OptionsTable.hxx"
#include "util/StringAPI.hxx"

void
ClusterOptions::ApplyLuaTable(lua_State *L, int table_idx)
{
	Lua::ApplyOptionsTable(L, table_idx, [this, L](const char *key, auto value_idx){
		if (StringIsEqual(key, "monitoring"))
			monitoring = Lua::CheckBool(L, value_idx,
						    "Bad `monitoring` option");
		else if (StringIsEqual(key, "user"))
			check.user = Lua::CheckStringView(L, value_idx,
							  "Bad 'user' value");
		else if (StringIsEqual(key, "password"))
			check.password = Lua::CheckStringView(L, value_idx,
							      "Bad 'password' value");
		else if (StringIsEqual(key, "no_read_only"))
			check.no_read_only = Lua::CheckBool(L, value_idx,
							    "Bad 'no_read_only' value");
		else if (StringIsEqual(key, "disconnect_unavailable"))
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
