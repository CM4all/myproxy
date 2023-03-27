/*
 * Utilities for buffered I/O.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "util/StaticFifoBuffer.hxx"

#include <sys/types.h>

/**
 * Appends data from a file to the buffer.
 *
 * @param fd the source file descriptor
 * @param buffer the destination buffer
 * @return -1 on error, -2 if the buffer is full, or the amount appended to the buffer
 */
ssize_t
read_to_buffer(int fd, StaticFifoBuffer<std::byte, 4096> &buffer, size_t length);

/**
 * Writes data from the buffer to the file.
 *
 * @param fd the destination file descriptor
 * @param buffer the source buffer
 * @return -1 on error, -2 if the buffer is empty, or the rest left in the buffer
 */
ssize_t
write_from_buffer(int fd, StaticFifoBuffer<std::byte, 4096> &buffer);

/**
 * Appends data from a socket to the buffer.
 *
 * @param fd the source socket
 * @param buffer the destination buffer
 * @return -1 on error, -2 if the buffer is full, or the amount appended to the buffer
 */
ssize_t
recv_to_buffer(int fd, StaticFifoBuffer<std::byte, 4096> &buffer, size_t length);

/**
 * Sends data from the buffer to the socket.
 *
 * @param fd the destination socket
 * @param buffer the source buffer
 * @return -1 on error, -2 if the buffer is empty, or the rest left in the buffer
 */
ssize_t
send_from_buffer(int fd, StaticFifoBuffer<std::byte, 4096> &buffer);

/**
 * Sends data from the buffer to the socket.
 *
 * @param fd the destination socket
 * @param buffer the source buffer
 * @param max the maximum number of bytes to transmit
 * @return -1 on error, -2 if the buffer is empty, or the rest left in the buffer
 */
ssize_t
send_from_buffer_n(int fd, StaticFifoBuffer<std::byte, 4096> &buffer, size_t max);