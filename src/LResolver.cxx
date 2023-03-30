// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LResolver.hxx"
#include "lua/Util.hxx"
#include "lua/Error.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "net/AllocatedSocketAddress.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <string.h>

static int
l_mysql_resolve(lua_State *L)
{
	if (lua_gettop(L) != 1)
		return luaL_error(L, "Invalid parameter count");

	const char *s = luaL_checkstring(L, 1);

	if (*s == '/' || *s == '@') {
		AllocatedSocketAddress address;
		address.SetLocal(s);
		Lua::NewSocketAddress(L, std::move(address));
		return 1;
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	AddressInfoList ai;

	try {
		ai = Resolve(s, 3306, &hints);
	} catch (...) {
		Lua::RaiseCurrent(L);
	}

	Lua::NewSocketAddress(L, std::move(ai.GetBest()));
	return 1;
}

void
RegisterLuaResolver(lua_State *L)
{
	Lua::SetGlobal(L, "mysql_resolve", l_mysql_resolve);
}

void
UnregisterLuaResolver(lua_State *L)
{
	Lua::SetGlobal(L, "mysql_resolve", nullptr);
}
