// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <span>

class MysqlHandler {
public:
	enum class Result {
		/**
		 * The packet was accepted and the handler is
		 * interested in getting raw data (and possibly the
		 * rest of the payload).
		 */
		OK,

		/**
		 * Ignore this packet from now on.  Neither raw data
		 * nor more of the payload will be delivered.
		 */
		IGNORE,

		/**
		 * The #MysqlReader has been closed.
		 */
		CLOSED,
	};

	/**
	 * A packet was received.
	 *
	 * @param number the packet number
	 * @param payload the payload (or the beginning of the payload
	 * if the payload is too large)
	 * @param complete false if the payload is incomplete (because
	 * it does not fit into the input buffer)
	 */
	virtual Result OnMysqlPacket(unsigned number, std::span<const std::byte> payload,
				     bool complete) noexcept = 0;

	/**
	 * @return a #Result code and the number of bytes that have
	 * been consumed
	 */
	virtual std::pair<Result, std::size_t> OnMysqlRaw(std::span<const std::byte> src) noexcept = 0;
};
