// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "lua/ForEach.hxx"
#include "lua/StringView.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <concepts>

namespace Lua {

struct ArgError { const char *extramsg; };

inline const char *
CheckString(lua_State *L, auto _idx, const char *extramsg)
{
	const int idx = GetStackIndex(_idx);
	if (!lua_isstring(L, idx))
		throw ArgError{extramsg};

	return lua_tostring(L, idx);
}

inline std::string_view
CheckStringView(lua_State *L, auto _idx, const char *extramsg)
{
	const int idx = GetStackIndex(_idx);
	if (!lua_isstring(L, idx))
		throw ArgError{extramsg};

	return ToStringView(L, idx);
}

inline void
ApplyOptionsTable(lua_State *L, int table_idx,
		  std::invocable<const char *, RelativeStackIndex> auto f)
try {
	ForEach(L, table_idx, [L, &f](auto key_idx, auto value_idx){
		const char *key = CheckString(L, key_idx, "Bad key type");
		f(key, value_idx);
	});
} catch (ArgError e) {
	luaL_argerror(L, table_idx, e.extramsg);
}

} // namespace Lua
