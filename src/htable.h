#ifndef _HTABLE_H_
#define _HTABLE_H_
#include <urcu.h>
#include <urcu/rculfhash.h>
#include "dd_classes.h"
// TODO: define these hashtables in the API as well
int insert_local_client(dd_broker_t *self, zframe_t *sockid, ddtenant_t *ten,
                        char *client_name);
void hashtable_remove_dist_node(dd_broker_t *self, char *prefix_name);
dist_client *hashtable_has_dist_node(dd_broker_t *self, char *prefix_name);
void hashtable_insert_dist_node(dd_broker_t *self, char *prefix_name,
                                zframe_t *sockid, int dist);
void delete_dist_clients(dd_broker_t *self, local_broker *br);
local_broker *hashtable_has_local_broker(dd_broker_t *self, zframe_t *sockid,
                                         uint64_t cookie, int update);

void hashtable_insert_local_broker(dd_broker_t *self, zframe_t *sockid,
                                   uint64_t cookie);
local_client *hashtable_has_rev_local_node(dd_broker_t *self, char *prefix_name,
                                           int update);
local_client *hashtable_has_local_node(dd_broker_t *self, zframe_t *sockid,
                                       zframe_t *cookie, int update);
void hashtable_unlink_rev_local_node(dd_broker_t *self, char *prefix_name);
void hashtable_unlink_local_node(dd_broker_t *self, zframe_t *sockid,
                                 uint64_t cookie);
void hashtable_insert_local_node(dd_broker_t *self, zframe_t *sockid,
                                 char *name);
int remove_subscriptions(dd_broker_t *self, zframe_t *sockid);
int remove_subscription(dd_broker_t *self, zframe_t *sockid, char *topic);
int insert_subscription(dd_broker_t *self, zframe_t *sockid, char *topic);
void hashtable_subscribe_destroy(struct cds_lfht **self_p);
void hashtable_local_client_destroy(struct cds_lfht **self_p);
int zlist_contains_str(zlist_t *list, char *string);
void print_zlist_str(zlist_t *list);
void print_sub_ht(dd_broker_t *self);
void print_local_ht(dd_broker_t *self);
void print_dist_ht(dd_broker_t *self);
void print_broker_ht(dd_broker_t *self);
#endif
