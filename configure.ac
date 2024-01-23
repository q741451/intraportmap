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
AC_ARG_WITH([event],
  AS_HELP_STRING([--with-event=DIR], [use a specific libevent library]),
  [
    EVENT_CFLAGS="-I$withval/include"
    EVENT_LIBS="-L$withval/lib -levent"
    AC_SUBST(EVENT_CFLAGS)
    AC_SUBST(EVENT_LIBS)

    CPPFLAGS="$CPPFLAGS $EVENT_CFLAGS"
    LDFLAGS="$LDFLAGS $EVENT_LIBS"
    AC_CHECK_HEADERS([event2/event.h], [], [AC_MSG_ERROR([Couldn't find event2/event.h in with event.])])
    AC_CHECK_LIB([event], [event_new], [], [AC_MSG_ERROR([Couldn't find libevent in with event.])])
  ],
  [
    AC_CHECK_HEADERS([event2/event.h], [], [AC_MSG_ERROR([Couldn't find event2/event.h. Try installing libevent-devel.])])
    AC_CHECK_LIB([event], [event_new], [], [AC_MSG_ERROR([Couldn't find libevent. Try installing libevent-devel.])])
  ]
)

# Checks for header files.
AC_CHECK_HEADERS([signal.h memory.h sys/socket.h sys/syscall.h netinet/in.h arpa/inet.h unistd.h event2/event.h event2/listener.h event2/buffer.h event2/bufferevent.h event2/dns.h vector string memory map set])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile src/Makefile])

AC_ARG_ENABLE([static],
  [AS_HELP_STRING([--enable-static],[Enable static linking])],
  [case "${enableval}" in
     yes) static=yes ;;
     no)  static=no ;;
     *) AC_MSG_ERROR([bad value ${enableval} for --enable-static]) ;;
   esac],
  [static=no])

AS_IF([test "x$static" = "xyes"], [
  LDFLAGS="$LDFLAGS -static"
])

AC_MSG_RESULT([config result: event($with_event)])
AC_OUTPUT
