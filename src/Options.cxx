// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
		else
			throw Lua::ArgError{"Unknown option"};
	});
}
