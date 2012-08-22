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
	src/clock.h \
	src/fd_util.c \
	src/fifo_buffer.c \
	src/buffered_io.c \
	src/Policy.cxx src/Policy.hxx \
	src/Listener.cxx src/Listener.hxx \
	src/Socket.cxx src/Socket.hxx \
	src/mysql_protocol.h \
	src/MySQLReader.cxx src/MySQLReader.hxx \
	src/Peer.cxx src/Peer.hxx \
	src/Connection.cxx src/Connection.hxx \
	src/Instance.cxx src/Instance.hxx \
	src/CommandLine.cxx src/CommandLine.hxx \
	src/Main.cxx
src_cm4all_myproxy_LDADD = $(AM_LDADD) \
	-lrt \
	$(LIBCM4ALL_DAEMON_LIBS) \
	$(LIBCM4ALL_INLINE_LIBS) \
	$(LIBCM4ALL_SOCKET_LIBS) \
	$(LIBEVENT_LIBS)
