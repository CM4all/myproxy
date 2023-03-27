/*
 * Manage connections from MySQL clients.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Connection.hxx"
#include "Instance.hxx"
#include "buffered_io.h"
#include "fd_util.h"
#include "fifo_buffer.h"
#include "mysql_protocol.h"
#include "clock.h"
#include "Policy.hxx"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>

Connection::~Connection() noexcept
{
	socket_close(&client.socket);
	socket_close(&server.socket);
	event_del(&delay_timer);
}

/**
 * @return true if the connection is still valid
 */
static bool
connection_send_to_socket(Connection *connection,
			  struct socket *s, struct fifo_buffer *buffer,
			  size_t max)
{
	ssize_t remaining = socket_send_from_buffer_n(s, buffer, max);
	assert(remaining != -2);

	if (remaining < 0) {
		delete connection;
		return false;
	}

	if (remaining > 0)
		socket_schedule_write(s, fifo_buffer_full(buffer));
	else
		socket_unschedule_write(s);

	return true;
}

static bool
connection_forward(Connection *connection,
		   Peer *src, Peer *dest, size_t length)
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
connection_handle_client_input(Connection *connection)
{
	while (true) {
		size_t nbytes = peer_feed(&connection->client);
		if (nbytes == 0)
			return !connection->delayed;

		if (!connection_forward(connection, &connection->client,
					&connection->server, nbytes))
			return false;

		if (connection->delayed)
			/* don't continue reading now */
			return false;
	}
}

static bool
connection_handle_server_input(Connection *connection)
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
connection_client_read_callback([[maybe_unused]] int fd,
				short event, void *ctx)
{
	Connection *connection = (Connection *)ctx;

	assert(!connection->delayed);

	if (event & EV_TIMEOUT) {
		delete connection;
		return;
	}

	ssize_t nbytes = socket_recv_to_buffer(&connection->client.socket);
	if (nbytes < 0 && errno == EAGAIN) {
		socket_schedule_read(&connection->client.socket, false);
		return;
	}

	if (nbytes <= 0) {
		delete connection;
		return;
	}

	if (connection_handle_client_input(connection) &&
	    !fifo_buffer_full(connection->client.socket.input))
		socket_schedule_read(&connection->client.socket, false);
}

static void
connection_client_write_callback([[maybe_unused]] int fd, short event, void *ctx)
{
	Connection *connection = (Connection *)ctx;

	if (event & EV_TIMEOUT) {
		delete connection;
		return;
	}

	if (connection_handle_server_input(connection) &&
	    !fifo_buffer_full(connection->server.socket.input))
		socket_schedule_read(&connection->server.socket, false);
}

static void
connection_server_read_callback([[maybe_unused]] int fd,
				short event, void *ctx)
{
	Connection *connection = (Connection *)ctx;

	if (event & EV_TIMEOUT) {
		delete connection;
		return;
	}

	if (connection->server.socket.state == SOCKET_CONNECTING) {
		int s_err = 0;
		socklen_t s_err_size = sizeof(s_err);

		if (getsockopt(connection->server.socket.fd, SOL_SOCKET, SO_ERROR,
			       (char*)&s_err, &s_err_size) < 0 ||
		    s_err != 0) {
			delete connection;
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
		delete connection;
		return;
	}

	if (connection_handle_server_input(connection) &&
	    !fifo_buffer_full(connection->server.socket.input))
		socket_schedule_read(&connection->server.socket, false);
}

static void
connection_server_write_callback([[maybe_unused]] int fd, short event, void *ctx)
{
	Connection *connection = (Connection *)ctx;

	assert(!connection->delayed);

	if (event & EV_TIMEOUT) {
		delete connection;
		return;
	}

	if (connection_handle_client_input(connection) &&
	    !fifo_buffer_full(connection->client.socket.input))
		socket_schedule_read(&connection->client.socket, false);
}

static bool
connection_login_packet(Connection *connection,
			const char *data, size_t length)
{
	if (length < 33)
		return false;

	const char *user = data + 32;
	const char *user_end = (const char *)
		memchr((const void *)user, 0, data + length - user);
	if (user_end == NULL || user_end == user)
		return false;

	size_t user_length = user_end - user;
	if (user_length >= sizeof(connection->user))
		user_length = sizeof(connection->user) - 1;

	memcpy(connection->user, user, user_length);
	connection->user[user_length] = 0;

	unsigned delay_ms = policy_login(connection->user);
	if (delay_ms > 0)
		connection_delay(connection, delay_ms);

	return true;
}

static void
connection_mysql_client_packet(unsigned number, size_t length,
			       const void *data, size_t available,
			       void *ctx)
{
	Connection *connection = (Connection *)ctx;

	if (mysql_is_query_packet(number, data, length) &&
	    connection->request_time == 0)
		connection->request_time = now_us();

	if (number == 1 && !connection->login_received) {
		connection->login_received =
			connection_login_packet(connection, (const char *)data, available);
	}
}

static const struct mysql_handler connection_mysql_client_handler = {
	.packet = connection_mysql_client_packet,
};

static void
connection_mysql_server_packet(unsigned number, size_t length,
			       const void *data, size_t available,
			       void *ctx)
{
	Connection *connection = (Connection *)ctx;

	(void)length;
	(void)data;
	(void)available;

	if (!connection->greeting_received && number == 0) {
		connection->greeting_received = true;
	}

	if (mysql_is_eof_packet(number, data, length) &&
	    connection->login_received &&
	    connection->request_time != 0) {
		uint64_t duration_us = now_us() - connection->request_time;
		policy_duration(connection->user, (unsigned)(duration_us / 1000));
		connection->request_time = 0;
	}
}

static const struct mysql_handler connection_mysql_server_handler = {
	.packet = connection_mysql_server_packet,
};

/**
 * Called when the artificial delay is over, and restarts the transfer
 * from the client to the server.
 */
static void
connection_delay_timer_callback([[maybe_unused]] int fd,
				[[maybe_unused]] short event, void *ctx)
{
	Connection *connection = (Connection *)ctx;

	assert(connection->delayed);

	connection->delayed = false;

	if (connection_handle_client_input(connection) &&
	    !fifo_buffer_full(connection->client.socket.input))
		socket_schedule_read(&connection->client.socket, false);
}

Connection::Connection(Instance &_instance, int fd)
	:instance(&_instance)
{
	event_set(&delay_timer, -1, EV_TIMEOUT,
		  connection_delay_timer_callback, this);
	delayed = false;

	greeting_received = false;
	login_received = false;
	request_time = 0;

	socket_init(&client.socket, SOCKET_ALIVE, fd, 4096,
		    connection_client_read_callback,
		    connection_client_write_callback, this);
	mysql_reader_init(&client.reader,
			  &connection_mysql_client_handler, this);

	const struct addrinfo *address = instance->server_address;
	assert(address != NULL);
	fd = socket_cloexec_nonblock(address->ai_family, address->ai_socktype,
				     address->ai_protocol);
	if (fd < 0) {
		socket_close(&client.socket);
		throw "Failed to create socket";
	}

	int ret = connect(fd, address->ai_addr, address->ai_addrlen);
	if (ret < 0 && errno != EINPROGRESS) {
		socket_close(&client.socket);
		throw "Failed to create socket";
	}

	socket_init(&server.socket, SOCKET_CONNECTING, fd, 4096,
		    connection_server_read_callback,
		    connection_server_write_callback, this);
	mysql_reader_init(&server.reader,
			  &connection_mysql_server_handler, this);

	socket_schedule_read(&client.socket, false);
	socket_schedule_read(&server.socket, true);

	event_add(&client.socket.read_event, NULL);
}

void
connection_delay(Connection *c, unsigned delay_ms)
{
	assert(delay_ms > 0);

	c->delayed = true;

	socket_unschedule_read(&c->client.socket);

	const struct timeval timeout = {
		.tv_sec = delay_ms / 1000,
		.tv_usec = (delay_ms % 1000) * 1000,
	};

	event_add(&c->delay_timer, &timeout);
}
