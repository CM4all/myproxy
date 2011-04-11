/*
 * Manage connections from MySQL clients.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "connection.h"
#include "instance.h"
#include "buffered_io.h"
#include "fd_util.h"
#include "fifo_buffer.h"

#include <inline/compiler.h>

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>

void
connection_close(struct connection *connection)
{
    socket_close(&connection->client.socket);
    socket_close(&connection->server.socket);

    list_remove(&connection->siblings);

    free(connection);
}

/**
 * @return true if the connection is still valid
 */
static bool
connection_send_to_socket(struct connection *connection,
                          struct socket *s, struct fifo_buffer *buffer,
                          size_t max)
{
    ssize_t remaining = socket_send_from_buffer_n(s, buffer, max);
    assert(remaining != -2);

    if (remaining < 0) {
        connection_close(connection);
        return false;
    }

    if (remaining > 0)
        socket_schedule_write(s, fifo_buffer_full(buffer));
    else
        socket_unschedule_write(s);

    return true;
}

static bool
connection_forward(struct connection *connection,
                   struct peer *src, struct peer *dest, size_t length)
{
    size_t before = fifo_buffer_available(src->socket.input);

    if (!connection_send_to_socket(connection, &dest->socket,
                                   src->socket.input, length))
        return false;

    size_t after = fifo_buffer_available(src->socket.input);
    assert(after <= before);
    size_t nbytes = before - after;

    if (nbytes > 0)
        mysql_reader_forwarded(&src->reader, nbytes);

    return true;
}

static bool
connection_handle_client_input(struct connection *connection)
{
    while (true) {
        size_t nbytes = peer_feed(&connection->client);
        if (nbytes == 0)
            return true;

        if (!connection_forward(connection, &connection->client,
                                &connection->server, nbytes))
            return false;
    }
}

static bool
connection_handle_server_input(struct connection *connection)
{
    while (true) {
        size_t nbytes = peer_feed(&connection->server);
        if (nbytes == 0)
            return true;

        if (!connection_forward(connection, &connection->server,
                                &connection->client, nbytes))
            return false;
    }
}

static void
connection_client_read_callback(__attr_unused int fd,
                                short event, void *ctx)
{
    struct connection *connection = ctx;

    if (event & EV_TIMEOUT) {
        connection_close(connection);
        return;
    }

    ssize_t nbytes = socket_recv_to_buffer(&connection->client.socket);
    if (nbytes < 0 && errno == EAGAIN) {
        socket_schedule_read(&connection->client.socket, false);
        return;
    }

    if (nbytes <= 0) {
        connection_close(connection);
        return;
    }

    if (connection_handle_client_input(connection) &&
        !fifo_buffer_full(connection->client.socket.input))
        socket_schedule_read(&connection->client.socket, false);
}

static void
connection_client_write_callback(__attr_unused int fd, short event, void *ctx)
{
    struct connection *connection = ctx;

    if (event & EV_TIMEOUT) {
        connection_close(connection);
        return;
    }

    if (connection_handle_server_input(connection) &&
        !fifo_buffer_full(connection->server.socket.input))
        socket_schedule_read(&connection->server.socket, false);
}

static void
connection_server_read_callback(__attr_unused int fd,
                                short event, void *ctx)
{
    struct connection *connection = ctx;

    if (event & EV_TIMEOUT) {
        connection_close(connection);
        return;
    }

    if (connection->server.socket.state == SOCKET_CONNECTING) {
        int s_err = 0;
        socklen_t s_err_size = sizeof(s_err);

        if (getsockopt(connection->server.socket.fd, SOL_SOCKET, SO_ERROR,
                       (char*)&s_err, &s_err_size) < 0 ||
            s_err != 0) {
            connection_close(connection);
            return;
        }

        connection->server.socket.state = SOCKET_ALIVE;
        socket_schedule_read(&connection->server.socket, false);
        return;
    }

    ssize_t nbytes = socket_recv_to_buffer(&connection->server.socket);
    if (nbytes < 0 && errno == EAGAIN) {
        socket_schedule_read(&connection->server.socket, false);
        return;
    }

    if (nbytes <= 0) {
        connection_close(connection);
        return;
    }

    if (connection_handle_server_input(connection) &&
        !fifo_buffer_full(connection->server.socket.input))
        socket_schedule_read(&connection->server.socket, false);
}

static void
connection_server_write_callback(__attr_unused int fd, short event, void *ctx)
{
    struct connection *connection = ctx;

    if (event & EV_TIMEOUT) {
        connection_close(connection);
        return;
    }

    if (connection_handle_client_input(connection) &&
        !fifo_buffer_full(connection->client.socket.input))
        socket_schedule_read(&connection->client.socket, false);
}

static void
connection_mysql_client_packet(unsigned number, size_t length,
                               const void *data, size_t available,
                               void *ctx)
{
    struct connection *connection = ctx;

    (void)connection;
    (void)number;
    (void)length;
    (void)data;
    (void)available;
}

static const struct mysql_handler connection_mysql_client_handler = {
    .packet = connection_mysql_client_packet,
};

static void
connection_mysql_server_packet(unsigned number, size_t length,
                               const void *data, size_t available,
                               void *ctx)
{
    struct connection *connection = ctx;

    (void)connection;
    (void)number;
    (void)length;
    (void)data;
    (void)available;
}

static const struct mysql_handler connection_mysql_server_handler = {
    .packet = connection_mysql_server_packet,
};

struct connection *
connection_new(struct instance *instance, int fd)
{
    struct connection *connection = malloc(sizeof(*connection));
    connection->instance = instance;

    socket_init(&connection->client.socket, SOCKET_ALIVE, fd, 4096,
                connection_client_read_callback,
                connection_client_write_callback, connection);
    mysql_reader_init(&connection->client.reader,
                      &connection_mysql_client_handler, connection);

    const struct addrinfo *address = connection->instance->server_address;
    assert(address != NULL);
    fd = socket_cloexec_nonblock(address->ai_family, address->ai_socktype,
                                 address->ai_protocol);
    if (fd < 0) {
        socket_close(&connection->client.socket);
        return NULL;
    }

    int ret = connect(fd, address->ai_addr, address->ai_addrlen);
    if (ret < 0 && errno != EINPROGRESS) {
        socket_close(&connection->client.socket);
        return NULL;
    }

    socket_init(&connection->server.socket, SOCKET_CONNECTING, fd, 4096,
                connection_server_read_callback,
                connection_server_write_callback, connection);
    mysql_reader_init(&connection->server.reader,
                      &connection_mysql_server_handler, connection);

    socket_schedule_read(&connection->client.socket, false);
    socket_schedule_read(&connection->server.socket, true);

    event_add(&connection->client.socket.read_event, NULL);

    return connection;
}
