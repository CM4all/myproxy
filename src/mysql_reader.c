/*
 * Definitions for the MySQL protocol.
 *
 * http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "mysql_reader.h"
#include "mysql_protocol.h"

#include <string.h>

void
mysql_reader_init(struct mysql_reader *reader,
                  const struct mysql_handler *handler, void *ctx)
{
    assert(handler != NULL);
    assert(handler->packet != NULL);

    reader->handler = handler;
    reader->handler_ctx = ctx;
    reader->have_packet = false;
    reader->forward = 0;
    reader->remaining = 0;
}

size_t
mysql_reader_feed(struct mysql_reader *reader,
                  const void *data, size_t length)
{
    size_t nbytes = 0;

    assert(reader != NULL);
    assert(data != NULL);
    assert(length > 0);

    if (reader->forward > 0)
        return reader->forward;

    if (reader->remaining > 0) {
        /* consume the remainder of the previous packet */
        assert(!reader->have_packet);

        if (length <= reader->remaining) {
            /* previous packet not yet done, consume all of the
               caller's input buffer */
            reader->remaining -= length;
            reader->forward = length;
            return length;
        }

        /* consume the remainder and then continue with the next
           packet */
        data = (const uint8_t *)data + reader->remaining;
        length -= reader->remaining;
        nbytes += reader->remaining;
        reader->remaining = 0;
    }

    assert(reader->remaining == 0);

    if (!reader->have_packet) {
        /* start of a new packet */
        const struct mysql_packet_header *header = data;

        if (length < sizeof(*header)) {
            /* need more data to complete the header */
            reader->forward = nbytes;
            return nbytes;
        }

        reader->have_packet = true;
        reader->number = header->number;
        reader->payload_length = mysql_packet_length(header);
        reader->payload_available = 0;

        data = header + 1;
        length -= sizeof(*header);
        nbytes += sizeof(*header);
    }

    if (length > reader->payload_length - reader->payload_available)
        /* limit "length" to the rest of the current packet */
        length = reader->payload_length - reader->payload_available;

    if (length > sizeof(reader->payload) - reader->payload_available)
        length = sizeof(reader->payload) - reader->payload_available;

    memcpy(reader->payload + reader->payload_available, data, length);
    reader->payload_available += length;
    nbytes += length;

    if (reader->payload_available == reader->payload_length ||
        reader->payload_available == sizeof(reader->payload)) {
        reader->have_packet = false;
        reader->handler->packet(reader->number, reader->payload_length,
                                reader->payload, reader->payload_available,
                                reader->handler_ctx);

        reader->remaining = reader->payload_length - reader->payload_available;
    }

    reader->forward = nbytes;
    return nbytes;
}
