#ifdef __cplusplus
extern "C" {
#endif
#ifndef _SUBLIST_H_
#define _SUBLIST_H_
#include "dd.h"
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


