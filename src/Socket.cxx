// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Socket.hxx"

#include <cassert>

static constexpr Event::Duration socket_timeout = std::chrono::minutes{1};

Socket::Socket(EventLoop &event_loop,
	       UniqueSocketDescriptor fd,
	       BufferedSocketHandler &_handler) noexcept
	:socket(event_loop),
	 handler(_handler)
{
	assert(fd.IsDefined());

	socket.Init(fd.Release(), FD_TCP, socket_timeout, handler);
}

Socket::~Socket() noexcept
{
	socket.Close();
}

void
socket_schedule_read(Socket *s)
{
	assert(s != NULL);

	s->socket.ScheduleRead();
}

void
socket_unschedule_read(Socket *s)
{
	assert(s != NULL);

	s->socket.UnscheduleRead();
}

void
Socket::OnReadTimeout() noexcept
{
	handler.OnBufferedTimeout();
}

