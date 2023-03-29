// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Mysql {

class PacketSerializer;

PacketSerializer
MakeHandshakeV10(std::string_view server_version,
		 std::string_view auth_plugin_name,
		 std::span<const std::byte> auth_plugin_data);

PacketSerializer
MakeHandshakeResponse41(std::string_view username, std::string_view auth_response,
			std::string_view database,
			std::string_view client_plugin_name);

PacketSerializer
MakeOk(uint_least8_t sequence_id, uint_least32_t capabilities);

} // namespace Mysql
