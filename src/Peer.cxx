// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Peer.hxx"
#include "MysqlSerializer.hxx"
#include "MysqlMakePacket.hxx"
#include "net/SocketError.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/Compiler.h"

#include <stdexcept>

bool
Peer::Send(std::span<const std::byte> src) noexcept
try {
	const auto result = socket.Write(src);
	if (result > 0) [[likely]] {
		if (static_cast<std::size_t>(result) != src.size()) [[unlikely]]
			throw std::runtime_error{"Short send"};

		return true;
	}

	switch (result) {
	case WRITE_ERRNO:
		throw MakeSocketError("Send failed");

	case WRITE_BLOCKING:
		throw SocketBufferFullError{};

	case WRITE_DESTROYED:
		return false;
	}

	throw std::runtime_error{"Send error"};
} catch (...) {
	handler.OnPeerError(std::current_exception());
	return false;
}

bool
Peer::Send(Mysql::PacketSerializer &&s) noexcept
{
	return Send(s.Finish());
}

bool
Peer::SendOk(uint_least8_t sequence_id) noexcept
{
	return Send(Mysql::MakeOk(sequence_id, capabilities, 0, 0, 0, 0, {}, {}));
}

bool
Peer::SendErr(uint_least8_t sequence_id, Mysql::ErrorCode error_code,
	      std::string_view sql_state, std::string_view msg) noexcept
{
	return Send(Mysql::MakeErr(sequence_id, capabilities, error_code,
				   sql_state, msg));
}

BufferedResult
Peer::OnBufferedData()
{
	switch (reader.Process(socket)) {
	case MysqlReader::ProcessResult::OK:
		return BufferedResult::OK;

	case MysqlReader::ProcessResult::BLOCKING:
		return BufferedResult::OK;

	case MysqlReader::ProcessResult::MORE:
		/* before asking BufferedSocket to receive more data,
		   we need to flush as much as possible to avoid
		   returning with a full buffer that cannot receive
		   more data */
		switch (reader.Flush(socket)) {
		case MysqlReader::FlushResult::DRAINED:
			break;

		case MysqlReader::FlushResult::BLOCKING:
			return BufferedResult::OK;

		case MysqlReader::FlushResult::MORE:
			break;

		case MysqlReader::FlushResult::CLOSED:
			return BufferedResult::DESTROYED;
		}

		return BufferedResult::MORE;

	case MysqlReader::ProcessResult::CLOSED:
		return BufferedResult::DESTROYED;
	}

	assert(false);
	gcc_unreachable();
}

bool
Peer::OnBufferedClosed() noexcept
{
	// TODO continue reading from buffer?
	handler.OnPeerClosed();
	return false;
}

bool
Peer::OnBufferedWrite()
{
	switch (handler.OnPeerWrite()) {
	case PeerHandler::WriteResult::DONE:
		socket.UnscheduleWrite();
		return true;

	case PeerHandler::WriteResult::MORE:
		socket.ScheduleWrite();
		return true;

	case PeerHandler::WriteResult::CLOSED:
		return false;
	}

	assert(false);
	gcc_unreachable();
}

enum write_result
Peer::OnBufferedBroken()
{
	return handler.OnPeerBroken()
		? WRITE_ERRNO
		: WRITE_DESTROYED;
}

void
Peer::OnBufferedError(std::exception_ptr e) noexcept
{
	handler.OnPeerError(std::move(e));
}
