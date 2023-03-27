/*
 * A stream socket.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/UniqueSocketDescriptor.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <cstddef> // for std::byte

#include <event.h>

enum socket_state {
	SOCKET_CLOSED,

	SOCKET_CONNECTING,

	SOCKET_ALIVE,
};

struct Socket {
	enum socket_state state;

	const UniqueSocketDescriptor fd;

	StaticFifoBuffer<std::byte, 4096> input;

	struct event read_event, write_event;

	Socket(enum socket_state _state,
	       UniqueSocketDescriptor _fd,
	       void (*read_callback)(int, short, void *),
	       void (*write_callback)(int, short, void *),
	       void *arg) noexcept;

	~Socket() noexcept;
};

void
socket_schedule_read(Socket *s, bool timeout);

void
socket_unschedule_read(Socket *s);

void
socket_schedule_write(Socket *s, bool timeout);

void
socket_unschedule_write(Socket *s);

ssize_t
socket_recv_to_buffer(Socket *s);

ssize_t
socket_send_from_buffer(Socket *s, StaticFifoBuffer<std::byte, 4096> &buffer);

ssize_t
socket_send_from_buffer_n(Socket *s, StaticFifoBuffer<std::byte, 4096> &buffer,
			  size_t max);
