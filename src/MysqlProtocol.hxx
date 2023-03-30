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

enum class ErrorCode : uint_least16_t {
	HANDSHAKE_ERROR = 1043,
};

static constexpr uint_least32_t CLIENT_CONNECT_WITH_DB = 8;
static constexpr uint_least32_t CLIENT_PROTOCOL_41 = 512;
static constexpr uint_least32_t CLIENT_TRANSACTIONS = 8192;
static constexpr uint_least32_t CLIENT_PLUGIN_AUTH = 1UL << 19;
static constexpr uint_least32_t CLIENT_CONNECT_ATTRS = 1UL << 20;
static constexpr uint_least32_t CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA = 1UL << 21;
static constexpr uint_least32_t CLIENT_SESSION_TRACK = 1UL << 23;

struct Int2 {
	uint8_t data[2];

public:
	Int2() noexcept = default;

	constexpr Int2(uint_least16_t value) noexcept
		:data{static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8)} {}

	constexpr operator uint_least16_t() const noexcept {
		return static_cast<uint_least16_t>(data[0]) |
			(static_cast<uint_least16_t>(data[1]) << 8);
	}
};

struct Int3 {
	uint8_t data[3];

public:
	Int3() noexcept = default;

	constexpr Int3(uint_least32_t value) noexcept
		:data{static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
		static_cast<uint8_t>(value >> 16)} {}

	constexpr operator uint_least32_t() const noexcept {
		return static_cast<uint_least32_t>(data[0]) |
			(static_cast<uint_least32_t>(data[1]) << 8) |
			(static_cast<uint_least32_t>(data[2]) << 16);
	}
};

struct Int4 {
	uint8_t data[4];

public:
	Int4() noexcept = default;

	constexpr Int4(uint_least32_t value) noexcept
		:data{static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
		static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)} {}

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
	Int6() noexcept = default;

	constexpr Int6(uint_least64_t value) noexcept
		:data{static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
		static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24),
		static_cast<uint8_t>(value >> 32), static_cast<uint8_t>(value >> 40)} {}

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
	Int8() noexcept = default;

	constexpr Int8(uint_least64_t value) noexcept
		:data{static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
		static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24),
		static_cast<uint8_t>(value >> 32), static_cast<uint8_t>(value >> 40),
		static_cast<uint8_t>(value >> 48), static_cast<uint8_t>(value >> 56)} {}

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
