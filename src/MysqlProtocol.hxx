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

static constexpr uint_least32_t CLIENT_CONNECT_WITH_DB = 8;
static constexpr uint_least32_t CLIENT_PROTOCOL_41 = 512;
static constexpr uint_least32_t CLIENT_PLUGIN_AUTH = (1UL << 19);
static constexpr uint_least32_t CLIENT_CONNECT_ATTRS = (1UL << 20);
static constexpr uint_least32_t CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA = 1UL << 21;

struct Int2 {
	uint8_t data[2];

public:
	constexpr operator uint_least16_t() const noexcept {
		return static_cast<uint_least16_t>(data[0]) |
			(static_cast<uint_least16_t>(data[1]) << 8);
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
IsEofPacket(std::span<const std::byte> payload) noexcept
{
	return !payload.empty() && payload.size() < 9 && payload.front() == std::byte{0xfe};
}

static constexpr bool
IsOkPacket(std::span<const std::byte> payload) noexcept
{
	return !payload.empty() &&
		(payload.front() == std::byte{0x00} || payload.front() == std::byte{0xfe});
}

static constexpr bool
IsErrPacket(std::span<const std::byte> payload) noexcept
{
	return !payload.empty() && payload.front() == std::byte{0xff};
}

} // namespace Mysql
