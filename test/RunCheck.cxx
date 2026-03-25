// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Check.hxx"
#include "Options.hxx"
#include "event/Loop.hxx"
#include "net/AddressInfo.hxx"
#include "net/Resolver.hxx"
#include "util/Cancellable.hxx"
#include "util/PrintException.hxx"
#include "DefaultFifoBuffer.hxx"

#include <fmt/format.h>

#include <cassert>

int
main(int argc, char **argv) noexcept
try {
	if (argc != 4) {
		fmt::print(stderr, "Usage: {} ADDRESS USER PASSWORD\n",
			   argv[0]);
		return EXIT_FAILURE;
	}

	const auto address = Resolve(argv[1], 3306, 0, SOCK_STREAM);

	const CheckOptions options {
		.user = argv[2],
		.password = argv[3],
		.no_read_only = true,
	};

	const ScopeInitDefaultFifoBuffer init_default_fifo_buffer;

	EventLoop event_loop;

	struct Handler final : public CheckServerHandler {
		bool finished = false;
		CheckServerResult result;

		void OnCheckServer(CheckServerResult _result) noexcept override {
			result = _result;
			finished = true;
		}
	} handler;

	CancellablePointer cancel_ptr;

	CheckServer(event_loop, address.GetBest(), options,
		    handler, cancel_ptr);

	if (!handler.finished)
		event_loop.Run();

	assert(handler.finished);

	switch (handler.result) {
	case CheckServerResult::OK:
		return EXIT_SUCCESS;

	case CheckServerResult::READ_ONLY:
		fmt::print(stderr, "read_only\n");
		return EXIT_FAILURE;

	case CheckServerResult::AUTH_FAILED:
		fmt::print(stderr, "auth_failed\n");
		return EXIT_FAILURE;

	case CheckServerResult::ERROR:
		fmt::print(stderr, "error\n");
		return EXIT_FAILURE;
	}
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
