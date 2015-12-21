AC_INIT([doubledecker], [0.2], [ponsko@acreo.se])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_PROG_AR
AC_PROG_CC
LT_INIT()
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])
AC_ARG_ENABLE([foo],
    AS_HELP_STRING([--disable-foo], [Disable feature foo]))

AS_IF([test "x$enable_foo" != "xno"], [
  dnl Do the stuff needed for enabling the feature
])

dnl Example of default-disabled feature
AC_ARG_ENABLE([bar],
    AS_HELP_STRING([--enable-bar], [Enable feature bar]))

AS_IF([test "x$enable_bar" = "xyes"],
  dnl Do the stuff needed for enabling the feature
])

AC_SEARCH_LIBS([zmq_strerror], [zmq], [], [
  AC_MSG_ERROR([unable to find the zmq_strerror() function, libzmq installed?])
])

AC_SEARCH_LIBS([zsock_new_router], [czmq], [], [
  AC_MSG_ERROR([unable to find the zsock_new_router function, libczmq installed? ])
])
AC_SEARCH_LIBS([rcu_read_lock_memb], [urcu], [], [
  AC_MSG_ERROR([unable to find the rcu_read_lock_memb function, liburcu installed? ])
])

AC_SEARCH_LIBS([cds_lfht_add], [urcu-cds], [], [
  AC_MSG_ERROR([unable to find the cds_lfht_add function, liburcu-cds installed? ])
])

AC_SEARCH_LIBS([sodium_bin2hex], [sodium], [], [
  AC_MSG_ERROR([unable to find the sodium_bin2hex function, libsodium installed? ])
])

AC_SEARCH_LIBS([json_tokener_parse], [json], [], [
  AC_MSG_ERROR([unable to find the json_tokener_parse function, libjson-c installed? ])
])

CFLAGS="$CFLAGS -Wno-format-security -rdynamic"

AC_ENABLE_SHARED
AC_OUTPUT



