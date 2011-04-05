AC_PREREQ(2.60)
AC_INIT(CM4all MySQL-Proxy, 0.1, mk@cm4all.com, cm4all-myproxy)
AC_COPYRIGHT(Copyright 2011 Content Management AG)
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_AUX_DIR(build)
AM_INIT_AUTOMAKE([foreign 1.10 dist-bzip2 subdir-objects])

dnl Checks for programs.

AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_CC_C99
AC_PROG_RANLIB

dnl Checks for OS features

AC_SYS_LARGEFILE

AC_CHECK_FUNCS(pipe2 accept4)

dnl Checks for libraries.

PKG_CHECK_MODULES([LIBCM4ALL_DAEMON], [libcm4all-daemon >= 0.1.2],,
	[AC_MSG_ERROR([libcm4all-daemon not found])])

PKG_CHECK_MODULES([LIBCM4ALL_INLINE], [libcm4all-inline >= 0.1.1],,
	[AC_MSG_ERROR([libcm4all-inline not found])])

PKG_CHECK_MODULES([LIBCM4ALL_SOCKET], [libcm4all-socket],,
	[AC_MSG_ERROR([libcm4all-socket not found])])

dnl
dnl CFLAGS
dnl

AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_CPPFLAGS)

AM_CFLAGS="-W -Wall"
AM_CPPFLAGS="-D_REENTRANT"

if test "$am_cv_CC_dependencies_compiler_type" = "gcc3"; then
	AM_CFLAGS="$AM_CFLAGS -Wall -Wextra -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Waggregate-return -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Wnested-externs -Wno-long-long -Wstrict-prototypes -Wundef -pedantic"
fi

dnl
dnl DEBUG
dnl

AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--disable-debug],
		[Disable debugging (default: enabled)]),,
	enable_debug=yes)
AM_CONDITIONAL(TEST, test x$enable_test = xyes)

if test x$enable_debug = xyes; then
	AM_CFLAGS="$AM_CFLAGS -Werror -pedantic-errors"
	AM_CPPFLAGS="$AM_CPPFLAGS -DPOISON -DVALGRIND"
else
	AM_CPPFLAGS="$AM_CPPFLAGS -DNDEBUG"
fi

dnl
dnl Output files
dnl

AC_OUTPUT(
Makefile
)
