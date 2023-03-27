// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MysqlReader.hxx"
#include "mysql_protocol.h"

#include <cstring>

size_t
MysqlReader::Feed(const void *data, size_t length) noexcept
{
	size_t nbytes = 0;

	assert(data != NULL);
	assert(length > 0);

	if (forward > 0)
		return forward;

	if (remaining > 0) {
		/* consume the remainder of the previous packet */
		assert(!have_packet);

		if (length <= remaining) {
			/* previous packet not yet done, consume all of the
			   caller's input buffer */
			remaining -= length;
			forward = length;
			return length;
		}

		/* consume the remainder and then continue with the next
		   packet */
		data = (const uint8_t *)data + remaining;
		length -= remaining;
		nbytes += remaining;
		remaining = 0;
	}

	assert(remaining == 0);

	if (!have_packet) {
		/* start of a new packet */
		const struct mysql_packet_header *header =
			(const struct mysql_packet_header *)data;

		if (length < sizeof(*header)) {
			/* need more data to complete the header */
			forward = nbytes;
			return nbytes;
		}

		have_packet = true;
		number = header->number;
		payload_length = mysql_packet_length(header);
		payload_available = 0;

		data = header + 1;
		length -= sizeof(*header);
		nbytes += sizeof(*header);
	}

	if (length > payload_length - payload_available)
		/* limit "length" to the rest of the current packet */
		length = payload_length - payload_available;

	if (length > sizeof(payload) - payload_available)
		length = sizeof(payload) - payload_available;

	memcpy(payload + payload_available, data, length);
	payload_available += length;
	nbytes += length;

	if (payload_available == payload_length ||
	    payload_available == sizeof(payload)) {
		have_packet = false;
		handler->packet(number, payload_length,
					payload, payload_available,
					handler_ctx);

		remaining = payload_length - payload_available;
	}

	forward = nbytes;
	return nbytes;
}
