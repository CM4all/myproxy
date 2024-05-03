// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"

#include <algorithm> // for std::lexicographical_compare()
#include <cstdint>
#include <map>

struct NodeStats {
	const char *state = nullptr;

	uint_least64_t n_connects = 0;
	uint_least64_t n_connect_errors = 0;
	uint_least64_t n_packets_received = 0;
	uint_least64_t n_bytes_received = 0;
	uint_least64_t n_malformed_packets = 0;
};

struct Stats {
	uint_least64_t n_accepted_connections = 0;
	uint_least64_t n_rejected_connections = 0;

	uint_least64_t n_client_packets_received = 0;
	uint_least64_t n_client_bytes_received = 0;
	uint_least64_t n_client_malformed_packets = 0;
	uint_least64_t n_client_handshake_responses = 0;
	uint_least64_t n_client_auth_ok = 0;
	uint_least64_t n_client_auth_err = 0;
	uint_least64_t n_client_queries = 0;

	uint_least64_t n_lua_errors = 0;

	struct CompareSocketAddress {
		using is_transparent = CompareSocketAddress;

		[[gnu::pure]]
		bool operator()(SocketAddress _a, SocketAddress _b) const noexcept {
			std::span<const std::byte> a{_a};
			std::span<const std::byte> b{_b};

			return std::lexicographical_compare(a.begin(), a.end(),
							    b.begin(), b.end());
		}
	};

	std::map<AllocatedSocketAddress, NodeStats, CompareSocketAddress> nodes;

	[[gnu::pure]]
	NodeStats &GetNode(SocketAddress address) noexcept {
		if (auto i = nodes.find(address); i != nodes.end())
			return i->second;

		return nodes.emplace(address, NodeStats{}).first->second;
	}
};
