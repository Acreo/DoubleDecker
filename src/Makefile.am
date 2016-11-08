AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -I$(top_srcdir)/include

lib_LTLIBRARIES = libdd.la
libdd_la_SOURCES = lib/protocol.c lib/client.c lib/keys.c lib/cdecode.c \
		lib/cencode.c lib/sublist.c hash/xxhash.c hash/murmurhash.c \
		lib/htable.c lib/trie.c lib/broker.c

libdd_la_LDFLAGS = -version-info 0:3:0 

bin_PROGRAMS = ddperf ddperf2 ddbroker ddclient ddbug  ddactor_test ddbroker_test ddkeygen

ddebug = ddbug.c
ddperf_SOURCES = ddperf.c
ddperf2_SOURCES = ddperf2.c

ddactor_test_SOURCES = actor_test.c
ddbroker_SOURCES = ddbroker.c 
ddbroker_test_SOURCES = broker_test.c 
ddkeygen_SOURCES = ddkeygen.c

ddclient_SOURCES =  ddclient.c cli_parser/cparser_tree.c  cli_parser/cparser.c\
		  cli_parser/cparser_fsm.c  cli_parser/cparser_io_unix.c\
		  cli_parser/cparser_line.c  cli_parser/cparser_token.c\
                  cli_parser/cparser_token_tbl.c
 

ddactor_test_LDADD = libdd.la
ddperf_LDADD = libdd.la
ddperf2_LDADD = libdd.la

ddbroker_LDADD = libdd.la
ddclient_LDADD = libdd.la
ddbroker_test_LDADD = libdd.la
ddkeygen_LDADD = libdd.la



