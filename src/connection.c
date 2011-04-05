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
    socket_close(&connection->client);
    socket_close(&connection->server);

    list_remove(&connection->siblings);

    free(connection);
}

/**
 * @return true if the connection is still valid
 */
static bool
connection_send_to_socket(struct connection *connection,
                          struct socket *s, struct fifo_buffer *buffer)
{
    ssize_t remaining = socket_send_from_buffer(s, buffer);
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
connection_handle_client_input(struct connection *connection)
{
    return connection_send_to_socket(connection, &connection->server,
                                     connection->client.input);
}

static bool
connection_handle_server_input(struct connection *connection)
{
    return connection_send_to_socket(connection, &connection->client,
                                     connection->server.input);
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

    ssize_t nbytes = socket_recv_to_buffer(&connection->client);
    if (nbytes < 0 && errno == EAGAIN) {
        socket_schedule_read(&connection->client, false);
        return;
    }

    if (nbytes <= 0) {
        connection_close(connection);
        return;
    }

    if (connection_handle_client_input(connection) &&
        !fifo_buffer_full(connection->client.input))
        socket_schedule_read(&connection->client, false);
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
        !fifo_buffer_full(connection->server.input))
        socket_schedule_read(&connection->server, false);
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

    if (connection->server.state == SOCKET_CONNECTING) {
        int s_err = 0;
        socklen_t s_err_size = sizeof(s_err);

        if (getsockopt(connection->server.fd, SOL_SOCKET, SO_ERROR,
                       (char*)&s_err, &s_err_size) < 0 ||
            s_err != 0) {
            connection_close(connection);
            return;
        }

        connection->server.state = SOCKET_ALIVE;
        socket_schedule_read(&connection->server, false);
        return;
    }

    ssize_t nbytes = socket_recv_to_buffer(&connection->server);
    if (nbytes < 0 && errno == EAGAIN) {
        socket_schedule_read(&connection->server, false);
        return;
    }

    if (nbytes <= 0) {
        connection_close(connection);
        return;
    }

    if (connection_handle_server_input(connection) &&
        !fifo_buffer_full(connection->server.input))
        socket_schedule_read(&connection->server, false);
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
        !fifo_buffer_full(connection->client.input))
        socket_schedule_read(&connection->client, false);
}

struct connection *
connection_new(struct instance *instance, int fd)
{
    struct connection *connection = malloc(sizeof(*connection));
    connection->instance = instance;
    connection->client.fd = fd;

    socket_init(&connection->client, SOCKET_ALIVE, fd, 4096,
                connection_client_read_callback,
                connection_client_write_callback, connection);

    const struct addrinfo *address = connection->instance->server_address;
    assert(address != NULL);
    fd = socket_cloexec_nonblock(address->ai_family, address->ai_socktype,
                                 address->ai_protocol);
    if (fd < 0) {
        socket_close(&connection->client);
        return NULL;
    }

    int ret = connect(fd, address->ai_addr, address->ai_addrlen);
    if (ret < 0 && errno != EINPROGRESS) {
        socket_close(&connection->client);
        return NULL;
    }

    socket_init(&connection->server, SOCKET_CONNECTING, fd, 4096,
                connection_server_read_callback,
                connection_server_write_callback, connection);

    socket_schedule_read(&connection->client, false);
    socket_schedule_read(&connection->server, true);

    event_add(&connection->client.read_event, NULL);

    return connection;
}
