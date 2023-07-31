// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Check.hxx"
#include "Options.hxx"
#include "Peer.hxx"
#include "MysqlAuth.hxx"
#include "MysqlHandler.hxx"
#include "MysqlParser.hxx"
#include "MysqlProtocol.hxx"
#include "MysqlSerializer.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"
#include "event/net/ConnectSocket.hxx"
#include "net/SocketAddress.hxx"
#include "util/Cancellable.hxx"

#include <optional>

constexpr uint_least32_t client_flag =
	Mysql::CLIENT_MYSQL |
	Mysql::CLIENT_PROTOCOL_41 |
	Mysql::CLIENT_IGNORE_SIGPIPE |
	Mysql::CLIENT_RESERVED |
	Mysql::CLIENT_SECURE_CONNECTION |
	Mysql::CLIENT_PLUGIN_AUTH |
	Mysql::CLIENT_PROGRESS;

class MysqlCheck final
	: Cancellable,
	  PeerHandler, MysqlHandler,
	  ConnectSocketHandler {
	CheckServerHandler &handler;

	SocketAddress address;

	const CheckOptions &options;

	ConnectSocket connect;

	std::optional<Peer> peer;

public:
	MysqlCheck(EventLoop &event_loop,
		   const CheckOptions &_options,
		   CheckServerHandler &_handler) noexcept
		:handler(_handler),
		 options(_options),
		 connect(event_loop, *this) {}

	void Start(SocketAddress _address, CancellablePointer &cancel_ptr) noexcept {
		address = _address;
		cancel_ptr = *this;
		connect.Connect(address, std::chrono::seconds{10});
	}

private:
	void DestroyOk() noexcept {
		fmt::print(stderr, "[check/{}] ok\n", address);

		handler.OnCheckServer(CheckServerResult::OK);
		delete this;
	}

	void DestroyError(std::string_view msg) noexcept {
		fmt::print(stderr, "[check/{}] {}\n", address, msg);

		handler.OnCheckServer(CheckServerResult::ERROR);
		delete this;
	}

	void DestroyError(const std::exception_ptr &error) noexcept {
		fmt::print(stderr, "[check/{}] {}\n", address, error);

		handler.OnCheckServer(CheckServerResult::ERROR);
		delete this;
	}

	void DestroyError(std::string_view msg, const std::exception_ptr &e) noexcept {
		fmt::print(stderr, "[check/{}] {}: e\n", address, msg, e);

		handler.OnCheckServer(CheckServerResult::ERROR);
		delete this;
	}

	Result OnHandshake(std::span<const std::byte> payload);

	/* virtual methods from Cancellable */
	void Cancel() noexcept override {
		delete this;
	}

	/* virtual methods from PeerSocketHandler */
	void OnPeerClosed() noexcept override {
		DestroyError("peer closed connection prematurely");
	}

	WriteResult OnPeerWrite() override {
		// should be unreachable
		return WriteResult::DONE;
	}

	void OnPeerError(std::exception_ptr e) noexcept override {
		DestroyError(e);
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
		DestroyError(e);
	}
};

inline MysqlHandler::Result
MysqlCheck::OnHandshake(std::span<const std::byte> payload)
{
	using namespace Mysql;

	const auto handshake = ParseHandshake(payload);

	if (options.user.empty()) {
		DestroyOk();
		return Result::CLOSED;
	}

	auto s = MakeHandshakeResponse41(handshake, client_flag,
					 options.user, options.password,
					 {});
	if (!peer->Send(s.Finish()))
		return Result::CLOSED;

	peer->handshake_response = true;
	return Result::IGNORE;
}

MysqlHandler::Result
MysqlCheck::OnMysqlPacket([[maybe_unused]] unsigned number,
			  std::span<const std::byte> payload,
			  [[maybe_unused]] bool complete) noexcept
try {
	if (!peer->handshake) {
		peer->handshake = true;

		try {
			return OnHandshake(payload);
		} catch (...) {
			DestroyError("failed to handle server handshake",
				     std::current_exception());
			return Result::CLOSED;
		}
	}

	const auto cmd = static_cast<Mysql::Command>(payload.front());

	if (!peer->command_phase) {
		switch (cmd) {
		case Mysql::Command::OK:
		case Mysql::Command::EOF_:
			peer->command_phase = true;

			DestroyOk();
			return Result::CLOSED;

		case Mysql::Command::ERR:
			throw FmtRuntimeError("Authentication error: {}",
					      Mysql::ParseErr(payload, client_flag).error_message);

		default:
			throw std::runtime_error{"Unexpected server reply to HandshakeResponse"};
		}
	}

	// should be unreachable
	DestroyError("command phase");
	return Result::CLOSED;
} catch (...) {
	DestroyError(std::current_exception());
	return Result::CLOSED;
}

void
CheckServer(EventLoop &event_loop, SocketAddress address,
	    const CheckOptions &options,
	    CheckServerHandler &handler, CancellablePointer &cancel_ptr) noexcept
{
	auto *check = new MysqlCheck(event_loop, options, handler);
	check->Start(address, cancel_ptr);
}
