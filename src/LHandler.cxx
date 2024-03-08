// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LHandler.hxx"
#include "lua/Assert.hxx"
#include "lua/ForEach.hxx"
#include "util/StringAPI.hxx"

#include <stdexcept>

LuaHandler::LuaHandler(lua_State *L, Lua::StackIndex idx)
	:on_connect(L),
	 on_handshake_response(L),
	 on_command_phase(L)
{
	const Lua::ScopeCheckStack check_stack{L};

	if (!lua_istable(L, Lua::GetStackIndex(idx)))
		throw std::invalid_argument{"Not a table"};

	bool have_on_handshake_response = false;

	Lua::ForEach(L, idx, [this, L, &have_on_handshake_response](auto key_idx, auto value_idx){
		if (lua_type(L, Lua::GetStackIndex(key_idx)) != LUA_TSTRING)
			throw std::invalid_argument{"Key is not a string"};

		const char *key = lua_tostring(L, Lua::GetStackIndex(key_idx));
		if (StringIsEqual(key, "on_connect")) {
			if (!lua_isfunction(L, Lua::GetStackIndex(value_idx)))
				throw std::invalid_argument{"Value is not a function"};

			on_connect.Set(L, value_idx);
		} else if (StringIsEqual(key, "on_handshake_response")) {
			if (!lua_isfunction(L, Lua::GetStackIndex(value_idx)))
				throw std::invalid_argument{"Value is not a function"};

			on_handshake_response.Set(L, value_idx);
			have_on_handshake_response = true;
		} else if (StringIsEqual(key, "on_command_phase")) {
			if (!lua_isfunction(L, Lua::GetStackIndex(value_idx)))
				throw std::invalid_argument{"Value is not a function"};

			on_command_phase.Set(L, value_idx);
		} else
			throw std::invalid_argument{"Unrecognized key"};
	});

	if (!have_on_handshake_response)
		throw std::invalid_argument{"No on_handshake_response handler"};
}
