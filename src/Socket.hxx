// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/net/BufferedSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "event/SocketEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <cstddef> // for std::byte

struct Socket {
	BufferedSocket socket;

	CoarseTimerEvent read_timeout;

	BufferedSocketHandler &handler;

	Socket(EventLoop &event_loop,
	       UniqueSocketDescriptor fd,
	       BufferedSocketHandler &_handler) noexcept;

	~Socket() noexcept;

private:
	void OnReadTimeout() noexcept;
};

void
socket_schedule_read(Socket *s, bool timeout);

void
socket_unschedule_read(Socket *s);

void
socket_schedule_write(Socket *s, bool timeout);

void
socket_unschedule_write(Socket *s);
