//#include "../include/dd.h"
//#include "../include/dd_classes.h"
#include "xxhash.h"
static int match_lcl_node_prename(struct cds_lfht_node *ht_node,
                                  const void *_key) {
  local_client *node = caa_container_of(ht_node, local_client, rev_node);
  const char *key = (const char *)_key;
  return (strncmp(node->prefix_name, key, strlen(key)) == 0);
}
static int match_lcl_node_sockid(struct cds_lfht_node *ht_node,
                                 const void *_key) {
  local_client *node = caa_container_of(ht_node, local_client, lcl_node);
  zframe_t *key = (zframe_t *)_key;
  return memcmp(zframe_data(node->sockid), zframe_data(key),
                zframe_size(key)) == 0;
}
static int match_lcl_broker(struct cds_lfht_node *ht_node, const void *_key) {

  local_broker *node = caa_container_of(ht_node, local_broker, node);
  zframe_t *key = (zframe_t *)_key;

  return memcmp(zframe_data(node->sockid), zframe_data(key),
                zframe_size(key)) == 0;
}
static int match_dist_node(struct cds_lfht_node *ht_node, const void *_key) {
  dist_client *node = caa_container_of(ht_node, dist_client, node);
  const char *key = (const char*) _key;
  return (strncmp(node->name, key, strlen(key)) == 0);
}
static int match_subscribe_node(struct cds_lfht_node *ht_node,
                                const void *_key) {
  subscribe_node *node = caa_container_of(ht_node, subscribe_node, node);
  zframe_t *key = (zframe_t *)_key;
  return memcmp(zframe_data(node->sockid), zframe_data(key),
                zframe_size(key)) == 0;
}

// what lookups are needed?
// sockid + cookie -> data || NULL  (local_cli / registered_client)
// "tenant.client_name" -> data || NULL (reverse_local_cli)
// any more?
int insert_local_client(dd_broker_t *self, zframe_t *sockid, ddtenant_t *ten,
                        char *client_name) {
  XXH32_state_t hash1;
  char prefix_name[MAXTENANTNAME];
  struct cds_lfht_iter iter;
  local_client *np;
  np = (local_client*) malloc(sizeof(local_client));
  np->cookie = ten->cookie;
  np->timeout = 0;
  np->sockid = zframe_dup(sockid);
  np->tenant = ten->name;
  np->name = strdup(client_name);

  int prelen =
      snprintf(prefix_name, MAXTENANTNAME, "%s.%s", ten->name, client_name);

  /* dd_debug("insert_local_client: prefix_name %s", prefix_name); */
  np->prefix_name = strdup(prefix_name);

  // Calculate the sockid_cookie hash
  XXH32_reset(&hash1, XXHSEED);
  XXH32_update(&hash1, zframe_data(sockid), zframe_size(sockid));
  XXH32_update(&hash1, &ten->cookie, sizeof(uint64_t));
  unsigned long int sockid_cookie = XXH32_digest(&hash1);

  // Calculate the prefix_name hash
  XXH32_reset(&hash1, XXHSEED);
  XXH32_update(&hash1, prefix_name, prelen + 1);
  unsigned long int prename = XXH32_digest(&hash1);

  dist_client *dn;
  if ((dn = hashtable_has_dist_node(self, np->prefix_name))) {
    goto cleanup;
  }

  // Check if already there?
  rcu_read_lock();
  cds_lfht_lookup(self->lcl_cli_ht, sockid_cookie, match_lcl_node_sockid,
                  sockid, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  if (ht_node) {
    rcu_read_unlock();
    dd_warning("Found duplicate node SOCKID+cookie");
    //    zframe_print(sockid, NULL);
    goto cleanup;
  }

  cds_lfht_lookup(self->rev_lcl_cli_ht, prename, match_lcl_node_prename,
                  prefix_name, &iter);
  ht_node = cds_lfht_iter_get_node(&iter);
  if (ht_node) {
    rcu_read_unlock();
    dd_warning("Found duplicate node %s", np->prefix_name);
    goto cleanup;
  }

  // add both sockid_cookie and prename to same hashtable
  cds_lfht_node_init(&np->lcl_node);
  cds_lfht_node_init(&np->rev_node);
  cds_lfht_add(self->lcl_cli_ht, sockid_cookie, &np->lcl_node);
  cds_lfht_add(self->rev_lcl_cli_ht, prename, &np->rev_node);
  rcu_read_unlock();
  return 1;

cleanup:
  zframe_destroy(&np->sockid);
  free(np->prefix_name);
  free(np->name);
  free(np);
  return -1;
}

void hashtable_remove_dist_node(dd_broker_t *self, char *prefix_name) {
  struct cds_lfht_iter iter;
  int hash = XXH32(prefix_name, strlen(prefix_name), XXHSEED);
  rcu_read_lock();
  cds_lfht_lookup(self->dist_cli_ht, hash, match_dist_node, prefix_name, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  if (!ht_node) {
    dd_warning("Distant client key %s not found", prefix_name);
    rcu_read_unlock();
  } else {
    int ret = cds_lfht_del(self->dist_cli_ht, ht_node);
    dist_client *mp = caa_container_of(ht_node, dist_client, node);
    if (ret) {
      dd_debug(" - Distant client %s deleted (concurrently)", mp->name);
      rcu_read_unlock();
    } else {
      rcu_read_unlock();
      dd_debug(" - Dist client %s deleted", mp->name);
      free(mp);
    }
  }
}
dist_client *hashtable_has_dist_node(dd_broker_t *self, char *prefix_name) {
  /*
   * hash lookup to see if local
   */
  struct cds_lfht_iter iter;
  dist_client *np;
  int hash = XXH32(prefix_name, strlen(prefix_name), XXHSEED);
  rcu_read_lock();
  cds_lfht_lookup(self->dist_cli_ht, hash, match_dist_node, prefix_name, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  rcu_read_unlock();
  if (ht_node) {
    np = caa_container_of(ht_node, dist_client, node);
    return np;
  }
  return NULL;
}
/*
 * distant node stuff
 */
void hashtable_insert_dist_node(dd_broker_t *self, char *prefix_name,
                                zframe_t *sockid, int dist) {
  // add to has table
  dist_client *mp = (dist_client*) malloc(sizeof(dist_client));
  int hash = XXH32(prefix_name, strlen(prefix_name), XXHSEED);
  cds_lfht_node_init(&mp->node);
  mp->name = prefix_name;
  mp->broker = zframe_dup(sockid);
  mp->distance = dist;
  rcu_read_lock();
  cds_lfht_add(self->dist_cli_ht, hash, &mp->node);
  rcu_read_unlock();
}
void delete_dist_clients(dd_broker_t *self, local_broker *br) {
  struct cds_lfht_iter iter;
  dist_client *mp;

  cds_lfht_first(self->dist_cli_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  /* dd_debug("delete_dist_clients:"); */
  //  zframe_print(br->sockid, "broker");
  while (ht_node != NULL) {
    mp = caa_container_of(ht_node, dist_client, node);
    dd_debug("Distclient %s", mp->name);
    //    zframe_print(mp->broker, "client");
    if (zframe_eq(mp->broker, br->sockid)) {
      char buf[256] = "";
      dd_debug("Was under missing broker %s", zframe_tostr(br->sockid, buf));
      del_cli_up(self, mp->name);
      rcu_read_lock();
        int ret = cds_lfht_del(self->dist_cli_ht, ht_node);
        if(ret < 0){
            dd_error("Could not delete distclient hashtable entry!");
        }
      rcu_read_unlock();
    }
    cds_lfht_next(self->dist_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }
}

local_broker *hashtable_has_local_broker(dd_broker_t *self, zframe_t *sockid,
                                         uint64_t cookie, int update) {
  /*
   * hash lookup to see if local
   */
  struct cds_lfht_iter iter;
  local_broker *np;
  XXH32_state_t hash1;
  XXH32_reset(&hash1, XXHSEED);
  XXH32_update(&hash1, zframe_data(sockid), zframe_size(sockid));
  XXH32_update(&hash1, &cookie, sizeof(uint64_t));
  unsigned long int sockid_cookie = XXH32_digest(&hash1);

  rcu_read_lock();
  cds_lfht_lookup(self->lcl_br_ht, sockid_cookie, match_lcl_broker, sockid,
                  &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  rcu_read_unlock();
  if (ht_node) {
    np = caa_container_of(ht_node, local_broker, node);
    if (update)
      np->timeout = 0;
    return np;
  }
  return NULL;
}
/*
 * local broker stuff
 */
void hashtable_insert_local_broker(dd_broker_t *self, zframe_t *sockid,
                                   uint64_t cookie) {
  local_broker *mp = (local_broker*) malloc(sizeof(local_broker));
  XXH32_state_t hash1;
  XXH32_reset(&hash1, XXHSEED);
  XXH32_update(&hash1, zframe_data(sockid), zframe_size(sockid));
  XXH32_update(&hash1, &cookie, sizeof(uint64_t));
  unsigned long int sockid_cookie = XXH32_digest(&hash1);
  cds_lfht_node_init(&mp->node);
  mp->cookie = cookie;
  mp->sockid = zframe_dup(sockid);
  mp->distance = 0;
  mp->timeout = 0;

  rcu_read_lock();
  cds_lfht_add(self->lcl_br_ht, sockid_cookie, &mp->node);
  rcu_read_unlock();
}

local_client *hashtable_has_rev_local_node(dd_broker_t *self, char *prefix_name,
                                           int update) {
  struct cds_lfht_iter iter;
  local_client *np;
  XXH32_state_t hash1;
  XXH32_reset(&hash1, XXHSEED);
  int prelen = strlen(prefix_name);
  XXH32_update(&hash1, prefix_name, prelen + 1);
  unsigned long int prename = XXH32_digest(&hash1);
  dd_debug("hashtable_has_rev_local_node\nprefix_name: %s hash: %lu",
           prefix_name, prename);
  rcu_read_lock();
  cds_lfht_lookup(self->rev_lcl_cli_ht, prename, match_lcl_node_prename,
                  prefix_name, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  rcu_read_unlock();
  if (ht_node) {
    dd_debug("hashtable_has_rev_local_node, match found!");
    np = caa_container_of(ht_node, local_client, rev_node);
    if (update)
      np->timeout = 0;
    return np;
  }
  dd_debug("hashtable_has_rev_local_node, no match found!");
  return NULL;
}

local_client *hashtable_has_local_node(dd_broker_t *self, zframe_t *sockid,
                                       zframe_t *cookie, int update) {
  struct cds_lfht_iter iter;
  local_client *np;
  XXH32_state_t hash1;
  XXH32_reset(&hash1, XXHSEED);
  XXH32_update(&hash1, zframe_data(sockid), zframe_size(sockid));
  XXH32_update(&hash1, zframe_data(cookie), zframe_size(cookie));
  unsigned long int sockid_cookie = XXH32_digest(&hash1);

  rcu_read_lock();
  cds_lfht_lookup(self->lcl_cli_ht, sockid_cookie, match_lcl_node_sockid,
                  sockid, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  rcu_read_unlock();
  if (ht_node) {
    np = caa_container_of(ht_node, local_client, lcl_node);
    if (update)
      np->timeout = 0;
    return np;
  }
  return NULL;
}
// Does not free the local_client pointer
void hashtable_unlink_rev_local_node(dd_broker_t *self, char *prefix_name) {

  struct cds_lfht_iter iter;
  local_client *np;
  XXH32_state_t hash1;
  XXH32_reset(&hash1, XXHSEED);
  int prelen = strlen(prefix_name);
  XXH32_update(&hash1, prefix_name, prelen + 1);
  unsigned long int prename = XXH32_digest(&hash1);
  dd_debug("hashtable_has_rev_local_node\nprefix_name: %s hash: %lu",
           prefix_name, prename);
  rcu_read_lock();
  cds_lfht_lookup(self->rev_lcl_cli_ht, prename, match_lcl_node_prename,
                  prefix_name, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  if (!ht_node) {
    dd_debug("hashtable_unlink_rev_local_node: Local key %s not found ",
             prefix_name);
    rcu_read_unlock();
  } else {
    int ret = cds_lfht_del(self->rev_lcl_cli_ht, ht_node);
    np = caa_container_of(ht_node, local_client, rev_node);
    if (ret) {
      dd_debug("hashtable_unlink_local_node: Local key %s concurrently "
               "unlinked",
               np->prefix_name);
      rcu_read_unlock();
    } else {
      rcu_read_unlock();
      synchronize_rcu();
      dd_debug("hashtable_unlink_local_node: Local key %s unlinked",
               np->prefix_name);
    }
  }
}
// Does not free the local_client pointer
void hashtable_unlink_local_node(dd_broker_t *self, zframe_t *sockid,
                                 uint64_t cookie) {
  struct cds_lfht_iter iter;
  local_client *np;
  XXH32_state_t hash1;
  XXH32_reset(&hash1, XXHSEED);
  XXH32_update(&hash1, zframe_data(sockid), zframe_size(sockid));
  XXH32_update(&hash1, &cookie, sizeof cookie);

  unsigned long int sockid_cookie = XXH32_digest(&hash1);

  rcu_read_lock();
  cds_lfht_lookup(self->lcl_cli_ht, sockid_cookie, match_lcl_node_sockid,
                  sockid, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  if (!ht_node) {
    dd_debug("hashtable_unlink_local_node: Local key not found ");
    zframe_print(sockid, NULL);
    rcu_read_unlock();
  } else {
    int ret = cds_lfht_del(self->lcl_cli_ht, ht_node);
    np = caa_container_of(ht_node, local_client, lcl_node);
    if (ret) {
      dd_debug("hashtable_unlink_local_node: Local key %s concurrently "
               "unlinked",
               np->prefix_name);
      rcu_read_unlock();
    } else {
      rcu_read_unlock();
      synchronize_rcu();
      dd_debug("hashtable_unlink_local_node: Local key %s unlinked",
               np->prefix_name);
    }
  }
}

/*
 * local node stuff
 */
void hashtable_insert_local_node(dd_broker_t *self, zframe_t *sockid,
                                 char *name) {
  // add to hash table
  local_client *mp = (local_client*) malloc(sizeof(local_client));
  int hash = XXH32(zframe_data(sockid), zframe_size(sockid), XXHSEED);
  cds_lfht_node_init(&mp->lcl_node);
  mp->sockid = sockid;
  mp->name = name;
  mp->timeout = 0;
  rcu_read_lock();
  cds_lfht_add(self->lcl_cli_ht, hash, &mp->lcl_node);
  rcu_read_unlock();
}

// return 0 if no subscriptions were found
// otherwise , return how many was removed
int remove_subscriptions(dd_broker_t *self, zframe_t *sockid) {
  struct cds_lfht_iter iter;
  int hash = XXH32(zframe_data(sockid), zframe_size(sockid), XXHSEED);
  subscribe_node *sn;

  rcu_read_lock();
  cds_lfht_lookup(self->subscribe_ht, hash, match_subscribe_node, sockid,
                  &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  rcu_read_unlock();
  if (!ht_node)
    return 0;

  sn = (subscribe_node *)caa_container_of(ht_node, subscribe_node, node);

  int ntop = 0;
  if (sn->topics) {
    ntop = zlist_size(sn->topics);
    char *topic = (char *) zlist_first(sn->topics);
    char *oldtopic;
    while (topic) {

      nn_trie_unsubscribe(&self->topics_trie, (uint8_t *)topic, strlen(topic),
                          sockid, 1);
      oldtopic = topic;
      topic = (char *)zlist_next(sn->topics);
      free(oldtopic);
    }

    zlist_destroy(&sn->topics);
    zframe_destroy(&sn->sockid);
  }
  rcu_read_lock();
  cds_lfht_del(self->subscribe_ht, ht_node);
  rcu_read_unlock();
  free(sn);
  return ntop;
}

// return 0 if no subscriptions were found
// otherwise , return how many was removed
int remove_subscription(dd_broker_t *self, zframe_t *sockid, char *topic) {
  struct cds_lfht_iter iter;
  int hash = XXH32(zframe_data(sockid), zframe_size(sockid), XXHSEED);
  subscribe_node *sn;

  rcu_read_lock();
  cds_lfht_lookup(self->subscribe_ht, hash, match_subscribe_node, sockid,
                  &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  rcu_read_unlock();
  if (!ht_node)
    return 0;

  sn = (subscribe_node *)caa_container_of(ht_node, subscribe_node, node);

  int ntop = 0;
  if (sn->topics) {
    ntop = zlist_size(sn->topics);
    char *t = (char *)zlist_first(sn->topics);
    while (t) {
      if (strcmp(topic, t) == 0) {
        nn_trie_unsubscribe(&self->topics_trie, (uint8_t *)topic, strlen(topic),
                            sockid, 1);
        zlist_remove(sn->topics, t);
        free(t);
      }
      t = (char *)zlist_next(sn->topics);
    }

    zlist_destroy(&sn->topics);
    zframe_destroy(&sn->sockid);
  }
  rcu_read_lock();
  cds_lfht_del(self->subscribe_ht, ht_node);
  rcu_read_unlock();
  free(sn);
  return ntop;
}

// add subscription for "topic" to "sockid"
// return 0 topic already existed
// return 1 if it was appended
// return 2 if new entry was created
int insert_subscription(dd_broker_t *self, zframe_t *sockid, char *topic) {
  struct cds_lfht_iter iter;
  int hash = XXH32(zframe_data(sockid), zframe_size(sockid), XXHSEED);
  subscribe_node *sn;

  rcu_read_lock();
  cds_lfht_lookup(self->subscribe_ht, hash, match_subscribe_node, sockid,
                  &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  rcu_read_unlock();

  // already there, append topic
  if (ht_node) {
    sn = (subscribe_node *)caa_container_of(ht_node, subscribe_node, node);
    if (zlist_contains_str(sn->topics, topic) == 0) {
      zlist_append(sn->topics, strdup(topic));
      return 1;
    } else {
      // already there, return 0
      return 0;
    }
  }
  // first insertion, create new node
  sn = (subscribe_node *)  malloc(sizeof(subscribe_node));
  cds_lfht_node_init(&sn->node);
  sn->sockid = zframe_dup(sockid);
  sn->topics = zlist_new();
  zlist_append(sn->topics, strdup(topic));
  rcu_read_lock();
  cds_lfht_add(self->subscribe_ht, hash, &sn->node);
  rcu_read_unlock();
  return 2;
}

void hashtable_subscribe_destroy(struct cds_lfht **self_p) {
  if (self_p) {
    struct cds_lfht *self = *self_p;
    subscribe_node *sn;
    struct cds_lfht_iter iter;

    rcu_read_lock();
    cds_lfht_first(self, &iter);
    struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
    while (ht_node) {
      sn = (subscribe_node *)caa_container_of(ht_node, subscribe_node, node);

      zframe_destroy(&sn->sockid);
      if (sn->topics) {
        zlist_autofree(sn->topics);
        zlist_destroy(&sn->topics);
      }
      cds_lfht_next(self, &iter);
      ht_node = cds_lfht_iter_get_node(&iter);
    }
    rcu_read_unlock();
    int rc = cds_lfht_destroy(self, NULL);
      if(rc != 0){
          dd_error("Could not delete hashtable!");
      }

    *self_p = NULL;
  }
}
void hashtable_local_client_destroy(struct cds_lfht **self_p) {
  if (self_p) {
    struct cds_lfht *self = *self_p;
    local_client *lc;
    struct cds_lfht_iter iter;

    rcu_read_lock();
    cds_lfht_first(self, &iter);
    struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
    while (ht_node) {
      lc = (local_client *)caa_container_of(ht_node, local_client, lcl_node);
      zframe_destroy(&lc->sockid);

      if (lc->name) {
        free(lc->name);
        lc->name = NULL;
      }
      if (lc->prefix_name) {
        free(lc->prefix_name);
        lc->prefix_name = NULL;
      }
      cds_lfht_next(self, &iter);
      ht_node = cds_lfht_iter_get_node(&iter);
    }
    rcu_read_unlock();

    int rc = cds_lfht_destroy(self, NULL);
      if(rc != 0){
          dd_error("Could not destroy hashtable!");
      }
    *self_p = NULL;
  }
}

int zlist_contains_str(zlist_t *list, char *string) {
  char *str = (char*) zlist_first(list);
  while (str) {
    if (strcmp(string, str) == 0)
      return 1;

    str = (char *)zlist_next(list);
  }
  return 0;
}

void print_zlist_str(zlist_t *list) {
  if (list == NULL)
    return;
  char *str = (char *)zlist_first(list);
  while (str) {
    dd_debug("topic: %s", str);
    str = (char *)zlist_next(list);
  }
}
void print_sub_ht(dd_broker_t *self) {
  struct cds_lfht_iter iter;
  subscribe_node *mp;
  cds_lfht_first(self->subscribe_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  dd_debug("Hashtable: subscribe_ht");
  while (ht_node != NULL) {
    mp = caa_container_of(ht_node, subscribe_node, node);
    zframe_print(mp->sockid, "mp->sockid");
    print_zlist_str(mp->topics);
    cds_lfht_next(self->lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }
}
void print_local_ht(dd_broker_t *self) {
  struct cds_lfht_iter iter;
  local_client *mp;
  cds_lfht_first(self->lcl_cli_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  dd_debug("Local clients: SOCKID");
  while (ht_node != NULL) {
    mp = caa_container_of(ht_node, local_client, lcl_node);
    zframe_print(mp->sockid, "mp->sockid");
    cds_lfht_next(self->lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }

  dd_debug("Local clients: PREFIXNAME");
  cds_lfht_first(self->rev_lcl_cli_ht, &iter);
  ht_node = cds_lfht_iter_get_node(&iter);
  while (ht_node != NULL) {
    local_client *mp = caa_container_of(ht_node, local_client, rev_node);
    dd_debug("\t%s", mp->prefix_name);
    cds_lfht_next(self->lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }
}
void print_dist_ht(dd_broker_t *self) {
  struct cds_lfht_iter iter;
  dist_client *mp;
  cds_lfht_first(self->dist_cli_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  dd_debug("Distant client: PREFIXNAME");
  while (ht_node != NULL) {
    mp = caa_container_of(ht_node, dist_client, node);
    dd_debug("\tname: %s", mp->name);
    cds_lfht_next(self->lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }
}
void print_broker_ht(dd_broker_t *self) {
  struct cds_lfht_iter iter;
  local_broker *mp;
  cds_lfht_first(self->lcl_br_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  dd_debug("Hashtable: lcl_br_ht");
  while (ht_node != NULL) {
    mp = caa_container_of(ht_node, local_broker, node);
    char buf[256];
    dd_debug("\tname: %s", zframe_tostr(mp->sockid, buf));
    cds_lfht_next(self->lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }
}
