/*
 * A stream socket.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Socket.hxx"
#include "fifo_buffer.h"

extern "C" {
#include "buffered_io.h"
}

#include <cassert>

#include <unistd.h>
#include <limits.h>

static const struct timeval socket_timeout = {
	.tv_sec = 60,
	.tv_usec = 0,
};

Socket::Socket(enum socket_state _state,
	       int _fd, size_t input_buffer_size,
	       void (*read_callback)(int, short, void *),
	       void (*write_callback)(int, short, void *),
	       void *arg) noexcept
	:state(_state), fd(_fd),
	 input(fifo_buffer_new(input_buffer_size))
{
	assert(state == SOCKET_CONNECTING || state == SOCKET_ALIVE);
	assert(fd >= 0);
	assert(input_buffer_size > 0);
	assert(read_callback != NULL);
	assert(write_callback != NULL);

	event_set(&read_event, fd, EV_READ|EV_TIMEOUT, read_callback, arg);
	event_set(&write_event, fd, EV_WRITE|EV_TIMEOUT, write_callback, arg);
}

Socket::~Socket() noexcept
{
	assert(state != SOCKET_CLOSED);
	assert(fd >= 0);

	event_del(&read_event);
	event_del(&write_event);

	close(fd);

	fifo_buffer_free(input);
}

void
socket_schedule_read(Socket *s, bool timeout)
{
	assert(s != NULL);
	assert(s->state != SOCKET_CLOSED);
	assert(s->fd >= 0);

	event_add(&s->read_event, timeout ? &socket_timeout : NULL);
}

void
socket_unschedule_read(Socket *s)
{
	assert(s != NULL);
	assert(s->state != SOCKET_CLOSED);
	assert(s->fd >= 0);

	event_del(&s->read_event);
}

void
socket_schedule_write(Socket *s, bool timeout)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);
	assert(s->fd >= 0);

	event_add(&s->write_event, timeout ? &socket_timeout : NULL);
}

void
socket_unschedule_write(Socket *s)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);
	assert(s->fd >= 0);

	event_del(&s->write_event);
}

ssize_t
socket_recv_to_buffer(Socket *s)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);
	assert(s->fd >= 0);

	return recv_to_buffer(s->fd, s->input, INT_MAX);
}

ssize_t
socket_send_from_buffer(Socket *s, struct fifo_buffer *buffer)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);
	assert(s->fd >= 0);

	return send_from_buffer(s->fd, buffer);
}

ssize_t
socket_send_from_buffer_n(Socket *s, struct fifo_buffer *buffer,
			  size_t max)
{
	assert(s != NULL);
	assert(s->state == SOCKET_ALIVE);
	assert(s->fd >= 0);

	return send_from_buffer_n(s->fd, buffer, max);
}
