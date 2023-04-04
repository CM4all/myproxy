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

enum class Command : uint_least8_t {
	OK = 0x00,
	QUERY = 0x03,
	CHANGE_USER = 0x11,
	RESET_CONNECTION = 0x1f,
	EOF_ = 0xfe,
	ERR = 0xff,
};

enum class ErrorCode : uint_least16_t {
	HANDSHAKE_ERROR = 1043,
};

static constexpr uint_least32_t CLIENT_MYSQL = 1;
static constexpr uint_least32_t CLIENT_FOUND_ROWS = 2;
static constexpr uint_least32_t CLIENT_LONG_FLAG = 4;
static constexpr uint_least32_t CLIENT_CONNECT_WITH_DB = 8;
static constexpr uint_least32_t CLIENT_NO_SCHEMA = 16;
static constexpr uint_least32_t CLIENT_COMPRESS = 32;
static constexpr uint_least32_t CLIENT_ODBC = 64;
static constexpr uint_least32_t CLIENT_LOCAL_FILES = 128;
static constexpr uint_least32_t CLIENT_IGNORE_SPACE = 256;
static constexpr uint_least32_t CLIENT_PROTOCOL_41 = 512;
static constexpr uint_least32_t CLIENT_INTERACTIVE = 1024;
static constexpr uint_least32_t CLIENT_SSL = 2048;
static constexpr uint_least32_t CLIENT_IGNORE_SIGPIPE = 4096;
static constexpr uint_least32_t CLIENT_TRANSACTIONS = 8192;
static constexpr uint_least32_t CLIENT_RESERVED = 16384;
static constexpr uint_least32_t CLIENT_SECURE_CONNECTION = 32768;
static constexpr uint_least32_t CLIENT_MULTI_STATEMENTS = 1UL << 16;
static constexpr uint_least32_t CLIENT_MULTI_RESULTS = 1UL << 17;
static constexpr uint_least32_t CLIENT_PS_MULTI_RESULTS = 1UL << 18;
static constexpr uint_least32_t CLIENT_PLUGIN_AUTH = 1UL << 19;
static constexpr uint_least32_t CLIENT_CONNECT_ATTRS = 1UL << 20;
static constexpr uint_least32_t CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA = 1UL << 21;
static constexpr uint_least32_t CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS = 1UL << 22;
static constexpr uint_least32_t CLIENT_SESSION_TRACK = 1UL << 23;
static constexpr uint_least32_t CLIENT_DEPRECATE_EOF = 1UL << 24;
static constexpr uint_least32_t CLIENT_OPTIONAL_RESULTSET_METADATA = 1UL << 25;
static constexpr uint_least32_t CLIENT_ZSTD_COMPRESSION = 1UL << 26;
static constexpr uint_least32_t CLIENT_QUERY_ATTRIBUTES = 1UL << 27;
static constexpr uint_least32_t MULTI_FACTOR_AUTHENTICATION = 1UL << 28;
static constexpr uint_least32_t CLIENT_PROGRESS = 1UL << 29;
static constexpr uint_least32_t CLIENT_SSL_VERIFY_SERVER_CERT = 1UL << 30;
static constexpr uint_least32_t CLIENT_REMEMBER_OPTIONS = 1UL << 31;

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

} // namespace Mysql
