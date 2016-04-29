#ifdef __cplusplus
extern "C" {
#endif
#ifndef _DD_CLASSES_H_
#define _DD_CLASSES_H_

#include "ddkeys.h"

// styles
#define DD_ACTOR 1
#define DD_CALLBACK 2

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

int sublist_cmp(const void *item1, const void *item2);
void sublist_free(void **item);
void *sublist_dup(const void *item);
void sublist_add(char *topic, char *scope, char active, dd_t *self);
void sublist_delete_topic(char *topic, dd_t *self);
void sublist_delete(char *topic, char *scope, dd_t *self);
void sublist_activate(char *topic, char *scope, dd_t *self);
void sublist_deactivate_all(dd_t *self);
void sublist_print(dd_t *self);

#endif
#ifdef __cplusplus
}
#endif
