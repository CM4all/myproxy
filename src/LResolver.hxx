// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;

void
RegisterLuaResolver(lua_State *L);

void
UnregisterLuaResolver(lua_State *L);
