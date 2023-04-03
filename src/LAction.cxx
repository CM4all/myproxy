// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LAction.hxx"
#include "Action.hxx"
#include "lua/Class.hxx"

static constexpr char err_action_class[] = "ErrAction";
using LuaErrAction = Lua::Class<ErrAction, err_action_class> ;

static constexpr char connect_action_class[] = "ConnectAction";
using LuaConnectAction = Lua::Class<ConnectAction, connect_action_class> ;

void
RegisterLuaAction(lua_State *L)
{
	LuaErrAction::Register(L);
	lua_pop(L, 1);

	LuaConnectAction::Register(L);
	lua_pop(L, 1);
}

ErrAction *
NewLuaErrAction(lua_State *L, ErrAction &&action)
{
	return LuaErrAction::New(L, std::move(action));
}

ErrAction *
CheckLuaErrAction(lua_State *L, int idx)
{
	return LuaErrAction::Check(L, idx);
}

ConnectAction *
NewLuaConnectAction(lua_State *L, ConnectAction &&action)
{
	return LuaConnectAction::New(L, std::move(action));
}

ConnectAction *
CheckLuaConnectAction(lua_State *L, int idx)
{
	return LuaConnectAction::Check(L, idx);
}
