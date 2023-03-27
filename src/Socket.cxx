// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Socket.hxx"

#include <cassert>

#include <unistd.h>
#include <limits.h>

static constexpr Event::Duration socket_timeout = std::chrono::minutes{1};

Socket::Socket(EventLoop &event_loop,
	       UniqueSocketDescriptor fd,
	       BufferedSocketHandler &_handler) noexcept
	:socket(event_loop),
	 read_timeout(event_loop, BIND_THIS_METHOD(OnReadTimeout)),
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
socket_schedule_read(Socket *s, bool timeout)
{
	assert(s != NULL);

	s->socket.ScheduleRead();

	if (timeout)
		s->read_timeout.Schedule(socket_timeout);
}

void
socket_unschedule_read(Socket *s)
{
	assert(s != NULL);

	s->socket.UnscheduleRead();
	s->read_timeout.Cancel();
}

void
Socket::OnReadTimeout() noexcept
{
	handler.OnBufferedTimeout();
}

