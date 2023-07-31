// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>

namespace Mysql {

struct ErrPacket;

/**
 * Handler for #TextResultsetParser.
 *
 * Exceptions thrown by the methods will be passed to
 * OnTextResultsetError().
 */
class TextResultsetHandler {
public:
	/**
	 * Learned about the number of columns.
	 */
	virtual void OnTextResultsetColumnCount(unsigned column_count) = 0;

	/**
	 * A row has been received.
	 *
	 * @param payload the values of the row; use
	 * Mysql::PacketDeserializer::ReadLengthEncodedString() to
	 * extract each of them
	 */
	virtual void OnTextResultsetRow(std::span<const std::byte> payload) = 0;

	/**
	 * The end of the resultset was reached.  This method is
	 * allowed to destroy the #TextResultsetParser.
	 */
	virtual void OnTextResultsetEnd() = 0;

	/**
	 * An "ERR" packet was received.  The default implementation
	 * throws it.
	 */
	virtual void OnTextResultsetErr(const ErrPacket &err);
};

/**
 * This class helps with parsing the packets of a MySQL text
 * resultset.  Call OnMysqlPacket() with all packets received from the
 * MySQL server until either OnTextResultsetEnd() or
 * OnTextResultsetError() gets called.
 */
class TextResultsetParser {
	TextResultsetHandler &handler;

	const uint_least32_t capabilities;

	enum class State : uint_least8_t {
		COLUMN_COUNT,
		COLUMN_DEFINITON,
		ROW,
	} state = State::COLUMN_COUNT;

public:
	/**
	 * @param _capabilities the capabilities that were announced
	 * to the server in the handshake response
	 */
	constexpr TextResultsetParser(uint_least32_t _capabilities,
				      TextResultsetHandler &_handler) noexcept
		:handler(_handler), capabilities(_capabilities) {}

	enum class Result : uint_least8_t {
		/**
		 * More packets are needed to finish the resultset.
		 */
		MORE,

		/**
		 * The resultset is complete and OnTextResultsetEnd()
		 * or OnTextResultsetErr() has been called.  Depending
		 * on what these methods do, the #TextResultsetParser
		 * may have been destroyed and the MySQL connection
		 * may have been closed.
		 */
		DONE,
	};

	/**
	 * Throws on error.
	 */
	Result OnMysqlPacket(unsigned number,
			     std::span<const std::byte> payload,
			     bool complete);

private:
	Result OnResponse(std::span<const std::byte> payload);
	Result OnRow(std::span<const std::byte> payload);
	Result OnEof();
	Result OnFinalEof();
};

} // namespace Mysql
