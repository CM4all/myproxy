// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MysqlForwardPacket.hxx"
#include "MysqlMakePacket.hxx"
#include "MysqlParser.hxx"
#include "MysqlSerializer.hxx"

using std::string_view_literals::operator""sv;

namespace Mysql {

PacketSerializer
MakeOk(uint_least8_t sequence_id, uint_least32_t capabilities,
       const OkPacket &src)
{
	return MakeOk(sequence_id, capabilities,
		      src.affected_rows,
		      src.last_insert_id,
		      src.status_flags,
		      src.warnings,
		      src.info,
		      src.session_state_info);
}

PacketSerializer
MakeErr(uint_least8_t sequence_id, uint_least32_t capabilities,
	const ErrPacket &src)
{
	return MakeErr(sequence_id, capabilities,
		       src.error_code,
		       src.sql_state.empty() ? "HY000"sv : src.sql_state,
		       src.error_message);
}

} // namespace Mysql
