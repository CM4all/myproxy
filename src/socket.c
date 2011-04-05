/*
 * A stream socket.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "socket.h"
#include "fifo_buffer.h"
#include "buffered_io.h"

#include <assert.h>
#include <unistd.h>
#include <limits.h>

static const struct timeval socket_timeout = {
    .tv_sec = 60,
    .tv_usec = 0,
};

void
socket_init(struct socket *s, enum socket_state state,
            int fd, size_t input_buffer_size,
            void (*read_callback)(int, short, void *),
            void (*write_callback)(int, short, void *),
            void *arg)
{
    assert(s != NULL);
    assert(state == SOCKET_CONNECTING || state == SOCKET_ALIVE);
    assert(fd >= 0);
    assert(input_buffer_size > 0);
    assert(read_callback != NULL);
    assert(write_callback != NULL);

    s->state = state;
    s->fd = fd;
    s->input = fifo_buffer_new(input_buffer_size);

    event_set(&s->read_event, fd, EV_READ|EV_TIMEOUT, read_callback, arg);
    event_set(&s->write_event, fd, EV_WRITE|EV_TIMEOUT, write_callback, arg);
}

void
socket_close(struct socket *s)
{
    assert(s != NULL);
    assert(s->state != SOCKET_CLOSED);
    assert(s->fd >= 0);

    event_del(&s->read_event);
    event_del(&s->write_event);

    close(s->fd);

    fifo_buffer_free(s->input);
}

void
socket_schedule_read(struct socket *s, bool timeout)
{
    assert(s != NULL);
    assert(s->state != SOCKET_CLOSED);
    assert(s->fd >= 0);

    event_add(&s->read_event, timeout ? &socket_timeout : NULL);
}

void
socket_unschedule_read(struct socket *s)
{
    assert(s != NULL);
    assert(s->state != SOCKET_CLOSED);
    assert(s->fd >= 0);

    event_del(&s->read_event);
}

void
socket_schedule_write(struct socket *s, bool timeout)
{
    assert(s != NULL);
    assert(s->state == SOCKET_ALIVE);
    assert(s->fd >= 0);

    event_add(&s->write_event, timeout ? &socket_timeout : NULL);
}

void
socket_unschedule_write(struct socket *s)
{
    assert(s != NULL);
    assert(s->state == SOCKET_ALIVE);
    assert(s->fd >= 0);

    event_del(&s->write_event);
}

ssize_t
socket_recv_to_buffer(struct socket *s)
{
    assert(s != NULL);
    assert(s->state == SOCKET_ALIVE);
    assert(s->fd >= 0);

    return recv_to_buffer(s->fd, s->input, INT_MAX);
}

ssize_t
socket_send_from_buffer(struct socket *s, struct fifo_buffer *buffer)
{
    assert(s != NULL);
    assert(s->state == SOCKET_ALIVE);
    assert(s->fd >= 0);

    return send_from_buffer(s->fd, buffer);
}
