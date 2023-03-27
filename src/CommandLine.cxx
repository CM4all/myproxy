// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

	static constexpr auto active_hints = MakeAddrInfo(AI_ADDRCONFIG, AF_UNSPEC, SOCK_STREAM);
	config.server_address = Resolve(argv[1], 3306, &active_hints).GetBest();
}
