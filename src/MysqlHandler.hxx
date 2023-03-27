// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>

class MysqlHandler {
public:
	/**
	 * A packet was received.
	 *
	 * @param number the packet number
	 * @param length the full payload length
	 * @param data the payload
	 * @param available the amount of payload that is available now
	 */
	virtual void OnMysqlPacket(unsigned number, size_t length,
				   const void *data, size_t available) = 0;
};
