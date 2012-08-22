/*
 * A stream socket.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef MYPROXY_SOCKET_H
#define MYPROXY_SOCKET_H

#include <stdbool.h>
#include <event.h>

enum socket_state {
    SOCKET_CLOSED,

    SOCKET_CONNECTING,

    SOCKET_ALIVE,
};

struct socket {
    enum socket_state state;

    int fd;

    struct fifo_buffer *input;

    struct event read_event, write_event;
};

void
socket_init(struct socket *s, enum socket_state state,
            int fd, size_t input_buffer_size,
            void (*read_callback)(int, short, void *),
            void (*write_callback)(int, short, void *),
            void *arg);

void
socket_close(struct socket *s);

void
socket_schedule_read(struct socket *s, bool timeout);

void
socket_unschedule_read(struct socket *s);

void
socket_schedule_write(struct socket *s, bool timeout);

void
socket_unschedule_write(struct socket *s);

ssize_t
socket_recv_to_buffer(struct socket *s);

ssize_t
socket_send_from_buffer(struct socket *s, struct fifo_buffer *buffer);

ssize_t
socket_send_from_buffer_n(struct socket *s, struct fifo_buffer *buffer,
                          size_t max);

#endif
