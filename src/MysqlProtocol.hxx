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

struct Int2 {
	uint8_t data[2];

public:
	constexpr operator uint_least32_t() const noexcept {
		return static_cast<uint_least32_t>(data[0]) |
			(static_cast<uint_least32_t>(data[1]) << 8);
	}
};

struct Int3 {
	uint8_t data[3];

public:
	constexpr operator uint_least32_t() const noexcept {
		return static_cast<uint_least32_t>(data[0]) |
			(static_cast<uint_least32_t>(data[1]) << 8) |
			(static_cast<uint_least32_t>(data[2]) << 16);
	}
};

struct Int4 {
	uint8_t data[4];

public:
	constexpr operator uint_least32_t() const noexcept {
		return static_cast<uint_least32_t>(data[0]) |
			(static_cast<uint_least32_t>(data[1]) << 8) |
			(static_cast<uint_least32_t>(data[2]) << 16) |
			(static_cast<uint_least32_t>(data[3]) << 24);
	}
};

struct Int6 {
	uint8_t data[6];

public:
	constexpr operator uint_least64_t() const noexcept {
		return static_cast<uint_least64_t>(data[0]) |
			(static_cast<uint_least64_t>(data[1]) << 8) |
			(static_cast<uint_least64_t>(data[2]) << 16) |
			(static_cast<uint_least64_t>(data[3]) << 24) |
			(static_cast<uint_least64_t>(data[4]) << 32) |
			(static_cast<uint_least64_t>(data[5]) << 40);
	}
};

struct Int8 {
	uint8_t data[8];

public:
	constexpr operator uint_least64_t() const noexcept {
		return static_cast<uint_least64_t>(data[0]) |
			(static_cast<uint_least64_t>(data[1]) << 8) |
			(static_cast<uint_least64_t>(data[2]) << 16) |
			(static_cast<uint_least64_t>(data[3]) << 24) |
			(static_cast<uint_least64_t>(data[4]) << 32) |
			(static_cast<uint_least64_t>(data[5]) << 40) |
			(static_cast<uint_least64_t>(data[6]) << 48) |
			(static_cast<uint_least64_t>(data[7]) << 56);
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
