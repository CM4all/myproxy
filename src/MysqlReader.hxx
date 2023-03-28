// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cassert>
#include <cstddef>
#include <span>

class MysqlHandler;

class MysqlReader {
	MysqlHandler &handler;

	bool have_packet = false;

	/**
	 * The number of bytes to forward.
	 */
	size_t forward = 0;

	/**
	 * The remaining number of the previous packet that should be
	 * forwarded as-is.
	 */
	size_t remaining = 0;

	unsigned number;

	/**
	 * Total length of the current packet payload.
	 */
	size_t payload_length;

	/**
	 * The amount of payload that is available in the #payload buffer.
	 */
	size_t payload_available;

	std::array<std::byte, 1024> payload;

public:
	explicit constexpr MysqlReader(MysqlHandler &_handler) noexcept
		:handler(_handler) {}

	/**
	 * Feed data into the reader.  It stops at the boundary of a packet,
	 * to allow the caller to inspect each packet.
	 *
	 * @return the number of bytes that should be forwarded in this step
	 */
	size_t Feed(std::span<const std::byte> src) noexcept;

	/**
	 * Indicates that the caller has forwarded the specified number of
	 * bytes.
	 */
	void Forwarded(size_t nbytes) noexcept {
		assert(nbytes > 0);
		assert(forward > 0);
		assert(nbytes <= forward);

		forward -= nbytes;
	}
};
