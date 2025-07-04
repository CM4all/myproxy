// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;
struct ErrAction;
struct ConnectAction;
struct InitDbAction;

void
RegisterLuaAction(lua_State *L);

ErrAction *
NewLuaErrAction(lua_State *L, ErrAction &&action);

ErrAction *
CheckLuaErrAction(lua_State *L, int idx);

ConnectAction *
NewLuaConnectAction(lua_State *L, ConnectAction &&action);

ConnectAction *
CheckLuaConnectAction(lua_State *L, int idx);

InitDbAction *
NewLuaInitDbAction(lua_State *L, InitDbAction &&action);

InitDbAction *
CheckLuaInitDbAction(lua_State *L, int idx);
