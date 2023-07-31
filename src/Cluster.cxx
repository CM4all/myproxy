// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Cluster.hxx"
#include "lib/sodium/GenericHash.hxx"
#include "lua/Class.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "util/djb_hash.hxx"
#include "util/SpanCast.hxx"

#include <algorithm> // for std::sort()

static std::size_t
AddressHash(SocketAddress address) noexcept
{
	/* use libsodium's "generichash" (BLAKE2b) which is secure
	   enough for class HashRing */
	union {
		std::array<std::byte, crypto_generichash_BYTES_MIN> hash;
		std::size_t result;
	} u;

	static_assert(sizeof(u.hash) >= sizeof(u.result));

	GenericHashState state{sizeof(u.hash)};
	state.Update(address.GetSteadyPart());
	state.Final(u.hash);

	return u.result;
}

struct Cluster::Node {
	AllocatedSocketAddress address;

	explicit Node(AllocatedSocketAddress &&_address) noexcept
		:address(std::move(_address)) {}
};

inline
Cluster::RendezvousNode::RendezvousNode(const Node &_node) noexcept
	:node(&_node),
	 hash(AddressHash(node->address))
{
}

Cluster::Cluster(std::forward_list<AllocatedSocketAddress> &&_nodes) noexcept
{
	for (auto &&i : _nodes)
		node_list.emplace_front(std::move(i));

	for (const auto &i : node_list)
		rendezvous_nodes.emplace_back(i);
}

Cluster::~Cluster() noexcept = default;

SocketAddress
Cluster::Pick(std::string_view account) noexcept
{
	const std::size_t account_hash = djb_hash(AsBytes(account));

	/* sort the list for Rendezvous Hashing */
	std::sort(rendezvous_nodes.begin(), rendezvous_nodes.end(),
		  [account_hash](const auto &a, const auto &b) noexcept
		  {
			  // TODO is XOR good enough to mix the two hashes?
			  return (a.hash ^ account_hash) <
				  (b.hash ^ account_hash);
		  });

	return rendezvous_nodes.front().node->address;
}

static constexpr char lua_cluster_class[] = "myproxy.cluster";
typedef Lua::Class<Cluster, lua_cluster_class> LuaCluster;

void
Cluster::Register(lua_State *L)
{
	LuaCluster::Register(L);
	lua_pop(L, 1);
}

Cluster *
Cluster::New(lua_State *L,
	     std::forward_list<AllocatedSocketAddress> &&nodes)
{
	return LuaCluster::New(L, std::move(nodes));
}

Cluster *
Cluster::Check(lua_State *L, int idx) noexcept
{
	return LuaCluster::Check(L, idx);
}
