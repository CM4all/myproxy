// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "lua/Value.hxx"

#include <sys/socket.h> // for struct ucred

struct lua_State;
class SocketAddress;
class SocketDescriptor;

class LClient {
	Lua::Value address;

	/**
	 * A table containing notes created by Lua code.
	 */
	Lua::Value notes;

	const struct ucred peer_cred;

public:
	LClient(lua_State *L, SocketDescriptor socket,
		SocketAddress _address);

	int Index(lua_State *L, const char *name);

	bool HavePeerCred() const noexcept {
		return peer_cred.pid >= 0;
	}
};

void
RegisterLuaClient(lua_State *L);

void
NewLuaClient(lua_State *L, SocketDescriptor socket, SocketAddress address);
