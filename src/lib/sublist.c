#include "../../include/dd.h"
// ///////////////////
// ///SUBLIST stuff //
// //////////////////
struct _ddtopic_t {
  char *topic;
  char *scope;
  char active;
};

const char* dd_sub_get_topic(ddtopic_t *sub){
  return (const char*) sub->topic;
}
const char* dd_sub_get_scope(ddtopic_t *sub){
  return (const char*) sub->scope;
}
char dd_sub_get_active(ddtopic_t *sub){
  return sub->active;
}

// - compare two items, for sorting
// typedef int (czmq_comparator) (const void *item1, const void *item2);
 int sublist_cmp(const void *item1, const void *item2) {
  ddtopic_t *i1, *i2;
  i1 = (ddtopic_t *)item1;
  i2 = (ddtopic_t *)item2;
  return strcmp(i1->topic, i2->topic);
}

// -- destroy an item
// typedef void (czmq_destructor) (void **item);
 void sublist_free(void **item) {
  ddtopic_t *i;
  i = *item;
  free(i->topic);
  free(i->scope);
  free(i);
}

// -- duplicate an item
// typedef void *(czmq_duplicator) (const void *item);
 void *sublist_dup(const void *item) {
  ddtopic_t *new, *old;
  old = (ddtopic_t *)item;
  new = malloc(sizeof(ddtopic_t));
  new->topic = strdup(old->topic);
  new->scope = strdup(old->scope);
  new->active = old->active;
  return new;
}

// update or add topic/scope/active to list
 void sublist_add(char *topic, char *scope, char active, dd_t *self) {
  ddtopic_t *item; // = zlistx_first(dd->sublist);
  int found = 0;
  // Check if already there, if so update
  // printf("after _first item = %p\n",item);

  while ((item = zlistx_next((zlistx_t*)dd_get_subscriptions(self)))) {
    if (strcmp(item->topic, topic) == 0 && strcmp(item->scope, scope) == 0) {
      item->active = active;
      found = 1;
    }
  }

  // Otherwise, add new
  if (!found) {
    ddtopic_t *new = malloc(sizeof(ddtopic_t));
    new->topic = topic;
    new->scope = scope;
    new->active = active;
    zlistx_add_start((zlistx_t*)dd_get_subscriptions(self), new);
  }
}

 void sublist_delete_topic(char *topic, dd_t *self) {
  ddtopic_t *item = zlistx_first((zlistx_t*)dd_get_subscriptions(self));
  do {
    if (strcmp(item->topic, topic) == 0) {
      zlistx_delete((zlistx_t*)dd_get_subscriptions(self), item);
    }
  } while ((item = zlistx_next((zlistx_t*)dd_get_subscriptions(self))));
}

 void sublist_delete(char *topic, char *scope, dd_t *self) {
  ddtopic_t del;
  del.topic = topic;
  del.scope = scope;
  ddtopic_t *item = zlistx_find((zlistx_t*)dd_get_subscriptions(self), &del);
  if (item)
    zlistx_delete((zlistx_t*)dd_get_subscriptions(self), item);
}

 void sublist_activate(char *topic, char *scope, dd_t *self) {
  ddtopic_t *item;
  while ((item = zlistx_next((zlistx_t*)dd_get_subscriptions(self)))) {
    if (strcmp(item->topic, topic) == 0 && strcmp(item->scope, scope) == 0) {
      item->active = 1;
    }
  }
}

 void sublist_deactivate_all(dd_t *self) {
  ddtopic_t *item;
  while ((item = zlistx_next((zlistx_t*)dd_get_subscriptions(self)))){
    item->active = 0;
  }
}


// TODO: This should be changed to return a copy of the sublist
// So that library clients can use it
 void sublist_print(dd_t *self) {
  ddtopic_t *item;
  while ((item = zlistx_next((zlistx_t*)dd_get_subscriptions(self)))) {
    printf("Topic: %s Scope: %s Active: %d\n", item->topic, item->scope,
           item->active);
  }
}
