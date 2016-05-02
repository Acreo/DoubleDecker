// Internal header, not for exporting!

#ifdef __cplusplus
extern "C" {
#endif
#ifndef _DD_CLASSES_H_
#define _DD_CLASSES_H_
#include "../config.h"
#include <czmq.h>
#include <czmq.h>
#include <err.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <urcu.h>
#include <urcu.h>
#include <urcu/rculfhash.h>
#include <zmq.h>

#ifdef HAVE_JSON_C_JSON_H
#include <json-c/json.h>
#elif HAVE_JSON_H
#include <json.h>
#elif HAVE_JSON_JSON_H
#include <json/json.h>
#endif

  
#include "cdecode.h"
#include "dd.h"
#include "ddlog.h"
#include "ddlog.h"
#include "keys.h"
#include "trie.h"
#include "broker.h"
#include "htable.h"
#include "murmurhash.h"
#include "protocol.h"
#include "sublist.h"
#include "xxhash.h"



// styles
#define DD_ACTOR 1
#define DD_CALLBACK 2

// preallocated data variables
extern const uint32_t dd_cmd_send;
extern const uint32_t dd_cmd_forward;
extern const uint32_t dd_cmd_ping;
extern const uint32_t dd_cmd_addlcl;
extern const uint32_t dd_cmd_adddcl;
extern const uint32_t dd_cmd_addbr;
extern const uint32_t dd_cmd_unreg;
extern const uint32_t dd_cmd_unregdcli;
extern const uint32_t dd_cmd_unregbr;
extern const uint32_t dd_cmd_data;
extern const uint32_t dd_cmd_error;
extern const uint32_t dd_cmd_regok;
extern const uint32_t dd_cmd_pong;
extern const uint32_t dd_cmd_chall;
extern const uint32_t dd_cmd_challok;
extern const uint32_t dd_cmd_pub;
extern const uint32_t dd_cmd_sub;
extern const uint32_t dd_cmd_unsub;
extern const uint32_t dd_cmd_sendpublic;
extern const uint32_t dd_cmd_pubpublic;
extern const uint32_t dd_cmd_sendpt;
extern const uint32_t dd_cmd_forwardpt;
extern const uint32_t dd_cmd_datapt;
extern const uint32_t dd_cmd_subok;
extern const uint32_t dd_version;
extern const uint32_t dd_error_regfail;
extern const uint32_t dd_error_nodst;
extern const uint32_t dd_error_version;

#endif
#ifdef __cplusplus
}
#endif
