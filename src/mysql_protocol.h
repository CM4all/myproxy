/*
 * Definitions for the MySQL protocol.
 *
 * http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_MYSQL_PROTOCOL_H
#define MYPROXY_MYSQL_PROTOCOL_H

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

#endif
