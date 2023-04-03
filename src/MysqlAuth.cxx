// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MysqlAuth.hxx"
#include "MysqlParser.hxx"
#include "MysqlMakePacket.hxx"
#include "MysqlSerializer.hxx"

using std::string_view_literals::operator""sv;

namespace Mysql {

PacketSerializer
MakeHandshakeResponse41(const HandshakePacket &handshake,
			std::string_view username, std::string_view password,
			std::string_view database)
{
	// TODO implement "mysql_native_password"
	(void)handshake;

	return Mysql::MakeHandshakeResponse41(username, password, database,
					      "mysql_clear_password"sv);
}

} // namespace Mysql
