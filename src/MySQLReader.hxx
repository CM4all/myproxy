/*
 * Definitions for the MySQL protocol.
 *
 * http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_MYSQL_READER_HXX
#define MYPROXY_MYSQL_READER_HXX

#include <cassert>
#include <cstddef>

struct mysql_handler {
    /**
     * A packet was received.
     *
     * @param number the packet number
     * @param length the full payload length
     * @param data the payload
     * @param available the amount of payload that is available now
     */
    void (*packet)(unsigned number, size_t length,
                   const void *data, size_t available,
                   void *ctx);
};

struct mysql_reader {
    const struct mysql_handler *handler;
    void *handler_ctx;

    bool have_packet;

    /**
     * The number of bytes to forward.
     */
    size_t forward;

    /**
     * The remaining number of the previous packet that should be
     * forwarded as-is.
     */
    size_t remaining;

    unsigned number;

    /**
     * Total length of the current packet payload.
     */
    size_t payload_length;

    /**
     * The amount of payload that is available in the #payload buffer.
     */
    size_t payload_available;

    char payload[1024];
};

void
mysql_reader_init(struct mysql_reader *reader,
                  const struct mysql_handler *handler, void *ctx);

/**
 * Feed data into the reader.  It stops at the boundary of a packet,
 * to allow the caller to inspect each packet.
 *
 * @return the number of bytes that should be forwarded in this step
 */
size_t
mysql_reader_feed(struct mysql_reader *reader,
                  const void *data, size_t length);

/**
 * Indicates that the caller has forwarded the specified number of
 * bytes.
 */
static inline void
mysql_reader_forwarded(struct mysql_reader *reader, size_t nbytes)
{
    assert(nbytes > 0);
    assert(reader->forward > 0);
    assert(nbytes <= reader->forward);

    reader->forward -= nbytes;
}

#endif
