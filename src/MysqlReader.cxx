// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MysqlReader.hxx"
#include "MysqlHandler.hxx"
#include "MysqlProtocol.hxx"
#include "event/net/BufferedSocket.hxx"

#include <cassert>

MysqlReader::ProcessResult
MysqlReader::Process(BufferedSocket &socket) noexcept
{
	while (true) {
		if (ignore_remaining > 0) {
			/* we need to flush the "ignore_remaining"
			   before we can continue processing any input
			   here */

			switch (Flush(socket)) {
			case FlushResult::DRAINED:
				assert(forward_remaining == 0);
				assert(ignore_remaining == 0);
				/* all scheduled data has been flushed
				   - we can continue processing more
				   input */
				break;

			case FlushResult::BLOCKING:
				return ProcessResult::BLOCKING;

			case FlushResult::MORE:
				return ProcessResult::MORE;

			case FlushResult::CLOSED:
				return ProcessResult::CLOSED;
			}
		}

		const auto r = socket.ReadBuffer();
		if (r.empty())
			return ProcessResult::OK;

		if (r.size() <= forward_remaining)
			return ProcessResult::MORE;

		const auto src = r.subspan(forward_remaining);

		const auto &header = *(const Mysql::PacketHeader *)src.data();
		if (src.size() < sizeof(header))
			/* need more data to complete the header */
			return ProcessResult::MORE;

		auto payload = src.subspan(sizeof(header));

		const std::size_t total_payload_size = header.GetLength();

		bool complete = true;
		if (payload.size() < total_payload_size)
			complete = false;
		else
			payload = payload.first(total_payload_size);

		if (!complete && (!socket.IsFull() || forward_remaining > 0))
			/* incomplete payload, but the buffer would be able to
			   hold more */
			return ProcessResult::MORE;

		const std::size_t total_packet_size = sizeof(header) + total_payload_size;

		switch (handler.OnMysqlPacket(header.number, payload, complete)) {
		case MysqlHandler::Result::FORWARD:
			forward_remaining += total_packet_size;
			break;

		case MysqlHandler::Result::BLOCKING:
			return ProcessResult::BLOCKING;

		case MysqlHandler::Result::IGNORE:
			ignore_remaining = total_packet_size;
			break;

		case MysqlHandler::Result::CLOSED:
			return ProcessResult::CLOSED;
		}
	}
}

MysqlReader::FlushResult
MysqlReader::Flush(BufferedSocket &socket) noexcept
{
	if (forward_remaining > 0) {
		switch (auto result = FlushForward(socket)) {
		case FlushResult::DRAINED:
			assert(forward_remaining == 0);
			break;

		case FlushResult::MORE:
			socket.ScheduleRead();
			return result;

		case FlushResult::BLOCKING:
		case FlushResult::CLOSED:
			return result;
		}
	}

	if (FlushIgnore(socket))
		return FlushResult::DRAINED;

	socket.ScheduleRead();
	return FlushResult::MORE;
}

inline MysqlReader::FlushResult
MysqlReader::FlushForward(BufferedSocket &socket) noexcept
{
	assert(forward_remaining > 0);

	const auto src = socket.ReadBuffer();
	if (src.empty())
		return FlushResult::MORE;

	const auto raw = src.first(std::min(src.size(), forward_remaining));

	auto [result, consumed] = handler.OnMysqlRaw(raw);
	switch (result) {
	case MysqlHandler::RawResult::OK:
		if (consumed == 0)
			return FlushResult::BLOCKING;

		forward_remaining -= consumed;
		socket.DisposeConsumed(consumed);
		break;

	case MysqlHandler::RawResult::CLOSED:
		assert(consumed == 0);
		return FlushResult::CLOSED;
	}

	return forward_remaining == 0 ? FlushResult::DRAINED : FlushResult::MORE;
}

inline bool
MysqlReader::FlushIgnore(BufferedSocket &socket) noexcept
{
	assert(forward_remaining == 0);

	if (ignore_remaining == 0)
		return true;

	const auto src = socket.ReadBuffer();
	if (src.empty())
		return false;

	if (src.size() >= ignore_remaining) {
		socket.DisposeConsumed(ignore_remaining);
		ignore_remaining = 0;
		return true;
	} else {
		socket.DisposeConsumed(src.size());
		ignore_remaining -= src.size();
		return false;
	}
}
