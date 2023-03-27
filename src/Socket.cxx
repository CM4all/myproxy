/*
 * A stream socket.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Socket.hxx"
#include "BufferedIO.hxx"

#include <cassert>

#include <unistd.h>
#include <limits.h>

static const struct timeval socket_timeout = {
	.tv_sec = 60,
	.tv_usec = 0,
};

Socket::Socket(enum socket_state _state,
	       UniqueSocketDescriptor _fd,
	       void (*read_callback)(int, short, void *),
	       void (*write_callback)(int, short, void *),
	       void *arg) noexcept
	:state(_state), fd(std::move(_fd))
{
	assert(state == SOCKET_CONNECTING || state == SOCKET_ALIVE);
	assert(fd.IsDefined());
	assert(read_callback != NULL);
	assert(write_callback != NULL);

	event_set(&read_event, fd.Get(), EV_READ|EV_TIMEOUT, read_callback, arg);
	event_set(&write_event, fd.Get(), EV_WRITE|EV_TIMEOUT, write_callback, arg);
}

Socket::~Socket() noexcept
{
	assert(state != SOCKET_CLOSED);
	assert(fd.IsDefined());

	event_del(&read_event);
	event_del(&write_event);
}

void
socket_schedule_read(Socket *s, bool timeout)
{
	assert(s != NULL);
	assert(s->state != SOCKET_CLOSED);
	assert(s->fd.IsDefined());

	event_add(&s->read_event, timeout ? &socket_timeout : NULL);
}

void
socket_unschedule_read(Socket *s)
{
	assert(s != NULL);
	assert(s->state != SOCKET_CLOSED);
	assert(s->fd.IsDefined());

	event_del(&s->read_event);
}

void
socket_schedule_write(Socket *s, bool timeout)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);
	assert(s->fd.IsDefined());

	event_add(&s->write_event, timeout ? &socket_timeout : NULL);
}

void
socket_unschedule_write(Socket *s)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);
	assert(s->fd.IsDefined());

	event_del(&s->write_event);
}

ssize_t
socket_recv_to_buffer(Socket *s)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);
	assert(s->fd.IsDefined());

	return recv_to_buffer(s->fd.Get(), s->input, INT_MAX);
}

ssize_t
socket_send_from_buffer(Socket *s, StaticFifoBuffer<std::byte, 4096> &buffer)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);
	assert(s->fd.IsDefined());

	return send_from_buffer(s->fd.Get(), buffer);
}

ssize_t
socket_send_from_buffer_n(Socket *s, StaticFifoBuffer<std::byte, 4096> &buffer,
			  size_t max)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);
	assert(s->fd.IsDefined());

	return send_from_buffer_n(s->fd.Get(), buffer, max);
}
