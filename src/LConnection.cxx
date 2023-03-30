// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LConnection.hxx"
#include "CgroupProc.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/Value.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketDescriptor.hxx"
#include "util/StringAPI.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <sys/socket.h> // for struct ucred

class LConnection {
	Lua::Value address;

	const struct ucred peer_cred;

public:
	LConnection(lua_State *L, SocketDescriptor socket,
		    SocketAddress _address)
		:address(L),
		 peer_cred(socket.GetPeerCredentials())
	{
		Lua::NewSocketAddress(L, _address);
		address.Set(Lua::RelativeStackIndex{-1});
		lua_pop(L, 1);
	}

	int Index(lua_State *L, const char *name) const;

	bool HavePeerCred() const noexcept {
		return peer_cred.pid >= 0;
	}
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
	} else if (StringIsEqual(name, "pid")) {
		if (!HavePeerCred())
			return 0;

		Lua::Push(L, static_cast<lua_Integer>(peer_cred.pid));
		return 1;
	} else if (StringIsEqual(name, "uid")) {
		if (!HavePeerCred())
			return 0;

		Lua::Push(L, static_cast<lua_Integer>(peer_cred.uid));
		return 1;
	} else if (StringIsEqual(name, "gid")) {
		if (!HavePeerCred())
			return 0;

		Lua::Push(L, static_cast<lua_Integer>(peer_cred.gid));
		return 1;
	} else if (StringIsEqual(name, "cgroup")) {
		if (!HavePeerCred())
			return 0;

		const auto path = ReadProcessCgroup(peer_cred.pid, "");
		if (path.empty())
			return 0;

		Lua::Push(L, path);
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
NewLuaConnection(lua_State *L, SocketDescriptor socket, SocketAddress address)
{
	LuaConnection::New(L, L, socket, address);
}
