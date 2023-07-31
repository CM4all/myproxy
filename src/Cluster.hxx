// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Options.hxx"

#include <forward_list>
#include <string_view>
#include <vector>

struct lua_State;
class SocketAddress;
class AllocatedSocketAddress;
class EventLoop;

class Cluster {
	const ClusterOptions options;

	struct Node;
	std::forward_list<Node> node_list;

	struct RendezvousNode {
		const Node *node;
		std::size_t hash;

		explicit RendezvousNode(const Node &_node) noexcept;
	};

	/**
	 * This is a copy of #node_list with precalculated hash for
	 * Rendezvous Hashing.  It will be sorted in each Pick() call.
	 */
	std::vector<RendezvousNode> rendezvous_nodes;

public:
	Cluster(EventLoop &event_loop,
		std::forward_list<AllocatedSocketAddress> &&_nodes,
		ClusterOptions &&_options) noexcept;
	~Cluster() noexcept;

	static void Register(lua_State *L);
	static Cluster *New(lua_State *L,
			    EventLoop &event_loop,
			    std::forward_list<AllocatedSocketAddress> &&nodes,
			    ClusterOptions &&options);

	[[gnu::pure]]
	static Cluster *Check(lua_State *L, int idx) noexcept;

	[[nodiscard]] [[gnu::pure]]
	SocketAddress Pick(std::string_view account) noexcept;
};
