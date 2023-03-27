// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Definitions for the MySQL protocol.
 *
 * http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol
 */

#pragma once

#include <cstdint>

namespace Mysql {

struct PacketHeader {
	uint8_t length[3];
	uint8_t number;

	constexpr std::size_t GetLength() const noexcept {
		return static_cast<std::size_t>(length[0]) |
			(static_cast<std::size_t>(length[1]) << 8) |
			(static_cast<std::size_t>(length[2]) << 16);
	}
};

static constexpr bool
IsQueryPacket(unsigned number, const void *data, size_t length) noexcept
{
	return number == 0 && length >= 1 && *(const uint8_t *)data == 0x03;
}

static constexpr bool
IsEofPacket(unsigned number, const void *data, size_t length) noexcept
{
	return number > 0 && length >= 1 && *(const uint8_t *)data == 0xfe;
}

} // namespace Mysql
