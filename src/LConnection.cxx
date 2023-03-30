// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LConnection.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/Value.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/SocketAddress.hxx"
#include "util/StringAPI.hxx"

extern "C" {
#include <lauxlib.h>
}

class LConnection {
	Lua::Value address;

public:
	LConnection(lua_State *L, SocketAddress _address)
		:address(L)
	{
		Lua::NewSocketAddress(L, _address);
		address.Set(Lua::RelativeStackIndex{-1});
		lua_pop(L, 1);
	}

	int Index(lua_State *L, const char *name) const;
};

static constexpr char lua_connection_class[] = "myproxy.connection";
typedef Lua::Class<LConnection, lua_connection_class> LuaConnection;

struct lua_State;
class SocketAddress;

inline int
LConnection::Index(lua_State *L, const char *name) const
{
	if (StringIsEqual(name, "address")) {
		address.Push(L);
		return 1;
	} else
		return luaL_error(L, "Unknown attribute");
}

static int
LuaConnectionIndex(lua_State *L)
{
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	return LuaConnection::Cast(L, 1).Index(L, luaL_checkstring(L, 2));
}

void
RegisterLuaConnection(lua_State *L)
{
	using namespace Lua;

	LuaConnection::Register(L);
	SetTable(L, RelativeStackIndex{-1}, "__index", LuaConnectionIndex);
	lua_pop(L, 1);
}

void
NewLuaConnection(lua_State *L, SocketAddress address)
{
	LuaConnection::New(L, L, address);
}
