#ifndef _DDHTABLE_H_
#define _DDHTABLE_H_
#include <urcu.h>
#include <urcu/rculfhash.h>
#include <czmq.h>
#include "ddkeys.h"
extern struct cds_lfht *lcl_cli_ht;
extern struct cds_lfht *rev_lcl_cli_ht;
extern struct cds_lfht *dist_cli_ht;
extern struct cds_lfht *lcl_br_ht;
extern struct cds_lfht *subscribe_ht;
extern struct cds_lfht *top_north_ht;
extern struct cds_lfht *top_south_ht;

#define RCU_MEMBARRIER
#define XXHSEED 1234
#define MAXTENANTNAME 256

// subscriptions[sockid] = ["b.topicA/0/1/2/", "b.topicB/0/1/2/"]
typedef struct subscription_node {
  zlist_t *topics;
  zframe_t *sockid;
  struct cds_lfht_node node;
} subscribe_node;

// Local nodes
typedef struct lcl_node {
  char *name; // client name		/* Node content */
  char *prefix_name;
  char *tenant;
  uint64_t cookie;
  zframe_t *sockid;
  int timeout;
  // sockid_node for lcl_cli_ht
  // prename_node and rev_lcl_cli_ht (combine with dist_node?)
  struct cds_lfht_node lcl_node; // Chaining in hash table
  struct cds_lfht_node rev_node; // Chaining in hash table
  // struct cds_lfht_node node;
} local_client;

// Distant nodes
typedef struct dist_node {
  char *name; /* Node content */
  zframe_t *broker;
  int distance;
  struct cds_lfht_node node; /* Chaining in hash table */
} dist_client;

// Local broker
typedef struct lcl_broker {
  zframe_t *sockid;
  uint64_t cookie;
  int distance;
  int timeout;
  struct cds_lfht_node node; /* Chaining in hash table */
} local_broker;

void init_hashtables(void);
int insert_local_client(zframe_t *, ddtenant_t *, char *);
void print_local_ht(void);
void hashtable_remove_dist_node(char *);
dist_client *hashtable_has_dist_node(char *);
local_broker *hashtable_has_local_broker(zframe_t *, uint64_t cookie,
                                         int update);
local_client *hashtable_has_rev_local_node(char *prefix_name, int update);
local_client *hashtable_has_local_node(zframe_t *sockid, zframe_t *cookie,
                                       int update);
void hashtable_insert_local_broker(zframe_t *sockid, uint64_t cookie);
void hashtable_insert_dist_node(char *prefix_name, zframe_t *sockid, int dist);
void hashtable_remove_local_broker(char *source);
void hashtable_unlink_rev_local_node(char *prefix_name);
void hashtable_unlink_local_node(zframe_t *sockid, uint64_t cookie);
void hashtable_insert_local_node(zframe_t *sockid, char *name);
int insert_subscription(zframe_t *sockid, char *topic);
int remove_subscriptions(zframe_t *sockid);
int remove_subscription(zframe_t *sockid, char *topic);
char *zframe_tostr(zframe_t *self, char *buffer);
#endif
