// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Peer.hxx"
#include "MysqlSerializer.hxx"
#include "MysqlMakePacket.hxx"
#include "net/SocketError.hxx"
#include "util/Compiler.h"

#include <stdexcept>

bool
Peer::Send(std::span<const std::byte> src) noexcept
try {
	const auto result = socket.Write(src.data(), src.size());
	if (result > 0) [[likely]] {
		if (static_cast<std::size_t>(result) != src.size()) [[unlikely]]
			throw std::runtime_error{"Short send"};

		return true;
	}

	switch (result) {
	case WRITE_ERRNO:
		throw MakeSocketError("Send failed");

	case WRITE_BLOCKING:
		throw std::runtime_error{"Socket buffer full"};

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
Peer::SendOk(uint8_t sequence_id) noexcept
{
	return Send(Mysql::MakeOk(sequence_id, capabilities));
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
	case MysqlReader::ProcessResult::EMPTY:
		return BufferedResult::OK;

	case MysqlReader::ProcessResult::OK:
		return BufferedResult::AGAIN;

	case MysqlReader::ProcessResult::BLOCKING:
		return BufferedResult::OK;

	case MysqlReader::ProcessResult::MORE:
		return BufferedResult::MORE;

	case MysqlReader::ProcessResult::CLOSED:
		return BufferedResult::CLOSED;
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
	return handler.OnPeerWrite();
}

void
Peer::OnBufferedError(std::exception_ptr e) noexcept
{
	handler.OnPeerError(std::move(e));
}
