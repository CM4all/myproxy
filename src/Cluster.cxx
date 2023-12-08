// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Cluster.hxx"
#include "Check.hxx"
#include "lib/sodium/GenericHash.hxx"
#include "lua/Class.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "util/djb_hash.hxx"
#include "util/SpanCast.hxx"
#include "util/Cancellable.hxx"

#include <algorithm> // for std::sort()
#include <cstdint>

[[gnu::pure]]
static std::size_t
RendezvousHash(SocketAddress address, std::string_view account) noexcept
{
	/* use libsodium's "generichash" (BLAKE2b) which is good
	   enough for rendezvous hashing */
	union {
		std::array<std::byte, crypto_generichash_BYTES_MIN> hash;
		std::size_t result;
	} u;

	static_assert(sizeof(u.hash) >= sizeof(u.result));

	GenericHashState state{sizeof(u.hash)};
	state.Update(address.GetSteadyPart());
	state.Update(account);
	state.Final(u.hash);

	return u.result;
}

struct Cluster::Node final : CheckServerHandler {
	Cluster &cluster;

	AllocatedSocketAddress address;

	const CheckOptions &check_options;

	CoarseTimerEvent check_timer;

	CancellablePointer check_cancel;

	enum class State : uint_least8_t {
		DEAD,
		AUTH_FAILED,
		UNKNOWN,
		READ_ONLY,
		ALIVE,
	} state = State::UNKNOWN;

	Node(Cluster &_cluster, EventLoop &event_loop,
	     AllocatedSocketAddress &&_address,
	     const ClusterOptions &options) noexcept
		:cluster(_cluster), address(std::move(_address)),
		 check_options(options.check),
		 check_timer(event_loop, BIND_THIS_METHOD(OnCheckTimer))
	{
		if (options.monitoring)
			check_timer.Schedule(Event::Duration{});
	}

	~Node() noexcept {
		if (check_cancel)
			check_cancel.Cancel();
	}

	auto &GetEventLoop() const noexcept {
		return check_timer.GetEventLoop();
	}

private:
	void OnCheckTimer() noexcept {
		CheckServer(GetEventLoop(), address, check_options,
			    *this, check_cancel);
	}

	// virtual methods from CheckServerHandler
	void OnCheckServer(CheckServerResult result) noexcept override {
		assert(check_cancel);
		check_cancel = nullptr;

		bool ready = false;

		if (state == State::UNKNOWN) {
			assert(cluster.n_unknown > 0);
			--cluster.n_unknown;

			if (cluster.n_unknown == 0)
				ready = true;
		}

		switch (result) {
		case CheckServerResult::OK:
			state = State::ALIVE;

			if (!cluster.found_alive) {
				cluster.found_alive = true;
				ready = true;
			}

			break;

		case CheckServerResult::READ_ONLY:
			state = State::READ_ONLY;
			break;

		case CheckServerResult::AUTH_FAILED:
			state = State::AUTH_FAILED;
			break;

		case CheckServerResult::ERROR:
			state = State::DEAD;
			break;
		}

		check_timer.Schedule(std::chrono::seconds{20});

		if (ready)
			cluster.InvokeReady();
	}
};

Cluster::Cluster(EventLoop &event_loop,
		 std::forward_list<AllocatedSocketAddress> &&_nodes,
		 ClusterOptions &&_options) noexcept
	:options(std::move(_options))
{
	for (auto &&i : _nodes)
		node_list.emplace_front(*this, event_loop, std::move(i), options);

	for (const auto &i : node_list)
		rendezvous_nodes.emplace_back(i);

	n_unknown = rendezvous_nodes.size();
}

Cluster::~Cluster() noexcept
{
	// at this point, all tasks must have been canceled
	assert(ready_tasks.empty());
}

SocketAddress
Cluster::Pick(std::string_view account) noexcept
{
	for (auto &i : rendezvous_nodes)
		i.hash = RendezvousHash(i.node->address, account);

	/* sort the list for Rendezvous Hashing */
	std::sort(rendezvous_nodes.begin(), rendezvous_nodes.end(),
		  [](const auto &a, const auto &b) noexcept
		  {
			  /* prefer nodes that are alive */
			  if (a.node->state != b.node->state)
				  return a.node->state > b.node->state;

			  return a.hash < b.hash;
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
	     EventLoop &event_loop,
	     std::forward_list<AllocatedSocketAddress> &&nodes,
	     ClusterOptions &&options)
{
	return LuaCluster::New(L, event_loop, std::move(nodes), std::move(options));
}

Cluster *
Cluster::Check(lua_State *L, int idx) noexcept
{
	return LuaCluster::Check(L, idx);
}

Cluster &
Cluster::Cast(lua_State *L, int idx) noexcept
{
	return LuaCluster::Cast(L, idx);
}

void
Cluster::InvokeReady() noexcept
{
	ready_tasks.clear_and_dispose([](auto *task){
		if (task->continuation)
			task->continuation.resume();
	});
}
