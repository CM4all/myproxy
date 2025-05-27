// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Options.hxx"
#include "util/IntrusiveList.hxx"

#include <coroutine>
#include <cstdint>
#include <forward_list>
#include <string_view>
#include <utility> // for std::pair
#include <vector>

struct lua_State;
struct Stats;
struct NodeStats;
class SocketAddress;
class AllocatedSocketAddress;
class EventLoop;
class ClusterNodeObserver;

class Cluster {
	const ClusterOptions options;

	enum class NodeState : uint_least8_t {
		DEAD,
		AUTH_FAILED,
		UNKNOWN,
		READ_ONLY,
		ALIVE,
	};

	struct Node;
	std::forward_list<Node> node_list;

	struct RendezvousNode {
		Node *node;
		std::size_t hash;

		explicit constexpr RendezvousNode(Node &_node) noexcept
			:node(&_node) {}
	};

	/**
	 * This is a copy of #node_list with precalculated hash for
	 * Rendezvous Hashing.  It will be sorted in each Pick() call.
	 */
	std::vector<RendezvousNode> rendezvous_nodes;

	class ReadyTask : public IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK> {
		friend class Cluster;

		std::coroutine_handle<> continuation;

		ReadyTask(Cluster &cluster) noexcept {
			if (!cluster.IsReady())
				cluster.ready_tasks.push_back(*this);
		}

	public:
		ReadyTask(const ReadyTask &) = delete;

		ReadyTask &operator=(const ReadyTask &) = delete;

		[[nodiscard]]
		bool await_ready() const noexcept {
			return !is_linked();
		}

		[[nodiscard]]
		std::coroutine_handle<> await_suspend(std::coroutine_handle<> _continuation) noexcept {
			continuation = _continuation;
			return std::noop_coroutine();
		}

		void await_resume() noexcept {
		}
	};

	IntrusiveList<ReadyTask> ready_tasks;

	std::size_t n_unknown;

	bool found_alive = false;

public:
	Cluster(EventLoop &event_loop, Stats &stats,
		std::forward_list<AllocatedSocketAddress> &&_nodes,
		ClusterOptions &&_options) noexcept;
	~Cluster() noexcept;

	static void Register(lua_State *L);
	static Cluster *New(lua_State *L,
			    EventLoop &event_loop, Stats &stats,
			    std::forward_list<AllocatedSocketAddress> &&nodes,
			    ClusterOptions &&options);

	[[gnu::pure]]
	static Cluster *Check(lua_State *L, int idx) noexcept;

	[[gnu::pure]]
	static Cluster &Cast(lua_State *L, int idx) noexcept;

	bool IsReady() const noexcept {
		return !options.monitoring || found_alive || n_unknown == 0;
	}

	/**
	 * The returned task finishes as soon as IsReady() becomes
	 * true.  This can be used to delay connect attempts to this
	 * cluster right after startup until all nodes have been
	 * checked.
	 */
	ReadyTask CoWaitReady() noexcept {
		return ReadyTask{*this};
	}

	[[nodiscard]] [[gnu::pure]]
	std::pair<SocketAddress, NodeStats &> Pick(std::string_view account,
						   ClusterNodeObserver *observer=nullptr) noexcept;

private:
	static constexpr const char *ToString(NodeState state) noexcept;

	void InvokeReady() noexcept;

	/**
	 * Does a node with a state better than this one exist?
	 */
	[[gnu::pure]]
	bool HasBetterState(NodeState state) const noexcept;

	/**
	 * Invoke OnClusterNodeUnavailable() on all nodes that are
	 * worse than this state.
	 */
	void InvokeUnavailableWorse(NodeState state) noexcept;
};
