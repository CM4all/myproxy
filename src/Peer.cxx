// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Peer.hxx"

std::pair<PeerHandler::ForwardResult, std::size_t>
Peer::Forward(std::span<std::byte> src) noexcept
{
	const auto r = socket.Write(src.data(), src.size());
	if (r > 0)
		return {PeerHandler::ForwardResult::OK, static_cast<std::size_t>(r)};

	switch (r) {
	case WRITE_BLOCKING:
		return {PeerHandler::ForwardResult::OK, 0};

	default:
		return {PeerHandler::ForwardResult::ERROR, 0};
	}
}

BufferedResult
Peer::OnBufferedData()
{
	const auto r = socket.ReadBuffer();
	if (r.empty())
		return BufferedResult::OK;

	size_t nbytes = reader.Feed(r.data(), r.size());
	assert(nbytes > 0);

	const auto [result, n_forwarded] = handler.OnPeerForward(r.first(nbytes));
	switch (result) {
	case PeerHandler::ForwardResult::OK:
		break;

	case PeerHandler::ForwardResult::ERROR:
		// TODO
		throw "Error";

	case PeerHandler::ForwardResult::CLOSED:
		return BufferedResult::CLOSED;
	}

	if (n_forwarded > 0) {
		socket.DisposeConsumed(n_forwarded);
		reader.Forwarded(n_forwarded);
		return BufferedResult::AGAIN;
	}

	return BufferedResult::OK;
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
