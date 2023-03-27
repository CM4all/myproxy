/*
 * Command-line parser.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CommandLine.hxx"
#include "Instance.hxx"

#include <socket/resolver.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

void
parse_cmdline(Instance *instance, int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s HOST[:PORT]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *ai;
    if (socket_resolve_host_port(argv[1], 3306, &hints, &ai) != 0) {
        fprintf(stderr, "Failed to resolve %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    instance->server_address = ai;
}
