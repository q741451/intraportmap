#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([intraportmap], [0.1.1], [q741451@gmail.com])
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/stdafx.h])
AC_CONFIG_HEADERS([config.h])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_ARG_ENABLE([debug], [AC_HELP_STRING([ --enable-debug], [Build a debug version])],
              [CXXFLAGS="$CXXFLAGS -g -O0 -DDEBUG -std=c++0x -Wall"],
              [CXXFLAGS="$CXXFLAGS -O2 -std=c++0x -Wall"])

AC_PROG_CC
AC_PROG_CXX
AC_C_BIGENDIAN

# Checks for libraries.
AC_ARG_WITH(event,
  AS_HELP_STRING([--with-event=DIR], [use a specific libevent library]),
  [CFLAGS="$CFLAGS -I$withval/include"
   CPPFLAGS="$CPPFLAGS -I$withval/include"
   LDFLAGS="$LDFLAGS -L$withval/lib"]
)
AC_CHECK_HEADERS([event2/event.h], [], [])
AC_CHECK_LIB([event], [event_new], [LIBS="-levent $LIBS"], [AC_MSG_ERROR([Couldn't find libevent. Try installing libevent-devel@<:@el@:>@.])])

# Checks for header files.
AC_CHECK_HEADERS([signal.h memory.h sys/socket.h sys/syscall.h netinet/in.h arpa/inet.h unistd.h event2/event.h event2/listener.h event2/buffer.h event2/bufferevent.h event2/dns.h vector string memory map set])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile src/Makefile])

AC_MSG_RESULT([config result: event($with_event)])
AC_OUTPUT
