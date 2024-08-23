// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MysqlParser.hxx"
#include "MysqlDeserializer.hxx"

namespace Mysql {

HandshakePacket
ParseHandshake(std::span<const std::byte> payload)
{
	PacketDeserializer d{payload};
	HandshakePacket packet{};

	packet.protocol_version = d.ReadInt1();

	if (packet.protocol_version == 10) {
		packet.server_version = d.ReadNullTerminatedString();
		packet.thread_id = d.ReadInt4();
		packet.auth_plugin_data1 = d.ReadVariableLengthString(8);
		d.ReadInt1(); //  filler
		packet.capabilities = d.ReadInt2();
		packet.character_set = d.ReadInt1();
		packet.status_flags = d.ReadInt2();
		packet.capabilities |= static_cast<uint_least32_t>(d.ReadInt2()) << 16;

		std::size_t auth_plugin_data_len = 0;
		if (packet.capabilities & CLIENT_PLUGIN_AUTH) {
			auth_plugin_data_len = d.ReadInt1();
		} else {
			d.ReadInt1(); // 00
		}

		d.ReadVariableLengthString(10); // reserved

		if (auth_plugin_data_len > 8)
			packet.auth_plugin_data2 = d.ReadVariableLengthString(auth_plugin_data_len - 8);

		if (packet.capabilities & CLIENT_PLUGIN_AUTH)
			packet.auth_plugin_name = d.ReadNullTerminatedString();
	} else if (packet.protocol_version == 9) {
		packet.server_version = d.ReadNullTerminatedString();
		packet.thread_id = d.ReadInt4();
		packet.scramble = d.ReadNullTerminatedString();
	} else
		throw MalformedPacket{};

	return packet;
}

HandshakeResponsePacket
ParseHandshakeResponse(std::span<const std::byte> payload)
{
	PacketDeserializer d{payload};
	HandshakeResponsePacket packet{};

	packet.capabilities = d.ReadInt2();
	if (packet.capabilities & CLIENT_PROTOCOL_41) {
		// HandshakeResponse41

		packet.capabilities |= static_cast<uint_least32_t>(d.ReadInt2()) << 16;
		packet.max_packet_size = d.ReadInt4();
		packet.character_set = d.ReadInt1();
		d.ReadN(23); // filler
		packet.user = d.ReadNullTerminatedString();

		if (packet.capabilities & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) {
			packet.auth_response = d.ReadLengthEncodedString();
		} else {
			const std::size_t auth_response_length = d.ReadInt1();
			packet.auth_response = d.ReadVariableLengthString(auth_response_length);
		}

		if (packet.capabilities & CLIENT_CONNECT_WITH_DB) {
			packet.database = d.ReadNullTerminatedString();
		}

		if (packet.capabilities & CLIENT_PLUGIN_AUTH) {
			packet.client_plugin_name = d.ReadNullTerminatedString();
		}

		/* https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_packets_protocol_handshake_response.html
		   does not mention that the packet can end here, but
		   apparently it does, therefore we have the "empty"
		   checks */

		if (!d.empty() && (packet.capabilities & CLIENT_CONNECT_ATTRS)) {
			const std::size_t length = d.ReadLengthEncodedInteger();
			d.ReadN(length);
		}

		if (!d.empty())
			d.ReadInt1(); // zstd_compression_level
	} else {
		// HandshakeResponse320

		packet.max_packet_size = d.ReadInt3();
		packet.user = d.ReadNullTerminatedString();

		if (packet.capabilities & CLIENT_CONNECT_WITH_DB) {
			packet.auth_response = d.ReadNullTerminatedString();
			packet.database = d.ReadNullTerminatedString();
		} else {
			packet.auth_response = d.ReadRestOfPacketString();
		}
	}

	d.MustBeEmpty();

	return packet;
}

AuthSwitchRequest
ParseAuthSwitchRequest(std::span<const std::byte> payload)
{
	assert(!payload.empty());
	assert(payload.front() == static_cast<std::byte>(Command::EOF_));

	PacketDeserializer d{payload};
	AuthSwitchRequest packet{};

	d.ReadInt1(); // command
	packet.auth_plugin_name = d.ReadNullTerminatedString();
	packet.auth_plugin_data = d.ReadRestOfPacketString();

	return packet;
}

OkPacket
ParseOk(std::span<const std::byte> payload, uint_least32_t capabilities)
{
	assert(!payload.empty());
	assert(payload.front() == static_cast<std::byte>(Command::OK));

	PacketDeserializer d{payload};
	OkPacket packet{};

	d.ReadInt1(); // command
	packet.affected_rows = d.ReadLengthEncodedInteger();
	packet.last_insert_id = d.ReadLengthEncodedInteger();

	if (capabilities & CLIENT_PROTOCOL_41) {
		packet.status_flags = d.ReadInt2();
		packet.warnings = d.ReadInt2();
	} else if (capabilities & CLIENT_TRANSACTIONS) {
		packet.status_flags = d.ReadInt2();
	}

	if (capabilities & CLIENT_SESSION_TRACK) {
		if (d.empty())
			/* contrary to the MySQL protocol
			   documentation, the OK packet can end
			   here */
			return packet;

		packet.info = d.ReadLengthEncodedString();

		if (packet.status_flags & SERVER_SESSION_STATE_CHANGED)
			packet.session_state_info = d.ReadLengthEncodedString();
	} else {
		packet.info = d.ReadRestOfPacketString();
	}

	return packet;
}

OkPacket
ParseEof(std::span<const std::byte> payload, uint_least32_t capabilities)
{
	assert(!payload.empty());
	assert(payload.front() == static_cast<std::byte>(Command::EOF_));

	PacketDeserializer d{payload};
	OkPacket packet{};

	d.ReadInt1(); // command

	if (capabilities & CLIENT_PROTOCOL_41) {
		packet.warnings = d.ReadInt2();
		packet.status_flags = d.ReadInt2();
	}

	d.MustBeEmpty();

	return packet;
}

ErrPacket
ParseErr(std::span<const std::byte> payload, uint_least32_t capabilities)
{
	assert(!payload.empty());
	assert(payload.front() == static_cast<std::byte>(Command::ERR));

	PacketDeserializer d{payload};
	ErrPacket packet{};

	d.ReadInt1(); // command
	packet.error_code = static_cast<ErrorCode>(d.ReadInt2());

	if (capabilities & CLIENT_PROTOCOL_41) {
		d.ReadVariableLengthString(1); // sql_state_marker
		packet.sql_state = d.ReadVariableLengthString(5); // sql_state
	}

	packet.error_message = d.ReadRestOfPacketString();

	return packet;
}

InitDbPacket
ParseInitDb(std::span<const std::byte> payload)
{
	assert(!payload.empty());
	assert(payload.front() == static_cast<std::byte>(Command::INIT_DB));

	PacketDeserializer d{payload};
	InitDbPacket packet{};

	d.ReadInt1(); // command
	packet.database = d.ReadRestOfPacketString();

	return packet;
}

ChangeUserPacket
ParseChangeUser(std::span<const std::byte> payload, uint_least32_t capabilities)
{
	assert(!payload.empty());
	assert(payload.front() == static_cast<std::byte>(Command::CHANGE_USER));

	PacketDeserializer d{payload};
	ChangeUserPacket packet{};

	d.ReadInt1(); // command
	packet.user = d.ReadNullTerminatedString();

	if (capabilities & CLIENT_SECURE_CONNECTION) {
		std::size_t auth_plugin_data_len = d.ReadInt1();
		packet.auth_plugin_data = d.ReadVariableLengthString(auth_plugin_data_len);
	} else {
		packet.auth_plugin_data = d.ReadNullTerminatedString();
	}

	packet.database = d.ReadNullTerminatedString();

	if (capabilities & CLIENT_PROTOCOL_41) {
		packet.character_set = d.ReadInt2();
	}

	if (capabilities & CLIENT_PLUGIN_AUTH) {
		packet.auth_plugin_name = d.ReadNullTerminatedString();
	}

	return packet;
}

QueryMetadataPacket
ParseQueryMetadata(std::span<const std::byte> payload)
{
	assert(!payload.empty());

	PacketDeserializer d{payload};
	QueryMetadataPacket packet{};

	// TODO support "metadata_follows"

	packet.column_count = d.ReadLengthEncodedInteger();

	return packet;
}

} // namespace Mysql
