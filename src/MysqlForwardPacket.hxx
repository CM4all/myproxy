// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstdint>

namespace Mysql {

class PacketSerializer;
struct OkPacket;
struct ErrPacket;

PacketSerializer
MakeOk(uint_least8_t sequence_id, uint_least32_t capabilities,
       const OkPacket &src);

PacketSerializer
MakeErr(uint_least8_t sequence_id, uint_least32_t capabilities,
	const ErrPacket &src);

} // namespace Mysql
