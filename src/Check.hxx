// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>

struct CheckOptions;
class EventLoop;
class SocketAddress;
class CancellablePointer;

enum class CheckServerResult : uint_least8_t {
	OK,
	READ_ONLY,
	ERROR,
};

class CheckServerHandler {
public:
	virtual void OnCheckServer(CheckServerResult result) noexcept = 0;
};

/**
 * Check whether the given MySQL server is available.
 */
void
CheckServer(EventLoop &event_loop, SocketAddress address,
	    const CheckOptions &options,
	    CheckServerHandler &handler, CancellablePointer &cancel_ptr) noexcept;
