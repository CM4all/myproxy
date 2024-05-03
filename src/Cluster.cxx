// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Cluster.hxx"
#include "Check.hxx"
#include "NodeObserver.hxx"
#include "Stats.hxx"
#include "lib/sodium/GenericHash.hxx"
#include "lua/Class.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "util/djb_hash.hxx"
#include "util/SpanCast.hxx"
#include "util/Cancellable.hxx"

#include <algorithm> // for std::sort()
#include <cstdint>

constexpr const char *
Cluster::ToString(NodeState state) noexcept
{
	switch (state) {
	case NodeState::DEAD:
		return "dead";

	case NodeState::AUTH_FAILED:
		return "auth_failed";

	case NodeState::UNKNOWN:
		return "unknown";

	case NodeState::READ_ONLY:
		return "read_only";

	case NodeState::ALIVE:
		return "alive";
	}

	return nullptr;
}

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

	NodeStats &stats;

	const CheckOptions &check_options;

	CoarseTimerEvent check_timer;

	CancellablePointer check_cancel;

	IntrusiveList<ClusterNodeObserver,
		      IntrusiveListBaseHookTraits<ClusterNodeObserver, Cluster>> observers;

	using State = Cluster::NodeState;
	State state = State::UNKNOWN;

	Node(Cluster &_cluster, EventLoop &event_loop,
	     AllocatedSocketAddress &&_address,
	     NodeStats &_stats,
	     const ClusterOptions &options) noexcept
		:cluster(_cluster), address(std::move(_address)),
		 stats(_stats),
		 check_options(options.check),
		 check_timer(event_loop, BIND_THIS_METHOD(OnCheckTimer))
	{
		if (options.monitoring)
			check_timer.Schedule(Event::Duration{});
	}

	~Node() noexcept {
		if (check_cancel)
			check_cancel.Cancel();

		InvokeUnavailable();
	}

	auto &GetEventLoop() const noexcept {
		return check_timer.GetEventLoop();
	}

	void InvokeUnavailable() noexcept {
		observers.clear_and_dispose([](auto *observer){
			observer->OnClusterNodeUnavailable();
		});
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
			stats.state = ToString(state);

			if (!cluster.found_alive) {
				cluster.found_alive = true;
				ready = true;
			}

			break;

		case CheckServerResult::READ_ONLY:
			state = State::READ_ONLY;
			stats.state = ToString(state);
			break;

		case CheckServerResult::AUTH_FAILED:
			state = State::AUTH_FAILED;
			stats.state = ToString(state);
			break;

		case CheckServerResult::ERROR:
			state = State::DEAD;
			stats.state = ToString(state);
			break;
		}

		check_timer.Schedule(std::chrono::seconds{20});

		if (ready)
			cluster.InvokeReady();

		if (cluster.HasBetterState(state))
			/* if this state is not "alive" and the
			   cluster has at least one node that is
			   better than this one, close all connections
			   to this node, so clients will reconnect to
			   better nodes; this avoids clients to
			   continue working with nodes that have
			   turned read-only, for example */
			InvokeUnavailable();
		else
			/* if this node has now become better than
			   others, then close all connections to these
			   worse nodes to give them a chance to
			   reconnect to this one */
			cluster.InvokeUnavailableWorse(state);
	}
};

Cluster::Cluster(EventLoop &event_loop, Stats &stats,
		 std::forward_list<AllocatedSocketAddress> &&_nodes,
		 ClusterOptions &&_options) noexcept
	:options(std::move(_options))
{
	for (auto &&i : _nodes) {
		auto &node_stats = stats.GetNode(i);
		node_list.emplace_front(*this, event_loop,
					std::move(i), node_stats,
					options);
	}

	for (auto &i : node_list)
		rendezvous_nodes.emplace_back(i);

	n_unknown = rendezvous_nodes.size();
}

Cluster::~Cluster() noexcept
{
	// at this point, all tasks must have been canceled
	assert(ready_tasks.empty());
}

std::pair<SocketAddress, NodeStats &>
Cluster::Pick(std::string_view account,
	      ClusterNodeObserver *observer) noexcept
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

	auto &node = *rendezvous_nodes.front().node;

	if (observer != nullptr && options.disconnect_unavailable) {
		/* register the observer only if option
		   "disconnect_unavailable" is enabled */
		assert(!observer->is_linked());
		node.observers.push_front(*observer);
	}

	return {node.address, node.stats};
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
	     EventLoop &event_loop, Stats &stats,
	     std::forward_list<AllocatedSocketAddress> &&nodes,
	     ClusterOptions &&options)
{
	return LuaCluster::New(L, event_loop, stats,
			       std::move(nodes), std::move(options));
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

inline bool
Cluster::HasBetterState(NodeState state) const noexcept
{
	if (state == NodeState::ALIVE)
		// can't be better than this
		return false;

	for (const auto &node : node_list)
		if (node.state > state)
			return true;

	return false;
}


inline void
Cluster::InvokeUnavailableWorse(NodeState state) noexcept
{
	if (state == NodeState::DEAD)
		// can't be worse than this
		return;

	for (auto &node : node_list)
		if (node.state < state)
			node.InvokeUnavailable();
}
