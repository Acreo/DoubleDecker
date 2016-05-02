/* Local Variables:  */
/* flycheck-gcc-include-path:
 * "/home/eponsko/double-decker/c-version/include/" */
/* End:              */
/*
   Copyright (c) 2015 Pontus Sköldström, Bertrand Pechenot

   This file is part of libdd, the DoubleDecker hierarchical
   messaging system DoubleDecker is free software; you can
   redistribute it and/or modify it under the terms of the GNU Lesser
   General Public License (LGPL) version 2.1 as published by the Free
   Software Foundation.

   As a special exception, the Authors give you permission to link this
   library with independent modules to produce an executable,
   regardless of the license terms of these independent modules, and to
   copy and distribute the resulting executable under terms of your
   choice, provided that you also meet, for each linked independent
   module, the terms and conditions of the license of that module. An
   independent module is a module which is not derived from or based on
   this library.  If you modify this library, you must extend this
   exception to your version of the library.  DoubleDecker is
   distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
   License for more details.  You should have received a copy of the
   GNU Lesser General Public License along with this program.  If not,
   see <http://www.gnu.org/licenses/>.

 * broker.c --- Filename: broker.c Description: Initial idea for a C
 * implementation of double-decker based around czmq and cds_lfht cds_lfht
 * is a high-performance multi-thread supporting hashtable Idea is to
 * have one thread (main) recieving all messages which are pushed using
 * inproc threads to processing threads.  Processing threads then perform
 * lookups in the shared hashtables and forward to the zmqsockets (they
 * are threadsafe I hope..?) Hashtable and usersparce RCU library
 * implementation at: git://git.lttng.org/userspace-rcu.git
 * http://lwn.net/Articles/573431/ Author: Pontus Sköldström
 * <ponsko@acreo.se> Created: tis mar 10 22:31:03 2015 (+0100)
 * Last-Updated: By:
 */
#define _GNU_SOURCE
#include "../include/dd.h"
#include "../include/dd_classes.h"
#define IPC_REGEX "(ipc://)(.+)"
#define TCP_REGEX "(tcp://[^:]+:)(\\d+)"

static int s_on_subN_msg(zloop_t *loop, zsock_t *handle, void *arg);
static int s_on_subS_msg(zloop_t *loop, zsock_t *handle, void *arg);
static int s_on_pubN_msg(zloop_t *loop, zsock_t *handle, void *arg);
static int s_on_pubS_msg(zloop_t *loop, zsock_t *handle, void *arg);
static int s_on_router_msg(zloop_t *loop, zsock_t *handle, void *arg);
static int s_on_dealer_msg(zloop_t *loop, zsock_t *handle, void *arg);
static int s_register(zloop_t *loop, int timer_id, void *arg);
static int s_heartbeat(zloop_t *loop, int timer_id, void *arg);
static int s_check_cli_timeout(zloop_t *loop, int timer_fd, void *arg);
static int s_check_br_timeout(zloop_t *loop, int timer_fd, void *arg);

static void s_cb_high_error(dd_broker_t *self, zmsg_t *msg);
static void s_cb_addbr(dd_broker_t *self, zframe_t *sockid, zmsg_t *msg);
static void s_cb_addlcl(dd_broker_t *self, zframe_t *sockid, zmsg_t *msg);
static void s_cb_adddcl(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie_frame,
                 zmsg_t *msg);
static void s_cb_chall(dd_broker_t *self, zmsg_t *msg);
static void s_cb_challok(dd_broker_t *self, zframe_t *sockid, zmsg_t *msg);
static void s_cb_forward_dsock(dd_broker_t *self, zmsg_t *msg);
static void s_cb_forward_rsock(dd_broker_t *self, zframe_t *sockid,
                        zframe_t *cookie_frame, zmsg_t *msg);
static void s_cb_nodst_dsock(dd_broker_t *self, zmsg_t *msg);
static void s_cb_nodst_rsock(dd_broker_t *self, zmsg_t *msg);
static void s_cb_pub(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie,
              zmsg_t *msg);
static void s_cb_ping(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie);
static void s_cb_regok(dd_broker_t *self, zmsg_t *msg);
static void s_cb_send(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie,
               zmsg_t *msg);
static void s_cb_sub(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie,
              zmsg_t *msg);
static void s_cb_unreg_br(dd_broker_t *self, char *name, zmsg_t *msg);
static void s_cb_unreg_cli(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie,
                    zmsg_t *msg);
static void s_cb_unreg_dist_cli(dd_broker_t *self, zframe_t *sockid,
                         zframe_t *cookie_frame, zmsg_t *msg);
static void s_cb_unsub(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie,
                zmsg_t *msg);


void print_ddbrokerkeys(ddbrokerkeys_t *keys);
void dest_invalid_rsock(dd_broker_t *self, zframe_t *sockid, char *src_string,
                        char *dst_string);
void dest_invalid_dsock(dd_broker_t *self, char *src_string, char *dst_string);

int loglevel = DD_LOG_INFO;

int is_int(char *s) {
  while (*s) {
    if (isdigit(*s++) == 0)
      return 0;
  }

  return 1;
}
void remote_reg_failed(dd_broker_t *self, zframe_t *sockid, char *cli_name) {
  zsock_send(self->rsock, "fbbbss", sockid, &dd_version, 4, &dd_cmd_error, 4,
             &dd_error_regfail, 4, cli_name);
}


/** Functions for handling incoming messages */

void s_cb_high_error(dd_broker_t *self, zmsg_t *msg) {
  zframe_t *code_frame = zmsg_pop(msg);
  if (code_frame == NULL) {
    dd_error("DD: Misformed ERROR message, missing ERROR_CODE!\n");
    return;
  }
  local_client *ln;
  dist_client *dn;

  int32_t *error_code = (int32_t *)zframe_data(code_frame);
  switch (*error_code) {
  case DD_ERROR_NODST:
    dd_debug("Recieved ERROR_NODST from higher broker!");
    // original destination
    char *dst_string = zmsg_popstr(msg);
    // source of failing command
    char *src_string = zmsg_popstr(msg);

    // Check if src_string is a local client
    if ((ln = hashtable_has_rev_local_node(self, src_string, 0))) {
      dd_debug("Source of NODST is local!");
      char *dot = strchr(dst_string, '.');
      dest_invalid_rsock(self, ln->sockid, src_string, dot + 1);

    } else if ((dn = hashtable_has_dist_node(self, src_string))) {
      dd_debug("Source of NODST is distant!");
      dest_invalid_rsock(self, dn->broker, src_string, dst_string);
    } else {
      dd_warning("Could not find NODST source, cannot 'raise' error");
    }

    free(dst_string);
    free(src_string);
    break;
  case DD_ERROR_REGFAIL:
    dd_debug("Recived ERROR_REGFAIL from higher broker!");
    char *cli_name = zmsg_popstr(msg);
    dn = hashtable_has_dist_node(self, cli_name);
    ln = hashtable_has_rev_local_node(self, cli_name, 0);
    // is cli_name a local client?
    if ((ln = hashtable_has_rev_local_node(self, cli_name, 0))) {
      dd_info(" - Removed local client: %s", ln->prefix_name);
      int a = remove_subscriptions(self, ln->sockid);
      dd_info("   - Removed %d subscriptions", a);
      hashtable_unlink_local_node(self, ln->sockid, ln->cookie);
      hashtable_unlink_rev_local_node(self, ln->prefix_name);
      remote_reg_failed(self, ln->sockid, "remote");
      zframe_destroy(&ln->sockid);
      free(ln->prefix_name);
      free(ln->name);
      free(ln);
    } else if ((dn = hashtable_has_dist_node(self, cli_name))) {
      dd_info(" - Removed distant client: %s", cli_name);
      remote_reg_failed(self, dn->broker, cli_name);
      hashtable_remove_dist_node(self, cli_name);
    } else {
      dd_warning("Could not locate offending client!");
    }
    free(cli_name);
    break;
  case DD_ERROR_VERSION:
    dd_error("ERROR_VERSION from higher broker!");
    break;
  default:
    dd_error("Unknown error code from higher broker!");
    break;
  }
  zframe_destroy(&code_frame);
}

void s_cb_addbr(dd_broker_t *self, zframe_t *sockid, zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_addbr called");
  zframe_print(sockid, "sockid");
  zmsg_print(msg);
#endif
  char *hash = zmsg_popstr(msg);
  if (hash == NULL) {
    dd_error("Error, got ADDBR without hash!");
    return;
  }
  //  printf("comparing hash %s with keys->hash %s\n", hash, keys->hash);
  if (strcmp(hash, self->keys->hash) != 0) {
    // TODO send error
    dd_error("Error, got ADDBR with wrong hash!");
    return;
  }

  int enclen = sizeof(uint64_t) + crypto_box_NONCEBYTES + crypto_box_MACBYTES;
  unsigned char *dest = calloc(1, enclen);
  unsigned char *ciphertext = dest; // dest+crypto_box_NONCEBYTES;

  // increment nonce
  sodium_increment((unsigned char *)self->nonce, crypto_box_NONCEBYTES);
  memcpy(dest, self->nonce, crypto_box_NONCEBYTES);

  dest += crypto_box_NONCEBYTES;
  int retval = crypto_box_easy_afternm(
      dest, (unsigned char *)&self->keys->cookie, sizeof(self->keys->cookie),
      (unsigned char *)self->nonce, self->keys->ddboxk);

  retval = zsock_send(self->rsock, "fbbbf", sockid, &dd_version, 4,
                      &dd_cmd_chall, 4, ciphertext, enclen, sockid);
  if (retval != 0) {
    dd_error("Error sending challenge!");
  }
}

void s_cb_addlcl(dd_broker_t *self, zframe_t *sockid, zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_addlcl called");
  zframe_print(sockid, "sockid");
  zmsg_print(msg);
#endif

  char *hash = zmsg_popstr(msg);
  if (hash == NULL) {
    dd_error("Error, got ADDLCL without hash!");
    return;
  }
  ddtenant_t *ten;

  ten = zhash_lookup(self->keys->tenantkeys, hash);
  free(hash);
  if (ten == NULL) {
    dd_error("Could not find key for client");
    zsock_send(self->rsock, "fbbbs", sockid, &dd_version, 4, &dd_cmd_error, 4,
               &dd_error_regfail, 4, "Authentication failed!");
    return;
  }

  int enclen = sizeof(uint64_t) + crypto_box_NONCEBYTES + crypto_box_MACBYTES;

  unsigned char *dest = calloc(1, enclen);
  unsigned char *ciphertext = dest; // dest+crypto_box_NONCEBYTES;

  // increment nonce
  sodium_increment((unsigned char *)self->nonce, crypto_box_NONCEBYTES);
  memcpy(dest, self->nonce, crypto_box_NONCEBYTES);

  dest += crypto_box_NONCEBYTES;
  int retval = crypto_box_easy_afternm(
      dest, (unsigned char *)&ten->cookie, sizeof(ten->cookie),
      (unsigned char *)self->nonce, (const unsigned char *)ten->boxk);

  retval = zsock_send(self->rsock, "fbbb", sockid, &dd_version, 4,
                      &dd_cmd_chall, 4, ciphertext, enclen);
  free(ciphertext);
  if (retval != 0) {
    dd_error("Error sending challenge!");
  }
}

void s_cb_adddcl(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie_frame,
                 zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_adddcl called");
  zframe_print(sockid, "sockid");
  zframe_print(cookie_frame, "cookie");
  zmsg_print(msg);
#endif
  uint64_t *cookie = (uint64_t *)zframe_data(cookie_frame);
  if (hashtable_has_local_broker(self, sockid, *cookie, 0) == NULL) {
    dd_warning("Got ADDDCL from unregistered broker...");
    return;
  }

  dist_client *dn;
  char *name = zmsg_popstr(msg);
  zframe_t *dist_frame = zmsg_pop(msg);
  int *dist = (int *)zframe_data(dist_frame);
  // does name exist in local hashtable?
  local_client *ln;

  if ((ln = hashtable_has_rev_local_node(self, name, 0))) {
    dd_info(" - Local client '%s' already exists!", name);
    remote_reg_failed(self, sockid, name);
    free(name);

  } else if ((dn = hashtable_has_dist_node(self, name))) {
    dd_info(" - Remote client '%s' already exists!", name);
    remote_reg_failed(self, sockid, name);
    free(name);

  } else {
    hashtable_insert_dist_node(self, name, sockid, *dist);
    dd_info(" + Added remote client: %s (%d)", name, *dist);
    add_cli_up(self, name, *dist);
  }
  zframe_destroy(&dist_frame);
}

void s_cb_chall(dd_broker_t *self, zmsg_t *msg) {
  int retval = 0;
#ifdef DEBUG
  dd_debug("s_cb_chall called");
  zmsg_print(msg);
#endif
  zframe_t *encrypted = zmsg_pop(msg);
  if (self->broker_id)
    zframe_destroy(&self->broker_id);
  self->broker_id = zmsg_pop(msg);
  unsigned char *data = zframe_data(encrypted);

  int enclen = zframe_size(encrypted);
  unsigned char *decrypted = calloc(1, enclen);

  retval = crypto_box_open_easy_afternm(decrypted, data + crypto_box_NONCEBYTES,
                                        enclen - crypto_box_NONCEBYTES, data,
                                        self->keys->ddboxk);
  if (retval != 0) {
    dd_error("Unable to decrypt CHALLENGE from broker");
    goto cleanup;
  }

  zsock_send(self->dsock, "bbfss", &dd_version, 4, &dd_cmd_challok, 4,
             zframe_new(decrypted,
                        enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES),
             self->keys->hash, "broker");
cleanup:

  zframe_destroy(&encrypted);
  free(decrypted);
}

void s_cb_challok(dd_broker_t *self, zframe_t *sockid, zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_challok called");
  zframe_print(sockid, "sockid");
  zmsg_print(msg);
#endif

  int retval;
  zframe_t *cook = zmsg_pop(msg);
  uint64_t *cookie = (uint64_t *)zframe_data(cook);
  char *hash = zmsg_popstr(msg);
  char *client_name = zmsg_popstr(msg);

  if (cook == NULL || cookie == NULL || hash == NULL || client_name == NULL) {

    dd_error("DD_CMD_CHALLOK: misformed message!");
    goto cleanup;
  }
  // broker <-> broker authentication
  if (strcmp(hash, self->keys->hash) == 0) {
    if (self->keys->cookie != *cookie) {
      dd_warning("DD_CHALL_OK: authentication error!");
      // TODO: send error message
      goto cleanup;
    }
    dd_debug("Authentication of broker %s successful!", client_name);
    if (NULL == hashtable_has_local_broker(self, sockid, *cookie, 0)) {
      hashtable_insert_local_broker(self, sockid, *cookie);

      const char *pubs_endpoint = zsock_endpoint(self->pubS);
      const char *subs_endpoint = zsock_endpoint(self->subS);

      zsock_send(self->rsock, "fbbbss", sockid, &dd_version, 4, &dd_cmd_regok,
                 4, &self->keys->cookie, sizeof(self->keys->cookie),
                 pubs_endpoint, subs_endpoint);
      char buf[256];
      dd_info(" + Added broker: %s", zframe_tostr(sockid, buf));
      goto cleanup;
    }
    goto cleanup;
  }
  // tenant <-> broker authentication
  ddtenant_t *ten;
  ten = zhash_lookup(self->keys->tenantkeys, hash);
  if (ten == NULL) {
    dd_warning("DD_CHALL_OK: could not find tenant for %s", hash);
    goto cleanup;
  }

  if (ten->cookie != *cookie) {
    dd_warning("DD_CHALL_OK: authentication error!");
    // TODO: send error message
    goto cleanup;
  }

  dd_info("Authentication of %s.%s successful!", ten->name, client_name);
  if (strcmp(client_name, "public") == 0) {
    // TODO: send error message
    dd_error("Client trying to use reserved name 'public'!");
    goto cleanup;
    return;
  }

  retval = insert_local_client(self, sockid, ten, client_name);
  if (retval == -1) {
    // TODO: send error message
    remote_reg_failed(self, sockid, "local");
    dd_error("DD_CMD_CHALLOK: Couldn't insert local client!");
    goto cleanup;
    return;
  }
  zsock_send(self->rsock, "fbbb", sockid, &dd_version, 4, &dd_cmd_regok, 4,
             &ten->cookie, sizeof(ten->cookie));
  dd_info(" + Added local client: %s.%s", ten->name, client_name);
  char prefix_name[MAXTENANTNAME];
  int prelen = snprintf(prefix_name, 200, "%s.%s", ten->name, client_name);
  if (self->state != DD_STATE_ROOT)
    add_cli_up(self, prefix_name, 0);

cleanup:
  if (hash)
    free(hash);
  if (client_name)
    free(client_name);
  if (cook)
    zframe_destroy(&cook);
}

void s_cb_forward_dsock(dd_broker_t *self, zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_forward_dsock called");
  zmsg_print(msg);
#endif

  if (zmsg_size(msg) < 2)
    return;
  char *src = zmsg_popstr(msg);
  char *dst = zmsg_popstr(msg);

  int srcpublic = 0, dstpublic = 0;

  if (strncmp(src, "public.", 7) == 0)
    srcpublic = 1;
  if (strncmp(dst, "public.", 7) == 0)
    dstpublic = 1;

  dd_debug("Forward_dsock: srcpublic = %d, dstpublic = %d\n", srcpublic,
           dstpublic);

  dist_client *dn;
  local_client *ln;

  if ((ln = hashtable_has_rev_local_node(self, dst, 0))) {
    if ((srcpublic && !dstpublic) || (!srcpublic && dstpublic)) {
      dd_debug("Forward_dsock, not stripping tenant %s", src);
      forward_locally(self, ln->sockid, src, msg);
    } else {
      dd_debug("Forward_dsock, stripping tenant %s", src);
      char *dot = strchr(src, '.');
      forward_locally(self, ln->sockid, dot + 1, msg);
    }
  } else if ((dn = hashtable_has_dist_node(self, dst))) {
    forward_down(self, src, dst, dn->broker, msg);
  } else if (self->state == DD_STATE_ROOT) {
    dest_invalid_dsock(self, src, dst);
  } else {
    forward_up(self, src, dst, msg);
  }
  free(src);
  free(dst);
}

void s_cb_forward_rsock(dd_broker_t *self, zframe_t *sockid,
                        zframe_t *cookie_frame, zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_forward_rsock called");
  zframe_print(sockid, "sockid");
  zframe_print(cookie_frame, "cookie");
  zmsg_print(msg);
#endif

  uint64_t *cookie;
  cookie = (uint64_t *)zframe_data(cookie_frame);
  if (!hashtable_has_local_broker(self, sockid, *cookie, 1)) {
    dd_warning("Unregistered broker trying to forward!");
    return;
  }

  if (zmsg_size(msg) < 2)
    return;

  char *src_string = zmsg_popstr(msg);
  char *dst_string = zmsg_popstr(msg);

  int srcpublic = 0, dstpublic = 0;

  if (strncmp(src_string, "public.", 7) == 0)
    srcpublic = 1;
  if (strncmp(dst_string, "public.", 7) == 0)
    dstpublic = 1;

  dd_debug("Forward_rsock: srcpublic = %d, dstpublic = %d\n", srcpublic,
           dstpublic);
  dist_client *dn;
  local_client *ln;
  if ((ln = hashtable_has_rev_local_node(self, dst_string, 0))) {
    if ((srcpublic && !dstpublic) || (!srcpublic && dstpublic)) {
      dd_debug("Forward_rsock, not stripping tenant %s", src_string);
      forward_locally(self, ln->sockid, src_string, msg);
    } else {
      dd_debug("Forward_dsock, stripping tenant %s", src_string);
      char *dot = strchr(src_string, '.');
      forward_locally(self, ln->sockid, dot + 1, msg);
    }
  } else if ((dn = hashtable_has_dist_node(self, dst_string))) {
    forward_down(self, src_string, dst_string, dn->broker, msg);
  } else if (self->state == DD_STATE_ROOT) {
    dest_invalid_rsock(self, sockid, src_string, dst_string);
  } else {
    forward_up(self, src_string, dst_string, msg);
  }
  free(src_string);
  free(dst_string);
}

/*
 * TODO: Add a lookup for dist_cli here as well!
 */
void s_cb_nodst_dsock(dd_broker_t *self, zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_nodst_dsock called");
  zmsg_print(msg);
#endif

  local_client *ln;
  char *dst_string = zmsg_popstr(msg);
  char *src_string = zmsg_popstr(msg);
  dd_debug("s_cb_nodst_dsock called!)");

  if ((ln = hashtable_has_rev_local_node(self, src_string, 0))) {
    zsock_send(self->rsock, "fbbbss", ln->sockid, &dd_version, 4, &dd_cmd_error,
               4, &dd_error_nodst, 4, dst_string, src_string);
  } else {
    dd_error("Could not forward NODST message downwards");
  }
}

void s_cb_nodst_rsock(dd_broker_t *self, zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_nodst_rsock called");
  zmsg_print(msg);
#endif
  dd_error("s_cb_nodst_rsock called, not implemented!");
}

void s_cb_pub(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie,
              zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_pub called");
  zframe_print(sockid, "sockid");
  zframe_print(cookie, "cookie");
  zmsg_print(msg);
#endif

  char *topic = zmsg_popstr(msg);
  zframe_t *pathv = zmsg_pop(msg);

  local_client *ln;
  ln = hashtable_has_local_node(self, sockid, cookie, 1);
  if (!ln) {
    dd_warning("Unregistered client trying to send!");
    zframe_destroy(&pathv);
    free(topic);
    return;
  }
  int srcpublic = 0;
  int dstpublic = 0;
  if (strcmp("public", ln->tenant) == 0)
    srcpublic = 1;
  if (strncmp(topic, "public.", 7) == 0)
    dstpublic = 1;

  char newtopic[256];
  char *prefix_topic = NULL;

  if (topic[strlen(topic) - 1] == '$') {
    topic[strlen(topic) - 1] = '\0';
    prefix_topic = topic;
  } else {
    snprintf(&newtopic[0], 256, "%s%s", topic, self->broker_scope);
    prefix_topic = &newtopic[0];
  }
  char *name = NULL;
  char *pubtopic = NULL;
  char tentopic[256];
  if (dstpublic) {
    pubtopic = prefix_topic;
    name = ln->prefix_name;
  } else {
    snprintf(&tentopic[0], 256, "%s.%s", ln->tenant, prefix_topic);
    pubtopic = &tentopic[0];
    name = ln->name;
  }

  if (self->pubN) {
    dd_debug("publishing north %s %s ", pubtopic, name);
    zsock_send(self->pubN, "ssfm", pubtopic, name, self->broker_id, msg);
  }

  if (self->pubS) {
    dd_debug("publishing south %s %s", pubtopic, name);

    zsock_send(self->pubS, "ssfm", pubtopic, name, self->broker_id_null, msg);
  }

  zlist_t *socks =
      nn_trie_tree(&self->topics_trie, (const uint8_t *)pubtopic, strlen(pubtopic));

  if (socks != NULL) {
    zframe_t *s = zlist_first(socks);
    dd_debug("Local sockids to send to: ");
    while (s) {
      print_zframe(s);
      zsock_send(self->rsock, "fbbssm", s, &dd_version, 4, &dd_cmd_pub, 4, name,
                 topic, msg);
      s = zlist_next(socks);
    }
    zlist_destroy(&socks);
  } else {
    dd_debug("No matching nodes found by nn_trie_tree");
  }
  free(topic);
  zframe_destroy(&pathv);
}

void s_cb_ping(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie) {
#ifdef DEBUG
  dd_debug("s_cb_ping called");
  zframe_print(sockid, "sockid");
  zframe_print(cookie, "cookie");
#endif

  if (hashtable_has_local_node(self, sockid, cookie, 1)) {
    zsock_send(self->rsock, "fbb", sockid, &dd_version, 4, &dd_cmd_pong, 4);
    return;
  }

  uint64_t *cook;
  cook = (uint64_t *)zframe_data(cookie);
  if (hashtable_has_local_broker(self, sockid, *cook, 1)) {
    zsock_send(self->rsock, "fbb", sockid, &dd_version, 4, &dd_cmd_pong, 4);
    return;
  }
  dd_warning("Ping from unregistered client/broker: ");
  zframe_print(sockid, NULL);
}

void s_cb_regok(dd_broker_t *self, zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_regok called");
  zmsg_print(msg);
#endif

  self->state = DD_STATE_REGISTERED;

  // stop trying to register
  zloop_timer_end(self->loop, self->reg_loop);
  self->heartbeat_loop =
      zloop_timer(self->loop, 1000, 0, s_heartbeat, self->dsock);

  // iterate through local clients and add_cli_up to transmit to next
  // broker
  struct cds_lfht_iter iter;
  local_client *np;
  rcu_read_lock();
  cds_lfht_first(self->lcl_cli_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  while (ht_node != NULL) {
    rcu_read_unlock();
    np = caa_container_of(ht_node, local_client, lcl_node);
    dd_debug("Registering, found local client: %s", np->name);
    add_cli_up(self, np->prefix_name, 0);
    rcu_read_lock();
    cds_lfht_next(self->lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  };
  rcu_read_unlock();
  // iterate through dist clients and add_cli_up to transmit to next
  // broker

  dist_client *nd;
  rcu_read_lock();
  cds_lfht_first(self->dist_cli_ht, &iter);
  ht_node = cds_lfht_iter_get_node(&iter);
  while (ht_node != NULL) {
    rcu_read_unlock();
    nd = caa_container_of(ht_node, dist_client, node);
    dd_debug("Registering, found distant client: %s", nd->name);
    add_cli_up(self, nd->name, nd->distance);
    rcu_read_lock();
    cds_lfht_next(self->dist_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  };
  rcu_read_unlock();

  if (3 == zmsg_size(msg)) {
    zframe_t *cook = zmsg_pop(msg);
    connect_pubsubN(self);
    zframe_destroy(&cook);
  } else {
    dd_warning("No PUB/SUB interface configured");
  }
}

void s_cb_send(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie,
               zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_send called");
  zframe_print(sockid, "sockid");
  zframe_print(cookie, "cookie");
  zmsg_print(msg);
#endif
  char *dest = zmsg_popstr(msg);

  int srcpublic = 0;
  int dstpublic = 0;

  // Add DEFINE for length here
  char dest_buf[MAXTENANTNAME];

  char *dst_string;
  char *src_string;

  local_client *ln;
  ln = hashtable_has_local_node(self, sockid, cookie, 1);
  if (!ln) {
    dd_error("Unregistered client trying to send!");
    // TODO
    // free some stuff here..
    return;
  }
  if (strcmp(ln->tenant, "public") == 0)
    srcpublic = 1;
  if (strncmp(dest, "public.", 7) == 0)
    dstpublic = 1;

  dd_debug("s_cb_send, srcpublic %d, dstpublic %d", srcpublic, dstpublic);

  // if destination is public, add tenant to source_name
  // but not on prefix_dst_name
  if (dstpublic == 1 && srcpublic == 0) {

    src_string = ln->prefix_name;
    dst_string = dest;
    dd_debug("s:0 d:1 , s: %s d: %s", src_string, dst_string);
    // if source is public and destination is public,
    // don't add additional 'public.' to prefix
  } else if (dstpublic == 1 && srcpublic == 1) {
    src_string = ln->prefix_name;
    dst_string = dest;
    dd_debug("s:1 d:1 , s: %s d: %s", src_string, dst_string);
    // if source is public but not destination, check if
    // 'public.' should be added.
    // if dest starts with "tenant." don't add public.
  } else if (dstpublic == 0 && srcpublic == 1) {
    dd_debug("dst not public, but src is");
    int add_prefix = 1;
    char *dot = strchr(dest, '.');
    if (dot) {
      dd_debug("destination has . in name");
      *dot = '\0';
      char *k = NULL;
      k = zlist_first(self->keys->tenants);
      while (k) {
        if (strncmp(dest, k, strlen(k)) == 0) {
          dd_debug("found matching tenant: %s, not adding prefix!", k);
          add_prefix = 0;
          break;
        }
        k = zlist_next(self->keys->tenants);
      }
      *dot = '.';
    }
    dd_debug("add_prefix: %d", add_prefix);
    if (add_prefix == 1) {
      src_string = ln->prefix_name;
      snprintf(dest_buf, MAXTENANTNAME, "%s.%s", ln->tenant, dest);
      dst_string = dest_buf;
    } else {
      dst_string = dest;
      src_string = ln->prefix_name;
    }
    dd_debug("s:1 d:0, s: %s d: %s", src_string, dst_string);
  } else {
    src_string = ln->prefix_name;
    snprintf(dest_buf, MAXTENANTNAME, "%s.%s", ln->tenant, dest);
    dst_string = dest_buf;
    dd_debug("s:0 d:0, s: %s d: %s", src_string, dst_string);
  }

  dd_debug("s_cb_send: src \"%s\", dst \"%s\"", src_string, dst_string);

  dist_client *dn;
  if ((ln = hashtable_has_rev_local_node(self, dst_string, 0))) {
    if ((!srcpublic && !dstpublic) || (srcpublic && dstpublic)) {
      char *dot = strchr(src_string, '.');
      forward_locally(self, ln->sockid, dot + 1, msg);
    } else {
      forward_locally(self, ln->sockid, src_string, msg);
    }
  } else if ((dn = hashtable_has_dist_node(self, dst_string))) {
    dd_debug("calling forward down");
    forward_down(self, src_string, dst_string, dn->broker, msg);
  } else if (self->state == DD_STATE_ROOT) {
    if ((!srcpublic && !dstpublic) || (srcpublic && dstpublic)) {
      char *src_dot = strchr(src_string, '.');
      char *dst_dot = strchr(dst_string, '.');
      dest_invalid_rsock(self, sockid, src_dot + 1, dst_dot + 1);
    } else {
      dest_invalid_rsock(self, sockid, src_string, dst_string);
    }
  } else {
    forward_up(self, src_string, dst_string, msg);
  }

  free(dest);
}

void s_cb_sub(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie,
              zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_sub called");
  zframe_print(sockid, "sockid");
  zframe_print(cookie, "cookie");
  zmsg_print(msg);
#endif

  char *topic = zmsg_popstr(msg);
  char *scopestr = zmsg_popstr(msg);

  local_client *ln;
  ln = hashtable_has_local_node(self, sockid, cookie, 1);
  if (!ln) {
    dd_warning("DD: Unregistered client trying to send!");
    free(topic);
    free(scopestr);
    return;
  }

  if (strcmp(topic, "public") == 0) {
    zsock_send(self->rsock, "fbbss", sockid, &dd_version, 4, &dd_cmd_data, 4,
               "ERROR: protected topic");
    free(topic);
    free(scopestr);
    return;
  }

  // scopestr = "/*/*/*/"
  // scopestr = "/43/*/"
  // scopestr ..
  // replace * with appropriate scope number assigned to broker
  int j;
  char *str1, *token, *saveptr1;
  char *brscope;
  char *t = zlist_first(self->scope);
  char *scopedup = strdup(scopestr);
  char newtopic[257];
  char *ntptr = &newtopic[1];
  char newscope[128];
  char *nsptr = &newscope[0];
  int len = 128;
  int retval;
  if (strcmp(scopestr, "noscope") == 0) {
    retval = snprintf(nsptr, len, "");
    len -= retval;
    nsptr += retval;
  } else {
    for (j = 1, str1 = scopestr;; j++, str1 = NULL) {
      token = strtok_r(str1, "/", &saveptr1);
      if (token == NULL)
        break;

      if (t != NULL) {
        if (strcmp(token, "*") == 0) {
          retval = snprintf(nsptr, len, "/%s", t);
          len -= retval;
          nsptr += retval;
        } else {
          if (is_int(token) == 0) {
            dd_error("%s in scope string is not an integer", token);
          }
          retval = snprintf(nsptr, len, "/%s", token);
          len -= retval;
          nsptr += retval;
        }
        t = zlist_next(self->scope);
      } else {
        dd_error("Requested scope is longer than assigned scope!");

        free(scopedup);
        free(scopestr);
        free(topic);
      }
    }
    retval = snprintf(nsptr, len, "/");
    len -= retval;
    nsptr += retval;
  }
  zsock_send(self->rsock, "fbbss", sockid, &dd_version, 4, &dd_cmd_subok, 4,
             topic, scopedup);
  free(scopedup);

  retval =
      snprintf(ntptr, 256, "%s.%s%s", ln->tenant, topic, (char *)&newscope[0]);
  //  dd_debug("newtopic = %s, len = %d\n", ntptr, retval);

  int new = 0;
  // Hashtable
  // subscriptions[sockid(5byte array)] = [topic,topic,topic]
  retval = insert_subscription(self, sockid, ntptr);

  if (retval != 0)
    new += 1;

#ifdef DEBUG
  print_sub_ht();
#endif

  // Trie
  // topics_trie[newtopic(char*)] = [sockid, sockid, sockid]
  retval = nn_trie_subscribe(&self->topics_trie, (const uint8_t *)ntptr,
                             strlen(ntptr), sockid, 1);
  // doesn't really matter
  if (retval == 0) {
    dd_info("topic %s already in trie!", ntptr);
  } else if (retval == 1) {
    dd_debug("new topic %s", ntptr);
  } else if (retval == 2) {
    dd_debug("inserted new sockid on topic %s", ntptr);
  }

  free(scopestr);
  free(topic);
#ifdef DEBUG
  nn_trie_dump(&self->topics_trie);
#endif
  // refcount -> integrate in the topic_trie as refcount_s and refcount_n
  // topic_north[newtopic(char*)] = int
  // topic_south[newtopic(char*)] = int

  if (retval != 2)
    return;

  // add subscription to the north and south sub sockets
  newtopic[0] = 1;
  ntptr = &newtopic[0];
  if (self->subN) {
    dd_debug("adding subscription for %s to north SUB", &newtopic[1]);
    retval =
        zsock_send(self->subN, "b", &newtopic[0], 1 + strlen(&newtopic[1]));
  }
  if (self->subS) {
    dd_debug("adding subscription for %s to south SUB", &newtopic[1]);
    retval =
        zsock_send(self->subS, "b", &newtopic[0], 1 + strlen(&newtopic[1]));
  }
}

/*
 * TODO fix this
 */
void s_cb_unreg_br(dd_broker_t *self, char *name, zmsg_t *msg) {
  dd_debug("s_cb_unreg_br called, not implemented");
  /*
   * tmp_to_del = [] print('unregistering', name) for cli in
   * self.dist_cli: print(cli, self.dist_cli[cli][0]) if
   * self.dist_cli[cli][0] == name: tmp_to_del.append(cli)
   *
   * for i in tmp_to_del: self.unreg_dist_cli(name, [i])
   * self.local_br.pop(name)
   */
}

void s_cb_unreg_cli(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie,
                    zmsg_t *msg) {

#ifdef DEBUG
  dd_debug("s_cb_unreg_cli called");
  zframe_print(sockid, "sockid");
  zframe_print(cookie, "cookie");
//        zmsg_print(msg);
#endif

  local_client *ln;
  if ((ln = hashtable_has_local_node(self, sockid, cookie, 0))) {
    dd_info(" - Removed local client: %s", ln->prefix_name);
    del_cli_up(self, ln->prefix_name);
    int a = remove_subscriptions(self, sockid);
    dd_info("   - Removed %d subscriptions", a);
    hashtable_unlink_local_node(self, ln->sockid, ln->cookie);
    hashtable_unlink_rev_local_node(self, ln->prefix_name);
    zframe_destroy(&ln->sockid);
    free(ln->prefix_name);
    free(ln->name);
    free(ln);
    print_local_ht(self);
  } else {
    dd_warning("Request to remove unknown client");
  }
}

void s_cb_unreg_dist_cli(dd_broker_t *self, zframe_t *sockid,
                         zframe_t *cookie_frame, zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_unreg_dist_cli called");
  zframe_print(sockid, "sockid");
  zframe_print(cookie_frame, "cookie");
  zmsg_print(msg);
#endif

  uint64_t *cook = (uint64_t *)zframe_data(cookie_frame);
  if (!hashtable_has_local_broker(self, sockid, *cook, 0)) {
    dd_error("Unregistered broker trying to remove clients!");
    return;
  }

  dist_client *dn;
  char *name = zmsg_popstr(msg);
  dd_debug("trying to remove distant client: %s", name);

  if ((dn = hashtable_has_dist_node(self, name))) {
    dd_info(" - Removed distant client: %s", name);
    hashtable_remove_dist_node(self, name);
    del_cli_up(self, name);
  }
  free(name);
}

void s_cb_unsub(dd_broker_t *self, zframe_t *sockid, zframe_t *cookie,
                zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("s_cb_unsub called");
  zframe_print(sockid, "sockid");
  zframe_print(cookie, "cookie");
  zmsg_print(msg);
#endif

  char *topic = zmsg_popstr(msg);
  char *scopestr = zmsg_popstr(msg);

  local_client *ln;
  ln = hashtable_has_local_node(self, sockid, cookie, 1);
  if (!ln) {
    dd_warning("Unregistered client trying to send!\n");
    free(topic);
    free(scopestr);
    return;
  }

  if (strcmp(topic, "public") == 0) {
    zsock_send(self->rsock, "fbbss", sockid, &dd_version, 4, &dd_cmd_data, 4,
               "ERROR: protected topic");
    free(topic);
    free(scopestr);
    return;
  }

  // scopestr = "/*/*/*/"
  // scopestr = "/43/*/"
  // scopestr ..
  // replace * with appropriate scope number assigned to broker
  int j;
  char *str1, *token, *saveptr1;
  char *brscope;
  char *t = zlist_first(self->scope);
  char *scopedup = strdup(scopestr);
  char newtopic[257];
  char *ntptr = &newtopic[1];
  char newscope[128];
  char *nsptr = &newscope[0];
  int len = 128;
  int retval;
  if (strcmp(scopestr, "noscope") == 0) {
    retval = snprintf(nsptr, len, "");
    len -= retval;
    nsptr += retval;
  } else {
    for (j = 1, str1 = scopestr;; j++, str1 = NULL) {
      token = strtok_r(str1, "/", &saveptr1);
      if (token == NULL)
        break;

      if (t != NULL) {
        if (strcmp(token, "*") == 0) {
          retval = snprintf(nsptr, len, "/%s", t);
          len -= retval;
          nsptr += retval;
        } else {
          if (is_int(token) == 0) {
            dd_error("%s in scope string is not an integer", token);
          }
          retval = snprintf(nsptr, len, "/%s", token);
          len -= retval;
          nsptr += retval;
        }
        t = zlist_next(self->scope);
      } else {
        dd_error("Requested scope is longer than assigned scope!");

        free(scopedup);
        free(scopestr);
        free(topic);
      }
    }
    retval = snprintf(nsptr, len, "/");
    len -= retval;
    nsptr += retval;
  }
  free(scopedup);

  retval =
      snprintf(ntptr, 256, "%s.%s%s", ln->tenant, topic, (char *)&newscope[0]);
  dd_info("deltopic = %s, len = %d\n", ntptr, retval);

  int new = 0;
  retval = remove_subscription(self, sockid, ntptr);

  // only delete a subscription if something was actually removed
  // from the trie and/or hashtable Otherwise multiple unsub from
  // a single client will f up for the  others

  if (retval == 0)
    return;

  newtopic[0] = 0;
  ntptr = &newtopic[0];
  if (self->subN) {
    dd_debug("deleting 1 subscription for %s to north SUB", &newtopic[1]);
    retval =
        zsock_send(self->subN, "b", &newtopic[0], 1 + strlen(&newtopic[1]));
  }
  if (self->subS) {
    dd_debug("deleting 1 subscription for %s to south SUB", &newtopic[1]);
    retval =
        zsock_send(self->subS, "b", &newtopic[0], 1 + strlen(&newtopic[1]));
  }
}

/* Functions called from zloop on timers or when message recieved */

int s_on_subN_msg(zloop_t *loop, zsock_t *handle, void *arg) {
  dd_broker_t *self = arg;
  zmsg_t *msg = zmsg_recv(handle);

#ifdef DEBUG
  dd_debug("s_on_subN_msg called");
  zmsg_print(msg);
#endif

  char *pubtopic = zmsg_popstr(msg);
  char *name = zmsg_popstr(msg);
  zframe_t *pathv = zmsg_pop(msg);

  if (zframe_eq(pathv, self->broker_id)) {
    goto cleanup;
  }

  dd_debug("pubtopic: %s source: %s", pubtopic, name);
  // zframe_print(pathv, "pathv: ");
  zlist_t *socks =
      nn_trie_tree(&self->topics_trie, (const uint8_t *)pubtopic, strlen(pubtopic));

  if (socks != NULL) {
    zframe_t *s = zlist_first(socks);
    dd_debug("Local sockids to send to: ");
    char *dot = strchr(pubtopic, '.');
    dot++;
    char *slash = strchr(dot, '/');
    if (slash)
      *slash = '\0';

    while (s) {
      print_zframe(s);
      zsock_send(self->rsock, "fbbssm", s, &dd_version, 4, &dd_cmd_pub, 4, name,
                 dot, msg);
      s = zlist_next(socks);
    }
    *slash = '/';
    zlist_destroy(&socks);
  } else {
    dd_debug("No matching nodes found by nn_trie_tree");
  }

  // If from north, only send south (only one reciever in the north)
  if (self->pubS)
    zsock_send(self->pubS, "ssfm", pubtopic, name, self->broker_id_null, msg);

cleanup:
  free(pubtopic);
  free(name);
  zframe_destroy(&pathv);
  zmsg_destroy(&msg);
  return 0;
}

int s_on_subS_msg(zloop_t *loop, zsock_t *handle, void *arg) {
  dd_broker_t *self = arg;
  zmsg_t *msg = zmsg_recv(handle);
#ifdef DEBUG
  dd_debug("s_on_subS_msg called");
  zmsg_print(msg);
#endif

  char *pubtopic = zmsg_popstr(msg);
  char *name = zmsg_popstr(msg);
  zframe_t *pathv = zmsg_pop(msg);

  dd_debug("pubtopic: %s source: %s", pubtopic, name);
  // zframe_print(pathv, "pathv: ");
  zlist_t *socks =
      nn_trie_tree(&self->topics_trie, (const uint8_t *)pubtopic, strlen(pubtopic));

  if (socks != NULL) {
    zframe_t *s = zlist_first(socks);
    dd_debug("Local sockids to send to: ");

    // TODO, this is a simplification, should take into account
    // srcpublic/dstpublic
    char *dot = strchr(pubtopic, '.');
    dot++;
    char *slash = strchr(dot, '/');
    if (slash)
      *slash = '\0';

    while (s) {
      print_zframe(s);
      zsock_send(self->rsock, "fbbssm", s, &dd_version, 4, &dd_cmd_pub, 4, name,
                 dot, msg);
      s = zlist_next(socks);
    }
    *slash = '/';
    zlist_destroy(&socks);
  } else {
    dd_debug("No matching nodes found by nn_trie_tree");
  }

  // if from the south, send north & south, multiple recievers south
  if (self->pubN)
    zsock_send(self->pubN, "ssfm", pubtopic, name, self->broker_id, msg);
  if (self->pubS)
    zsock_send(self->pubS, "ssfm", pubtopic, name, pathv, msg);

cleanup:
  free(pubtopic);
  free(name);
  zframe_destroy(&pathv);
  zmsg_destroy(&msg);
  return 0;
}
int s_on_pubN_msg(zloop_t *loop, zsock_t *handle, void *arg) {
  dd_broker_t *self = arg;
  zmsg_t *msg = zmsg_recv(handle);
#ifdef DEBUG
  dd_debug("s_on_pubN_msg called");
  zmsg_print(msg);
#endif

  zframe_t *topic_frame = zmsg_pop(msg);
  char *topic = (char *)zframe_data(topic_frame);

  if (topic[0] == 1) {
    dd_info(" + Got subscription for: %s", &topic[1]);
    nn_trie_add_sub_north(&self->topics_trie, (const uint8_t *)&topic[1],
                          zframe_size(topic_frame) - 1);
  }
  if (topic[0] == 0) {
    dd_info(" - Got unsubscription for: %s", &topic[1]);
    nn_trie_del_sub_north(&self->topics_trie, (const uint8_t *)&topic[1],
                          zframe_size(topic_frame) - 1);
  }

  // subs from north should continue down
  if (self->subS)
    zsock_send(self->subS, "f", topic_frame);

  zframe_destroy(&topic_frame);
  zmsg_destroy(&msg);
  return 0;
}

int s_on_pubS_msg(zloop_t *loop, zsock_t *handle, void *arg) {
  dd_broker_t *self = arg;
  zmsg_t *msg = zmsg_recv(handle);
#ifdef DEBUG
  dd_debug("s_on_pubS_msg called");
  zmsg_print(msg);
#endif

  zframe_t *topic_frame = zmsg_pop(msg);
  char *topic = (char *)zframe_data(topic_frame);

  if (topic[0] == 1) {
    dd_info(" + Got subscription for: %s", &topic[1]);
    nn_trie_add_sub_south(&self->topics_trie, (const uint8_t *)&topic[1],
                          zframe_size(topic_frame) - 1);
  }
  if (topic[0] == 0) {
    dd_info(" - Got unsubscription for: %s", &topic[1]);
    nn_trie_del_sub_south(&self->topics_trie, (const uint8_t *)&topic[1],
                          zframe_size(topic_frame) - 1);
  }

  // subs from north should continue down
  if (self->subS)
    zsock_send(self->subS, "f", topic_frame);
  if (self->subN)
    zsock_send(self->subN, "f", topic_frame);

  zframe_destroy(&topic_frame);
  zmsg_destroy(&msg);
  return 0;
}

int s_on_router_msg(zloop_t *loop, zsock_t *handle, void *arg) {
  dd_broker_t *self = arg;
  zmsg_t *msg = zmsg_recv(handle);

#ifdef DEBUG
  dd_debug("s_on_router_msg called");
  zmsg_print(msg);
#endif

  if (msg == NULL) {
    dd_error("zmsg_recv returned NULL");
    return 0;
  }
  if (zmsg_size(msg) < 3) {
    dd_error("message less than 3, error!");
    zmsg_destroy(&msg);
    return 0;
  }
  zframe_t *source_frame = NULL;
  zframe_t *proto_frame = NULL;
  zframe_t *cmd_frame = NULL;
  zframe_t *cookie_frame = NULL;

  source_frame = zmsg_pop(msg);
  if (source_frame == NULL) {
    dd_error("Malformed message, missing SOURCE");
    goto cleanup;
  }
  proto_frame = zmsg_pop(msg);
  uint32_t *pver;
  pver = (uint32_t *)zframe_data(proto_frame);
  if (*pver != DD_VERSION) {
    dd_error("Wrong version, expected 0x%x, got 0x%x", DD_VERSION, *pver);
    zsock_send(self->rsock, "fbbbs", source_frame, pver, 4, &dd_cmd_error, 4,
               &dd_error_version, 4, "Different versions in use");
    goto cleanup;
  }
  cmd_frame = zmsg_pop(msg);
  if (cmd_frame == NULL) {
    dd_error("Malformed message, missing CMD");
    goto cleanup;
  }
  uint32_t cmd = *((uint32_t *)zframe_data(cmd_frame));

  switch (cmd) {
  case DD_CMD_SEND:
    cookie_frame = zmsg_pop(msg);
    if (cookie_frame == NULL) {
      dd_error("Malformed SEND, missing COOKIE");
      goto cleanup;
    }
    s_cb_send(self, source_frame, cookie_frame, msg);
    break;

  case DD_CMD_FORWARD:
    cookie_frame = zmsg_pop(msg);
    if (cookie_frame == NULL) {
      dd_error("Malformed FORWARD, missing COOKIE");
      goto cleanup;
    }
    s_cb_forward_rsock(self, source_frame, cookie_frame, msg);
    break;

  case DD_CMD_PING:
    cookie_frame = zmsg_pop(msg);
    if (cookie_frame == NULL) {
      dd_error("Malformed PING, missing COOKIE");
      goto cleanup;
    }
    s_cb_ping(self, source_frame, cookie_frame);
    break;

  case DD_CMD_SUB:
    cookie_frame = zmsg_pop(msg);
    if (cookie_frame == NULL) {
      dd_error("Malformed SUB, missing COOKIE");
      goto cleanup;
    }
    s_cb_sub(self, source_frame, cookie_frame, msg);
    break;

  case DD_CMD_UNSUB:
    cookie_frame = zmsg_pop(msg);
    if (cookie_frame == NULL) {
      dd_error("Malformed UNSUB, missing COOKIE");
      goto cleanup;
    }
    s_cb_unsub(self, source_frame, cookie_frame, msg);
    break;

  case DD_CMD_PUB:
    cookie_frame = zmsg_pop(msg);
    if (cookie_frame == NULL) {
      dd_error("Malformed PUB, missing COOKIE");
      goto cleanup;
    }
    s_cb_pub(self, source_frame, cookie_frame, msg);
    break;

  case DD_CMD_ADDLCL:
    s_cb_addlcl(self, source_frame, msg);
    break;

  case DD_CMD_ADDDCL:
    cookie_frame = zmsg_pop(msg);
    if (cookie_frame == NULL) {
      dd_error("Malformed ADDDCL, missing COOKIE");
      goto cleanup;
    }
    s_cb_adddcl(self, source_frame, cookie_frame, msg);
    break;

  case DD_CMD_ADDBR:
    s_cb_addbr(self, source_frame, msg);
    break;

  case DD_CMD_UNREG:
    cookie_frame = zmsg_pop(msg);
    if (cookie_frame == NULL) {
      dd_error("Malformed ADDBR, missing COOKIE");
      goto cleanup;
    }
    s_cb_unreg_cli(self, source_frame, cookie_frame, msg);
    break;

  case DD_CMD_UNREGDCLI:
    cookie_frame = zmsg_pop(msg);
    if (cookie_frame == NULL) {
      dd_error("Malformed UNREGDCLI, missing COOKIE");
      goto cleanup;
    }
    s_cb_unreg_dist_cli(self, source_frame, cookie_frame, msg);
    break;

  case DD_CMD_UNREGBR:
    s_cb_unreg_br(self, NULL, msg);
    break;

  case DD_CMD_CHALLOK:
    s_cb_challok(self, source_frame, msg);
    break;

  case DD_CMD_ERROR:
    // TODO implment
    dd_error("Recived CMD_ERROR from a client!");
    break;

  default:
    dd_error("Unknown command, value: 0x%x", cmd);
    break;
  }

cleanup:
  if (source_frame)
    zframe_destroy(&source_frame);
  if (proto_frame)
    zframe_destroy(&proto_frame);
  if (cmd_frame)
    zframe_destroy(&cmd_frame);
  if (cookie_frame)
    zframe_destroy(&cookie_frame);
  if (msg)
    zmsg_destroy(&msg);

  return 0;
}

int s_on_dealer_msg(zloop_t *loop, zsock_t *handle, void *arg) {
  dd_broker_t *self = arg;
  self->timeout = 0;
  zmsg_t *msg = zmsg_recv(handle);
#ifdef DEBUG
  dd_debug("s_on_dealer_msg called");
  zmsg_print(msg);
#endif

  if (msg == NULL) {
    dd_error("zmsg_recv returned NULL");
    return 0;
  }
  if (zmsg_size(msg) < 2) {
    dd_error("message less than 2, error!");
    zmsg_destroy(&msg);
    return 0;
  }

  zframe_t *proto_frame = zmsg_pop(msg);

  if (*((uint32_t *)zframe_data(proto_frame)) != DD_VERSION) {
    dd_error("Wrong version, expected 0x%x, got 0x%x", DD_VERSION,
             *zframe_data(proto_frame));
    zframe_destroy(&proto_frame);
    return 0;
  }
  zframe_t *cmd_frame = zmsg_pop(msg);
  uint32_t cmd = *((uint32_t *)zframe_data(cmd_frame));
  zframe_destroy(&cmd_frame);
  switch (cmd) {
  case DD_CMD_REGOK:
    s_cb_regok(self, msg);
    break;
  case DD_CMD_FORWARD:
    s_cb_forward_dsock(self, msg);
    break;
  case DD_CMD_CHALL:
    s_cb_chall(self, msg);
    break;
  case DD_CMD_PONG:
    break;
  case DD_CMD_ERROR:
    s_cb_high_error(self, msg);
    break;
  default:
    dd_error("Unknown command, value: 0x%x", cmd);
    break;
  }
  zmsg_destroy(&msg);
  return 0;
}

int s_register(zloop_t *loop, int timer_id, void *arg) {
  dd_broker_t *self = arg;
  dd_debug("trying to register..");

  if (self->state == DD_STATE_UNREG || self->state == DD_STATE_ROOT) {
    zsock_set_linger(self->dsock, 0);
    zloop_reader_end(self->loop, self->dsock);
    zsock_destroy((zsock_t **)&self->dsock);
    self->dsock = zsock_new_dealer(NULL);
    if (!self->dsock) {
      dd_error("Error in zsock_new_dealer: %s", zmq_strerror(errno));
      return -1;
    }

    int rc = zsock_connect(self->dsock, self->dealer_connect);
    if (rc != 0) {
      dd_error("Error in zmq_connect: %s", zmq_strerror(errno));
      return -1;
    }
    zloop_reader(self->loop, self->dsock, s_on_dealer_msg, NULL);

    zsock_send(self->dsock, "bbs", &dd_version, 4, &dd_cmd_addbr, 4,
               self->keys->hash);
  }
  return 0;
}

int s_heartbeat(zloop_t *loop, int timer_id, void *arg) {
  dd_broker_t *self = arg;
  // how to pass self? socket = dsock
  self->timeout += 1;
  if (self->timeout > 3) {
    self->state = DD_STATE_ROOT;
    zloop_timer_end(self->loop, self->heartbeat_loop);
    self->reg_loop = zloop_timer(self->loop, 1000, 0, s_register, self->dsock);
  }
  zsock_send(self->dsock, "bbb", &dd_version, sizeof(dd_version), &dd_cmd_ping,
             sizeof(dd_cmd_ping), &self->keys->cookie,
             sizeof(self->keys->cookie));
  return 0;
}

int s_check_cli_timeout(zloop_t *loop, int timer_fd, void *arg) {
  dd_broker_t *self = arg;
  // iterate through local clients and check if they should time out
  struct cds_lfht_iter iter;
  local_client *np;
  rcu_read_lock();
  cds_lfht_first(self->lcl_cli_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  while (ht_node != NULL) {
    rcu_read_unlock();
    np = caa_container_of(ht_node, local_client, lcl_node);
    if (np->timeout < 3) {
      np->timeout += 1;
    } else {
      dd_debug("deleting local client %s", np->prefix_name);
      unreg_cli(self, np->sockid, np->cookie);
    }
    rcu_read_lock();
    cds_lfht_next(self->lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  };
  rcu_read_unlock();
  return 0;
}

int s_check_br_timeout(zloop_t *loop, int timer_fd, void *arg) {
  dd_broker_t *self = arg;
  // iterate through local brokers and check if they should time out
  struct cds_lfht_iter iter;
  local_broker *np;
  rcu_read_lock();
  cds_lfht_first(self->lcl_br_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  rcu_read_unlock();
  while (ht_node != NULL) {
    np = caa_container_of(ht_node, local_broker, node);
    if (np->timeout < 3) {
      np->timeout += 1;
    } else {
      char buf[256];
      dd_debug("Deleting local broker %s", zframe_tostr(np->sockid, buf));

      delete_dist_clients(self, np);

      rcu_read_lock();
      int ret = cds_lfht_del(self->lcl_br_ht, ht_node);
      rcu_read_unlock();
      if (ret) {
        dd_info(" - Local broker %s removed (concurrently)",
                zframe_tostr(np->sockid, buf));
        free(np);
      } else {
        synchronize_rcu();
        dd_info(" - Local broker %s removed", zframe_tostr(np->sockid, buf));
        free(np);
      }
    }
    rcu_read_lock();
    cds_lfht_next(self->lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
    rcu_read_unlock();
  }
  return 0;
}

/* helper functions */

void add_cli_up(dd_broker_t *self, char *prefix_name, int distance) {
  if (self->state == DD_STATE_ROOT)
    return;

  dd_debug("add_cli_up(%s,%d), state = %d", prefix_name, distance, self->state);
  zsock_send(self->dsock, "bbbsb", &dd_version, 4, &dd_cmd_adddcl, 4,
             &self->keys->cookie, sizeof(self->keys->cookie), prefix_name,
             &distance, sizeof(distance));
}

void del_cli_up(dd_broker_t *self, char *prefix_name) {
  if (self->state != DD_STATE_ROOT) {
    dd_debug("del_cli_up %s", prefix_name);
    zsock_send(self->dsock, "bbbs", &dd_version, 4, &dd_cmd_unregdcli, 4,
               &self->keys->cookie, sizeof(self->keys->cookie), prefix_name);
  }
}

void forward_locally(dd_broker_t *self, zframe_t *dest_sockid, char *src_string,
                     zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("forward_locally: src: %s", src_string);
  zframe_print(dest_sockid, "dest_sockid");
  zmsg_print(msg);
#endif

  zsock_send(self->rsock, "fbbsm", dest_sockid, &dd_version, 4, &dd_cmd_data, 4,
             src_string, msg);
}

void forward_down(dd_broker_t *self, char *src_string, char *dst_string,
                  zframe_t *br_sockid, zmsg_t *msg) {
#ifdef DEBUG
  dd_info("Sending CMD_FORWARD to broker with sockid");
  print_zframe(br_sockid);
#endif
  zsock_send(self->rsock, "fbbssm", br_sockid, &dd_version, 4, &dd_cmd_forward,
             4, src_string, dst_string, msg);
}
void forward_up(dd_broker_t *self, char *src_string, char *dst_string,
                zmsg_t *msg) {
#ifdef DEBUG
  dd_debug("forward_up called s: %s d: %s", src_string, dst_string);
  zmsg_print(msg);
#endif
  if (self->state == DD_STATE_REGISTERED)
    zsock_send(self->dsock, "bbbssm", &dd_version, 4, &dd_cmd_forward, 4,
               &self->keys->cookie, sizeof(self->keys->cookie), src_string,
               dst_string, msg);
}

void dest_invalid_rsock(dd_broker_t *self, zframe_t *sockid, char *src_string,
                        char *dst_string) {
  zsock_send(self->rsock, "fbbbss", sockid, &dd_version, 4, &dd_cmd_error, 4,
             &dd_error_nodst, 4, dst_string, src_string);
}

void dest_invalid_dsock(dd_broker_t *self, char *src_string, char *dst_string) {
  zsock_send(self->dsock, "bbss", &dd_version, 4, &dd_cmd_error, 4,
             &dd_error_nodst, 4, dst_string, src_string);
}

void unreg_cli(dd_broker_t *self, zframe_t *sockid, uint64_t cookie) {
  zframe_t *cookie_frame = zframe_new(&cookie, sizeof cookie);
  s_cb_unreg_cli(self, sockid, cookie_frame, NULL);
  zframe_destroy(&cookie_frame);
}

void unreg_broker(dd_broker_t *self, local_broker *np) {
  dd_warning("unreg_broker called, unimplemented!\n");
}

void connect_pubsubN(dd_broker_t *self) {
  dd_debug("Connect pubsubN");

  zrex_t *rexipc = zrex_new(IPC_REGEX);
  assert(zrex_valid(rexipc));
  zrex_t *rextcp = zrex_new(TCP_REGEX);
  assert(zrex_valid(rextcp));
  self->sub_connect = malloc(strlen(self->dealer_connect) + 5);
  self->pub_connect = malloc(strlen(self->dealer_connect) + 5);

  char tmpfile[1024];
  if (zrex_matches(rexipc, self->dealer_connect)) {
    sprintf(self->sub_connect, "%s.pub", self->dealer_connect);
    sprintf(self->pub_connect, "%s.sub", self->dealer_connect);
  } else if (zrex_matches(rextcp, self->dealer_connect)) {
    int port = atoi(zrex_hit(rextcp, 2));
    sprintf(self->pub_connect, "%s%d", zrex_hit(rextcp, 1), port + 2);
    sprintf(self->sub_connect, "%s%d", zrex_hit(rextcp, 1), port + 1);
  } else {
    dd_error("%s doesnt match anything!");
    exit(EXIT_FAILURE);
  }

  zrex_destroy(&rexipc);
  zrex_destroy(&rextcp);

  dd_info("pub_connect: %s sub_connect: %s", self->pub_connect,
          self->sub_connect);
  self->pubN = zsock_new(ZMQ_XPUB);
  self->subN = zsock_new(ZMQ_XSUB);
  int rc = zsock_connect(self->pubN, self->pub_connect);
  if (rc < 0) {
    dd_error("Unable to connect pubN to %s", self->pub_connect);
    perror("Error: ");
    exit(EXIT_FAILURE);
  }

  rc = zsock_connect(self->subN, self->sub_connect);
  if (rc < 0) {
    dd_error("Unable to connect subN to %s", self->sub_connect);
    perror("Error: ");
    exit(EXIT_FAILURE);
  }
  rc = zloop_reader(self->loop, self->pubN, s_on_pubN_msg, NULL);
  assert(rc == 0);
  zloop_reader_set_tolerant(self->loop, self->pubN);

  rc = zloop_reader(self->loop, self->subN, s_on_subN_msg, NULL);
  assert(rc == 0);
  zloop_reader_set_tolerant(self->loop, self->subN);
}

char *str_replace(const char *string, const char *substr,
                  const char *replacement) {
  char *tok = NULL;
  char *newstr = NULL;
  char *oldstr = NULL;
  char *head = NULL;
  /*
   * if either substr or replacement is NULL, duplicate string a let
   * caller handle it
   */
  if (substr == NULL || replacement == NULL)
    return strdup(string);

  newstr = strdup(string);
  head = newstr;
  while ((tok = strstr(head, substr))) {
    oldstr = newstr;
    newstr = malloc(strlen(oldstr) - strlen(substr) + strlen(replacement) + 1);
    /*
     * failed to alloc mem, free old string and return NULL
     */
    if (newstr == NULL) {
      free(oldstr);
      return NULL;
    }
    memcpy(newstr, oldstr, tok - oldstr);
    memcpy(newstr + (tok - oldstr), replacement, strlen(replacement));
    memcpy(newstr + (tok - oldstr) + strlen(replacement), tok + strlen(substr),
           strlen(oldstr) - strlen(substr) - (tok - oldstr));
    memset(newstr + strlen(oldstr) - strlen(substr) + strlen(replacement), 0,
           1);
    /*
     * move back head right after the last replacement
     */
    head = newstr + (tok - oldstr) + strlen(replacement);
    free(oldstr);
  }
  return newstr;
}

void print_ddbrokerkeys(ddbrokerkeys_t *keys) {
  int siz = zlist_size(keys->tenants);
  dd_debug("Loaded %d tenant keys: ", siz);

  char *k = NULL;
  k = zlist_first(keys->tenants);

  dd_debug("Tenant keys: ");
  zlist_t *precalc = zhash_keys(keys->tenantkeys);
  ddtenant_t *ten;
  k = zlist_first(precalc);
  while (k) {
    ten = zhash_lookup(keys->tenantkeys, k);
    dd_debug("\t name: %s \tcookie: %llu", ten->name, ten->cookie);
    k = zlist_next(precalc);
  }
  //  free(hex);
}


void change_permission(char *t) {
  dd_debug("Setting permission on \"%s\" to rw-rw-rw-", t);
  mode_t rw_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  int rc = chmod(t, rw_mode);
  if (rc == -1) {
    perror("Error: ");
    dd_error("Couldn't set permissions on IPC socket\n");
    exit(EXIT_FAILURE);
  }
}

void start_pubsub(dd_broker_t *self) {
  zrex_t *rexipc = zrex_new(IPC_REGEX);
  assert(zrex_valid(rexipc));
  zrex_t *rextcp = zrex_new(TCP_REGEX);
  assert(zrex_valid(rextcp));

  self->pub_strings = zlist_new();
  self->sub_strings = zlist_new();
  char *t = zlist_first(self->rstrings);
  char tmpfile[1024];
  while (t != NULL) {
    if (zrex_matches(rexipc, t)) {
      sprintf(tmpfile, "%s.pub", zrex_hit(rexipc, 2));
      if (zfile_exists(tmpfile)) {
        dd_error("File %s already exists, aborting.", tmpfile);
        exit(EXIT_FAILURE);
      }
      sprintf(tmpfile, "%s.sub", zrex_hit(rexipc, 2));
      if (zfile_exists(tmpfile)) {
        dd_error("File %s already exists, aborting.", tmpfile);
        exit(EXIT_FAILURE);
      }
      char *sub_ipc = malloc(strlen(t) + 5);
      char *pub_ipc = malloc(strlen(t) + 5);
      sprintf(sub_ipc, "%s.sub", t);
      sprintf(pub_ipc, "%s.pub", t);
      zlist_append(self->sub_strings, sub_ipc);
      zlist_append(self->pub_strings, pub_ipc);
      // Should not be necessary, but weird results otherwise..
      zrex_destroy(&rexipc);
      rexipc = zrex_new(IPC_REGEX);
    } else if (zrex_matches(rextcp, t)) {
      int port = atoi(zrex_hit(rextcp, 2));
      char *sub_tcp = malloc(strlen(t) + 1);
      char *pub_tcp = malloc(strlen(t) + 1);
      sprintf(pub_tcp, "%s%d", zrex_hit(rextcp, 1), port + 1);
      sprintf(sub_tcp, "%s%d", zrex_hit(rextcp, 1), port + 2);
      zlist_append(self->sub_strings, sub_tcp);
      zlist_append(self->pub_strings, pub_tcp);
      // Should not be necessary, but weird results otherwise..
      zrex_destroy(&rextcp);
      rextcp = zrex_new(TCP_REGEX);
    } else {
      dd_error("%s doesnt match anything!");
      exit(EXIT_FAILURE);
    }
    t = zlist_next(self->rstrings);
  }

  zrex_destroy(&rextcp);
  zrex_destroy(&rexipc);

  t = zlist_first(self->pub_strings);
  int pub_strings_len = 0;
  while (t != NULL) {
    pub_strings_len += strlen(t) + 1;
    t = zlist_next(self->pub_strings);
  }

  int sub_strings_len = 0;
  t = zlist_first(self->sub_strings);
  while (t != NULL) {
    sub_strings_len += strlen(t) + 1;
    t = zlist_next(self->sub_strings);
  }

  if (zlist_size(self->pub_strings) < 1) {
    dd_error("pub_strings zlist empty!");
    exit(EXIT_FAILURE);
  }
  if (zlist_size(self->sub_strings) < 1) {
    dd_error("sub_strings zlist empty!");
    exit(EXIT_FAILURE);
  }

  self->pub_bind = malloc(pub_strings_len);
  self->sub_bind = malloc(sub_strings_len);

  int i, written = 0;
  int num_len = zlist_size(self->pub_strings);

  for (i = 0; i < num_len; i++) {
    if (i == 0) {
      t = zlist_first(self->pub_strings);
    } else {
      t = zlist_next(self->pub_strings);
    }
    written += snprintf(self->pub_bind + written, pub_strings_len - written,
                        (i != 0 ? ",%s" : "%s"), t);
    if (written == pub_strings_len)
      break;
  }

  written = 0;
  num_len = zlist_size(self->sub_strings);

  for (i = 0; i < num_len; i++) {
    if (i == 0) {
      t = zlist_first(self->sub_strings);
    } else {
      t = zlist_next(self->sub_strings);
    }
    written += snprintf(self->sub_bind + written, sub_strings_len - written,
                        (i != 0 ? ",%s" : "%s"), t);
    if (written == sub_strings_len)
      break;
  }

  self->pubS = zsock_new(ZMQ_XPUB);
  self->subS = zsock_new(ZMQ_XSUB);
  int rc = zsock_attach(self->pubS, self->pub_bind, true);
  if (rc < 0) {
    dd_error("Unable to attach pubS to %s", self->pub_bind);
    perror("Error: ");
    exit(EXIT_FAILURE);
  }
  rc = zsock_attach(self->subS, self->sub_bind, true);
  if (rc < 0) {
    dd_error("Unable to attach subS to %s", self->sub_bind);
    perror("Error: ");
    exit(EXIT_FAILURE);
  }

  t = zlist_first(self->pub_strings);
  while (t != NULL) {
    if (strcasestr(t, "ipc://")) {
      change_permission(t + 6);
    }
    t = zlist_next(self->pub_strings);
  }

  t = zlist_first(self->sub_strings);
  while (t != NULL) {
    if (strcasestr(t, "ipc://")) {
      change_permission(t + 6);
    }
    t = zlist_next(self->sub_strings);
  }

  rc = zloop_reader(self->loop, self->pubS, s_on_pubS_msg, NULL);
  assert(rc == 0);
  zloop_reader_set_tolerant(self->loop, self->pubS);

  rc = zloop_reader(self->loop, self->subS, s_on_subS_msg, NULL);
  assert(rc == 0);
  zloop_reader_set_tolerant(self->loop, self->subS);
}

char *zframe_tojson(zframe_t *self, char *buffer);
json_object *json_stats(dd_broker_t *self, int flags) {
  json_object *jobj = json_object_new_object();
  json_object *jdist_array = json_object_new_array();

  // iterate through distant clients
  struct cds_lfht_iter iter;
  dist_client *mp;
  cds_lfht_first(self->dist_cli_ht, &iter);
  struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
  while (ht_node != NULL) {
    mp = caa_container_of(ht_node, dist_client, node);
    json_object_array_add(jdist_array, json_object_new_string(mp->name));
    cds_lfht_next(self->dist_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }

  // Iterate through local clients
  json_object *jlocal_obj = json_object_new_object();
  local_client *lp;
  cds_lfht_first(self->rev_lcl_cli_ht, &iter);
  ht_node = cds_lfht_iter_get_node(&iter);
  while (ht_node != NULL) {
    local_client *lp = caa_container_of(ht_node, local_client, rev_node);
    char buf[256];
    json_object *strval = json_object_new_string(lp->prefix_name);
    json_object_object_add(jlocal_obj, zframe_tojson(lp->sockid, buf), strval);
    cds_lfht_next(self->rev_lcl_cli_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }
  // iterate through brokers
  json_object *jbr_array = json_object_new_array();
  local_broker *br;
  cds_lfht_first(self->lcl_br_ht, &iter);
  ht_node = cds_lfht_iter_get_node(&iter);
  while (ht_node != NULL) {
    br = caa_container_of(ht_node, local_broker, node);
    char buf[256];
    json_object_array_add(
        jbr_array, json_object_new_string(zframe_tojson(br->sockid, buf)));
    cds_lfht_next(self->lcl_br_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }

  // iterate through subscriptions
  subscribe_node *sn;
  json_object *jsub_dict = json_object_new_object();
  cds_lfht_first(self->subscribe_ht, &iter);
  ht_node = cds_lfht_iter_get_node(&iter);
  while (ht_node != NULL) {
    sn = caa_container_of(ht_node, subscribe_node, node);
    json_object *jsub_array = json_object_new_array();
    if (sn->topics) {
      char *str = zlist_first(sn->topics);
      while (str) {
        json_object_array_add(jsub_array, json_object_new_string(str));
        str = zlist_next(sn->topics);
      }
    } else {
      json_object_array_add(jsub_array, json_object_new_string("empty!"));
    }
    char buf[256];
    json_object_object_add(jsub_dict, zframe_tojson(sn->sockid, buf),
                           jsub_array);
    cds_lfht_next(self->subscribe_ht, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
  }

  json_object_object_add(jobj, "brokers", jbr_array);
  json_object_object_add(jobj, "local", jlocal_obj);
  json_object_object_add(jobj, "distant", jdist_array);
  json_object_object_add(jobj, "subs", jsub_dict);
  json_object_object_add(jobj, "version",
                         json_object_new_string(PACKAGE_VERSION));
  return jobj;
}

// seperate to a different thread?
int s_on_http(zloop_t *loop, zsock_t *handle, void *arg) {
  dd_broker_t *self = arg;
  zmsg_t *msg = zmsg_recv(handle);
  zframe_t *id = zmsg_pop(msg);
  zframe_t *data = zmsg_pop(msg);
  char *http_request = (char *)zframe_data(data);
  char *http_all = "GET / HTTP/1.1\r\n";
  char *http_dist = "GET /distant HTTP/1.1\r\n";
  char *http_local = "GET /local HTTP/1.1\r\n";
  char *http_sub = "GET /subscriptions HTTP/1.1\r\n";

  int flags = 0;
#define HTTP_ALL 0b111
#define HTTP_SUBS 0b001
#define HTTP_LOCAL 0b010
#define HTTP_DIST 0b100
  if (strncmp(http_all, http_request, strlen(http_all)) == 0) {
    flags |= HTTP_ALL;
  } else if (strncmp(http_dist, http_request, strlen(http_dist)) == 0) {
    flags |= HTTP_DIST;
  } else if (strncmp(http_local, http_request, strlen(http_local)) == 0) {
    flags |= HTTP_LOCAL;
  } else if (strncmp(http_sub, http_request, strlen(http_sub)) == 0) {
    flags |= HTTP_SUBS;
  }

  if (flags == 0) {
    dd_error("Got unknown http request %s", strchr(http_request, '\r'));
    char http_response[] = "HTTP/1.1 404 Not Found\r\n"
                           "Date: Fri, 22 Apr 2016 19:04:59 GMT\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Access-Control-Allow-Methods: GET\r\n"
                           "Access-Control-Allow-Headers: Content-Type\r\n"
                           "Content-Type: application/json\r\n"
                           "Server: DoubleDecker\r\n"
                           "\r\n";
    zsock_send(handle, "fs", id, http_response);
    zsock_send(handle, "fz", id);
  } else {
    char timebuf[32];
    struct tm tmstruct;
    time_t inctime = time(NULL);
    if (!gmtime_r(&inctime, &tmstruct))
      return -1;
    int tlen = strftime(timebuf, 32, "%a, %d %b %Y %T GMT", &tmstruct);
    if (tlen <= 0)
      return -1;

    char *http_res;
    char http_ok[] = "HTTP/1.1 200 OK\r\n";
    char http_stat[] = "Access-Control-Allow-Origin: *\r\n"
                       "Access-Control-Allow-Methods: GET\r\n"
                       "Access-Control-Allow-Headers: Content-Type\r\n"
                       "Content-Type: application/json\r\n"
                       "Server: DoubleDecker\r\n"
                       "Connection: close\r\n";
    json_object *jobj = json_stats(self, flags);
    const char *json = json_object_to_json_string(jobj);
    // get rid of json object
    int retval = asprintf(&http_res, "%s%s\r\n%sContent-Length: %lu\r\n\r\n%s",
                          http_ok, timebuf, http_stat, strlen(json), json);
    zsock_send(handle, "fs", id, http_res);
    zsock_send(handle, "fz", id);
    free(http_res);
    json_object_put(jobj);
  }
  zframe_destroy(&id);
  zframe_destroy(&data);
  zmsg_destroy(&msg);
  return 0;
}

void start_httpd(dd_broker_t *self) {
  self->http = zsock_new(ZMQ_STREAM);
  int rc = zsock_bind(self->http, self->reststr);
  if (rc == -1) {
    dd_error("Could not initilize HTTP port 9080!");
    zsock_destroy(&self->http);
    return;
  }

  rc = zloop_reader(self->loop, self->http, s_on_http, NULL);
  assert(rc == 0);
  zloop_reader_set_tolerant(self->loop, self->http);
}


int dd_broker_start(dd_broker_t *self) {
  dd_info("%s - <%s> - %s", PACKAGE_STRING, PACKAGE_BUGREPORT, PACKAGE_URL);
  dd_info("Starting broker, router at %s, dealer at %s", self->router_bind,
          self->dealer_connect);

  randombytes_buf(self->nonce, crypto_box_NONCEBYTES);
  // needs to be called for each thread using RCU lib
  rcu_register_thread();
  self->loop = zloop_new();
  assert(self->loop);

  bind_router(self);
  assert(self->rsock);
  int rc = zloop_reader(self->loop, self->rsock, s_on_router_msg, self);
  assert(rc == 0);
  zloop_reader_set_tolerant(self->loop, self->rsock);

  if (self->dealer_connect) {    
    rc = zloop_reader(self->loop, self->dsock, s_on_dealer_msg, self);
    assert(rc == 0);
    zloop_reader_set_tolerant(self->loop, self->dsock);
    self->reg_loop = zloop_timer(self->loop, 1000, 0, s_register, self);
  } else {
    dd_info("No dealer defined, the broker will act as the root");
    self->state = DD_STATE_ROOT;
  }

  self->cli_timeout_loop =
      zloop_timer(self->loop, 3000, 0, s_check_cli_timeout, self);
  self->br_timeout_loop =
      zloop_timer(self->loop, 1000, 0, s_check_br_timeout, self);

  // create and attach the pubsub southbound sockets
  start_pubsub(self);

  if (self->reststr)
    start_httpd(self);

  zloop_start(self->loop);

  zloop_destroy(&self->loop);
  if (self->http)
    zsock_set_linger(self->http, 0);
  if (self->pubS)
    zsock_set_linger(self->pubS, 0);
  if (self->pubN)
    zsock_set_linger(self->pubN, 0);
  if (self->subS)
    zsock_set_linger(self->subS, 0);
  if (self->subN)
    zsock_set_linger(self->subN, 0);
  if (self->dsock)
    zsock_set_linger(self->dsock, 0);
  if (self->rsock)
    zsock_set_linger(self->rsock, 0);

  zsock_destroy(&self->http);
  zsock_destroy(&self->pubS);
  zsock_destroy(&self->pubN);
  zsock_destroy(&self->subS);
  zsock_destroy(&self->subN);
  zsock_destroy(&self->dsock);
  zsock_destroy(&self->rsock);
  dd_info("Destroyed all open sockets, waiting a second..");
  // TODO:
  // Weird bug here, if run in interactive mode and killed with ctrl-c
  // All IPC unix domain socket files seems to be removed just fine
  // However, running in daemonized mode and killed with killall (sigterm)
  // unix socket files are sometimes left. sleeping a second here seems
  // to fix it.. some background threads that dont have time to finish properly?
  sleep(1);
  zsys_shutdown();
  return 1;
}


int dd_broker_set_config(dd_broker_t *self, char *conffile) {
  zconfig_t *root = zconfig_load(conffile);
  dd_info("Reading configuration file: %s", conffile);
  if (root == NULL) {
    dd_error("Could not read configuration file \"%s\"\n", conffile);
    exit(EXIT_FAILURE);
  }
  zconfig_t *child = zconfig_child(root);
  while (child != NULL) {
    if (streq(zconfig_name(child), "dealer")) {
      dd_broker_set_dealer(self, zconfig_value(child));
    } else if (streq(zconfig_name(child), "scope")) {
      dd_broker_set_scope(self, zconfig_value(child));
    } else if (streq(zconfig_name(child), "router")) {
      dd_broker_set_router(self, zconfig_value(child));
    } else if (streq(zconfig_name(child), "rest")) {
      dd_broker_set_rest(self, zconfig_value(child));
    } else if (streq(zconfig_name(child), "loglevel")) {
      dd_broker_set_loglevel(self, zconfig_value(child));
    } else if (streq(zconfig_name(child), "keyfile")) {
      dd_broker_set_keyfile(self, zconfig_value(child));
    } else if (streq(zconfig_name(child), "logfile")) {
      dd_broker_set_logfile(self, zconfig_value(child));
    } else if (streq(zconfig_name(child), "syslog")) {
      zsys_set_logsystem(true);
    } else {
      dd_error("Unknown key in configuration file, \"%s\"",
               zconfig_name(child));
    }
    child = zconfig_next(child);
  }
  zconfig_destroy(&root);
  return 0;
}
int dd_broker_set_dealer(dd_broker_t *self, char *dealerstr) {
  dd_info("Setting dealer: %s", dealerstr);
  if(self->dealer_connect)
    free(self->dealer_connect);
  if(self->dsock)
    zsock_destroy(&self->dsock);
  self->dealer_connect = strdup(dealerstr);
  self->dsock = zsock_new(ZMQ_DEALER);
  zsock_connect(self->dsock, self->dealer_connect);
  if (self->dsock == NULL) {
    dd_error("Couldn't connect dealer socket to %s", self->dealer_connect);
    perror("Error: ");
    exit(EXIT_FAILURE);
  }
  return 0;
}
int dd_broker_set_keyfile(dd_broker_t *self, char *keyfile) {
  dd_info("Setting keys from %s", keyfile);

  if(self->keys){
    dd_error("Keys already read!");
    return -1;
  }
  self->keys = read_ddbrokerkeys(keyfile);
  assert(self->keys);
  print_ddbrokerkeys(self->keys);
  return 0;
}
int dd_broker_set_router(dd_broker_t *self, char *routerstr) {
  dd_info("Adding router %s", routerstr);

  if (self->router_bind == NULL) {
    self->router_bind = strdup(routerstr);
  } else {
    char *new_router_bind;
    asprintf(&new_router_bind, "%s,%s", self->router_bind, routerstr);
    self->router_bind = new_router_bind;
  }
  return 0;
}

void bind_router(dd_broker_t *self){
  char *str1, *token;
  char *saveptr1;
  int j;
  char *rbind_cpy = strdup(self->router_bind);
  dd_info("binding router %s",self->router_bind);
  token = strtok(rbind_cpy, ",");
  while (token) {
    zlist_append(self->rstrings, token);
    token = strtok(NULL, ",");
  }

  char *t = zlist_first(self->rstrings);
  while (t != NULL) {
    dd_debug("Found router string %s", t);
    t = zlist_next(self->rstrings);
  }
  self->rsock = zsock_new(ZMQ_ROUTER);
  // Look for IPC strings in the rstrings list, check if the files already exist
  t = (char *)zlist_first(self->rstrings);
  char *needle;
  while (t != NULL) {
    needle = strcasestr(t, "ipc://");
    if (needle) {
      if (zfile_exists(t + 6)) {
        dd_error("File %s already exists, aborting.", t + 6);
        exit(EXIT_FAILURE);
      }
    }
    t = zlist_next(self->rstrings);
  }

  int rc;
  dd_info("Attaching ROUTER socket to: %s", self->router_bind);
  rc = zsock_attach(self->rsock, self->router_bind, true);
  if (rc == 0) {
    dd_info("Successfully bound router to %s", self->router_bind);
  } else {
    dd_info("Failed to bind router to %s", self->router_bind);
    exit(EXIT_FAILURE);
  }

  if (self->rsock == NULL) {
    dd_error("Couldn't bind router socket to %s", self->router_bind);
    perror("Error: ");
    exit(EXIT_FAILURE);
  }

  // change the permission on the IPC sockets to allow anyone to connect
  t = zlist_first(self->rstrings);
  while (t != NULL) {
    needle = strcasestr(t, "ipc://");
    if (needle) {
      change_permission(t + 6);
    }
    t = zlist_next(self->rstrings);
  }
}

int dd_broker_set_scope(dd_broker_t *self, char *scopestr) {
  char *str1, *token;
  char *saveptr1;
  int j;

  for (j = 1, str1 = scopestr;; j++, str1 = NULL) {
    token = strtok_r(str1, "/", &saveptr1);
    if (token == NULL)
      break;
    if (!is_int(token)) {
      dd_error("Only '/' and digits in scope, %s is not!", token);
      exit(EXIT_FAILURE);
    }
    zlist_append(self->scope, token);
  }

  char brokerscope[256];
  self->broker_scope = &brokerscope[0];
  int len = 256;
  int retval = snprintf(self->broker_scope, len, "/");
  self->broker_scope += retval;
  len -= retval;

  char *t = zlist_first(self->scope);
  while (t != NULL) {
    retval = snprintf(self->broker_scope, len, "%s/", t);
    self->broker_scope += retval;
    len -= retval;
    t = zlist_next(self->scope);
  }
  self->broker_scope = &brokerscope[0];
  dd_debug("broker scope set to: %s", self->broker_scope);
  return 0;
}
int dd_broker_set_loglevel(dd_broker_t *self, char *logstr) {
  int i;
  for (i = 0; logstr[i]; i++)
    logstr[i] = tolower(logstr[i]);

  if (strncmp(logstr, "e", 1) == 0)
    loglevel = DD_LOG_ERROR;
  else if (strncmp(logstr, "w", 1) == 0)
    loglevel = DD_LOG_WARNING;
  else if (strncmp(logstr, "n", 1) == 0)
    loglevel = DD_LOG_NOTICE;
  else if (strncmp(logstr, "i", 1) == 0)
    loglevel = DD_LOG_INFO;
  else if (strncmp(logstr, "d", 1) == 0)
    loglevel = DD_LOG_DEBUG;
  else if (strncmp(logstr, "q", 1) == 0)
    loglevel = DD_LOG_NONE;
  else
    return -1;
  return 0;
}
dd_broker_t *dd_broker_new() {
  dd_broker_t *self = calloc(1, sizeof(dd_broker_t));
  assert(self);
  self->dealer_connect = NULL;
  self->router_bind = NULL;
  self->reststr = NULL;
  self->pub_bind = NULL;
  self->pub_connect = NULL;
  self->sub_bind = NULL;
  self->sub_connect = NULL;
  self->logfile = NULL;
  self->syslog_enabled = NULL;
  self->keys = NULL;

  // timer IDs
  self->br_timeout_loop = -1;
  self->cli_timeout_loop = -1;
  self->heartbeat_loop = -1;
  self->reg_loop = -1;
  self->state = DD_STATE_UNREG;
  self->timeout = 0;

  nn_trie_init(&self->topics_trie);

  // Broker Identity, assigned by higher broker
  self->broker_id = zframe_new("root", 4);
  assert(self->broker_id);
  self->broker_id_null = zframe_new("", 0);
  assert(self->broker_id_null);
  self->scope = zlist_new();
  assert(self->scope);
  self->broker_scope = NULL;
  self->rstrings = zlist_new();
  assert(self->rstrings);

  // Broker sockets
  self->loop = NULL;
  self->pubN = NULL;
  self->subN = NULL;
  self->pubS = NULL;
  self->subS = NULL;
  self->rsock = NULL;
  self->dsock = NULL;
  self->http = NULL;

  // client and broker tables
  self->lcl_cli_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  self->rev_lcl_cli_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  self->dist_cli_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  self->lcl_br_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  // subscriptions
  self->subscribe_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  self->top_north_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);
  self->top_south_ht = cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL);

  // Logfile
  self->logfp = NULL;
  dd_info("Initialized new ddbroker_t!");
  return self;
}
int dd_broker_set_logfile(dd_broker_t *self, char *logfile) {
  // TODO check if already open and close
  self->logfp = fopen(logfile, "w+");
  if (self->logfp) {
    zsys_set_logstream(self->logfp);
  } else {
    dd_error("Couldn't open logfile %s", logfile);
    exit(EXIT_FAILURE);
  }
}
int dd_broker_set_rest(dd_broker_t *self, char *reststr){
  self->reststr = strdup(reststr);
}

