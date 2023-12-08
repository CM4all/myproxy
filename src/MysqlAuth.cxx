// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MysqlAuth.hxx"
#include "MysqlParser.hxx"
#include "MysqlMakePacket.hxx"
#include "MysqlSerializer.hxx"
#include "SHA1.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/SpanCast.hxx"

using std::string_view_literals::operator""sv;

namespace Mysql {

static auto
MakeMysqlNativePasswordSHA1(std::span<const std::byte> password_sha1,
			    std::string_view auth_plugin_data1,
			    std::string_view auth_plugin_data2) noexcept
{
	assert(auth_plugin_data1.size() + auth_plugin_data2.size() == 20);

	const auto password_sha1_sha1 = SHA1(password_sha1);

	SHA1State s;
	s.Update(auth_plugin_data1);
	s.Update(auth_plugin_data2);
	s.Update(password_sha1_sha1);

	auto auth_response = s.Final();
	for (std::size_t i = 0; i < auth_response.size(); ++i)
		auth_response[i] ^= password_sha1[i];

	return auth_response;
}

PacketSerializer
MakeHandshakeResponse41SHA1(const HandshakePacket &handshake,
			    uint_least8_t sequence_id, uint_least32_t client_flag,
			    std::string_view user,
			    std::span<const std::byte, SHA1_DIGEST_LENGTH> password_sha1,
			    std::string_view database)
{
	const auto auth_response =
		MakeMysqlNativePasswordSHA1(password_sha1,
					    handshake.auth_plugin_data1,
					    handshake.auth_plugin_data2.substr(0, 12));

	return Mysql::MakeHandshakeResponse41(sequence_id, client_flag, user,
					      ToStringView(auth_response),
					      database,
					      "mysql_native_password"sv);
}

PacketSerializer
MakeHandshakeResponse41(const HandshakePacket &handshake,
			uint_least8_t sequence_id, uint_least32_t client_flag,
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

		return MakeHandshakeResponse41SHA1(handshake, sequence_id, client_flag,
						   user, SHA1(password),
						   database);
	}

	return Mysql::MakeHandshakeResponse41(sequence_id, client_flag,
					      user, password, database,
					      "mysql_clear_password"sv);
}

PacketSerializer
MakeAuthSwitchResponse(const AuthSwitchRequest &auth_switch_request,
		       uint_least8_t sequence_id,
		       std::string_view password,
		       std::string_view _password_sha1)
{
	if (auth_switch_request.auth_plugin_name == "mysql_native_password"sv) {
		if (auth_switch_request.auth_plugin_data.size() != 21 ||
		    auth_switch_request.auth_plugin_data.back() != '\0')
			throw SocketProtocolError{"Malformed auth_plugin_data"};

		SHA1Digest password_sha1_buffer;

		std::span<const std::byte> password_sha1 = AsBytes(_password_sha1);
		if (password_sha1.empty())
			password_sha1 = password_sha1_buffer = SHA1(password);

		const auto auth_response =
			MakeMysqlNativePasswordSHA1(password_sha1,
						    auth_switch_request.auth_plugin_data.substr(0, 20),
						    {});

		Mysql::PacketSerializer s{sequence_id};
		s.WriteN(auth_response);
		return s;
	} else
		throw SocketProtocolError{"Unsupported auth_plugin"};
}

} // namespace Mysql
