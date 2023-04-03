// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>
#include <string_view>

namespace Mysql {

struct HandshakePacket;
class PacketSerializer;

PacketSerializer
MakeHandshakeResponse41(const HandshakePacket &handshake,
			uint_least32_t client_flag,
			std::string_view username, std::string_view password,
			std::string_view database);

} // namespace Mysql
