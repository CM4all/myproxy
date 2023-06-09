// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MysqlMakePacket.hxx"
#include "MysqlSerializer.hxx"
#include "MysqlProtocol.hxx"

#include <array>

using std::string_view_literals::operator""sv;

namespace Mysql {

PacketSerializer
MakeHandshakeV10(std::string_view server_version,
		 std::string_view auth_plugin_name,
		 std::span<const std::byte> auth_plugin_data)
{
	assert(auth_plugin_data.size() >= 8);

	static constexpr uint_least32_t capabilities =
		CLIENT_MYSQL | CLIENT_FOUND_ROWS |
		CLIENT_LONG_FLAG | CLIENT_CONNECT_WITH_DB |
		CLIENT_PROTOCOL_41 |
		CLIENT_IGNORE_SIGPIPE | CLIENT_TRANSACTIONS |
		CLIENT_RESERVED |
		CLIENT_SECURE_CONNECTION | // TODO removing this breaks the HandshakeResponse??
		CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS |
		CLIENT_PS_MULTI_RESULTS |
		CLIENT_PLUGIN_AUTH |
		CLIENT_SESSION_TRACK |
		CLIENT_DEPRECATE_EOF |
		CLIENT_REMEMBER_OPTIONS;

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
	s.WriteInt1(auth_plugin_data.size());
	s.WriteZero(10); // reserved
	s.WriteN(auth_plugin_data.subspan(8));

	s.WriteNullTerminatedString(auth_plugin_name);

	return s;
}

PacketSerializer
MakeHandshakeResponse41(uint_least32_t client_flag,
			std::string_view user, std::string_view auth_response,
			std::string_view database,
			std::string_view client_plugin_name)
{
	client_flag |= CLIENT_PROTOCOL_41;
	client_flag &= ~(CLIENT_CONNECT_WITH_DB|
			 CLIENT_COMPRESS|
			 CLIENT_SSL|
			 CLIENT_SECURE_CONNECTION|
			 CLIENT_PLUGIN_AUTH|CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA|
			 CLIENT_CONNECT_ATTRS);

	if (!database.empty())
		client_flag |= CLIENT_CONNECT_WITH_DB;

	if (!client_plugin_name.empty())
		client_flag |= CLIENT_PLUGIN_AUTH|CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA;

	Mysql::PacketSerializer s{1};
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
MakeResetConnection(uint_least8_t sequence_id)
{
	Mysql::PacketSerializer s{sequence_id};
	s.WriteCommand(Command::RESET_CONNECTION);
	return s;
}

PacketSerializer
MakeOk(uint_least8_t sequence_id, uint_least32_t capabilities)
{
	Mysql::PacketSerializer s{sequence_id};
	s.WriteCommand(Command::OK);
	s.WriteLengthEncodedInteger(0); // affected_rows
	s.WriteLengthEncodedInteger(0); // last_insert_id

	if (capabilities & Mysql::CLIENT_PROTOCOL_41) {
		s.WriteInt2(0); // status_flags
		s.WriteInt2(0); // warnings
	} else if (capabilities & Mysql::CLIENT_TRANSACTIONS) {
		s.WriteInt2(0); // status_flags
	}

#if 0
	// TODO
	if (capabilities & Mysql::CLIENT_SESSION_TRACK) {
		s.WriteLengthEncodedString({}); // info

		/* TODO
		if (status_flags & SERVER_SESSION_STATE_CHANGED)
			s.WriteLengthEncodedString({}); // session state info
		*/
	} else {
		// (no) info
	}
#endif

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

} // namespace Mysql
