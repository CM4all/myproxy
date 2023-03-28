// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <span>

class MysqlHandler {
public:
	/**
	 * A packet was received.
	 *
	 * @param number the packet number
	 * @param length the full payload length
	 * @param payload the portion of payload that is available now
	 */
	virtual void OnMysqlPacket(unsigned number, size_t length,
				   std::span<const std::byte> payload) = 0;
};
