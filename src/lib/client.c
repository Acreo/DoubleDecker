#include "../config.h"
#include "dd_classes.h"
// Structure of the ddclient class
struct _dd_t {
  zsock_t *socket; //  Socket for clients & workers
  void *pipe;
  int verbose;                //  Print activity to stdout
  unsigned char *endpoint;    //  Broker binds to this endpoint
  unsigned char *keyfile;     // JSON file with pub/priv keys
  unsigned char *client_name; // This client name
  int timeout;                // Incremental timeout (trigger > 3)
  int state;                  // Internal state
  int registration_loop;      // Timer ID for registration loop
  int heartbeat_loop;         // Timer ID for heartbeat loop
  uint64_t cookie;            // Cookie from authentication
  dd_keys_t *keys;            // Encryption keys loaded from JSON file
  zlistx_t *sublist;          // List of subscriptions, and if they're active
  zloop_t *loop;
  int style;
  FILE *logfp;                // logging file-pointer

  unsigned char nonce[crypto_box_NONCEBYTES];
  dd_on_con(*on_reg);
  dd_on_discon(*on_discon);
  dd_on_data(*on_data);
  dd_on_pub(*on_pub);
  dd_on_error(*on_error);
};

static void sublist_resubscribe(dd_t *self);
static int s_ping(zloop_t *loop, int timerid, void *args);
static int s_heartbeat(zloop_t *loop, int timerid, void *args);
static int s_ask_registration(zloop_t *loop, int timerid, void *args);
static void cb_regok(dd_t *self, zmsg_t *msg, zloop_t *loop);
static void cb_pong(dd_t *self, zmsg_t *msg, zloop_t *loop);
static void cb_chall(dd_t *self, zmsg_t *msg);
static void cb_data(dd_t *self, zmsg_t *msg);
static void cb_pub(dd_t *self, zmsg_t *msg);
static void cb_subok(dd_t *self, zmsg_t *msg);
static void cb_error(dd_t *self, zmsg_t *msg);
static int s_on_pipe_msg(zloop_t *loop, zsock_t *handle, void *args);
static int s_on_dealer_msg(zloop_t *loop, zsock_t *handle, void *args);
static void dd_keys_print(dd_keys_t *keys);

static void sublist_resubscribe(dd_t *self) {
  ddtopic_t *item;
  while ((item = zlistx_next((zlistx_t *)dd_get_subscriptions(self)))) {
    zsock_send(self->socket, "bbbss", &dd_version, 4, &dd_cmd_sub, 4,
               &self->cookie, sizeof(self->cookie), dd_sub_get_topic(item),
               dd_sub_get_scope(item));
  }
}

// ////////////////////////////////////////////////////
// // Commands for subscribe / publish / sendmessage //
// ////////////////////////////////////////////////////
const char *dd_get_version() { return PACKAGE_VERSION; }

int dd_get_state(dd_t *self) { return self->state; }

const char *dd_get_endpoint(dd_t *self) { return (const char *)self->endpoint; }

const char *dd_get_keyfile(dd_t *self) { return (const char *)self->keyfile; }

char *dd_get_privkey(dd_t *self) {
  char *hex = malloc(100);
  sodium_bin2hex(hex, 100, dd_keys_priv(self->keys), crypto_box_SECRETKEYBYTES);
  return hex;
}
char *dd_get_pubkey(dd_t *self) {
  assert(self);
  char *hex = malloc(100);
  assert(hex);
  sodium_bin2hex(hex, 100, dd_keys_pub(self->keys), crypto_box_PUBLICKEYBYTES);
  return hex;
}
char *dd_get_publickey(dd_t *self) {
  assert(self);
  char *hex = malloc(100);
  assert(hex);
  sodium_bin2hex(hex, 100, dd_keys_publicpub(self->keys),
                 crypto_box_PUBLICKEYBYTES);
  return hex;
}

const zlistx_t *dd_get_subscriptions(dd_t *self) { return self->sublist; }

int dd_subscribe(dd_t *self, char *topic, char *scope) {
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
    // TODO: Check rexscope in broker.c
    // check that scope follows re.fullmatch("/((\d)+/)+", scope):
    scopestr = scope;
  }
  sublist_add(self, topic, scopestr, 0);
  if (self->state == DD_STATE_REGISTERED) {
        dd_info("%s: Sending subscription for topic %s", self->client_name, topic);
    zsock_send(self->socket, "bbbss", &dd_version, 4, &dd_cmd_sub, 4,
               &self->cookie, sizeof(self->cookie), topic, scopestr);
    return 0;
    } else {
        dd_warning("%s: Not sending subscription for topic %s, not registered with broker!",
                   self->client_name, topic);
  return -1;
    }
}

int dd_unsubscribe(dd_t *self, char *topic, char *scope) {
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
    // TODO: check rexscope in broker.c
    // check that scope follows re.fullmatch("/((\d)+/)+", scope):
    scopestr = scope;
  }
    sublist_delete(self, (char *) topic, scopestr);
    if (self->state == DD_STATE_REGISTERED) {
        dd_info("%s: Sending unsubscription for topic %s", self->client_name, topic);
    zsock_send(self->socket, "bbbss", &dd_version, 4, &dd_cmd_unsub, 4,
               &self->cookie, sizeof(self->cookie), topic, scopestr);
  return 0;
    } else {
        dd_info("%s: Not sending unsubscription for topic %s, not registered with broker!", self->client_name,
                topic);
        return -1;
    }
}

int dd_publish(dd_t *self, char *topic, char *message, int mlen) {
  const unsigned char *precalck = NULL;
  int srcpublic = 0;
  int dstpublic = 0;
  int retval;

  if (dd_keys_ispublic(self->keys)) {
    srcpublic = 1;
  }
  if (strncmp("public.", topic, strlen("public.")) == 0) {
    dstpublic = 1;
  }

  char *dot = strchr(topic, '.');
  if (dot && srcpublic) {
    *dot = '\0';
    precalck = zhash_lookup(dd_keys_clients(self->keys), topic);
    if (precalck) {
      // TODO: This is not allowed by the broker
      // We should return an error if this is happening
            dd_error("%s: Public client cannot publish to tenants!", self->client_name);
      return -1;
    }
    *dot = '.';
  }
  if (!precalck && !dstpublic) {
    precalck = dd_keys_custboxk(self->keys);
  } else if (dstpublic) {
    precalck = dd_keys_pubboxk(self->keys);
  }

  int enclen = mlen + crypto_box_NONCEBYTES + crypto_box_MACBYTES;
  unsigned char *dest = calloc(1, enclen);
  unsigned char *ciphertext = dest; // dest+crypto_box_NONCEBYTES;

  // increment nonce
  nonce_increment(self->nonce, crypto_box_NONCEBYTES);
  memcpy(dest, self->nonce, crypto_box_NONCEBYTES);

  dest += crypto_box_NONCEBYTES;
  retval = crypto_box_easy_afternm(dest, (const unsigned char *)message, mlen,
                                   self->nonce, precalck);

  if (retval != 0) {
        dd_error("%s: Unable to encrypt %zu bytes!", self->client_name, mlen);
    free(ciphertext);
    return -1;
  }
  if (self->state == DD_STATE_REGISTERED) {
        dd_info("%s: Publishing message on topic %s", self->client_name, topic);
    zsock_send(self->socket, "bbbszb", &dd_version, 4, &dd_cmd_pub, 4,
               &self->cookie, sizeof(self->cookie), topic, ciphertext, enclen);
    } else {
        dd_info("%s: Not publishing message on topic %s, not registered with broker!", self->client_name,
                topic);
  }
  free(ciphertext);
  return 0;
}

int dd_notify(dd_t *self, char *target, char *message, int mlen) {
  const uint8_t *precalck = NULL;
  int srcpublic = 0;
  int dstpublic = 0;

  if (dd_keys_ispublic(self->keys)) {
    srcpublic = 1;
  }
  if (strncmp("public.", target, strlen("public.")) == 0) {
    dstpublic = 1;
  }

  /* printf ("self->sendmsg called t: %s m: %s l: %d\n", target, message,
   * mlen);
   */
  char *dot = strchr(target, '.');

  int retval;
  if (dot && srcpublic) {
    *dot = '\0';
    precalck = zhash_lookup(dd_keys_clients(self->keys), target);
    if (precalck) {
      /* printf("encrypting with tenant key: %s\n",target); */
    }
    *dot = '.';
  }
  if (!precalck && !dstpublic) {
    precalck = dd_keys_custboxk(self->keys);
    /* printf ("encrypting with my own key\n"); */
  } else if (dstpublic) {
    precalck = dd_keys_pubboxk(self->keys);
    /* printf("encrypting with public key\n"); */
  }

  int enclen = mlen + crypto_box_NONCEBYTES + crypto_box_MACBYTES;
  unsigned char *dest = calloc(1, enclen);
  unsigned char *ciphertext = dest; // dest+crypto_box_NONCEBYTES;

  // increment nonce
  nonce_increment(self->nonce, crypto_box_NONCEBYTES);
  memcpy(dest, self->nonce, crypto_box_NONCEBYTES);

  dest += crypto_box_NONCEBYTES;
  retval = crypto_box_easy_afternm(dest, (const unsigned char *)message, mlen,
                                   self->nonce, precalck);
  /* char *hex = calloc (1, 1000); */
  /* sodium_bin2hex (hex, 1000, ciphertext, enclen); */
  /* printf ("ciphertext size %d: %s\n", enclen, hex); */
  /* free (hex); */

    if (retval != 0) {
        dd_error("%s: Unable to encrypt %zu bytes!", self->client_name, mlen);
    free(ciphertext);
    return -1;
  }
  if (self->state == DD_STATE_REGISTERED) {
        dd_info("%s: Sending notification to %s", self->client_name, target);
    zsock_send(self->socket, "bbbsb", &dd_version, 4, &dd_cmd_send, 4,
               &self->cookie, sizeof(self->cookie), target, ciphertext, enclen);
    } else {
        dd_info("%s: Not sending notification to %s, not registered with broker", self->client_name, target);
  }
  free(ciphertext);
  return 0;
}

//  Send logging output to syslog.
void dd_set_syslog(dd_t *self) {
    zsys_set_logsystem(true);
}

//  Set the logging file of the client, will default to stderr if not set.
//  Will try to create/open a file with the provided name.
//  Returns 0 on success, -1 on failure
int dd_set_logfile(dd_t *self, const char *logfile) {
    self->logfp = fopen(logfile, "w+");
    if (self->logfp == NULL) {
      dd_error("Cannot open logfile %s\n", logfile);
        perror("Logfile open");
        return -1;
    }
    zsys_set_logstream(self->logfp);
    return 0;
}


//  Set the logging file of the client, using an already existing FILE
//  pointer.
//  Returns 0 on success, -1 on failure
int dd_set_logfp(dd_t *self, FILE *logfile){
    if(logfile == NULL)
        return -1;

    self->logfp = logfile;
    zsys_set_logstream(self->logfp);
    return 0;
}



//  Set the client loglevel, as a single character string.
//  Where "e":error,"w":warning,"n":notice,"i":info, and "d":debug.
//  Default is "n". For no output, "q" will keep it quiet.
int dd_set_loglevel(dd_t *self, const char *logstr) {
    if (streq(logstr, "e"))
        loglevel = DD_LOG_ERROR;
    else if (streq(logstr, "w"))
        loglevel = DD_LOG_WARNING;
    else if (streq(logstr, "n"))
        loglevel = DD_LOG_NOTICE;
    else if (streq(logstr, "i"))
        loglevel = DD_LOG_INFO;
    else if (streq(logstr, "d"))
        loglevel = DD_LOG_DEBUG;
    else if (streq(logstr, "q"))
        loglevel = DD_LOG_NONE;
    else
        return -1;
    return 0;
}


// ////////////////////////
// callbacks from zloop //
// ////////////////////////

static int s_ping(zloop_t *loop, int timerid, void *args) {
  dd_t *self = (dd_t *)args;
  if (self->state == DD_STATE_REGISTERED)
    zsock_send(self->socket, "bbb", &dd_version, 4, &dd_cmd_ping, 4,
               &self->cookie, sizeof(self->cookie));
  return 0;
}

static int s_heartbeat(zloop_t *loop, int timerid, void *args) {
  dd_t *self = (dd_t *)args;
  self->timeout++;
  if (self->timeout > 3) {
    self->state = DD_STATE_UNREG;
    self->registration_loop =
        zloop_timer(loop, 1000, 0, s_ask_registration, self);
    zloop_timer_end(loop, self->heartbeat_loop);
    sublist_deactivate_all(self);
    self->on_discon(self);
  }
  return 0;
}

static int s_ask_registration(zloop_t *loop, int timerid, void *args) {
  dd_t *self = (dd_t *)args;
  if (self->state == DD_STATE_UNREG) {
    zsock_set_linger(self->socket, 0);
    zloop_reader_end(loop, self->socket);
    zsock_destroy((zsock_t **)&self->socket);
    self->socket = zsock_new_dealer(NULL);
    if (!self->socket) {
            dd_error("%s: Error in zsock_new_dealer %s", self->client_name, zmq_strerror(errno));
      free(self);
      return -1;
    }
        int rc = zsock_connect(self->socket, "%s", (const char *) self->endpoint);
    if (rc != 0) {
            dd_error("%s: Error in zmq_connect %s", self->client_name, zmq_strerror(errno));
      free(self);
      return -1;
    }
    zloop_reader(loop, self->socket, s_on_dealer_msg, self);
        dd_info("%s: Trying to register with broker", self->client_name);
    zsock_send(self->socket, "bbs", &dd_version, 4, &dd_cmd_addlcl, 4,
               (char *)dd_keys_hash(self->keys));
  }
  return 0;
}

// /////////////////////////////////////
// / callbacks for different messages //
// ////////////////////////////////////
static void cb_regok(dd_t *self, zmsg_t *msg, zloop_t *loop) {
  zframe_t *cookie_frame;
  cookie_frame = zmsg_pop(msg);
  if (cookie_frame == NULL) {
        dd_error("%s: Misformed REGOK message, missing COOKIE", self->client_name);
    return;
  }
  uint64_t *cookie2 = (uint64_t *)zframe_data(cookie_frame);
  self->cookie = *cookie2;
  zframe_destroy(&cookie_frame);
  self->state = DD_STATE_REGISTERED;
  zsock_send(self->socket, "bbb", &dd_version, 4, &dd_cmd_ping, 4,
             &self->cookie, sizeof(self->cookie));

  self->heartbeat_loop = zloop_timer(loop, 1500, 0, s_heartbeat, self);
  zloop_timer_end(loop, self->registration_loop);
  // if this is re-registration, we should try to subscribe again
  sublist_resubscribe(self);
  dd_info("%s: Registered with broker", self->client_name);
  self->on_reg(self);
}

static void cb_pong(dd_t *self, zmsg_t *msg, zloop_t *loop) {
  zloop_timer(loop, 1500, 1, s_ping, self);
}

static void cb_chall(dd_t *self, zmsg_t *msg) {
  int retval = 0;
  zframe_t *encrypted = zmsg_first(msg);
  unsigned char *data = zframe_data(encrypted);
  int enclen = zframe_size(encrypted);
  unsigned char *decrypted = calloc(1, enclen);

  retval = crypto_box_open_easy_afternm(decrypted, data + crypto_box_NONCEBYTES,
                                        enclen - crypto_box_NONCEBYTES, data,
                                        dd_keys_ddboxk(self->keys));
  if (retval != 0) {
        dd_error("%s: Unable to decrypt CHALLENGE from broker", self->client_name);
    return;
  }

  zframe_t *temp_frame = zframe_new(decrypted, enclen - crypto_box_NONCEBYTES -
                                                   crypto_box_MACBYTES);
    dd_info("%s: Sending response to broker challenge", self->client_name);
  zsock_send(self->socket, "bbfss", &dd_version, 4, &dd_cmd_challok, 4,
             temp_frame, dd_keys_hash(self->keys), self->client_name);
  zframe_destroy(&temp_frame);
  free(decrypted);
}

static void cb_data(dd_t *self, zmsg_t *msg) {
  int retval;
  char *source = zmsg_popstr(msg);
  /* printf("cb_data: S: %s\n", source); */
  zframe_t *encrypted = zmsg_first(msg);
  unsigned char *data = zframe_data(encrypted);
  int enclen = zframe_size(encrypted);
  unsigned char *decrypted =
      calloc(1, enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES);
  const uint8_t *precalck = NULL;
  char *dot = strchr(source, '.');
  if (dot) {
    *dot = '\0';
    precalck = zhash_lookup(dd_keys_clients(self->keys), source);
    if (precalck) {
    }
    *dot = '.';
  }

  if (!precalck) {
    if (strncmp("public.", source, strlen("public.")) == 0) {
      precalck = dd_keys_pubboxk(self->keys);
    } else {
      precalck = dd_keys_custboxk(self->keys);
    }
  }
  retval = crypto_box_open_easy_afternm(decrypted, data + crypto_box_NONCEBYTES,
                                        enclen - crypto_box_NONCEBYTES, data,
                                        precalck);
  if (retval == 0) {
        dd_info("%s: Got notification from %s", self->client_name, source);
    self->on_data(source, decrypted,
                  enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES, self);
  } else {
        dd_error("%s: Unable to decrypt %zu bytes from %s", self->client_name,
            enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES, source);
  }
  free(decrypted);
  free(source);
}

// up to the user to free the memory!
static void cb_pub(dd_t *self, zmsg_t *msg) {
  int retval;
  char *source = zmsg_popstr(msg);
  char *topic = zmsg_popstr(msg);
  zframe_t *encrypted = zmsg_first(msg);
  unsigned char *data = zframe_data(encrypted);
  int enclen = zframe_size(encrypted);

  int mlen = enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES;
  unsigned char *decrypted = calloc(1, mlen);

  const unsigned char *precalck = NULL;
  char *dot = strchr(source, '.');
  if (dot) {
    *dot = '\0';
    precalck = zhash_lookup(dd_keys_clients(self->keys), source);
    if (precalck) {
    }
    *dot = '.';
  }
  if (!precalck) {
    if (strncmp("public.", source, strlen("public.")) == 0) {
      precalck = dd_keys_pubboxk(self->keys);
    } else {
      precalck = dd_keys_custboxk(self->keys);
    }
  }

  retval = crypto_box_open_easy_afternm(decrypted, data + crypto_box_NONCEBYTES,
                                        enclen - crypto_box_NONCEBYTES, data,
                                        precalck);

  if (retval == 0) {
        dd_info("%s: Got publication on topic %s", self->client_name, topic);
    self->on_pub(source, topic, decrypted, mlen, self);
  } else {
        dd_error("%s: Unable to decrypt %zu bytes from %s, topic %s", self->client_name,
                 mlen, source, topic);
  }
  free(decrypted);
  free(topic);
  free(source);
}

static void cb_subok(dd_t *self, zmsg_t *msg) {
  char *topic = zmsg_popstr(msg);
  char *scope = zmsg_popstr(msg);
  sublist_activate(self, topic, scope);
    dd_info("%s: Subscription to %s activated", self->client_name, topic);
  free(topic);
  free(scope);
}

static void cb_error(dd_t *self, zmsg_t *msg) {
  zframe_t *code_frame;
  code_frame = zmsg_pop(msg);
  if (code_frame == NULL) {
        dd_error("%s: Misformed ERROR message, missing ERROR_CODE", self->client_name);
    return;
  }

  int32_t *error_code = (int32_t *)zframe_data(code_frame);
  char *error_msg = zmsg_popstr(msg);
    dd_info("%s: Got error message code %d msg %s", self->client_name, *error_code, error_msg);
  self->on_error(*error_code, error_msg, self);
  zframe_destroy(&code_frame);
  free(error_msg);
}

static int s_on_pipe_msg(zloop_t *loop, zsock_t *handle, void *args) {
  dd_t *self = (dd_t *)args;
  zmsg_t *msg = zmsg_recv(handle);
  char *command = zmsg_popstr(msg);
  //  All actors must handle $TERM in this way
  // returning -1 should stop zloop_start and terminate the actor
  if (streq(command, "$TERM")) {
    free(command);
    zmsg_destroy(&msg);
    return -1;

  } else if (streq(command, "subscribe")) {
    char *topic = zmsg_popstr(msg);
    char *scope = zmsg_popstr(msg);
    dd_subscribe(self, topic, scope);
    free(topic);
    free(scope);
    free(command);
    zmsg_destroy(&msg);
  } else if (streq(command, "unsubscribe")) {
    char *topic = zmsg_popstr(msg);
    char *scope = zmsg_popstr(msg);
    dd_unsubscribe(self, topic, scope);
    free(topic);
    free(scope);
    free(command);
    zmsg_destroy(&msg);
  } else if (streq(command, "publish")) {
    char *topic = zmsg_popstr(msg);
    char *message = zmsg_popstr(msg);
    zframe_t *mlen = zmsg_pop(msg);
    uint32_t len = *((uint32_t *)zframe_data(mlen));
    dd_publish(self, topic, message, len);
    zframe_destroy(&mlen);
    free(topic);
    free(message);
    free(command);
    zmsg_destroy(&msg);

  } else if (streq(command, "notify")) {
    char *target = zmsg_popstr(msg);
    char *message = zmsg_popstr(msg);
    zframe_t *mlen = zmsg_pop(msg);
    uint32_t len = *((uint32_t *)zframe_data(mlen));
    dd_notify(self, target, message, len);
    zframe_destroy(&mlen);
    free(target);
    free(message);
    free(command);
    zmsg_destroy(&msg);
  } else {
    dd_error( "s_on_pipe_msg, got unknown command: %s\n", command);
    free(command);
    zmsg_destroy(&msg);
  }
  return 0;
}

void actor_con(void *args) {
  dd_t *self = (dd_t *)args;
  zsock_send(self->pipe, "ss", "reg", self->endpoint);
}

void actor_discon(void *args) {
  dd_t *self = (dd_t *)args;
  zsock_send(self->pipe, "ss", "discon", self->endpoint);
}

void actor_pub(char *source, char *topic, unsigned char *data, int length,
               void *args) {
  dd_t *self = (dd_t *)args;
  zsock_send(self->pipe, "sssbb", "pub", source, topic, &length, sizeof(length),
             data, length);
}

void actor_data(char *source, unsigned char *data, int length, void *args) {
  dd_t *self = (dd_t *)args;
  zsock_send(self->pipe, "ssbb", "data", source, &length, sizeof(length), data,
             length);
}
void actor_error(int error_code, char *error_message, void *args) {
  dd_t *self = (dd_t *)args;
  zsock_send(self->pipe, "ssb", "error", error_message, &error_code,
             sizeof(error_code));
}

static int s_on_dealer_msg(zloop_t *loop, zsock_t *handle, void *args) {
  dd_t *self = (dd_t *)args;
  self->timeout = 0;
  zmsg_t *msg = zmsg_recv(handle);

  if (msg == NULL) {
        dd_error("%s: zmsg_recv returned NULL", self->client_name);
    return 0;
  }
  if (zmsg_size(msg) < 2) {
        dd_error("%s: Number of frames less than 2, error!", self->client_name);
    zmsg_destroy(&msg);
    return 0;
  }

  zframe_t *proto_frame = zmsg_pop(msg);

  if (*((uint32_t *)zframe_data(proto_frame)) != DD_VERSION) {
     dd_error("%s: Wrong version, expected 0x%x, got 0x%x", self->client_name, DD_VERSION,
            *zframe_data(proto_frame));
    zframe_destroy(&proto_frame);
    zmsg_destroy(&msg);
    return 0;
  }
  zframe_t *cmd_frame = zmsg_pop(msg);
  uint32_t cmd = *((uint32_t *)zframe_data(cmd_frame));
  switch (cmd) {
  case DD_CMD_SEND:
            dd_error("%s: Got unexpected command DD_CMD_SEND", self->client_name);
    break;
  case DD_CMD_FORWARD:
            dd_error("%s: Got unexpected command DD_CMD_FORWARD", self->client_name);
    break;
  case DD_CMD_PING:
            dd_error("%s: Got unexpected command DD_CMD_PING", self->client_name);
    break;
  case DD_CMD_ADDLCL:
            dd_error("%s: Got unexpected command DD_CMD_ADDLCL", self->client_name);
    break;
  case DD_CMD_ADDDCL:
            dd_error("%s: Got unexpected command DD_CMD_ADDDCL", self->client_name);
    break;
  case DD_CMD_ADDBR:
            dd_error("%s: Got unexpected command DD_CMD_ADDBR", self->client_name);
    break;
  case DD_CMD_UNREG:
            dd_error("%s: Got unexpected command DD_CMD_UNREG", self->client_name);
    break;
  case DD_CMD_UNREGDCLI:
            dd_error("%s: Got unexpected command DD_CMD_UNREGDCLI", self->client_name);
    break;
  case DD_CMD_UNREGBR:
            dd_error("%s: Got unexpected command DD_CMD_UNREGBR", self->client_name);
    break;
  case DD_CMD_DATA:
    cb_data(self, msg);
    break;
  case DD_CMD_ERROR:
    cb_error(self, msg);
    break;
  case DD_CMD_REGOK:
    cb_regok(self, msg, loop);
    break;
  case DD_CMD_PONG:
    cb_pong(self, msg, loop);
    break;
  case DD_CMD_CHALL:
    cb_chall(self, msg);
    break;
  case DD_CMD_CHALLOK:
            dd_error("%s: Got unexpected command DD_CMD_CHALLOK", self->client_name);
    break;
  case DD_CMD_PUB:
    cb_pub(self, msg);
    break;
  case DD_CMD_SUB:
            dd_error("%s: Got unexpected command DD_CMD_SUB", self->client_name);
    break;
  case DD_CMD_UNSUB:
            dd_error("%s: Got unexpected command DD_CMD_UNSUB", self->client_name);
    break;
  case DD_CMD_SENDPUBLIC:
            dd_error("%s: Got unexpected command DD_CMD_SENDPUBLIC", self->client_name);
    break;
  case DD_CMD_PUBPUBLIC:
            dd_error("%s: Got unexpected command DD_CMD_PUBPUBLIC", self->client_name);
    break;
  case DD_CMD_SENDPT:
            dd_error("%s: Got unexpected command DD_CMD_SENDPT", self->client_name);
    break;
  case DD_CMD_FORWARDPT:
            dd_error("%s: Got unexpected command DD_CMD_FORWARDPT", self->client_name);
    break;
  case DD_CMD_DATAPT:
            dd_error("%s: Got unexpected command DD_CMD_DATAPT", self->client_name);
    break;
  case DD_CMD_SUBOK:
    cb_subok(self, msg);
    break;
  default:
            dd_error("%s: Unknown command, value: 0x%x\n", cmd);
    break;
  }
  zframe_destroy(&proto_frame);
  zframe_destroy(&cmd_frame);
  zmsg_destroy(&msg);
  return 0;
}

// Threads
void *ddthread(void *args) {
  dd_t *self = (dd_t *)args;
  int rc;

  self->socket = zsock_new_dealer(NULL);
  if (!self->socket) {
    dd_error("%s: Error in zsock_new_dealer: %s", self->client_name,zmq_strerror(errno));
    free(self);
    return NULL;
  }

  rc = zsock_connect(self->socket, (const char *)self->endpoint);
  if (rc != 0) {
    dd_error("%s: Error in zmq_connect: %s", self->client_name, zmq_strerror(errno));
    free(self);
    return NULL;
  }

  self->keys = dd_keys_new(self->keyfile);
  if (self->keys == NULL) {
    dd_error("%s: Error reading keyfile!", self->client_name);
    return NULL;
  }

  self->sublist = sublist_new();

  self->loop = zloop_new();
  assert(self->loop);
  self->registration_loop =
      zloop_timer(self->loop, 1000, 0, s_ask_registration, self);
  rc = zloop_reader(self->loop, self->socket, s_on_dealer_msg, self);
  zloop_start(self->loop);
  return self;
}

void dd_destroy(dd_t **self_p) {
  assert(self_p);
  if (*self_p) {
    dd_t *self = *self_p;

    if (self->state == DD_STATE_REGISTERED) {
      zsock_send(self->socket, "bbb", &dd_version, 4, &dd_cmd_unreg, 4,
                 &self->cookie, sizeof(self->cookie));
    }

    zsock_destroy(&self->socket);
    if (self->endpoint) {
      free(self->endpoint);
      self->endpoint = NULL;
    }
    if (self->keyfile) {
      free(self->keyfile);
      self->keyfile = NULL;
    }
    if (self->client_name) {
      free(self->client_name);
      self->client_name = NULL;
    }

    dd_keys_destroy(&self->keys);
    sublist_destroy(&self->sublist);
    zloop_destroy(&self->loop);

    free(self);
    *self_p = NULL;
  }
}
void dd_actor(zsock_t *pipe, void *args) {
  dd_t *self = (dd_t *)args;
  int rc;
  zsock_signal(pipe, 0);
  self->pipe = pipe;
  self->socket = zsock_new_dealer(NULL);
  if (!self->socket) {
    dd_error("%s: Error in zsock_new_dealer: %s\n", self->client_name, zmq_strerror(errno));
    zsock_send(self->pipe, "ss", "$TERM", "Error creating socket");
    dd_destroy(&self);
    return;
  }
  rc = zsock_connect(self->socket, (const char *)self->endpoint);
  if (rc != 0) {
    dd_error("%s: Error in zmq_connect: %s", self->client_name, zmq_strerror(errno));
    zsock_send(self->pipe, "ss", "$TERM", "Connection failed");
    dd_destroy(&self);
    return;
  }

  self->keys = dd_keys_new((const char *)self->keyfile);
  if (self->keys == NULL) {
    dd_error("%s: Error reading keyfile!",self->client_name);
    zsock_send(self->pipe, "ss", "$TERM", "Missing keyfile");
    dd_destroy(&self);
    return;
  }

  self->sublist = sublist_new();
  self->loop = zloop_new();
  assert(self->loop);
  self->registration_loop =
      zloop_timer(self->loop, 1000, 0, s_ask_registration, self);
  rc = zloop_reader(self->loop, self->socket, s_on_dealer_msg, self);
  rc = zloop_reader(self->loop, pipe, s_on_pipe_msg, self);
  while (rc == 0){
    rc = zloop_start(self->loop);
    if(rc == 0) {
      dd_error("%s: dd_actor:zloop_start returned 0, interrupted! - terminating(might not be the best choice)", self->client_name);
     // terminate, maybe not always a good choice here :(
     rc = -1;
    } else if (rc == -1) {
    }
  }
   dd_destroy(&self);
}

zactor_t *ddactor_new(char *client_name, char *endpoint, char *keyfile) {
  // Make sure that ZMQ doesn't affect main process signal handling
  zsys_init();
  zsys_handler_reset(); 
  dd_t *self = malloc(sizeof(dd_t));
  self->style = DD_ACTOR;  
  self->client_name = (unsigned char *)strdup(client_name);
  self->endpoint = (unsigned char *)strdup(endpoint);
  self->keyfile = (unsigned char *)strdup(keyfile);
  self->timeout = 0;
  self->state = DD_STATE_UNREG;

  self->pipe = NULL;
  self->sublist = NULL;
  self->loop = NULL;

  randombytes_buf(self->nonce, crypto_box_NONCEBYTES);
  self->on_reg = actor_con;
  self->on_discon = actor_discon;
  self->on_data = actor_data;
  self->on_pub = actor_pub;
  self->on_error = actor_error;
  zactor_t *actor = zactor_new(dd_actor, self);
  return actor;
}

dd_t *dd_new(char *client_name, char *endpoint, char *keyfile, dd_on_con con,
             dd_on_discon discon, dd_on_data data, dd_on_pub pub,
             dd_on_error error) {
  // Make sure that ZMQ doesn't affect main process signal handling
  zsys_init();
  zsys_handler_reset(); 
  dd_t *self = malloc(sizeof(dd_t));
  self->style = DD_CALLBACK;
  self->client_name = (unsigned char *)strdup(client_name);
  self->endpoint = (unsigned char *)strdup(endpoint);
  self->keyfile = (unsigned char *)strdup(keyfile);
  self->timeout = 0;
  self->state = DD_STATE_UNREG;
  randombytes_buf(self->nonce, crypto_box_NONCEBYTES);
  self->on_reg = con;
  self->on_discon = discon;
  self->on_data = data;
  self->on_pub = pub;
  self->on_error = error;
  zthread_new(ddthread, self);
  zsys_set_logident("ddclient");
  dd_set_logfp(self, stdout);
  return self;
}

static void dd_keys_print(dd_keys_t *keys) {
  char *hex = malloc(100);
  printf("Hash value: \t%s", dd_keys_hash(keys));
  printf("Private key: \t%s", sodium_bin2hex(hex, 100, dd_keys_priv(keys), 32));
  printf("Public key: \t%s", sodium_bin2hex(hex, 100, dd_keys_pub(keys), 32));
  printf("DDPublic key: \t%s",
         sodium_bin2hex(hex, 100, dd_keys_ddpub(keys), 32));
  printf("PublicPub key: \t%s",
         sodium_bin2hex(hex, 100, dd_keys_publicpub(keys), 32));
  free(hex);
}
