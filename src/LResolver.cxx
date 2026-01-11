// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "LResolver.hxx"
#include "Cluster.hxx"
#include "Options.hxx"
#include "lua/LightUserData.hxx"
#include "lua/Util.hxx"
#include "lua/Error.hxx"
#include "lua/ForEach.hxx"
#include "lua/PushCClosure.hxx"
#include "lua/net/Resolver.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "config.h"

#ifdef ENABLE_CONTROL
#include "net/control/Protocol.hxx"
#endif

extern "C" {
#include <lauxlib.h>
}

#include <netdb.h>
#include <string.h>

static int
l_mysql_cluster(lua_State *L)
try {
	auto &event_loop = *(EventLoop *)lua_touserdata(L, lua_upvalueindex(1));
	auto &stats = *(Stats *)lua_touserdata(L, lua_upvalueindex(2));

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

	Cluster::New(L, event_loop, stats,
		     std::move(nodes), std::move(options));
	return 1;
} catch (...) {
	Lua::RaiseCurrent(L);
}

void
RegisterLuaResolver(lua_State *L, EventLoop &event_loop, Stats &stats)
{
	Cluster::Register(L);

	static constexpr struct addrinfo hints{
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

#ifdef ENABLE_CONTROL
	Lua::PushResolveFunction(L, hints, BengControl::DEFAULT_PORT);
	lua_setglobal(L, "control_resolve");
#endif

	Lua::PushResolveFunction(L, hints, 3306);
	lua_setglobal(L, "mysql_resolve");

	Lua::SetGlobal(L, "mysql_cluster",
		       Lua::MakeCClosure(l_mysql_cluster,
					 Lua::LightUserData{&event_loop},
					 Lua::LightUserData{&stats}));
}

void
UnregisterLuaResolver(lua_State *L)
{
	Lua::SetGlobal(L, "mysql_resolve", nullptr);
}
