// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;
class EventLoop;

int
l_mysql_async_resolve(lua_State *L);

void
InitAsyncResolver(EventLoop &event_loop, lua_State *L) noexcept;
