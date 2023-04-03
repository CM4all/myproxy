// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

namespace Mysql {

struct HandshakePacket;
class PacketSerializer;

PacketSerializer
MakeHandshakeResponse41(const HandshakePacket &handshake,
			std::string_view username, std::string_view password,
			std::string_view database);

} // namespace Mysql
