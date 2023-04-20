// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <sha1.h> // for SHA1_DIGEST_LENGTH

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Mysql {

struct HandshakePacket;
class PacketSerializer;

PacketSerializer
MakeHandshakeResponse41SHA1(const HandshakePacket &handshake, uint_least32_t client_flag,
			    std::string_view user,
			    std::span<const std::byte, SHA1_DIGEST_LENGTH> password_sha1,
			    std::string_view database);

PacketSerializer
MakeHandshakeResponse41(const HandshakePacket &handshake,
			uint_least32_t client_flag,
			std::string_view user, std::string_view password,
			std::string_view database);

} // namespace Mysql
