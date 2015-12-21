AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -I$(top_srcdir)/include -I/usr/include/json-c
AM_LDFLAGS = -ljson -lsodium -lczmq -lzmq -lurcu -lurcu-cds 

lib_LTLIBRARIES = libdd.la
libdd_la_SOURCES = lib/protocol.c lib/client.c lib/keys.c lib/b64/cdecode.c
# lib/ddlog.c
libdd_la_LDFLAGS = -version-info 0:1:0

bin_PROGRAMS = ddperf ddbroker ddclient ddbug #ddbroker_threaded 

ddebug = ddbug.c
ddperf_SOURCES = ddperf.c
ddbroker_SOURCES = ddbroker.c hash/xxhash.c hash/murmurhash.c ddhtable.c trie.c
#ddbroker_threaded_SOURCES = ddbroker_threaded.c hash/xxhash.c hash/murmurhash.c
ddclient_SOURCES =  ddcli.c cli_parser/cparser_tree.c  cli_parser/cparser.c\
		  cli_parser/cparser_fsm.c  cli_parser/cparser_io_unix.c\
		  cli_parser/cparser_line.c  cli_parser/cparser_token.c\
                  cli_parser/cparser_token_tbl.c 

ddperf_LDADD = libdd.la
ddbroker_LDADD = libdd.la
#ddbroker_threaded_LDADD = libdd.la
ddclient_LDADD = libdd.la
