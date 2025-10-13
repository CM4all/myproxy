// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "MysqlTextResultsetParser.hxx"
#include "MysqlDeserializer.hxx"
#include "MysqlParser.hxx"
#include "MysqlProtocol.hxx"
#include "net/SocketProtocolError.hxx"

#include <stdexcept>
#include <utility> // for std::unreachable()

namespace Mysql {

#ifdef __GNUC__
#pragma GCC diagnostic push
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wmissing-noreturn"
#else
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#endif
#endif

void
TextResultsetHandler::OnTextResultsetErr(const ErrPacket &err)
{
	throw err;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

inline TextResultsetParser::Result
TextResultsetParser::OnResponse(std::span<const std::byte> payload)
{
	switch (state) {
	case State::COLUMN_COUNT:
		values.ResizeDiscard(Mysql::ParseQueryMetadata(payload).column_count);
		state = State::COLUMN_DEFINITON;
		return Result::MORE;

	case State::COLUMN_DEFINITON:
		return Result::MORE;

	case State::ROW:
		return OnRow(payload);
	}

	std::unreachable();
}

inline TextResultsetParser::Result
TextResultsetParser::OnRow(std::span<const std::byte> payload)
{
	Mysql::PacketDeserializer d{payload};
	for (auto &i : values)
		i = d.ReadLengthEncodedString();
	d.MustBeEmpty();

	handler.OnTextResultsetRow(values);
	return Result::MORE;
}

inline TextResultsetParser::Result
TextResultsetParser::OnEof()
{
	switch (state) {
	case State::COLUMN_COUNT:
		throw SocketProtocolError{"COLUMN_COUNT expected"};

	case State::COLUMN_DEFINITON:
		state = State::ROW;
		return Result::MORE;

	case State::ROW:
		return OnFinalEof();
	}

	std::unreachable();
}

inline TextResultsetParser::Result
TextResultsetParser::OnFinalEof()
{
	handler.OnTextResultsetEnd();
	return Result::DONE;
}

TextResultsetParser::Result
TextResultsetParser::OnMysqlPacket([[maybe_unused]] unsigned number,
				   std::span<const std::byte> payload,
				   [[maybe_unused]] bool complete)
{
	const auto cmd = static_cast<Mysql::Command>(payload.front());

	switch (cmd) {
	case Mysql::Command::OK:
		return OnFinalEof();

	case Mysql::Command::EOF_:
		return OnEof();

	case Mysql::Command::ERR:
		handler.OnTextResultsetErr(Mysql::ParseErr(payload, capabilities));
		return Result::DONE;

	default:
		return OnResponse(payload);
	}
}

} // namespace Mysql
