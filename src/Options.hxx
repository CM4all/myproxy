// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;

struct ClusterOptions {
	bool monitoring = false;

	void ApplyLuaTable(lua_State *L, int table_idx);
};
