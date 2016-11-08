#include "../../include/sublist.h"
#include "../../include/dd.h-old"
// ///////////////////
// ///SUBLIST stuff //
// //////////////////
struct _ddtopic_t {
  char *topic;
  char *scope;
  char active;
};

const char *dd_sub_get_topic(ddtopic_t *sub) {
  return (const char *)sub->topic;
}
const char *dd_sub_get_scope(ddtopic_t *sub) {
  return (const char *)sub->scope;
}
char dd_sub_get_active(ddtopic_t *sub) { return sub->active; }

// - compare two items, for sorting
// typedef int (czmq_comparator) (const void *item1, const void *item2);
static int s_sublist_cmp(const void *item1, const void *item2) {
  ddtopic_t *i1, *i2;
  i1 = (ddtopic_t *)item1;
  i2 = (ddtopic_t *)item2;
  return strcmp(i1->topic, i2->topic);
}

// -- destroy an item
// typedef void (czmq_destructor) (void **item);
static void s_sublist_free(void **item) {
    // TODO
    // There's a bug lurking here.. the new_top pointer is not the same
    // as the the one passed to s_sublist_free..
  
  ddtopic_t *i;
  i = (ddtopic_t *)*item;
  //printf("s_sublist_free called p: %p t: %p s: %p \n", i, i->topic, i->scope);
  free(i->topic);
  free(i->scope);
  free(i);
  i = NULL;
}

// -- duplicate an item
// typedef void *(czmq_duplicator) (const void *item);
static void *s_sublist_dup(const void *item) {
  ddtopic_t *new, *old;
  old = (ddtopic_t *)item;
  new = malloc(sizeof(ddtopic_t));
  new->topic = strdup(old->topic);
  new->scope = strdup(old->scope);
  new->active = old->active;
  return new;
}

zlistx_t *sublist_new() {
  zlistx_t *n = zlistx_new();
  zlistx_set_destructor(n, (czmq_destructor *)s_sublist_free);
  zlistx_set_duplicator(n, (czmq_duplicator *)s_sublist_dup);
  zlistx_set_comparator(n, (czmq_comparator *)s_sublist_cmp);
  return n;
}
void sublist_destroy(zlistx_t **self_p) {
  zlistx_destroy(self_p);
  *self_p = NULL;
}
// update or add topic/scope/active to list
void sublist_add(dd_t *self, char *topic, char *scope, char active) {
  ddtopic_t *item;
  int found = 0;

  while ((item = zlistx_next((zlistx_t *)dd_get_subscriptions(self)))) {
    if (streq(item->topic, topic) && streq(item->scope, scope)) {
      item->active = active;
      found = 1;
    }
  }

  // Otherwise, add new
  if (!found) {
    ddtopic_t *new_top = malloc(sizeof(ddtopic_t));

    // TODO
    // There's a bug lurking here.. the new_top pointer is not the same
    // as the the one passed to s_sublist_free..
    /* printf("sublist_add, new %p (%d) t: %p s %p \n", new_top,
     * sizeof(ddtopic_t), */
    /*        topic, scope); */

    new_top->topic = topic;
    new_top->scope = scope;
    new_top->active = active;
    zlistx_add_start((zlistx_t *)dd_get_subscriptions(self), new_top);
  }
}

void sublist_delete_topic(dd_t *self, char *topic) {
  ddtopic_t *item = zlistx_first((zlistx_t *)dd_get_subscriptions(self));
  do {
    if (streq(item->topic, topic)) {
      zlistx_delete((zlistx_t *)dd_get_subscriptions(self), item);
    }
  } while ((item = zlistx_next((zlistx_t *)dd_get_subscriptions(self))));
}

int sublist_delete(dd_t *self, char *topic, char *scope) {
  ddtopic_t del;
  del.topic = topic;
  del.scope = scope;
  ddtopic_t *item = zlistx_find((zlistx_t *)dd_get_subscriptions(self), &del);
  if (item)
    return zlistx_delete((zlistx_t *)dd_get_subscriptions(self), item);
  return -1;
}

void sublist_activate(dd_t *self, char *topic, char *scope) {
  ddtopic_t *item;
  while ((item = zlistx_next((zlistx_t *)dd_get_subscriptions(self)))) {
    if (strcmp(item->topic, topic) == 0 && strcmp(item->scope, scope) == 0) {
      item->active = 1;
    }
  }
}

void sublist_deactivate_all(dd_t *self) {
  ddtopic_t *item;
  while ((item = zlistx_next((zlistx_t *)dd_get_subscriptions(self)))) {
    item->active = 0;
  }
}

// TODO: This should be changed to return a copy of the sublist
// So that library clients can use it
void sublist_print(dd_t *self) {
  ddtopic_t *item;
  while ((item = zlistx_next((zlistx_t *)dd_get_subscriptions(self)))) {
    printf("Topic: %s Scope: %s Active: %d\n", item->topic, item->scope,
           item->active);
  }
}
