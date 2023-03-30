// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;
class SocketAddress;

void
RegisterLuaConnection(lua_State *L);

void
NewLuaConnection(lua_State *L, SocketAddress address);
