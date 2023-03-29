// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MysqlReader.hxx"
#include "MysqlHandler.hxx"
#include "MysqlProtocol.hxx"
#include "event/net/BufferedSocket.hxx"

MysqlReader::ProcessResult
MysqlReader::Process(BufferedSocket &socket) noexcept
{
	const auto src = socket.ReadBuffer();
	if (src.empty())
		return ProcessResult::EMPTY;

	if (remaining == 0) {
		const auto &header = *(const Mysql::PacketHeader *)src.data();

		if (src.size() < sizeof(header))
			/* need more data to complete the header */
			return ProcessResult::MORE;

		const auto payload = src.subspan(sizeof(header));

		const std::size_t total_payload_size = header.GetLength();
		if (payload.size() < total_payload_size && !socket.IsFull())
			/* incomplete payload, but the buffer would be
			   able to hold more */
			return ProcessResult::MORE;

		remaining = sizeof(header) + total_payload_size;

		switch (handler.OnMysqlPacket(header.number, payload,
					      payload.size() < total_payload_size)) {
		case MysqlHandler::Result::OK:
			ignore = false;
			break;

		case MysqlHandler::Result::IGNORE:
			ignore = true;
			break;

		case MysqlHandler::Result::CLOSED:
			return ProcessResult::CLOSED;
		}
	}

	assert(remaining > 0);

	const auto raw = src.first(std::min(src.size(), remaining));

	if (ignore) {
		const auto consumed = raw.size();
		remaining -= consumed;
		socket.DisposeConsumed(consumed);
		return ProcessResult::OK;
	}

	auto [result, consumed] = handler.OnMysqlRaw(raw);
	switch (result) {
	case MysqlHandler::Result::IGNORE:
		assert(consumed == 0);
		ignore = true;
		consumed = raw.size();
		[[fallthrough]];

	case MysqlHandler::Result::OK:
		remaining -= consumed;
		socket.DisposeConsumed(consumed);

		if (consumed == 0)
			return ProcessResult::BLOCKING;

		break;

	case MysqlHandler::Result::CLOSED:
		assert(consumed == 0);
		return ProcessResult::CLOSED;
	}

	return ProcessResult::OK;
}
