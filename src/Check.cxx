// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Check.hxx"
#include "Peer.hxx"
#include "MysqlHandler.hxx"
#include "MysqlParser.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"
#include "event/net/ConnectSocket.hxx"
#include "net/SocketAddress.hxx"
#include "util/Cancellable.hxx"

#include <optional>

class MysqlCheck final
	: Cancellable,
	  PeerHandler, MysqlHandler,
	  ConnectSocketHandler {
	CheckServerHandler &handler;

	SocketAddress address;

	ConnectSocket connect;

	std::optional<Peer> peer;

public:
	MysqlCheck(EventLoop &event_loop, CheckServerHandler &_handler) noexcept
		:handler(_handler), connect(event_loop, *this) {}

	void Start(SocketAddress _address, CancellablePointer &cancel_ptr) noexcept {
		address = _address;
		cancel_ptr = *this;
		connect.Connect(address, std::chrono::seconds{10});
	}

private:
	/* virtual methods from Cancellable */
	void Cancel() noexcept override {
		delete this;
	}

	/* virtual methods from PeerSocketHandler */
	void OnPeerClosed() noexcept override {
		// TODO log?
		fmt::print(stderr, "[check/{}] peer closed connection prematurely\n",
			   address);

		handler.OnCheckServer(false);
		delete this;
	}

	WriteResult OnPeerWrite() override {
		// should be unreachable
		return WriteResult::DONE;
	}

	void OnPeerError(std::exception_ptr e) noexcept override {
		// TODO log exception?
		fmt::print(stderr, "[check/{}] peer closed connection prematurely: {}\n",
			   address, e);

		handler.OnCheckServer(false);
		delete this;
	}

	/* virtual methods from MysqlHandler */
	Result OnMysqlPacket(unsigned number, std::span<const std::byte> payload,
			     bool complete) noexcept override;

	std::pair<RawResult, std::size_t> OnMysqlRaw(std::span<const std::byte> src) noexcept override {
		// should be unreachable
		return {RawResult::OK, src.size()};
	}

	/* virtual methods from ConnectSocketHandler */
	void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override {
		/* disable Nagle's algorithm to reduce latency */
		fd.SetNoDelay();

		PeerHandler &peer_handler = *this;
		MysqlHandler &mysql_handler = *this;
		peer.emplace(connect.GetEventLoop(), std::move(fd),
			     peer_handler, mysql_handler);
	}

	void OnSocketConnectError(std::exception_ptr e) noexcept override {
		// TODO log exception?
		fmt::print(stderr, "[check/{}] connect failed: {}\n",
			   address, e);

		handler.OnCheckServer(false);
		delete this;
	}
};

MysqlHandler::Result
MysqlCheck::OnMysqlPacket([[maybe_unused]] unsigned number,
			  std::span<const std::byte> payload,
			  [[maybe_unused]] bool complete) noexcept
try {
	Mysql::ParseHandshake(payload);

	fmt::print(stderr, "[check/{}] ok\n", address);

	handler.OnCheckServer(true);
	delete this;

	return Result::CLOSED;
} catch (...) {
	// TODO log exception?
	fmt::print(stderr, "[check/{}] failed to parse server handshake: {}\n",
		   address, std::current_exception());

	handler.OnCheckServer(false);
	delete this;

	return Result::CLOSED;
}

void
CheckServer(EventLoop &event_loop, SocketAddress address,
	    CheckServerHandler &handler, CancellablePointer &cancel_ptr) noexcept
{
	auto *check = new MysqlCheck(event_loop, handler);
	check->Start(address, cancel_ptr);
}
