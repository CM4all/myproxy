// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Cluster.hxx"
#include "lib/sodium/GenericHash.hxx"
#include "lua/Class.hxx"
#include "util/djb_hash.hxx"
#include "util/SpanCast.hxx"

static std::size_t
AddressHash(SocketAddress address, std::size_t replica) noexcept
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
	state.UpdateT(replica);
	state.Final(u.hash);

	return u.result;
}

Cluster::Cluster(std::forward_list<AllocatedSocketAddress> &&_nodes) noexcept
	:nodes(std::move(_nodes))
{
	ring.Build(nodes, AddressHash);
}

SocketAddress
Cluster::Pick(std::string_view account) const noexcept
{
	return ring.Pick(djb_hash(AsBytes(account)));
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
