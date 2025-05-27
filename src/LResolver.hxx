// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;
struct Stats;
class EventLoop;

void
RegisterLuaResolver(lua_State *L, EventLoop &event_loop,
		    Stats &stats);

void
UnregisterLuaResolver(lua_State *L);
