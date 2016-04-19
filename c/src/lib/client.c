#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <czmq.h>
#include "dd.h"
#include "ddkeys.h"

// ///////////////////
// ///SUBLIST stuff //
// //////////////////
typedef struct {
  char *topic;
  char *scope;
  char active;
} ddtopic_t;

// - compare two items, for sorting
// typedef int (czmq_comparator) (const void *item1, const void *item2);
static int sublist_cmp(const void *item1, const void *item2) {
  ddtopic_t *i1, *i2;
  i1 = (ddtopic_t *)item1;
  i2 = (ddtopic_t *)item2;
  return strcmp(i1->topic, i2->topic);
}

// -- destroy an item
// typedef void (czmq_destructor) (void **item);
static void sublist_free(void **item) {
  ddtopic_t *i;
  i = *item;
  free(i->topic);
  free(i->scope);
  free(i);
}

// -- duplicate an item
// typedef void *(czmq_duplicator) (const void *item);
static void *sublist_dup(const void *item) {
  ddtopic_t *new, *old;
  old = (ddtopic_t *)item;
  new = malloc(sizeof(ddtopic_t));
  new->topic = strdup(old->topic);
  new->scope = strdup(old->scope);
  new->active = old->active;
  return new;
}

// update or add topic/scope/active to list
static void sublist_add(char *topic, char *scope, char active, ddclient_t *dd) {
  ddtopic_t *item; // = zlistx_first(dd->sublist);
  int found = 0;
  // Check if already there, if so update
  // printf("after _first item = %p\n",item);

  while (item = zlistx_next(dd->sublist)) {
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
    zlistx_add_start(dd->sublist, new);
  }
}

static void sublist_delete_topic(char *topic, ddclient_t *dd) {
  ddtopic_t *item = zlistx_first(dd->sublist);
  do {
    if (strcmp(item->topic, topic) == 0) {
      zlistx_delete(dd->sublist, item);
    }
  } while (item = zlistx_next(dd->sublist));
}

static void sublist_delete(char *topic, char *scope, ddclient_t *dd) {
  ddtopic_t del;
  del.topic = topic;
  del.scope = scope;
  ddtopic_t *item = zlistx_find(dd->sublist, &del);
  if (item)
    zlistx_delete(dd->sublist, item);
}

static void sublist_activate(char *topic, char *scope, ddclient_t *dd) {
  ddtopic_t *item; // = zlistx_first(dd->sublist);
  while (item = zlistx_next(dd->sublist)) {
    if (strcmp(item->topic, topic) == 0 && strcmp(item->scope, scope) == 0) {
      item->active = 1;
    }
  }
}

static void sublist_deactivate_all(ddclient_t *dd) {
  ddtopic_t *item;
  while (item = zlistx_next(dd->sublist)) {
    item->active = 0;
  }
}

static void sublist_resubscribe(ddclient_t *dd) {
  ddtopic_t *item; // = zlistx_first(dd->sublist);
  while (item = zlistx_next(dd->sublist)) {
    zsock_send(dd->socket, "bbbss", &dd_version, 4, &dd_cmd_sub, 4, &dd->cookie,
               sizeof(dd->cookie), item->topic, item->scope);
  }
}

void sublist_print(ddclient_t *dd) {
  ddtopic_t *item;
  while (item = zlistx_next(dd->sublist)) {
    printf("Topic: %s Scope: %s Active: %d\n", item->topic, item->scope,
           item->active);
  }
}

// ////////////////////////////////////////////////////
// // Commands for subscribe / publish / sendmessage //
// ////////////////////////////////////////////////////
static int subscribe(char *topic, char *scope, ddclient_t *dd) {
  char *scopestr;
  if (strcmp(scope, "all") == 0) {
    scopestr = "/";
  } else if (strcmp(scope, "region") == 0) {
    scopestr = "/*/";
  } else if (strcmp(scope, "cluster") == 0) {
    scopestr = "/*/*/";
  } else if (strcmp(scope, "node") == 0) {
    scopestr = "/*/*/*/";
  } else if (strcmp(scope, "noscope") == 0) {
    scopestr = "noscope";
  } else {
    // TODO
    // check that scope follows re.fullmatch("/((\d)+/)+", scope):
    scopestr = scope;
  }
  sublist_add(topic, scopestr, 0, dd);
  if (dd->state == DD_STATE_REGISTERED)
    zsock_send(dd->socket, "bbbss", &dd_version, 4, &dd_cmd_sub, 4, &dd->cookie,
               sizeof(dd->cookie), topic, scopestr);
  return 0;
}

static int unsubscribe(char *topic, char *scope, struct ddclient *dd) {
  char *scopestr;
  if (strcmp(scope, "all") == 0) {
    scopestr = "/";
  } else if (strcmp(scope, "region") == 0) {
    scopestr = "/*/";
  } else if (strcmp(scope, "cluster") == 0) {
    scopestr = "/*/*/";
  } else if (strcmp(scope, "node") == 0) {
    scopestr = "/*/*/*/";
  } else if (strcmp(scope, "noscope") == 0) {
    scopestr = "noscope";
  } else {
    // TODO
    // check that scope follows re.fullmatch("/((\d)+/)+", scope):
    scopestr = scope;
  }
  sublist_delete(topic, scopestr, dd);
  if (dd->state == DD_STATE_REGISTERED)
    zsock_send(dd->socket, "bbbss", &dd_version, 4, &dd_cmd_unsub, 4,
               &dd->cookie, sizeof(dd->cookie), topic, scopestr);
  return 0;
}

static int publish(char *topic, char *message, int mlen, ddclient_t *dd) {
  unsigned char *precalck = NULL;
  int srcpublic = 0;
  int dstpublic = 0;
  int retval;

  // printf ("dd->publish called t: %s m: %s l: %d\n", topic, message,
  // mlen);

  if (strcmp(dd->customer, "public") == 0) {
    srcpublic = 1;
  }
  if (strncmp("public.", topic, strlen("public.")) == 0) {
    dstpublic = 1;
  }

  char *dot = strchr(topic, '.');
  if (dot && srcpublic) {
    *dot = '\0';
    precalck = zhash_lookup(dd->keys->clientkeys, topic);
    if (precalck) {
      //	printf("encrypting with tenant key: %s\n",topic);
      // TODO: This is not allowed by the broker
      // We should return an error if this is happening
      fprintf(stderr, "Public client cannot publish to tenants!\n");
      return -1;
    }
    *dot = '.';
  }
  if (!precalck && !dstpublic) {
    precalck = dd->keys->custboxk;
    //      printf ("encrypting with my own key\n");
  } else if (dstpublic) {
    precalck = dd->keys->pubboxk;
    //    printf("encrypting with public key\n");
  }

  int enclen = mlen + crypto_box_NONCEBYTES + crypto_box_MACBYTES;
  //  printf ("encrypted message will be %d bytes\n", enclen);
  // unsigned char ciphertext[enclen];
  unsigned char *dest = calloc(1, enclen);
  unsigned char *ciphertext = dest; // dest+crypto_box_NONCEBYTES;

  // increment nonce
  sodium_increment(dd->nonce, crypto_box_NONCEBYTES);
  memcpy(dest, dd->nonce, crypto_box_NONCEBYTES);

  dest += crypto_box_NONCEBYTES;
  retval = crypto_box_easy_afternm(dest, message, mlen, dd->nonce, precalck);
  //  char *hex = calloc (1, 1000);
  //  sodium_bin2hex (hex, 1000, ciphertext, enclen);
  //  printf ("ciphertext size %d: %s\n", enclen, hex);
  //  free (hex);

  if (retval != 0) {
    fprintf(stderr, "DD: Unable to encrypt %d bytes!\n", mlen);
    free(ciphertext);
    return -1;
  }
  if (dd->state == DD_STATE_REGISTERED) {
    zsock_send(dd->socket, "bbbszb", &dd_version, 4, &dd_cmd_pub, 4,
               &dd->cookie, sizeof(dd->cookie), topic, ciphertext, enclen);
  }
  free(ciphertext);
}

// - Publish between public and other customers not working
static int notify(char *target, char *message, int mlen, ddclient_t *dd) {
  unsigned char *precalck = NULL;
  int srcpublic = 0;
  int dstpublic = 0;

  if (strcmp(dd->customer, "public") == 0) {
    srcpublic = 1;
  }
  if (strncmp("public.", target, strlen("public.")) == 0) {
    dstpublic = 1;
  }

  /* printf ("dd->sendmsg called t: %s m: %s l: %d\n", target, message,
   * mlen);
   */
  char *dot = strchr(target, '.');

  int retval;
  if (dot && srcpublic) {
    *dot = '\0';
    precalck = zhash_lookup(dd->keys->clientkeys, target);
    if (precalck) {
      /* printf("encrypting with tenant key: %s\n",target); */
    }
    *dot = '.';
  }
  if (!precalck && !dstpublic) {
    precalck = dd->keys->custboxk;
    /* printf ("encrypting with my own key\n"); */
  } else if (dstpublic) {
    precalck = dd->keys->pubboxk;
    /* printf("encrypting with public key\n"); */
  }

  int enclen = mlen + crypto_box_NONCEBYTES + crypto_box_MACBYTES;
  /* printf ("encrypted message will be %d bytes\n", enclen); */
  // unsigned char ciphertext[enclen];
  unsigned char *dest = calloc(1, enclen);
  unsigned char *ciphertext = dest; // dest+crypto_box_NONCEBYTES;

  // increment nonce
  sodium_increment(dd->nonce, crypto_box_NONCEBYTES);
  memcpy(dest, dd->nonce, crypto_box_NONCEBYTES);

  dest += crypto_box_NONCEBYTES;
  retval = crypto_box_easy_afternm(dest, message, mlen, dd->nonce, precalck);
  /* char *hex = calloc (1, 1000); */
  /* sodium_bin2hex (hex, 1000, ciphertext, enclen); */
  /* printf ("ciphertext size %d: %s\n", enclen, hex); */
  /* free (hex); */

  if (retval == 0) {
  } else {
    fprintf(stderr, "DD: Unable to encrypt %d bytes!\n", mlen);
    free(ciphertext);
    return -1;
  }
  if (dd->state == DD_STATE_REGISTERED) {
    zsock_send(dd->socket, "bbbsb", &dd_version, 4, &dd_cmd_send, 4,
               &dd->cookie, sizeof(dd->cookie), target, ciphertext, enclen);
  }
  free(ciphertext);
}

static int ddthread_shutdown(ddclient_t *dd) {
  printf("DD: Shutting down ddthread..\n");
  if (dd->state == DD_STATE_REGISTERED) {
    zsock_send(dd->socket, "bbb", &dd_version, 4, &dd_cmd_unreg, 4, &dd->cookie,
               sizeof(dd->cookie));
  }
  if (dd->sublist)
    zlistx_destroy(&dd->sublist);
  if (dd->loop)
    zloop_destroy(&dd->loop);
  if (dd->socket)
    zsock_destroy((zsock_t **)&dd->socket);
  dd->state = DD_STATE_EXIT;
}

// ////////////////////////
// callbacks from zloop //
// ////////////////////////

static int s_on_dealer_msg(zloop_t *loop, zsock_t *handle, void *args);

static int s_ask_registration(zloop_t *loop, int timerid, void *args);

static int s_ping(zloop_t *loop, int timerid, void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  if (dd->state == DD_STATE_REGISTERED)
    zsock_send(dd->socket, "bbb", &dd_version, 4, &dd_cmd_ping, 4, &dd->cookie,
               sizeof(dd->cookie));
}

static int s_heartbeat(zloop_t *loop, int timerid, void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  dd->timeout++;
  if (dd->timeout > 3) {
    dd->state = DD_STATE_UNREG;
    dd->registration_loop = zloop_timer(loop, 1000, 0, s_ask_registration, dd);
    zloop_timer_end(loop, dd->heartbeat_loop);
    sublist_deactivate_all(dd);
    dd->on_discon(dd);
  }
}

static int s_ask_registration(zloop_t *loop, int timerid, void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  if (dd->state == DD_STATE_UNREG) {
    zsock_set_linger(dd->socket, 0);
    zloop_reader_end(loop, dd->socket);
    zsock_destroy((zsock_t **)&dd->socket);
    dd->socket = zsock_new_dealer(NULL);
    if (!dd->socket) {
      fprintf(stderr, "DD: Error in zsock_new_dealer: %s\n",
              zmq_strerror(errno));
      free(dd);
      return -1;
    }
    //      zsock_set_identity (dd->socket, dd->client_name);
    int rc = zsock_connect(dd->socket, dd->endpoint);
    if (rc != 0) {
      fprintf(stderr, "DD: Error in zmq_connect: %s\n", zmq_strerror(errno));
      free(dd);
      return -1;
    }
    zloop_reader(loop, dd->socket, s_on_dealer_msg, dd);
    struct ddkeystate *k = dd->keys;
    zsock_send(dd->socket, "bbs", &dd_version, 4, &dd_cmd_addlcl, 4, k->hash);
  }
  return 0;
}

// /////////////////////////////////////
// / callbacks for different messages //
// ////////////////////////////////////
static void cmd_cb_regok(zmsg_t *msg, ddclient_t *dd, zloop_t *loop) {
  zframe_t *cookie_frame;
  cookie_frame = zmsg_pop(msg);
  if (cookie_frame == NULL) {
    fprintf(stderr, "DD: Misformed REGOK message, missing COOKIE!\n");
    return;
  }
  uint64_t *cookie2 = (uint64_t *)zframe_data(cookie_frame);
  dd->cookie = *cookie2;
  zframe_destroy(&cookie_frame);
  dd->state = DD_STATE_REGISTERED;
  zsock_send(dd->socket, "bbb", &dd_version, 4, &dd_cmd_ping, 4, &dd->cookie,
             sizeof(dd->cookie));

  dd->heartbeat_loop = zloop_timer(loop, 1500, 0, s_heartbeat, dd);
  zloop_timer_end(loop, dd->registration_loop);
  // if this is re-registration, we should try to subscribe again
  sublist_resubscribe(dd);
  dd->on_reg(dd);
}

static void cmd_cb_pong(zmsg_t *msg, ddclient_t *dd, zloop_t *loop) {
  zloop_timer(loop, 1500, 1, s_ping, dd);
}

static void cmd_cb_chall(zmsg_t *msg, ddclient_t *dd) {
  int retval = 0;
  //  fprintf(stderr,"cmd_cb_chall called\n");

  // zmsg_print(msg);
  zframe_t *encrypted = zmsg_first(msg);
  unsigned char *data = zframe_data(encrypted);
  int enclen = zframe_size(encrypted);
  unsigned char *decrypted = calloc(1, enclen);

  retval = crypto_box_open_easy_afternm(decrypted, data + crypto_box_NONCEBYTES,
                                        enclen - crypto_box_NONCEBYTES, data,
                                        dd->keys->ddboxk);
  if (retval != 0) {
    fprintf(stderr, "Unable to decrypt CHALLENGE from broker\n");
    return;
  }

  zsock_send(dd->socket, "bbfss", &dd_version, 4, &dd_cmd_challok, 4,
             zframe_new(decrypted,
                        enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES),
             dd->keys->hash, dd->client_name);
}

static void cmd_cb_data(zmsg_t *msg, ddclient_t *dd) {
  int retval;
  char *source = zmsg_popstr(msg);
  /* printf("cmd_cb_data: S: %s\n", source); */
  zframe_t *encrypted = zmsg_first(msg);
  unsigned char *data = zframe_data(encrypted);
  int enclen = zframe_size(encrypted);
  unsigned char *decrypted =
      calloc(1, enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES);
  unsigned char *precalck = NULL;
  char *dot = strchr(source, '.');
  if (dot) {
    *dot = '\0';
    precalck = zhash_lookup(dd->keys->clientkeys, source);
    if (precalck) {
      // printf("decrypting with tenant key:%s\n", source);
    }
    *dot = '.';
  }

  if (!precalck) {
    if (strncmp("public.", source, strlen("public.")) == 0) {
      precalck = dd->keys->pubboxk;
      //	printf("decrypting with public tenant key\n");
    } else {
      precalck = dd->keys->custboxk;
      //      printf("decrypting with my own key\n");
    }
  }
  retval = crypto_box_open_easy_afternm(decrypted, data + crypto_box_NONCEBYTES,
                                        enclen - crypto_box_NONCEBYTES, data,
                                        precalck);
  if (retval == 0) {
    dd->on_data(source, decrypted,
                enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES, dd);
  } else {
    fprintf(stderr, "DD: Unable to decrypt %d bytes from %s\n",
            enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES, source);
  }
  free(decrypted);
}

// up to the user to free the memory!
static void cmd_cb_pub(zmsg_t *msg, ddclient_t *dd) {
  int retval;
  char *source = zmsg_popstr(msg);
  char *topic = zmsg_popstr(msg);
  zframe_t *encrypted = zmsg_first(msg);
  unsigned char *data = zframe_data(encrypted);
  int enclen = zframe_size(encrypted);

  int mlen = enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES;
  unsigned char *decrypted = calloc(1, mlen);

  unsigned char *precalck = NULL;
  char *dot = strchr(source, '.');
  if (dot) {
    *dot = '\0';
    precalck = zhash_lookup(dd->keys->clientkeys, source);
    if (precalck) {
      //	printf("decrypting with tenant key:%s\n", source);
    }
    *dot = '.';
  }
  if (!precalck) {
    if (strncmp("public.", source, strlen("public.")) == 0) {
      precalck = dd->keys->pubboxk;
      //	printf("decrypting with public tenant key\n");
    } else {
      precalck = dd->keys->custboxk;
      //      printf("decrypting with my own key\n");
    }
  }

  retval = crypto_box_open_easy_afternm(decrypted, data + crypto_box_NONCEBYTES,
                                        enclen - crypto_box_NONCEBYTES, data,
                                        precalck);

  if (retval == 0) {
    dd->on_pub(source, topic, decrypted, mlen, dd);
  } else {
    fprintf(stderr, "DD: Unable to decrypt %d bytes from %s, topic %s\n", mlen,
            source, topic);
  }
  free(decrypted);
}

static void cmd_cb_subok(zmsg_t *msg, ddclient_t *dd) {
  char *topic = zmsg_popstr(msg);
  char *scope = zmsg_popstr(msg);
  sublist_activate(topic, scope, dd);
}

static void cmd_cb_error(zmsg_t *msg, ddclient_t *dd) {
  zframe_t *code_frame;
  code_frame = zmsg_pop(msg);
  if (code_frame == NULL) {
    fprintf(stderr, "DD: Misformed ERROR message, missing ERROR_CODE!\n");
    return;
  }

  int32_t *error_code = (int32_t *)zframe_data(code_frame);
  char *error_msg = zmsg_popstr(msg);
  dd->on_error(*error_code, error_msg, dd);
  zframe_destroy(&code_frame);
  free(error_msg);
}

// static void cmd_cb_nodst(zmsg_t *msg, ddclient_t *dd) {
//  char *destination = zmsg_popstr(msg);
//  dd->on_nodst(destination, dd);
//}

static int s_on_pipe_msg(zloop_t *loop, zsock_t *handle, void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  zmsg_t *msg = zmsg_recv(handle);
  zmsg_print(msg);
  char *command = zmsg_popstr(msg);
  //  All actors must handle $TERM in this way
  // returning -1 should stop zloop_start and terminate the actor
  if (streq(command, "$TERM")) {
    fprintf(stderr, "s_on_pipe_msg, got $TERM, quitting\n");

    return -1;
  } else if (streq(command, "subscribe")) {
    char *topic = zmsg_popstr(msg);
    char *scope = zmsg_popstr(msg);
    dd->subscribe(topic, scope, dd);
    free(topic);
    free(scope);
  } else if (streq(command, "unsubscribe")) {
    char *topic = zmsg_popstr(msg);
    char *scope = zmsg_popstr(msg);
    dd->unsubscribe(topic, scope, dd);
    free(topic);
    free(scope);
  } else if (streq(command, "publish")) {
    char *topic = zmsg_popstr(msg);
    char *message = zmsg_popstr(msg);
    zframe_t *mlen = zmsg_pop(msg);
    uint32_t len = *((uint32_t *)zframe_data(mlen));
    dd->publish(topic, message, len, dd);
    zframe_destroy(&mlen);
    free(topic);
    free(message);

  } else if (streq(command, "notify")) {
    char *target = zmsg_popstr(msg);
    char *message = zmsg_popstr(msg);
    zframe_t *mlen = zmsg_pop(msg);
    uint32_t len = *((uint32_t *)zframe_data(mlen));
    dd->notify(target, message, len, dd);
    zframe_destroy(&mlen);
    free(target);
    free(message);
  } else {
    fprintf(stderr, "s_on_pipe_msg, got unknown command: %s\n", command);
  }
  zmsg_destroy(&msg);
  free(command);
  return 0;
}

void actor_con(void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  fprintf(stderr, "actor Registered with broker %s!\n", dd->endpoint);
  zsock_send(dd->pipe, "ss", "reg", dd->endpoint);
}

void actor_discon(void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  fprintf(stderr, "actor Got disconnected from broker %s!\n", dd->endpoint);
  zsock_send(dd->pipe, "ss", "discon", dd->endpoint);
}

void actor_pub(char *source, char *topic, unsigned char *data, int length,
               void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  fprintf(stderr, "actor PUB S: %s T: %s L: %d D: '%s'", source, topic, length,
          data);
  zsock_send(dd->pipe, "sssbb", "pub", source, topic, &length, sizeof(length),
             data, length);
}

void actor_data(char *source, unsigned char *data, int length, void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  fprintf(stderr, "actor DATA S: %s L: %d D: '%s'", source, length, data);
  zsock_send(dd->pipe, "ssbb", "data", source, &length, sizeof(length), data,
             length);
}
void actor_error(int error_code, char *error_message, void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  fprintf(stderr, "actor Error %d : %s", error_code, error_message);
  zsock_send(dd->pipe, "ssb", "error", error_message, &error_code,
             sizeof(error_code));
}

static int s_on_dealer_msg(zloop_t *loop, zsock_t *handle, void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  dd->timeout = 0;
  zmsg_t *msg = zmsg_recv(handle);
  // zmsg_print(msg);

  if (msg == NULL) {
    fprintf(stderr, "DD: zmsg_recv returned NULL\n");
    return 0;
  }
  if (zmsg_size(msg) < 2) {
    fprintf(stderr, "DD: Message length less than 2, error!\n");
    zmsg_destroy(&msg);
    return 0;
  }

  zframe_t *proto_frame = zmsg_pop(msg);

  if (*((uint32_t *)zframe_data(proto_frame)) != DD_VERSION) {
    fprintf(stderr, "DD: Wrong version, expected 0x%x, got 0x%x\n", DD_VERSION,
            *zframe_data(proto_frame));
    zframe_destroy(&proto_frame);
    return 0;
  }
  zframe_t *cmd_frame = zmsg_pop(msg);
  uint32_t cmd = *((uint32_t *)zframe_data(cmd_frame));
  zframe_destroy(&cmd_frame);
  switch (cmd) {
  case DD_CMD_SEND:
    fprintf(stderr, "DD: Got command DD_CMD_SEND\n");
    break;
  case DD_CMD_FORWARD:
    fprintf(stderr, "DD: Got command DD_CMD_FORWARD\n");
    break;
  case DD_CMD_PING:
    fprintf(stderr, "DD: Got command DD_CMD_PING\n");
    break;
  case DD_CMD_ADDLCL:
    fprintf(stderr, "DD: Got command DD_CMD_ADDLCL\n");
    break;
  case DD_CMD_ADDDCL:
    fprintf(stderr, "DD: Got command DD_CMD_ADDDCL\n");
    break;
  case DD_CMD_ADDBR:
    fprintf(stderr, "DD: Got command DD_CMD_ADDBR\n");
    break;
  case DD_CMD_UNREG:
    fprintf(stderr, "DD: Got command DD_CMD_UNREG\n");
    break;
  case DD_CMD_UNREGDCLI:
    fprintf(stderr, "DD: Got command DD_CMD_UNREGDCLI\n");
    break;
  case DD_CMD_UNREGBR:
    fprintf(stderr, "DD: Got command DD_CMD_UNREGBR\n");
    break;
  case DD_CMD_DATA:
    cmd_cb_data(msg, dd);
    break;
  case DD_CMD_ERROR:
    cmd_cb_error(msg, dd);
    break;
  case DD_CMD_REGOK:
    cmd_cb_regok(msg, dd, loop);
    break;
  case DD_CMD_PONG:
    cmd_cb_pong(msg, dd, loop);
    break;
  case DD_CMD_CHALL:
    cmd_cb_chall(msg, dd);
    break;
  case DD_CMD_CHALLOK:
    fprintf(stderr, "DD: Got command DD_CMD_CHALLOK\n");
    break;
  case DD_CMD_PUB:
    cmd_cb_pub(msg, dd);
    break;
  case DD_CMD_SUB:
    fprintf(stderr, "DD: Got command DD_CMD_SUB\n");
    break;
  case DD_CMD_UNSUB:
    fprintf(stderr, "DD: Got command DD_CMD_UNSUB\n");
    break;
  case DD_CMD_SENDPUBLIC:
    fprintf(stderr, "DD: Got command DD_CMD_SENDPUBLIC\n");
    break;
  case DD_CMD_PUBPUBLIC:
    fprintf(stderr, "DD: Got command DD_CMD_PUBPUBLIC\n");
    break;
  case DD_CMD_SENDPT:
    fprintf(stderr, "DD: Got command DD_CMD_SENDPT\n");
    break;
  case DD_CMD_FORWARDPT:
    fprintf(stderr, "DD: Got command DD_CMD_FORWARDPT\n");
    break;
  case DD_CMD_DATAPT:
    fprintf(stderr, "DD: Got command DD_CMD_DATAPT\n");
    break;
  case DD_CMD_SUBOK:
    cmd_cb_subok(msg, dd);
    // printf("Got command DD_CMD_SUBOK\n");
    break;
  default:
    fprintf(stderr, "DD: Unknown command, value: 0x%x\n", cmd);
    break;
  }
  zmsg_destroy(&msg);
  return 0;
}

// Threads
void *ddthread(void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  int rc;

  dd->socket = zsock_new_dealer(NULL);
  if (!dd->socket) {
    fprintf(stderr, "DD: Error in zsock_new_dealer: %s\n", zmq_strerror(errno));
    free(dd);
    return NULL;
  }
  //  zsock_set_identity (dd->socket, dd->client_name);
  rc = zsock_connect(dd->socket, dd->endpoint);
  if (rc != 0) {
    fprintf(stderr, "DD: Error in zmq_connect: %s\n", zmq_strerror(errno));
    free(dd);
    return NULL;
  }

  dd->keys = read_ddkeys(dd->keyfile, dd->customer);
  if (dd->keys == NULL) {
    fprintf(stderr, "DD: Error reading keyfile!\n");
    return NULL;
  }

  dd->sublist = zlistx_new();
  zlistx_set_destructor(dd->sublist, (czmq_destructor *)sublist_free);
  zlistx_set_duplicator(dd->sublist, (czmq_duplicator *)sublist_dup);
  zlistx_set_comparator(dd->sublist, (czmq_comparator *)sublist_cmp);

  dd->loop = zloop_new();
  assert(dd->loop);
  dd->registration_loop =
      zloop_timer(dd->loop, 1000, 0, s_ask_registration, dd);
  rc = zloop_reader(dd->loop, dd->socket, s_on_dealer_msg, dd);
  zloop_start(dd->loop);
  zloop_destroy(&dd->loop);
}

void dd_actor(zsock_t *pipe, void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  int rc;
  zsock_signal(pipe, 0);
  dd->pipe = pipe;
  dd->socket = zsock_new_dealer(NULL);
  if (!dd->socket) {
    fprintf(stderr, "DD: Error in zsock_new_dealer: %s\n", zmq_strerror(errno));
    zsock_send(dd->pipe, "ss", "$TERM", "Error creating socket");
    dd->shutdown(dd);
    free(dd);
    return;
  }
  //  zsock_set_identity (dd->socket, dd->client_name);
  rc = zsock_connect(dd->socket, dd->endpoint);
  if (rc != 0) {
    fprintf(stderr, "DD: Error in zmq_connect: %s\n", zmq_strerror(errno));
    zsock_send(dd->pipe, "ss", "$TERM", "Connection failed");
    dd->shutdown(dd);
    free(dd);
    return;
  }

  dd->keys = read_ddkeys(dd->keyfile, dd->customer);
  if (dd->keys == NULL) {
    fprintf(stderr, "DD: Error reading keyfile!\n");
    zsock_send(dd->pipe, "ss", "$TERM", "Missing keyfile");
    dd->shutdown(dd);
    free(dd);
    return;
  }

  dd->sublist = zlistx_new();
  zlistx_set_destructor(dd->sublist, (czmq_destructor *)sublist_free);
  zlistx_set_duplicator(dd->sublist, (czmq_duplicator *)sublist_dup);
  zlistx_set_comparator(dd->sublist, (czmq_comparator *)sublist_cmp);

  dd->loop = zloop_new();
  assert(dd->loop);
  dd->registration_loop =
      zloop_timer(dd->loop, 1000, 0, s_ask_registration, dd);
  rc = zloop_reader(dd->loop, dd->socket, s_on_dealer_msg, dd);
  rc = zloop_reader(dd->loop, pipe, s_on_pipe_msg, dd);
  zloop_start(dd->loop);
  zloop_destroy(&dd->loop);
  zsock_destroy(&pipe);
}

zactor_t *start_ddactor(int verbose, char *client_name, char *customer,
                        char *endpoint, char *keyfile) {
  ddclient_t *dd = malloc(sizeof(ddclient_t));
  dd->style = DD_ACTOR;
  dd->verbose = verbose;
  dd->client_name = strdup(client_name);
  dd->customer = strdup(customer);
  dd->endpoint = strdup(endpoint);
  dd->keyfile = strdup(keyfile);
  dd->timeout = 0;
  dd->state = DD_STATE_UNREG;

  dd->pipe = NULL;
  dd->sublist = NULL;
  dd->loop = NULL;

  randombytes_buf(dd->nonce, crypto_box_NONCEBYTES);
  dd->on_reg = actor_con;
  dd->on_discon = actor_discon;
  dd->on_data = actor_data;
  dd->on_pub = actor_pub;
  dd->on_error = actor_error;
  dd->subscribe = subscribe;
  dd->unsubscribe = unsubscribe;
  dd->publish = publish;
  dd->notify = notify;
  dd->shutdown = ddthread_shutdown;
  zactor_t *actor = zactor_new(dd_actor, dd);
  return actor;
}

ddclient_t *start_ddthread(int verbose, char *client_name, char *customer,
                           char *endpoint, char *keyfile, dd_con con,
                           dd_discon discon, dd_data data, dd_pub pub,
                           dd_error error) {
  ddclient_t *dd = malloc(sizeof(ddclient_t));
  dd->style = DD_CALLBACK;
  dd->verbose = verbose;
  dd->client_name = strdup(client_name);
  dd->customer = strdup(customer);
  dd->endpoint = strdup(endpoint);
  dd->keyfile = strdup(keyfile);
  dd->timeout = 0;
  dd->state = DD_STATE_UNREG;
  randombytes_buf(dd->nonce, crypto_box_NONCEBYTES);
  dd->on_reg = con;
  dd->on_discon = discon;
  dd->on_data = data;
  dd->on_pub = pub;
  // dd->on_nodst = nodst;
  dd->on_error = error;
  dd->subscribe = subscribe;
  dd->unsubscribe = unsubscribe;
  dd->publish = publish;
  dd->notify = notify;
  dd->shutdown = ddthread_shutdown;
  zthread_new(ddthread, dd);
  return dd;
}

void print_ddkeystate(ddkeystate_t *keys) {
  char *hex = malloc(100);
  printf("Hash value: \t%s", keys->hash);
  printf("Private key: \t%s", sodium_bin2hex(hex, 100, keys->privkey, 32));
  printf("Public key: \t%s", sodium_bin2hex(hex, 100, keys->pubkey, 32));
  printf("DDPublic key: \t%s", sodium_bin2hex(hex, 100, keys->ddpubkey, 32));
  printf("PublicPub key: \t%s",
         sodium_bin2hex(hex, 100, keys->publicpubkey, 32));
  free(hex);
}
