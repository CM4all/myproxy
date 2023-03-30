// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Config.hxx"
#include "Instance.hxx"
#include "BufferedIO.hxx"
#include "MysqlProtocol.hxx"
#include "MysqlDeserializer.hxx"
#include "Policy.hxx"
#include "net/ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/PrintException.hxx"

#include <fmt/core.h>

#include <cassert>
#include <cstring>

Connection::~Connection() noexcept = default;

bool
Connection::Outgoing::OnPeerWrite()
{
	if (connection.delayed)
		/* don't continue reading now */
		return true;

	return connection.incoming.socket.Read(); // TODO return value?
}

void
Connection::Outgoing::OnPeerClosed() noexcept
{
	delete &connection;
}

void
Connection::Outgoing::OnPeerError(std::exception_ptr e) noexcept
{
	PrintException(e);
	delete &connection;
}

void
Connection::OnPeerClosed() noexcept
{
	delete this;
}

bool
Connection::OnPeerWrite()
{
	if (!outgoing)
		return true;

	return outgoing->peer.socket.Read(); // TODO return value?
}

void
Connection::OnPeerError(std::exception_ptr e) noexcept
{
	PrintException(e);
	delete this;
}

void
Connection::OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept
{
	assert(!outgoing);

	outgoing.emplace(*this, std::move(fd));
}

void
Connection::OnSocketConnectError(std::exception_ptr e) noexcept
{
	assert(!outgoing);

	PrintException(e);
	delete this;
}

inline void
Connection::OnHandshakeResponse(Mysql::PacketDeserializer p)
{
	std::string_view username, auth_response, database;

	incoming.capabilities = p.ReadInt2();
	if (incoming.capabilities & Mysql::CLIENT_PROTOCOL_41) {
		// HandshakeResponse41

		incoming.capabilities |= static_cast<uint_least32_t>(p.ReadInt2()) << 16;
		p.ReadInt4(); // max_packet_size
		p.ReadInt1(); // character_set
		p.ReadN(23); // filler
		username = p.ReadNullTerminatedString();

		if (incoming.capabilities & Mysql::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) {
			auth_response = p.ReadLengthEncodedString();
		} else {
			const std::size_t auth_response_length = p.ReadInt1();
			auth_response = p.ReadVariableLengthString(auth_response_length);
		}

		if (incoming.capabilities & Mysql::CLIENT_CONNECT_WITH_DB) {
			database = p.ReadNullTerminatedString();
		}

		if (incoming.capabilities & Mysql::CLIENT_PLUGIN_AUTH) {
			p.ReadNullTerminatedString(); // client_plugin_name
		}

		/* https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_packets_protocol_handshake_response.html
		   does not mention that the packet can end here, but
		   apparently it does, therefore we have the "empty"
		   checks */

		if (!p.empty() && (incoming.capabilities & Mysql::CLIENT_CONNECT_ATTRS)) {
			const std::size_t length = p.ReadLengthEncodedInteger();
			p.ReadN(length);
		}

		if (!p.empty())
			p.ReadInt1(); // zstd_compression_level
	} else {
		// HandshakeResponse320

		p.ReadInt3(); // max_packet_size
		username = p.ReadNullTerminatedString();

		if (incoming.capabilities & Mysql::CLIENT_CONNECT_WITH_DB) {
			auth_response = p.ReadNullTerminatedString();
			database = p.ReadNullTerminatedString();
		} else {
			auth_response = p.ReadRestOfPacketString();
		}
	}

	p.MustBeEmpty();

	fmt::print("login username='{}' database='{}'\n", username, database);

	user = username;

	const auto delay = policy_login(user.c_str());
	if (delay.count() > 0)
		Delay(delay);
}

MysqlHandler::Result
Connection::OnMysqlPacket(unsigned number, std::span<const std::byte> payload,
			  [[maybe_unused]] bool complete) noexcept
try {
	if (!incoming.handshake)
		throw Mysql::MalformedPacket{};

	if (!incoming.handshake_response) {
		OnHandshakeResponse(payload);
		incoming.handshake_response = true;
		if (outgoing)
			outgoing->peer.handshake_response = true;
		return Result::OK;
	}

	if (Mysql::IsQueryPacket(number, payload) &&
	    request_time == Event::TimePoint{})
		request_time = GetEventLoop().SteadyNow();

	return Result::OK;
} catch (Mysql::MalformedPacket) {
	delete this;
	return Result::CLOSED;
}

std::pair<MysqlHandler::Result, std::size_t>
Connection::OnMysqlRaw(std::span<const std::byte> src) noexcept
{
	if (delayed)
		/* don't continue reading now */
		return {Result::OK, 0U};

	if (!outgoing)
		return {Result::OK, 0U};

	const auto result = outgoing->peer.socket.Write(src.data(), src.size());
	if (result > 0) [[likely]]
		return {Result::OK, static_cast<std::size_t>(result)};

	switch (result) {
	case WRITE_BLOCKING:
		return {Result::OK, 0U};

	default:
		// TODO
		return {Result::CLOSED, 0U};
	}
}

void
Connection::Outgoing::OnHandshake(Mysql::PacketDeserializer p)
{
	const auto protocol_version = p.ReadInt1();

	std::string_view server_version;

	if (protocol_version == 10) {
		server_version = p.ReadNullTerminatedString();
		p.ReadInt4(); // thread id
		p.ReadVariableLengthString(8); // auth-plugin-data-part-1
		p.ReadInt1(); //  filler
		peer.capabilities = p.ReadInt2();
		p.ReadInt1(); // character_set
		p.ReadInt2(); // status_flags
		peer.capabilities |= static_cast<uint_least32_t>(p.ReadInt2()) << 16;

		std::size_t auth_plugin_data_len = 0;
		if (peer.capabilities & Mysql::CLIENT_PLUGIN_AUTH) {
			auth_plugin_data_len = p.ReadInt1();
		} else {
			p.ReadInt1(); // 00
		}

		p.ReadVariableLengthString(10); // reserved

		if (auth_plugin_data_len > 8)
			p.ReadVariableLengthString(auth_plugin_data_len - 8);

		if (peer.capabilities & Mysql::CLIENT_PLUGIN_AUTH)
			p.ReadNullTerminatedString(); // auth_plugin_name
	} else if (protocol_version == 9) {
		server_version = p.ReadNullTerminatedString();
		p.ReadInt4(); // thread id
		p.ReadNullTerminatedString(); // scramble
	} else
		throw Mysql::MalformedPacket{};

	fmt::print("handshake server_version='{}'\n", server_version);
}

MysqlHandler::Result
Connection::Outgoing::OnMysqlPacket([[maybe_unused]] unsigned number,
				    std::span<const std::byte> payload,
				    [[maybe_unused]] bool complete) noexcept
try {
	if (!peer.handshake) {
		peer.handshake = true;
		OnHandshake(payload);
		connection.incoming.handshake = true;
		return Result::OK;
	}

	if (!peer.command_phase) {
		if (Mysql::IsOkPacket(payload)) {
			peer.command_phase = true;
			connection.incoming.command_phase = true;
			return Result::OK;
		} else if (Mysql::IsErrPacket(payload)) {
			// TODO extract message
			throw Mysql::MalformedPacket{};
		} else
			throw Mysql::MalformedPacket{};
	}

	if (Mysql::IsEofPacket(payload) &&
	    connection.request_time != Event::TimePoint{}) {
		const auto duration = connection.GetEventLoop().SteadyNow() - connection.request_time;
		policy_duration(connection.user.c_str(), duration);
		connection.request_time = Event::TimePoint{};
	}

	return Result::OK;
} catch (Mysql::MalformedPacket) {
	delete &connection;
	return Result::CLOSED;
}

std::pair<MysqlHandler::Result, std::size_t>
Connection::Outgoing::OnMysqlRaw(std::span<const std::byte> src) noexcept
{
	const auto result = connection.incoming.socket.Write(src.data(), src.size());
	if (result > 0) [[likely]]
		return {Result::OK, static_cast<std::size_t>(result)};

	switch (result) {
	case WRITE_BLOCKING:
		return {Result::OK, 0U};

	default:
		// TODO
		return {Result::CLOSED, 0U};
	}
}

Connection::Outgoing::Outgoing(Connection &_connection,
			       UniqueSocketDescriptor fd) noexcept
	:connection(_connection),
	 peer(connection.GetEventLoop(), std::move(fd), *this, *this)
{
}

void
Connection::OnDelayTimer() noexcept
{
	assert(delayed);

	delayed = false;

	incoming.socket.Read();
}

Connection::Connection(Instance &_instance, UniqueSocketDescriptor fd,
		       SocketAddress)
	:instance(&_instance),
	 delay_timer(instance->event_loop, BIND_THIS_METHOD(OnDelayTimer)),
	 incoming(instance->event_loop, std::move(fd), *this, *this),
	 connect(instance->event_loop, *this)
{
	// TODO move this call out of the ctor
	connect.Connect(instance->config.server_address, std::chrono::seconds{30});
}

void
Connection::Delay(Event::Duration duration) noexcept
{
	delayed = true;

	incoming.socket.UnscheduleRead();

	delay_timer.Schedule(duration);
}
