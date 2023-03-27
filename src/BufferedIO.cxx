/*
 * Utilities for buffered I/O.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "BufferedIO.hxx"

#include <cassert>
#include <cerrno>

#include <unistd.h>
#include <sys/socket.h>

ssize_t
read_to_buffer(int fd, StaticFifoBuffer<std::byte, 4096> &buffer, size_t length)
{
	ssize_t nbytes;

	assert(fd >= 0);

	const auto w = buffer.Write();
	if (w.empty())
		return -2;

	if (length > w.size())
		length = w.size();

	nbytes = read(fd, w.data(), length);
	if (nbytes > 0)
		buffer.Append((size_t)nbytes);

	return nbytes;
}

ssize_t
write_from_buffer(int fd, StaticFifoBuffer<std::byte, 4096> &buffer)
{
	ssize_t nbytes;

	const auto r = buffer.Read();
	if (r.empty())
		return -2;

	nbytes = write(fd, r.data(), r.size());
	if (nbytes < 0 && errno != EAGAIN)
		return -1;

	if (nbytes <= 0)
		return r.size();

	buffer.Consume((size_t)nbytes);
	return (ssize_t)r.size() - nbytes;
}

ssize_t
recv_to_buffer(int fd, StaticFifoBuffer<std::byte, 4096> &buffer, size_t length)
{
	ssize_t nbytes;

	assert(fd >= 0);

	const auto w = buffer.Write();
	if (w.empty())
		return -2;

	if (length > w.size())
		length = w.size();

	nbytes = recv(fd, w.data(), length, MSG_DONTWAIT);
	if (nbytes > 0)
		buffer.Append((size_t)nbytes);

	return nbytes;
}

ssize_t
send_from_buffer(int fd, StaticFifoBuffer<std::byte, 4096> &buffer)
{
	ssize_t nbytes;

	const auto r = buffer.Read();
	if (r.empty())
		return -2;

	nbytes = send(fd, r.data(), r.size(), MSG_DONTWAIT|MSG_NOSIGNAL);
	if (nbytes < 0 && errno != EAGAIN)
		return -1;

	if (nbytes <= 0)
		return r.size();

	buffer.Consume((size_t)nbytes);
	return (ssize_t)r.size() - nbytes;
}

ssize_t
send_from_buffer_n(int fd, StaticFifoBuffer<std::byte, 4096> &buffer, size_t max)
{
	assert(max > 0);

	auto r = buffer.Read();
	if (r.empty())
		return -2;

	if (r.size() > max)
		r = r.first(max);

	ssize_t nbytes = send(fd, r.data(), r.size(), MSG_DONTWAIT|MSG_NOSIGNAL);
	if (nbytes < 0 && errno != EAGAIN)
		return -1;

	if (nbytes <= 0)
		return r.size();

	buffer.Consume((size_t)nbytes);
	return (ssize_t)r.size() - nbytes;
}
