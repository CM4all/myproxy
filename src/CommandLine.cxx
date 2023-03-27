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
	if (argc != 3) {
		fprintf(stderr, "Usage: %s LISTEN HOST[:PORT]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	static constexpr auto passive_hints = MakeAddrInfo(AI_PASSIVE|AI_ADDRCONFIG, AF_UNSPEC, SOCK_STREAM);
	config.listener.bind_address = Resolve(argv[1], 3306, &passive_hints).GetBest();
	config.listener.listen = 16;
	config.listener.reuse_port = true;
	config.listener.tcp_no_delay = true;

	static constexpr auto active_hints = MakeAddrInfo(AI_ADDRCONFIG, AF_UNSPEC, SOCK_STREAM);
	config.server_address = Resolve(argv[2], 3306, &active_hints).GetBest();
}
