// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Peer.hxx"
#include "util/Compiler.h"

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
