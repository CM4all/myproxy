// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LClient.hxx"
#include "Action.hxx"
#include "LAction.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/ForEach.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketDescriptor.hxx"
#include "net/ToString.hxx"
#include "io/linux/ProcCgroup.hxx"
#include "util/StringAPI.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <fmt/core.h>

[[gnu::pure]]
static std::string
MakeClientName(SocketAddress address, const struct ucred &cred) noexcept
{
	if (cred.pid >= 0)
		return fmt::format("pid={} uid={}", cred.pid, cred.uid);

	char buffer[256];
	if (ToString(buffer, sizeof(buffer), address))
		return buffer;

	return "?";
}

inline
LClient::LClient(lua_State *L, SocketDescriptor socket,
		 SocketAddress _address,
		 std::string_view _server_version)
	:address(L),
	 server_version(_server_version),
	 notes(L),
	 peer_cred(socket.GetPeerCredentials()),
	 name_(MakeClientName(_address, peer_cred))
{
	Lua::NewSocketAddress(L, _address);
	address.Set(L, Lua::RelativeStackIndex{-1});
	lua_pop(L, 1);
}

static constexpr char lua_client_class[] = "myproxy.client";
typedef Lua::Class<LClient, lua_client_class> LuaClient;

static int
NewErrAction(lua_State *L)
{
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	NewLuaErrAction(L, {
		.msg = luaL_checkstring(L, 2),
	});
	return 1;
}

struct ArgError { const char *extramsg; };

static const char *
CheckString(lua_State *L, auto _idx, const char *extramsg)
{
	const int idx = Lua::GetStackIndex(_idx);
	if (!lua_isstring(L, idx))
		throw ArgError{extramsg};

	return lua_tostring(L, idx);
}

static void
Apply(lua_State *L, ConnectAction &action, const char *name, auto value_idx)
{
	if (StringIsEqual(name, "user"))
		action.user = CheckString(L, value_idx, "Bad value type");
	else if (StringIsEqual(name, "password"))
		action.password = CheckString(L, value_idx, "Bad value type");
	else if (StringIsEqual(name, "database"))
		action.database = CheckString(L, value_idx, "Bad value type");
	else
		throw ArgError{"Unknown attribute"};
}

static int
NewConnectAction(lua_State *L)
try {
	if (lua_gettop(L) != 3)
		return luaL_error(L, "Invalid parameters");

	ConnectAction action;

	action.address = Lua::ToSocketAddress(L, 2, 3306);

	try {
		Lua::ForEach(L, 3, [L, &action](auto key_idx, auto value_idx){
			const char *key = CheckString(L, key_idx, "Bad key type");
			Apply(L, action, key, value_idx);
		});
	} catch (ArgError e) {
		luaL_argerror(L, 3, e.extramsg);
	}

	NewLuaConnectAction(L, std::move(action));
	return 1;
} catch (...) {
	Lua::RaiseCurrent(L);
}

static constexpr struct luaL_Reg client_methods [] = {
	{"err", NewErrAction},
	{"connect", NewConnectAction},
	{nullptr, nullptr}
};

inline int
LClient::Index(lua_State *L, const char *name)
{
	for (const auto *i = client_methods; i->name != nullptr; ++i) {
		if (StringIsEqual(i->name, name)) {
			Lua::Push(L, i->func);
			return 1;
		}
	}

	if (StringIsEqual(name, "address")) {
		address.Push(L);
		return 1;
	} else if (StringIsEqual(name, "notes")) {
		notes.Push(L);

		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			lua_newtable(L);
			notes.Set(L, Lua::RelativeStackIndex{-1});
		}

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
	} else if (StringIsEqual(name, "server_version")) {
		Lua::Push(L, server_version);
		return 1;
	} else
		return luaL_error(L, "Unknown attribute");
}

int
LClient::_Index(lua_State *L)
{
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	return LuaClient::Cast(L, 1).Index(L, luaL_checkstring(L, 2));
}

inline int
LClient::NewIndex(lua_State *L, const char *name, int value_idx)
{
	if (StringIsEqual(name, "server_version")) {
		const char *new_value = luaL_checkstring(L, value_idx);
		luaL_argcheck(L, *new_value != 0, value_idx, "Empty string not allowed");

		server_version = new_value;
		return 0;
	} else
		return luaL_error(L, "Unknown attribute");
}

int
LClient::_NewIndex(lua_State *L)
{
	if (lua_gettop(L) != 3)
		return luaL_error(L, "Invalid parameters");

	return LuaClient::Cast(L, 1).NewIndex(L, luaL_checkstring(L, 2), 3);
}

void
LClient::Register(lua_State *L)
{
	using namespace Lua;

	LuaClient::Register(L);
	SetTable(L, RelativeStackIndex{-1}, "__index", _Index);
	SetTable(L, RelativeStackIndex{-1}, "__newindex", _NewIndex);
	lua_pop(L, 1);
}

LClient *
LClient::New(lua_State *L, SocketDescriptor socket, SocketAddress address,
	     std::string_view server_version)
{
	return LuaClient::New(L, L, socket, address, server_version);
}
