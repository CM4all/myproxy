// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Instance.hxx"
#include "event/net/control/Server.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketConfig.hxx"
#include "util/PrintException.hxx"
#include "util/SpanCast.hxx"

#include <fmt/core.h>

void
Instance::AddControlListener(const SocketConfig &config)
{
	BengControl::Handler &handler = *this;
	control_listeners.emplace_front(event_loop, config.Create(SOCK_DGRAM),
					handler);
}

inline void
Instance::DisconnectDatabase(std::string_view account) noexcept
{
	std::size_t n = 0;

	for (auto &listener : listeners)
		n += listener.CloseConnectionsIf([account](const auto &connection){
			return connection.IsAccount(account);
		});

	if (n > 0)
		fmt::print(stderr, "Closed {} connections for account {:?}\n", n, account);
}

void
Instance::OnControlPacket(BengControl::Command command,
			  std::span<const std::byte> payload,
			  [[maybe_unused]] std::span<UniqueFileDescriptor> fds,
			  [[maybe_unused]] SocketAddress address,
			  [[maybe_unused]] int uid)
{
	using namespace BengControl;

	switch (command) {
	case Command::NOP:
		/* duh! */
		break;

	case Command::TCACHE_INVALIDATE:
	case Command::DUMP_POOLS:
	case Command::ENABLE_NODE:
	case Command::FADE_NODE:
	case Command::NODE_STATUS:
	case Command::STATS:
	case Command::VERBOSE:
	case Command::FADE_CHILDREN:
	case Command::DISABLE_ZEROCONF:
	case Command::ENABLE_ZEROCONF:
	case Command::FLUSH_NFS_CACHE:
	case Command::FLUSH_FILTER_CACHE:
	case Command::STOPWATCH_PIPE:
	case Command::DISCARD_SESSION:
	case Command::FLUSH_HTTP_CACHE:
	case Command::ENABLE_QUEUE:
	case Command::DISABLE_QUEUE:
	case Command::RELOAD_STATE:
	case Command::TERMINATE_CHILDREN:
	case Command::DISABLE_URING:
	case Command::RESET_LIMITER:
		break;

	case Command::DISCONNECT_DATABASE:
		if (!payload.empty())
			DisconnectDatabase(ToStringView(payload));
		break;
	}
}

void
Instance::OnControlError(std::exception_ptr &&error) noexcept
{
	PrintException(std::move(error));
}
