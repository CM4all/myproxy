// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "CommandLine.hxx"
#include "Policy.hxx"
#include "Connection.hxx"
#include "system/SetupProcess.hxx"
#include "util/PrintException.hxx"

#include <stddef.h>
#include <stdlib.h>

int
main(int argc, char **argv) noexcept
try {
	Config config;
	parse_cmdline(config, argc, argv);

	SetupProcess();

	Instance instance{config};

	policy_init();

	instance.GetEventLoop().Run();

	policy_deinit();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
