// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Options.hxx"
#include "util/IntrusiveList.hxx"

#include <coroutine>
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

		explicit constexpr RendezvousNode(const Node &_node) noexcept
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
	SocketAddress Pick(std::string_view account) noexcept;

private:
	void InvokeReady() noexcept;
};
