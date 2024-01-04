// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Check.hxx"
#include "Options.hxx"
#include "memory/fb_pool.hxx"
#include "event/Loop.hxx"
#include "net/AddressInfo.hxx"
#include "net/Resolver.hxx"
#include "util/Cancellable.hxx"
#include "util/PrintException.hxx"

#include <fmt/format.h>

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

	const ScopeFbPoolInit fb_pool_init;

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

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
