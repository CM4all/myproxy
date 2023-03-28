// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Definitions for the MySQL protocol.
 *
 * http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Mysql {

struct Int3 {
	uint8_t data[3];

public:
	constexpr operator uint_least32_t() const noexcept {
		return static_cast<uint_least32_t>(data[0]) |
			(static_cast<uint_least32_t>(data[1]) << 8) |
			(static_cast<uint_least32_t>(data[2]) << 16);
	}
};

struct PacketHeader {
	Int3 length;
	uint8_t number;

	constexpr std::size_t GetLength() const noexcept {
		return length;
	}
};

static constexpr bool
IsQueryPacket(unsigned number, std::span<const std::byte> payload) noexcept
{
	return number == 0 && !payload.empty() && payload.front() == std::byte{0x03};
}

static constexpr bool
IsEofPacket(unsigned number, std::span<const std::byte> payload) noexcept
{
	return number > 0 && !payload.empty() && payload.front() == std::byte{0xfe};
}

} // namespace Mysql
