#ifdef __cplusplus
extern "C" {
#endif
#ifndef _SUBLIST_H_
#define _SUBLIST_H_
#include "dd.h"
zlistx_t *sublist_new();
void sublist_destroy(zlistx_t **self);
int sublist_cmp(const void *item1, const void *item2);
void sublist_free(void **item);
void *sublist_dup(const void *item);
void sublist_add(dd_t *self, char *topic, char *scope, char active);
void sublist_delete_topic(dd_t *self, char *topic);
int sublist_delete(dd_t *self, char *topic, char *scope);
void sublist_activate(dd_t *self, char *topic, char *scope);
void sublist_deactivate_all(dd_t *self);
void sublist_print(dd_t *self);
#endif
#ifdef __cplusplus
}
#endif
