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

HAVE_AMD64_ASM_V=0
AS_IF([test "$enable_asm" != "no"],[
  AC_MSG_CHECKING(whether we can use x86_64 asm code)
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
  ]], [[
#if defined(__amd64) || defined(__amd64__) || defined(__x86_64__)
# if defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(_WIN32) || defined(_WIN64)
#  error Windows x86_64 calling conventions are not supported yet
# endif
/* neat */
#else
# error !x86_64
#endif
unsigned char i = 0, o = 0, t;
__asm__ __volatile__ ("pxor %%xmm12, %%xmm6 \n"
                      "movb (%[i]), %[t] \n"   
                      "addb %[t], (%[o]) \n"   
                      : [t] "=&r"(t)
                      : [o] "D"(&o), [i] "S"(&i)
                      : "memory", "flags", "cc");
]])],
  [AC_MSG_RESULT(yes)
   AC_DEFINE([HAVE_AMD64_ASM], [1], [x86_64 asm code can be used])
   HAVE_AMD64_ASM_V=1],
  [AC_MSG_RESULT(no)]) 
])
AM_CONDITIONAL([HAVE_AMD64_ASM], [test $HAVE_AMD64_ASM_V = 1])
AC_SUBST(HAVE_AMD64_ASM_V)

CFLAGS="$CFLAGS -Wno-format-security -rdynamic"

AC_ENABLE_SHARED
AC_OUTPUT



