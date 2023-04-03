// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;
class SocketAddress;
class SocketDescriptor;

void
RegisterLuaClient(lua_State *L);

void
NewLuaClient(lua_State *L, SocketDescriptor socket, SocketAddress address);
