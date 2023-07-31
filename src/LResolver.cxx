// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LResolver.hxx"
#include "Cluster.hxx"
#include "Options.hxx"
#include "lua/Util.hxx"
#include "lua/Error.hxx"
#include "lua/ForEach.hxx"
#include "lua/PushCClosure.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "net/AllocatedSocketAddress.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <string.h>

static constexpr auto hints = MakeAddrInfo(0, AF_UNSPEC, SOCK_STREAM);

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

	AddressInfoList ai;

	try {
		ai = Resolve(s, 3306, &hints);
	} catch (...) {
		Lua::RaiseCurrent(L);
	}

	Lua::NewSocketAddress(L, std::move(ai.GetBest()));
	return 1;
}

static int
l_mysql_cluster(lua_State *L)
try {
	auto &event_loop = *(EventLoop *)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_gettop(L) < 1)
		return luaL_error(L, "Not enough parameters");

	if (lua_gettop(L) > 2)
		return luaL_error(L, "Too many parameters");

	luaL_argcheck(L, lua_istable(L, 1), 1, "Table expected");

	std::forward_list<AllocatedSocketAddress> nodes;

	Lua::ForEach(L, 1, [L, &nodes](auto key_idx, auto value_idx) {
		if (!lua_isnumber(L, Lua::GetStackIndex(key_idx)))
			throw std::invalid_argument{"Key is not a number"};

		nodes.emplace_front(Lua::ToSocketAddress(L, Lua::GetStackIndex(value_idx), 3306));
	});

	ClusterOptions options;

	if (lua_gettop(L) >= 2)
		options.ApplyLuaTable(L, 2);

	luaL_argcheck(L, !nodes.empty(), 1, "Cluster is empty");

	Cluster::New(L, event_loop, std::move(nodes),
		     options.monitoring);
	return 1;
} catch (...) {
	Lua::RaiseCurrent(L);
}

void
RegisterLuaResolver(lua_State *L, EventLoop &event_loop)
{
	Cluster::Register(L);
	Lua::SetGlobal(L, "mysql_resolve", l_mysql_resolve);
	Lua::SetGlobal(L, "mysql_cluster",
		       Lua::MakeCClosure(l_mysql_cluster,
					 Lua::LightUserData{&event_loop}));
}

void
UnregisterLuaResolver(lua_State *L)
{
	Lua::SetGlobal(L, "mysql_resolve", nullptr);
}
