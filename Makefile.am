AUTOMAKE_OPTIONS = foreign 1.10 dist-bzip2 subdir-objects

#
# Compiler/linker flags
#

LIBEVENT_CFLAGS =
LIBEVENT_LIBS = -levent_core

AM_CPPFLAGS += \
	$(LIBCM4ALL_DAEMON_CFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(LIBCM4ALL_SOCKET_CFLAGS) \
	$(LIBWAS_CFLAGS) \
	$(LIBEVENT_CFLAGS) \
	-D_GNU_SOURCE
INCLUDES = -I$(srcdir)/src -I$(srcdir)/include

bin_PROGRAMS = src/cm4all-myproxy
src_cm4all_myproxy_SOURCES = \
	src/fd_util.c \
	src/fifo_buffer.c \
	src/buffered_io.c \
	src/listener.c \
	src/socket.c \
	src/mysql_protocol.h \
	src/mysql_reader.h \
	src/mysql_reader.c \
	src/peer.h \
	src/peer.c \
	src/connection.c \
	src/instance.c \
	src/cmdline.c \
	src/main.c
src_cm4all_myproxy_LDADD = $(AM_LDADD) \
	$(LIBCM4ALL_DAEMON_LIBS) \
	$(LIBCM4ALL_INLINE_LIBS) \
	$(LIBCM4ALL_SOCKET_LIBS) \
	$(LIBEVENT_LIBS)
