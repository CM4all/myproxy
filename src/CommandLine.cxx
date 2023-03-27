/*
 * Command-line parser.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CommandLine.hxx"
#include "Config.hxx"
#include "net/AddressInfo.hxx"
#include "net/Resolver.hxx"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

void
parse_cmdline(Config &config, int argc, char **argv)
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

	config.server_address = Resolve(argv[1], 3306, &hints).GetBest();
}
