/*
 * Definitions for the MySQL protocol.
 *
 * http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct mysql_packet_header {
    uint8_t length[3];
    uint8_t number;
};

static inline size_t
mysql_packet_length(const struct mysql_packet_header *header)
{
    return header->length[0] | (header->length[1] << 8) |
        (header->length[2] << 16);
}

static inline bool
mysql_is_query_packet(unsigned number, const void *data, size_t length)
{
    return number == 0 && length >= 1 && *(const uint8_t *)data == 0x03;
}

static inline bool
mysql_is_eof_packet(unsigned number, const void *data, size_t length)
{
    return number > 0 && length >= 1 && *(const uint8_t *)data == 0xfe;
}
