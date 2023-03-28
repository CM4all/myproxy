// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MysqlReader.hxx"
#include "MysqlHandler.hxx"
#include "MysqlProtocol.hxx"

#include <cstring>

size_t
MysqlReader::Feed(std::span<const std::byte> src) noexcept
{
	assert(!src.empty());

	size_t nbytes = 0;

	if (forward > 0)
		return forward;

	if (remaining > 0) {
		/* consume the remainder of the previous packet */
		assert(!have_packet);

		if (src.size() <= remaining) {
			/* previous packet not yet done, consume all of the
			   caller's input buffer */
			remaining -= src.size();
			forward = src.size();
			return src.size();
		}

		/* consume the remainder and then continue with the next
		   packet */
		src = src.subspan(remaining);
		nbytes += remaining;
		remaining = 0;
	}

	assert(remaining == 0);

	if (!have_packet) {
		/* start of a new packet */
		const auto &header = *(const Mysql::PacketHeader *)src.data();

		if (src.size() < sizeof(header)) {
			/* need more data to complete the header */
			forward = nbytes;
			return nbytes;
		}

		have_packet = true;
		number = header.number;
		payload_length = header.GetLength();
		payload_available = 0;

		src = src.subspan(sizeof(header));
		nbytes += sizeof(header);
	}

	if (src.size() > payload_length - payload_available)
		/* limit "length" to the rest of the current packet */
		src = src.first(payload_length - payload_available);

	if (src.size() > sizeof(payload) - payload_available)
		src = src.first(sizeof(payload) - payload_available);

	std::copy(src.begin(), src.end(), payload.begin());
	payload_available += src.size();
	nbytes += src.size();

	if (payload_available == payload_length ||
	    payload_available == sizeof(payload)) {
		have_packet = false;
		handler.OnMysqlPacket(number, payload_length,
				      std::span{payload}.first(payload_available));

		remaining = payload_length - payload_available;
	}

	forward = nbytes;
	return nbytes;
}
