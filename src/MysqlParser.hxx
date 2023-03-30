// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Mysql {

struct HandshakePacket {
	std::string_view server_version;

	std::string_view auth_plugin_data1, auth_plugin_data2;
	std::string_view auth_plugin_name;

	std::string_view scramble;

	uint_least32_t capabilities;
	uint_least32_t thread_id;

	uint_least16_t status_flags;

	uint_least8_t protocol_version;
	uint_least8_t character_set;
};

HandshakePacket
ParseHandshake(std::span<const std::byte> payload);

struct HandshakeResponsePacket {
	std::string_view username, auth_response;
	std::string_view database;
	std::string_view client_plugin_name;

	uint_least32_t capabilities;
	uint_least32_t max_packet_size;

	uint_least8_t character_set;
};

HandshakeResponsePacket
ParseHandshakeResponse(std::span<const std::byte> payload);

} // namespace Mysql
