// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LHandler.hxx"

#include <stdexcept>

LuaHandler::LuaHandler(lua_State *L, Lua::StackIndex idx)
	:on_handshake_response(L, idx)
{
	if (!lua_isfunction(L, Lua::GetStackIndex(idx)))
		throw std::invalid_argument{"Not a function"};
}
