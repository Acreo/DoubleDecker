// hash-table for local clients
struct cds_lfht *lcl_cli_ht;
struct cds_lfht *rev_lcl_cli_ht;

// hash-table for distant clients
struct cds_lfht *dist_cli_ht;
// hash-table for local br
struct cds_lfht *lcl_br_ht;

// hash-table for subscriptions
struct cds_lfht *subscribe_ht;
// hash-table for topics north
struct cds_lfht *top_north_ht;
// hash-table for topics  south
struct cds_lfht *top_south_ht;

#include <urcu.h>
#include <urcu/rculfhash.h>
#include "../include/ddlog.h"
#include "../include/ddhtable.h"
#include "../include/xxhash.h"
#include "../include/murmurhash.h"
#include "../include/ddkeys.h"
#include "../include/trie.h"

void init_hashtables() {
  lcl_cli_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  rev_lcl_cli_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  dist_cli_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  lcl_br_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);

  subscribe_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  top_north_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  top_south_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
}
static int match_lcl_node_prename(struct cds_lfht_node *ht_node,
                                  const void *_key) {
  local_client *node = caa_container_of(ht_node, local_client, rev_node);
  const char *key = _key;
  return (strncmp(node->prefix_name, key, strlen(key)) == 0);
}

static int match_lcl_node_sockid(struct cds_lfht_node *ht_node,
                                 const void *_key) {
  local_client *node = caa_container_of(ht_node, local_client, lcl_node);
  zframe_t *key = (zframe_t *)_key;
  return memcmp(zframe_data(node->sockid), zframe_data(key),
                zframe_size(key)) == 0;
}

static int match_lcl_broker(struct cds_lfht_node *ht_node,
                            const void *_key) {

  local_broker *node = caa_container_of(ht_node, local_broker, node);
  zframe_t *key = _key;

  return memcmp(zframe_data(node->sockid), zframe_data(key),
                zframe_size(key)) == 0;
}

static int match_dist_node(struct cds_lfht_node *ht_node,
                           const void *_key) {
  struct dist_node *node =
      caa_container_of(ht_node, struct dist_node, node);
  const char *key = _key;
  return (strncmp(node->name, key, strlen(key)) == 0);
}

// what lookups are needed?
// sockid + cookie -> data || NULL  (local_cli / registered_client)
// "tenant.client_name" -> data || NULL (reverse_local_cli)
// any more?
int insert_local_client(zframe_t *sockid, ddtenant_t *ten,
                        char *client_name) {
  XXH32_state_t hash1;
  char prefix_name[MAXTENANTNAME];
  struct cds_lfht_iter iter;
  local_client *np;
  np = malloc(sizeof(local_client));
  np->cookie = ten->cookie;
  np->timeout = 0;
  np->sockid = zframe_dup(sockid);
  np->tenant = ten->name;
  np->name = strdup(client_name);

  int prelen = snprintf(prefix_name, MAXTENANTNAME, "%s.%s", ten->name,
                        client_name);

  dd_debug("insert_local_client: prefix_name %s", prefix_name);
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

  // Check if already there?
  rcu_read_lock();
  cds_lfht_lookup(lcl_cli_ht, sockid_cookie, match_lcl_node_sockid, sockid,
                  &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  if (ht_node) {
    rcu_read_unlock();
    dd_warning("Found duplicate node SOCKID+cookie");
    zframe_print(sockid, NULL);
    goto cleanup;
  }

  cds_lfht_lookup(rev_lcl_cli_ht, prename, match_lcl_node_prename,
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
  cds_lfht_add(lcl_cli_ht, sockid_cookie, &np->lcl_node);
  cds_lfht_add(rev_lcl_cli_ht, prename, &np->rev_node);
  rcu_read_unlock();
  return 1;

cleanup:
  zframe_destroy(&np->sockid);
  free(np->prefix_name);
  free(np->name);
  free(np);
  return -1;
}

void print_local_ht() {
  struct cds_lfht_iter iter;
  local_client *mp;
  cds_lfht_first(lcl_cli_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  dd_debug("Local clients: SOCKID");
  while (ht_node != NULL) {
    mp = caa_container_of(ht_node, local_client, lcl_node);
    zframe_print(mp->sockid, NULL);
    cds_lfht_next(lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }

  dd_debug("Local clients: PREFIXNAME");
  cds_lfht_first(rev_lcl_cli_ht, &iter);
  ht_node = cds_lfht_iter_get_node(&iter);
  while (ht_node != NULL) {
    local_client *mp = caa_container_of(ht_node, local_client, rev_node);
    dd_debug("\t%s", mp->prefix_name);
    cds_lfht_next(lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }
}

void print_dist_ht() {
  struct cds_lfht_iter iter;
  dist_client *mp;
  cds_lfht_first(dist_cli_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  dd_debug("Distant client: PREFIXNAME");
  while (ht_node != NULL) {
    mp = caa_container_of(ht_node, dist_client, node);
    dd_debug("\tname: %s", mp->name);
    cds_lfht_next(lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }
}

void print_broker_ht() {
  struct cds_lfht_iter iter;
  local_broker *mp;
  cds_lfht_first(lcl_br_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  dd_debug("Hashtable: lcl_br_ht");
  while (ht_node != NULL) {
    mp = caa_container_of(ht_node, local_broker, node);
    char buf[256];
    dd_debug("\tname: %s", zframe_tostr(mp->sockid, buf));
    cds_lfht_next(lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }
}

void hashtable_remove_dist_node(char *prefix_name) {
  struct cds_lfht_iter iter;
  int hash = XXH32(prefix_name, strlen(prefix_name), XXHSEED);
  rcu_read_lock();
  cds_lfht_lookup(dist_cli_ht, hash, match_dist_node, prefix_name, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  if (!ht_node) {
    dd_warning("Distant client key %s not found", prefix_name);
    rcu_read_unlock();
  } else {
    int ret = cds_lfht_del(dist_cli_ht, ht_node);
    struct dist_node *mp =
        caa_container_of(ht_node, struct dist_node, node);
    if (ret) {
      dd_info(" - Distant client %s deleted (concurrently)", mp->name);
      rcu_read_unlock();
    } else {
      rcu_read_unlock();
      synchronize_rcu();
      dd_info(" - Dist client %s deleted", mp->name);
      free(mp);
    }
  }
}
struct dist_node *hashtable_has_dist_node(char *prefix_name) {
  /*
   * hash lookup to see if local
   */
  struct cds_lfht_iter iter;
  dist_client *np;
  int hash = XXH32(prefix_name, strlen(prefix_name), XXHSEED);
  rcu_read_lock();
  cds_lfht_lookup(dist_cli_ht, hash, match_dist_node, prefix_name, &iter);
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
void hashtable_insert_dist_node(char *prefix_name, zframe_t *sockid,
                                int dist) {
  // add to has table
  dist_client *mp = malloc(sizeof(dist_client));
  int hash = XXH32(prefix_name, strlen(prefix_name), XXHSEED);
  cds_lfht_node_init(&mp->node);
  mp->name = prefix_name;
  mp->broker = zframe_dup(sockid);
  mp->distance = dist;
  rcu_read_lock();
  cds_lfht_add(dist_cli_ht, hash, &mp->node);
  rcu_read_unlock();
}

local_broker *hashtable_has_local_broker(zframe_t *sockid, uint64_t cookie,
                                         int update) {
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
  cds_lfht_lookup(lcl_br_ht, sockid_cookie, match_lcl_broker, sockid,
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
void hashtable_insert_local_broker(zframe_t *sockid, uint64_t cookie) {
  local_broker *mp = malloc(sizeof(local_broker));
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
  cds_lfht_add(lcl_br_ht, sockid_cookie, &mp->node);
  rcu_read_unlock();
}

local_client *hashtable_has_rev_local_node(char *prefix_name, int update) {
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
  cds_lfht_lookup(rev_lcl_cli_ht, prename, match_lcl_node_prename,
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

local_client *hashtable_has_local_node(zframe_t *sockid, zframe_t *cookie,
                                       int update) {
  struct cds_lfht_iter iter;
  local_client *np;
  XXH32_state_t hash1;
  XXH32_reset(&hash1, XXHSEED);
  XXH32_update(&hash1, zframe_data(sockid), zframe_size(sockid));
  XXH32_update(&hash1, zframe_data(cookie), zframe_size(cookie));
  unsigned long int sockid_cookie = XXH32_digest(&hash1);

  rcu_read_lock();
  cds_lfht_lookup(lcl_cli_ht, sockid_cookie, match_lcl_node_sockid, sockid,
                  &iter);
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
void hashtable_unlink_rev_local_node(char *prefix_name) {

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
  cds_lfht_lookup(rev_lcl_cli_ht, prename, match_lcl_node_prename,
                  prefix_name, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  if (!ht_node) {
    dd_debug("hashtable_unlink_rev_local_node: Local key %s not found ",
             prefix_name);
    rcu_read_unlock();
  } else {
    int ret = cds_lfht_del(rev_lcl_cli_ht, ht_node);
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
void hashtable_unlink_local_node(zframe_t *sockid, uint64_t cookie) {
  struct cds_lfht_iter iter;
  local_client *np;
  XXH32_state_t hash1;
  XXH32_reset(&hash1, XXHSEED);
  XXH32_update(&hash1, zframe_data(sockid), zframe_size(sockid));
  XXH32_update(&hash1, &cookie, sizeof cookie);

  unsigned long int sockid_cookie = XXH32_digest(&hash1);

  rcu_read_lock();
  cds_lfht_lookup(lcl_cli_ht, sockid_cookie, match_lcl_node_sockid, sockid,
                  &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  if (!ht_node) {
    dd_debug("hashtable_unlink_local_node: Local key not found ");
    zframe_print(sockid, NULL);
    rcu_read_unlock();
  } else {
    int ret = cds_lfht_del(lcl_cli_ht, ht_node);
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
void hashtable_insert_local_node(zframe_t *sockid, char *name) {
  // add to hash table
  local_client *mp = malloc(sizeof(local_client));
  int hash = XXH32(zframe_data(sockid), zframe_size(sockid), XXHSEED);
  cds_lfht_node_init(&mp->lcl_node);
  mp->sockid = sockid;
  mp->name = name;
  mp->timeout = 0;
  rcu_read_lock();
  cds_lfht_add(lcl_cli_ht, hash, &mp->lcl_node);
  rcu_read_unlock();
}

static int match_subscribe_node(struct cds_lfht_node *ht_node,
                                const void *_key) {
  subscribe_node *node = caa_container_of(ht_node, subscribe_node, node);
  zframe_t *key = (zframe_t *)_key;
  return memcmp(zframe_data(node->sockid), zframe_data(key),
                zframe_size(key)) == 0;
}

int zlist_contains_str(zlist_t *list, char *string) {
  char *str = zlist_first(list);
  while (str) {
    if (strcmp(string, str) == 0)
      return 1;

    str = zlist_next(list);
  }
  return 0;
}

void print_zlist_str(zlist_t *list) {
  if (list == NULL)
    return;
  char *str = zlist_first(list);
  while (str) {
    dd_debug("topic: %s", str);
    str = zlist_next(list);
  }
}

void print_sub_ht() {
  struct cds_lfht_iter iter;
  subscribe_node *mp;
  cds_lfht_first(subscribe_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  dd_debug("Hashtable: subscribe_ht");
  while (ht_node != NULL) {
    mp = caa_container_of(ht_node, subscribe_node, node);
    dd_debug("\tsockid: ");
    zframe_print(mp->sockid, NULL);
    print_zlist_str(mp->topics);
    cds_lfht_next(lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }
}

// return 0 if no subscriptions were found
// otherwise , return how many was removed
int remove_subscriptions(zframe_t *sockid) {
  struct cds_lfht_iter iter;
  int hash = XXH32(zframe_data(sockid), zframe_size(sockid), XXHSEED);
  subscribe_node *sn;

  rcu_read_lock();
  cds_lfht_lookup(subscribe_ht, hash, match_subscribe_node, sockid, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  rcu_read_unlock();
  if (!ht_node)
    return 0;

  sn = (subscribe_node *)caa_container_of(ht_node, subscribe_node, node);

  int ntop = 0;
  if (sn->topics) {
    ntop = zlist_size(sn->topics);
    char *topic = zlist_first(sn->topics);
    char *oldtopic;
    while (topic) {

      nn_trie_unsubscribe(&topics_trie, (uint8_t *)topic, strlen(topic),
                          sockid, 1);
      oldtopic = topic;
      topic = zlist_next(sn->topics);
      free(oldtopic);
    }

    zlist_destroy(&sn->topics);
    zframe_destroy(&sn->sockid);
  }
  rcu_read_lock();
  cds_lfht_del(subscribe_ht, ht_node);
  rcu_read_unlock();
  free(sn);
  return ntop;
}

// return 0 if no subscriptions were found
// otherwise , return how many was removed
int remove_subscription(zframe_t *sockid, char *topic) {
  struct cds_lfht_iter iter;
  int hash = XXH32(zframe_data(sockid), zframe_size(sockid), XXHSEED);
  subscribe_node *sn;

  rcu_read_lock();
  cds_lfht_lookup(subscribe_ht, hash, match_subscribe_node, sockid, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  rcu_read_unlock();
  if (!ht_node)
    return 0;

  sn = (subscribe_node *)caa_container_of(ht_node, subscribe_node, node);

  int ntop = 0;
  if (sn->topics) {
    ntop = zlist_size(sn->topics);
    char *t = zlist_first(sn->topics);
    char *oldt;
    while (topic) {
      if (strcmp(topic, t) == 0) {
        nn_trie_unsubscribe(&topics_trie, (uint8_t *)topic, strlen(topic),
                            sockid, 1);
        zlist_remove(sn->topics, t);
        free(t);
      }
      topic = zlist_next(sn->topics);
    }

    zlist_destroy(&sn->topics);
    zframe_destroy(&sn->sockid);
  }
  rcu_read_lock();
  cds_lfht_del(subscribe_ht, ht_node);
  rcu_read_unlock();
  free(sn);
  return ntop;
}

// add subscription for "topic" to "sockid"
// return 0 topic already existed
// return 1 if it was appended
// return 2 if new entry was created
int insert_subscription(zframe_t *sockid, char *topic) {
  struct cds_lfht_iter iter;
  int hash = XXH32(zframe_data(sockid), zframe_size(sockid), XXHSEED);
  subscribe_node *sn;

  rcu_read_lock();
  cds_lfht_lookup(subscribe_ht, hash, match_subscribe_node, sockid, &iter);
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
  sn = malloc(sizeof(subscribe_node));
  cds_lfht_node_init(&sn->node);
  sn->sockid = zframe_dup(sockid);
  sn->topics = zlist_new();
  zlist_append(sn->topics, strdup(topic));
  rcu_read_lock();
  cds_lfht_add(subscribe_ht, hash, &sn->node);
  rcu_read_unlock();
  return 2;
}

char *zframe_tostr(zframe_t *self, char *buffer) {
  assert(self);
  assert(zframe_is(self));

  byte *data = zframe_data(self);
  size_t size = zframe_size(self);
  buffer[0] = '\0';
  //  Probe data to check if it looks like unprintable binary
  int is_bin = 0;
  uint char_nbr;
  for (char_nbr = 0; char_nbr < size; char_nbr++)
    if (data[char_nbr] < 9 || data[char_nbr] > 127)
      is_bin = 1;

  snprintf(buffer, 30, "[%03d] ", (int)size);
  size_t max_size = is_bin ? 35 : 70;
  const char *ellipsis = "";
  if (size > max_size) {
    size = max_size;
    ellipsis = "...";
  }
  for (char_nbr = 0; char_nbr < size; char_nbr++) {
    if (is_bin)
      sprintf(buffer + strlen(buffer), "%02X",
              (unsigned char)data[char_nbr]);
    else
      sprintf(buffer + strlen(buffer), "%c", data[char_nbr]);
  }
  strcat(buffer, ellipsis);
  return buffer;
}
