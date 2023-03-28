// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cassert>
#include <cstddef>
#include <span>

class MysqlHandler;
class BufferedSocket;

class MysqlReader {
	MysqlHandler &handler;

	/**
	 * The remaining number of bytes of the current packet that
	 * should be passed to OnMysqlRaw().
	 */
	std::size_t remaining = 0;

	bool ignore;

public:
	explicit constexpr MysqlReader(MysqlHandler &_handler) noexcept
		:handler(_handler) {}

	enum class ProcessResult {
		/**
		 * The buffer is empty.
		 */
		EMPTY,

		/**
		 * Some data has been consumed.  Call Process() again
		 * until #EMPTY is returned.
		 */
		OK,

		/**
		 * Need more data.1
		 */
		MORE,

		/**
		 * The #MysqlReader has been closed.
		 */
		CLOSED,
	};

	/**
	 * Process data from a #BufferedSocket.  It will invoke
	 * #MysqlReader.
	 */
	ProcessResult Process(BufferedSocket &socket) noexcept;
};
