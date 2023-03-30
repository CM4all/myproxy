// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CommandLine.hxx"
#include "Config.hxx"
#include "util/StringAPI.hxx"

void
parse_cmdline(Config &config, int argc, char **argv)
{
	if (argc == 3 && StringIsEqual(argv[1], "--config"))
		config.config_path = argv[2];
	else if (argc != 1)
		throw "Usage: cm4all-myproxy [--config PATH]";
}
