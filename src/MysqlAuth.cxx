// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MysqlAuth.hxx"
#include "MysqlParser.hxx"
#include "MysqlMakePacket.hxx"
#include "MysqlSerializer.hxx"
#include "SHA1.hxx"
#include "util/SpanCast.hxx"

using std::string_view_literals::operator""sv;

namespace Mysql {

PacketSerializer
MakeHandshakeResponse41SHA1(const HandshakePacket &handshake, uint_least32_t client_flag,
			    std::string_view user,
			    std::span<const std::byte, SHA1_DIGEST_LENGTH> password_sha1,
			    std::string_view database)
{
	const auto password_sha1_sha1 = SHA1(password_sha1);

	SHA1State s;
	s.Update(handshake.auth_plugin_data1);
	s.Update(handshake.auth_plugin_data2.substr(0, 12));
	s.Update(password_sha1_sha1);

	auto auth_response = s.Final();
	for (std::size_t i = 0; i < auth_response.size(); ++i)
		auth_response[i] ^= password_sha1[i];

	return Mysql::MakeHandshakeResponse41(client_flag, user,
					      ToStringView(auth_response),
					      database,
					      "mysql_native_password"sv);
}

PacketSerializer
MakeHandshakeResponse41(const HandshakePacket &handshake, uint_least32_t client_flag,
			std::string_view user, std::string_view password,
			std::string_view database)
{
	if (handshake.auth_plugin_name == "mysql_native_password"sv &&
	    handshake.auth_plugin_data1.size() == 8 &&
	    handshake.auth_plugin_data2.size() == 13 &&
	    handshake.auth_plugin_data2.back() == '\0') {
		/* the protocol documentation at
		   https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_authentication_methods_native_password_authentication.html
		   writes that auth_plugin_data should be "20 random
		   bytes", but it really is 21 bytes with a trailing
		   null byte that must be ignored */

		return MakeHandshakeResponse41SHA1(handshake, client_flag,
						   user, SHA1(password),
						   database);
	}

	return Mysql::MakeHandshakeResponse41(client_flag,
					      user, password, database,
					      "mysql_clear_password"sv);
}

} // namespace Mysql
