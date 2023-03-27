/*
 * TCP listener.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Listener.hxx"
#include "Instance.hxx"
#include "Connection.hxx"

extern "C" {
#include "fd_util.h"
}

#include <daemon/log.h>
#include <socket/util.h>

#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

static void
listener_event_callback(int fd, [[maybe_unused]] short event, void *ctx)
{
	Instance *instance = (Instance *)ctx;

	struct sockaddr_storage sa;
	size_t sa_len = sizeof(sa);
	int remote_fd = accept_cloexec_nonblock(fd, (struct sockaddr*)&sa,
						&sa_len);
	if (remote_fd < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			daemon_log(1, "accept() failed: %s\n", strerror(errno));
		return;
	}

	if (!socket_set_nodelay(remote_fd, true)) {
		daemon_log(1, "setsockopt(TCP_NODELAY) failed: %s\n", strerror(errno));
		close(remote_fd);
		return;
	}

	Connection *connection = new Connection(*instance, UniqueSocketDescriptor{remote_fd});
	instance->connections.push_back(*connection);
}

static int
listener_create_socket(int family, int socktype, int protocol,
		       const struct sockaddr *address, size_t address_length)
{
	assert(address != NULL);
	assert(address_length > 0);

	int fd = socket_cloexec_nonblock(family, socktype, protocol);
	if (fd < 0)
		return -1;

	int param = 1;
	int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &param, sizeof(param));
	if (ret < 0) {
		int save_errno = errno;
		close(fd);
		errno = save_errno;
		return -1;
	}

	ret = bind(fd, address, address_length);
	if (ret < 0) {
		int save_errno = errno;
		close(fd);
		errno = save_errno;
		return -1;
	}

	ret = listen(fd, 16);
	if (ret < 0) {
		int save_errno = errno;
		close(fd);
		errno = save_errno;
		return -1;
	}

	return fd;
}

static bool
listener_init_address(Instance *instance,
		      int family, int socktype, int protocol,
		      const struct sockaddr *address, size_t address_length)
{
	instance->listener_socket =
		listener_create_socket(family, socktype, protocol,
				       address, address_length);
	if (instance->listener_socket < 0)
		return false;

	event_set(&instance->listener_event, instance->listener_socket,
		  EV_READ|EV_PERSIST, listener_event_callback, instance);
	event_add(&instance->listener_event, NULL);

	return true;
}


void
listener_init(Instance *instance, unsigned port)
{
	assert(port > 0);

#if 0
	/* try IPv6 first */

	struct sockaddr_in6 sa6;
	memset(&sa6, 0, sizeof(sa6));
	sa6.sin6_family = AF_INET6;
	sa6.sin6_addr = in6addr_any;
	sa6.sin6_port = htons((uint16_t)port);

	if (listener_init_address(instance, PF_INET6, SOCK_STREAM, 0,
				  (const struct sockaddr *)&sa6, sizeof(sa6)))
		return;
#endif

	/* fall back to IPv4 first */

	struct sockaddr_in sa4;
	memset(&sa4, 0, sizeof(sa4));
	sa4.sin_family = AF_INET;
	sa4.sin_addr.s_addr = INADDR_ANY;
	sa4.sin_port = htons((uint16_t)port);

	if (listener_init_address(instance, PF_INET, SOCK_STREAM, 0,
				  (const struct sockaddr *)&sa4, sizeof(sa4)))
		return;

	/* error */

	fprintf(stderr, "Failed to listen: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
}

void
listener_deinit(Instance *instance)
{
	event_del(&instance->listener_event);
	close(instance->listener_socket);
}
