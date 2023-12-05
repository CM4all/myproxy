// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Mysql {

enum class ErrorCode : uint_least16_t;
class PacketSerializer;

PacketSerializer
MakeHandshakeV10(std::string_view server_version,
		 uint_least32_t capabilities,
		 std::string_view auth_plugin_name,
		 std::span<const std::byte> auth_plugin_data);

PacketSerializer
MakeHandshakeResponse41(uint_least8_t sequence_id, uint_least32_t client_flag,
			std::string_view user, std::string_view auth_response,
			std::string_view database,
			std::string_view client_plugin_name);

PacketSerializer
MakeResetConnection(uint_least8_t sequence_id);

PacketSerializer
MakeOk(uint_least8_t sequence_id, uint_least32_t capabilities);

PacketSerializer
MakeErr(uint_least8_t sequence_id, uint_least32_t capabilities,
	ErrorCode error_code,
	std::string_view sql_state, std::string_view msg);

PacketSerializer
MakeQuery(uint_least8_t sequence_id, std::string_view query);

} // namespace Mysql
