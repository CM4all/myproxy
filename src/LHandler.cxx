// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LHandler.hxx"

LuaHandler::LuaHandler(lua_State *L, Lua::StackIndex idx)
	:on_handshake_response(L, idx)
{
}
