// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"

#include <forward_list>
#include <vector>

struct lua_State;

class Cluster {
	std::forward_list<AllocatedSocketAddress> node_list;

	struct RendezvousNode {
		SocketAddress address;
		std::size_t hash;

		explicit RendezvousNode(SocketAddress _address) noexcept;
	};

	/**
	 * This is a copy of #node_list with precalculated hash for
	 * Rendezvous Hashing.  It will be sorted in each Pick() call.
	 */
	std::vector<RendezvousNode> rendezvous_nodes;

public:
	explicit Cluster(std::forward_list<AllocatedSocketAddress> &&_nodes) noexcept;

	static void Register(lua_State *L);
	static Cluster *New(lua_State *L,
			    std::forward_list<AllocatedSocketAddress> &&nodes);

	[[gnu::pure]]
	static Cluster *Check(lua_State *L, int idx) noexcept;

	[[nodiscard]] [[gnu::pure]]
	SocketAddress Pick(std::string_view account) noexcept;
};
