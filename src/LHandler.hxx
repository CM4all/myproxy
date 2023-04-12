// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "lua/Value.hxx"

class LuaHandler {
	Lua::Value on_connect;
	Lua::Value on_handshake_response;

public:
	LuaHandler(lua_State *L, Lua::StackIndex idx);

	lua_State *GetState() const noexcept {
		return on_handshake_response.GetState();
	}

	void PushOnConnect(lua_State *L) {
		on_connect.Push(L);
	}

	void PushOnHandshakeResponse(lua_State *L) {
		on_handshake_response.Push(L);
	}
};
