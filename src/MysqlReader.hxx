// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <span>

class MysqlHandler;
class BufferedSocket;

class MysqlReader {
	MysqlHandler &handler;

	/**
	 * The remaining number of bytes that should be passed to
	 * OnMysqlRaw().
	 */
	std::size_t forward_remaining = 0;

	/**
	 * The remaining number of bytes to ignore (after #remaining).
	 */
	std::size_t ignore_remaining = 0;

public:
	explicit constexpr MysqlReader(MysqlHandler &_handler) noexcept
		:handler(_handler) {}

	enum class ProcessResult {
		/**
		 * The Process() method has finished successfully.
		 */
		OK,

		/**
		 * Currently, no data can be consumed.
		 */
		BLOCKING,

		/**
		 * Need more data.
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

	enum class FlushResult {
		/**
		 * No more raw data to be processed.
		 */
		DRAINED,

		/**
		 * Currently, no data can be consumed.
		 */
		BLOCKING,

		/**
		 * Need to receive more data from the socket.
		 */
		MORE,

		/**
		 * The #MysqlReader has been closed.
		 */
		CLOSED,
	};

	/**
	 * Submit raw data to #MysqlHandler::OnMysqlRaw() (for
	 * #MysqlHandler::Result::OK) or discard raw data (for
	 * #MysqlHandler::Result::IGNORE).
	 */
	FlushResult Flush(BufferedSocket &socket) noexcept;

private:
	FlushResult FlushForward(BufferedSocket &socket) noexcept;

	/**
	 * @return true if ignore==0
	 */
	bool FlushIgnore(BufferedSocket &socket) noexcept;
};
