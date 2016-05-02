AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -I$(top_srcdir)/include

lib_LTLIBRARIES = libdd.la
libdd_la_SOURCES = lib/protocol.c lib/client.c lib/keys.c lib/cdecode.c lib/sublist.c hash/xxhash.c hash/murmurhash.c lib/htable.c lib/trie.c lib/broker.c
libdd_la_LDFLAGS = -version-info 0:3:0 -L/usr/local/lib

bin_PROGRAMS = ddperf ddbroker ddclient ddbug  actor_test

ddebug = ddbug.c
ddperf_SOURCES = ddperf.c
actor_test_SOURCES = actor_test.c
ddbroker_SOURCES = ddbroker.c 
ddclient_SOURCES =  ddcli.c cli_parser/cparser_tree.c  cli_parser/cparser.c\
		  cli_parser/cparser_fsm.c  cli_parser/cparser_io_unix.c\
		  cli_parser/cparser_line.c  cli_parser/cparser_token.c\
                  cli_parser/cparser_token_tbl.c
 
ddperf_LDFLAGS = -L/usr/local/lib
ddbroker_LDFLAGS = -L/usr/local/lib
ddclient_LDFLAGS = -L/usr/local/lib
actor_test_LDFLAGS = -L/usr/local/lib

actor_test_LDADD = libdd.la
ddperf_LDADD = libdd.la
ddbroker_LDADD = libdd.la
ddclient_LDADD = libdd.la



