// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Check.hxx"
#include "Options.hxx"
#include "Peer.hxx"
#include "MysqlAuth.hxx"
#include "MysqlHandler.hxx"
#include "MysqlMakePacket.hxx"
#include "MysqlParser.hxx"
#include "MysqlDeserializer.hxx" // for Mysql::MalformedPacket
#include "MysqlProtocol.hxx"
#include "MysqlSerializer.hxx"
#include "MysqlTextResultsetParser.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"
#include "event/net/ConnectSocket.hxx"
#include "net/SocketAddress.hxx"
#include "util/Cancellable.hxx"

#include <optional>

using std::string_view_literals::operator""sv;

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
	  Mysql::TextResultsetHandler,
	  ConnectSocketHandler {
	CheckServerHandler &handler;

	SocketAddress address;

	const CheckOptions &options;

	ConnectSocket connect;

	std::optional<Peer> peer;

	std::optional<Mysql::TextResultsetParser> text_resultset_parser;

	bool have_read_only;

	/**
	 * The value of `@@global.read_only`.  Only set on
	 * QueryState::AFTER_ROW.
	 */
	bool read_only;

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

	void DestroyReadOnly() noexcept {
		fmt::print(stderr, "[check/{}] read only\n", address);

		handler.OnCheckServer(CheckServerResult::READ_ONLY);
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
		fmt::print(stderr, "[check/{}] {}: {}\n", address, msg, e);

		handler.OnCheckServer(CheckServerResult::ERROR);
		delete this;
	}

	Result OnHandshake(std::span<const std::byte> payload);
	Result OnCommandPhase();

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

	/* virtual methods from Mysql:TextResultsetHandler */
	void OnTextResultsetRow(std::span<const std::string_view> values) override;
	void OnTextResultsetEnd() override;

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

	if (!payload.empty() && static_cast<Mysql::Command>(payload.front()) == Mysql::Command::ERR) {
		const auto err = Mysql::ParseErr(payload, peer->capabilities);
		throw FmtRuntimeError("Connection rejected by server: {}",
				      err.error_message);
	}

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

inline MysqlHandler::Result
MysqlCheck::OnCommandPhase()
{
	if (!options.no_read_only) {
		DestroyOk();
		return Result::CLOSED;
	}

	Mysql::TextResultsetHandler &_handler = *this;
	text_resultset_parser.emplace(client_flag, _handler);

	have_read_only = false;
	auto s = Mysql::MakeQuery(0x00, "SELECT @@global.read_only");
	if (!peer->Send(s.Finish()))
		return Result::CLOSED;

	return Result::IGNORE;
}

void
MysqlCheck::OnTextResultsetRow(std::span<const std::string_view> values)
{
	assert(options.no_read_only);

	if (have_read_only)
		return;

	if (values.size() != 1)
		throw std::runtime_error{"Wrong column count"};

	const auto value = values.front();

	if (value == "0"sv)
		read_only = false;
	else if (value == "1"sv)
		read_only = true;
	else
		throw std::runtime_error{"Malformed boolean"};

	have_read_only = true;
}

void
MysqlCheck::OnTextResultsetEnd()
{
	assert(options.no_read_only);

	if (!have_read_only)
		throw std::runtime_error{"No row"};

	if (read_only)
		DestroyReadOnly();
	else
		DestroyOk();
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
		} catch (const Mysql::ErrPacket &err) {
			throw;
		} catch (Mysql::MalformedPacket) {
			throw;
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
			return OnCommandPhase();

		case Mysql::Command::ERR:
			throw FmtRuntimeError("Authentication error: {}",
					      Mysql::ParseErr(payload, client_flag).error_message);

		default:
			throw std::runtime_error{"Unexpected server reply to HandshakeResponse"};
		}
	}

	if (text_resultset_parser) {
		switch (text_resultset_parser->OnMysqlPacket(number, payload,
							     complete)) {
		case Mysql::TextResultsetParser::Result::MORE:
			return Result::IGNORE;

		case Mysql::TextResultsetParser::Result::DONE:
			return Result::CLOSED;
		}
	}

	// TODO unreachable
	return Result::IGNORE;
} catch (const Mysql::ErrPacket &err) {
	DestroyError(err.error_message);
	return Result::CLOSED;
} catch (Mysql::MalformedPacket) {
	DestroyError("Malformed packet");
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
