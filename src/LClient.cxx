// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LClient.hxx"
#include "Cluster.hxx"
#include "Action.hxx"
#include "LAction.hxx"
#include "OptionsTable.hxx"
#include "lua/AutoCloseList.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/FenvCache.hxx"
#include "lua/ForEach.hxx"
#include "lua/StringView.hxx"
#include "lua/io/CgroupInfo.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketDescriptor.hxx"
#include "net/FormatAddress.hxx"
#include "io/Beneath.hxx"
#include "io/FileAt.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/linux/ProcCgroup.hxx"
#include "util/StringAPI.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <sha1.h> // for SHA1_DIGEST_LENGTH

#include <fmt/core.h>

[[gnu::pure]]
static std::string
MakeClientName(SocketAddress address, const struct ucred &cred) noexcept
{
	if (cred.pid >= 0)
		return fmt::format("pid={} uid={}", cred.pid, cred.uid);

	char buffer[256];
	if (ToString(buffer, address))
		return buffer;

	return "?";
}

inline
LClient::LClient(lua_State *L, Lua::AutoCloseList &_auto_close,
		 SocketDescriptor socket,
		 SocketAddress _address,
		 std::string_view _server_version)
	:lua_state(L), auto_close(&_auto_close),
	 server_version(_server_version),
	 peer_cred(socket.GetPeerCredentials()),
	 name_(MakeClientName(_address, peer_cred))
{
	auto_close->Add(L, Lua::RelativeStackIndex{-1});

	lua_newtable(L);

	Lua::NewSocketAddress(L, _address);
	lua_setfield(L, -2, "address");

	lua_setfenv(L, -2);
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

static void
Apply(lua_State *L, ConnectAction &action, const char *name, auto value_idx)
{
	if (StringIsEqual(name, "user"))
		action.user = CheckString(L, value_idx, "Bad value type");
	else if (StringIsEqual(name, "password"))
		action.password = CheckString(L, value_idx, "Bad value type");
	else if (StringIsEqual(name, "password_sha1")) {
		const auto password_sha1 = CheckStringView(L, value_idx, "Bad value type");
		if (password_sha1.length() != SHA1_DIGEST_LENGTH)
			luaL_error(L, "Bad SHA1 length");

		action.password_sha1 = password_sha1;
	} else if (StringIsEqual(name, "database"))
		action.database = CheckString(L, value_idx, "Bad value type");
	else
		throw Lua::ArgError{"Unknown attribute"};
}

static int
NewConnectAction(lua_State *L)
try {
	if (lua_gettop(L) != 3)
		return luaL_error(L, "Invalid parameters");

	const auto &client = LuaClient::Cast(L, 1);

	ConnectAction action;

	if (Cluster::Check(L, 2)) {
		/* allocate the Lua::Value with the global Lua state,
		   so it gets destructed by it (when the this thread
		   may have been gone already); but fetch the object
		   from this thread's stack */
		action.cluster = std::make_shared<Lua::Value>(client.GetLuaState());
		action.cluster->Set(L, Lua::StackIndex{2});
	} else
		action.address = Lua::ToSocketAddress(L, 2, 3306);

	Lua::ApplyOptionsTable(L, 3, [L, &action](const char *key, auto value_idx){
		Apply(L, action, key, value_idx);
	});

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
LClient::Index(lua_State *L)
{
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	constexpr Lua::StackIndex name_idx{2};
	const char *const name = luaL_checkstring(L, 2);

	if (IsStale())
		return luaL_error(L, "Stale object");

	for (const auto *i = client_methods; i->name != nullptr; ++i) {
		if (StringIsEqual(i->name, name)) {
			Lua::Push(L, i->func);
			return 1;
		}
	}

	// look it up in the fenv (our cache)
	if (Lua::GetFenvCache(L, 1, name_idx))
		return 1;

	if (StringIsEqual(name, "account")) {
		if (!account.empty())
			Lua::Push(L, account);
		else
			lua_pushnil(L);
		return 1;
	} else if (StringIsEqual(name, "notes")) {
		lua_newtable(L);

		// copy a reference to the fenv (our cache)
		Lua::SetFenvCache(L, 1, name_idx, Lua::RelativeStackIndex{-1});

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

		const auto path = ReadProcessCgroup(peer_cred.pid);
		if (path.empty())
			return 0;

		Lua::NewCgroupInfo(L, *auto_close, path);

		// copy a reference to the fenv (our cache)
		Lua::SetFenvCache(L, 1, name_idx, Lua::RelativeStackIndex{-1});

		return 1;
	} else if (StringIsEqual(name, "server_version")) {
		Lua::Push(L, server_version);
		return 1;
	} else
		return luaL_error(L, "Unknown attribute");
}

inline int
LClient::NewIndex(lua_State *L)
{
	if (lua_gettop(L) != 3)
		return luaL_error(L, "Invalid parameters");

	const char *const name = luaL_checkstring(L, 2);
	constexpr int value_idx = 3;

	if (IsStale())
		return luaL_error(L, "Stale object");

	if (StringIsEqual(name, "server_version")) {
		const char *new_value = luaL_checkstring(L, value_idx);
		luaL_argcheck(L, *new_value != 0, value_idx, "Empty string not allowed");

		server_version = new_value;
		return 0;
	} else if (StringIsEqual(name, "account")) {
		if (lua_isnil(L, value_idx)) {
			account.clear();
		} else {
			const char *new_value = luaL_checkstring(L, value_idx);
			luaL_argcheck(L, *new_value != 0, value_idx,
				      "Empty string not allowed");

			account = new_value;
		}

		if (Lua::GetFenvCache(L, 1, "address")) {
			name_ = MakeClientName(Lua::GetSocketAddress(L, -1), peer_cred);
			lua_pop(L, 1);
		}

		if (!account.empty())
			name_ += fmt::format(" \"{}\"", account);
		return 0;
	} else
		return luaL_error(L, "Unknown attribute");
}

void
LClient::Register(lua_State *L)
{
	using namespace Lua;

	LuaClient::Register(L);
	SetTable(L, RelativeStackIndex{-1}, "__close", LuaClient::WrapMethod<&LClient::Close>());
	SetField(L, RelativeStackIndex{-1}, "__index", LuaClient::WrapMethod<&LClient::Index>());
	SetField(L, RelativeStackIndex{-1}, "__newindex", LuaClient::WrapMethod<&LClient::NewIndex>());
	lua_pop(L, 1);
}

LClient *
LClient::New(lua_State *L, Lua::AutoCloseList &auto_close,
	     SocketDescriptor socket, SocketAddress address,
	     std::string_view server_version)
{
	return LuaClient::New(L, L, auto_close,
			      socket, address, server_version);
}
