AC_INIT([DoubleDecker],
    m4_esyscmd_s(echo $(git describe --abbrev=7 --dirty --always --tags || echo "0.4-tgz")),
    [ponsko@acreo.se],
    [],
    [http://acreo.github.io/DoubleDecker/])
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

PKG_CHECK_MODULES([json], [json-c],,
	[PKG_CHECK_MODULES([json], [json],,
		[PKG_CHECK_MODULES([json], [json0],,
			[AC_MSG_WARN([No pkg-config for json-c (aka libjson0, libjson)])])])])
AC_CHECK_LIB([json], [json_object_new_string],,
	[AC_CHECK_LIB([json-c], [json_object_new_string],,
		[AC_MSG_ERROR([Cannot find required library: libjson-c (aka libjson0, libjson)])])
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
  AC_MSG_ERROR([unable to find the sodium_increment function, libsodium installed? ])
])


AC_CHECK_HEADERS([json-c/json.h json/json.h json.h])


AM_CONDITIONAL([HAVE_JSON_JSON_H],[test "$ac_cv_header_json_json_h" = 'yes'])
AM_CONDITIONAL([HAVE_JSON_C_JSON_H],[test "$ac_cv_header_json_c_json_h" = 'yes'])
AM_CONDITIONAL([HAVE_JSON_H],[test "$ac_cv_header_json_h" = 'yes'])

CFLAGS="$CFLAGS -Wno-format-security -rdynamic"

AC_ENABLE_SHARED
AC_OUTPUT



