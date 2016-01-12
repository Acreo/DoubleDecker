AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -I$(top_srcdir)/include

lib_LTLIBRARIES = libdd.la
libdd_la_SOURCES = lib/protocol.c lib/client.c lib/keys.c lib/b64/cdecode.c
libdd_la_LDFLAGS = -version-info 0:1:0

bin_PROGRAMS = ddperf ddbroker ddclient ddbug 

ddebug = ddbug.c
ddperf_SOURCES = ddperf.c
ddbroker_SOURCES = ddbroker.c hash/xxhash.c hash/murmurhash.c ddhtable.c trie.c
ddclient_SOURCES =  ddcli.c cli_parser/cparser_tree.c  cli_parser/cparser.c\
		  cli_parser/cparser_fsm.c  cli_parser/cparser_io_unix.c\
		  cli_parser/cparser_line.c  cli_parser/cparser_token.c\
                  cli_parser/cparser_token_tbl.c 

ddperf_LDADD = libdd.la
ddbroker_LDADD = libdd.la
ddclient_LDADD = libdd.la
