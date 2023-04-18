// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"
#include "util/HashRing.hxx"

#include <forward_list>

struct lua_State;

class Cluster {
	std::forward_list<AllocatedSocketAddress> nodes;

	HashRing<AllocatedSocketAddress, std::size_t, 8192, 64> ring;

public:
	Cluster(std::forward_list<AllocatedSocketAddress> &&_nodes) noexcept;

	static void Register(lua_State *L);
	static Cluster *New(lua_State *L,
			    std::forward_list<AllocatedSocketAddress> &&nodes);

	[[gnu::pure]]
	static Cluster *Check(lua_State *L, int idx) noexcept;

	[[nodiscard]] [[gnu::pure]]
	SocketAddress Pick(std::string_view account) const noexcept;
};
