// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "MysqlMakePacket.hxx"
#include "MysqlSerializer.hxx"
#include "MysqlProtocol.hxx"

#include <array>

using std::string_view_literals::operator""sv;

namespace Mysql {

PacketSerializer
MakeHandshakeV10(std::string_view server_version,
		 uint_least32_t capabilities,
		 std::string_view auth_plugin_name,
		 std::span<const std::byte> auth_plugin_data)
{
	assert(auth_plugin_data.size() >= 8);

	Mysql::PacketSerializer s{0};
	s.WriteInt1(10);
	s.WriteNullTerminatedString(server_version);
	s.WriteInt4(0xdeadbeef); // thread id

	s.WriteN(auth_plugin_data.first(8));

	s.WriteInt1(0); // filler
	s.WriteInt2(capabilities & 0xffff);
	s.WriteInt1(0x21); // character_set
	s.WriteInt2(0x0002); // status_flags
	s.WriteInt2((capabilities >> 16) & 0xffff);

	if (capabilities & CLIENT_PLUGIN_AUTH) {
		s.WriteInt1(auth_plugin_data.size());
	} else {
		s.WriteInt1(0);
	}

	s.WriteZero(10); // reserved
	s.WriteN(auth_plugin_data.subspan(8));

	if (capabilities & CLIENT_PLUGIN_AUTH)
		s.WriteNullTerminatedString(auth_plugin_name);

	return s;
}

PacketSerializer
MakeHandshakeResponse41(uint_least8_t sequence_id, uint_least32_t client_flag,
			std::string_view user, std::string_view auth_response,
			std::string_view database,
			std::string_view client_plugin_name)
{
	client_flag |= CLIENT_PROTOCOL_41|
		/* deprecated according to MySQL but necessary for
		   MariaDB */
		CLIENT_SECURE_CONNECTION;
	client_flag &= ~(CLIENT_CONNECT_WITH_DB|
			 CLIENT_COMPRESS|
			 CLIENT_SSL|
			 CLIENT_PLUGIN_AUTH|CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA|
			 CLIENT_CONNECT_ATTRS);

	if (!database.empty())
		client_flag |= CLIENT_CONNECT_WITH_DB;

	if (!client_plugin_name.empty())
		client_flag |= CLIENT_PLUGIN_AUTH|CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA;

	Mysql::PacketSerializer s{sequence_id};
	s.WriteInt4(client_flag);
	s.WriteInt4(0x1000000); // max_packet_size
	s.WriteInt1(0x21); // character_set
	s.WriteZero(23); // filler
	s.WriteNullTerminatedString(user);
	s.WriteLengthEncodedString(auth_response);

	if (!database.empty())
		s.WriteNullTerminatedString(database);

	if (!client_plugin_name.empty())
		s.WriteNullTerminatedString(client_plugin_name);

	return s;
}

PacketSerializer
MakeQuit(uint_least8_t sequence_id)
{
	Mysql::PacketSerializer s{sequence_id};
	s.WriteCommand(Command::QUIT);
	return s;
}

PacketSerializer
MakeResetConnection(uint_least8_t sequence_id)
{
	Mysql::PacketSerializer s{sequence_id};
	s.WriteCommand(Command::RESET_CONNECTION);
	return s;
}

PacketSerializer
MakeOk(uint_least8_t sequence_id, uint_least32_t capabilities,
       uint_least64_t affected_rows,
       uint_least64_t last_insert_id,
       uint_least16_t status_flags,
       uint_least16_t warnings,
       std::string_view info,
       std::string_view session_state_info)
{
	if (session_state_info.data() != nullptr)
		status_flags |= SERVER_SESSION_STATE_CHANGED;
	else
		status_flags &= ~SERVER_SESSION_STATE_CHANGED;

	Mysql::PacketSerializer s{sequence_id};
	s.WriteCommand(Command::OK);
	s.WriteLengthEncodedInteger(affected_rows);
	s.WriteLengthEncodedInteger(last_insert_id);

	if (capabilities & Mysql::CLIENT_PROTOCOL_41) {
		s.WriteInt2(status_flags);
		s.WriteInt2(warnings);
	} else if (capabilities & Mysql::CLIENT_TRANSACTIONS) {
		s.WriteInt2(status_flags);
	}

	if (capabilities & Mysql::CLIENT_SESSION_TRACK) {
		s.WriteLengthEncodedString(info);

		if (session_state_info.data() != nullptr)
			s.WriteLengthEncodedString(session_state_info);
	} else {
		s.WriteVariableLengthString(info);
	}

	return s;
}

PacketSerializer
MakeErr(uint_least8_t sequence_id, uint_least32_t capabilities,
	ErrorCode error_code,
	std::string_view sql_state, std::string_view msg)
{
	assert(sql_state.size() == 5);

	Mysql::PacketSerializer s{sequence_id};
	s.WriteCommand(Command::ERR);
	s.WriteInt2(static_cast<uint_least16_t>(error_code));

	if (capabilities & Mysql::CLIENT_PROTOCOL_41) {
		s.WriteVariableLengthString("#"sv);
		s.WriteVariableLengthString(sql_state);
	}

	s.WriteVariableLengthString(msg);

	return s;
}

PacketSerializer
MakeQuery(uint_least8_t sequence_id, std::string_view query)
{
	Mysql::PacketSerializer s{sequence_id};
	s.WriteCommand(Command::QUERY);
	s.WriteVariableLengthString(query);
	return s;
}

} // namespace Mysql
