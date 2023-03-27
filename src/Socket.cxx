// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Socket.hxx"

#include <cassert>

#include <unistd.h>
#include <limits.h>

static constexpr Event::Duration socket_timeout = std::chrono::minutes{1};

Socket::Socket(EventLoop &event_loop,
	       enum socket_state _state,
	       UniqueSocketDescriptor fd,
	       BufferedSocketHandler &_handler) noexcept
	:state(_state),
	 socket(event_loop),
	 read_timeout(event_loop, BIND_THIS_METHOD(OnReadTimeout)),
	 handler(_handler)
{
	assert(state == SOCKET_CONNECTING || state == SOCKET_ALIVE);
	assert(fd.IsDefined());

	socket.Init(fd.Release(), FD_TCP, socket_timeout, handler);
}

Socket::~Socket() noexcept
{
	assert(state != SOCKET_CLOSED);

	socket.Close();
}

void
socket_schedule_read(Socket *s, bool timeout)
{
	assert(s != NULL);
	assert(s->state != SOCKET_CLOSED);

	s->socket.ScheduleRead();

	if (timeout)
		s->read_timeout.Schedule(socket_timeout);
}

void
socket_unschedule_read(Socket *s)
{
	assert(s != NULL);
	assert(s->state != SOCKET_CLOSED);

	s->socket.UnscheduleRead();
	s->read_timeout.Cancel();
}

void
socket_schedule_write(Socket *s, bool timeout)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);

	s->socket.SetWriteTimeout(timeout ? socket_timeout : Event::Duration{1});
	s->socket.ScheduleWrite();
}

void
socket_unschedule_write(Socket *s)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);

	s->socket.UnscheduleWrite();
}

void
Socket::OnReadTimeout() noexcept
{
	handler.OnBufferedTimeout();
}

