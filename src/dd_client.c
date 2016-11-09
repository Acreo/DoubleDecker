/*  =========================================================================
    dd_client - class description

    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of CZMQ, the high-level C binding for 0MQ:       
    http://czmq.zeromq.org.                                            
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

/*
@header
    dd_client - 
@discuss
@end
*/
#ifndef __USE_GNU
#define __USE_GNU 1
#endif

#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include "dd_classes.h"

//  Structure of our class

struct _dd_client_t {
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

    unsigned char nonce[crypto_box_NONCEBYTES];
    dd_client_on_con(*on_reg);
    dd_client_on_discon(*on_discon);
    dd_client_on_data(*on_data);
    dd_client_on_pub(*on_pub);
    dd_client_on_error(*on_error);
};

static void sublist_resubscribe(dd_client_t *self);
static int s_ping(zloop_t *loop, int timerid, void *args);
static int s_heartbeat(zloop_t *loop, int timerid, void *args);
static int s_ask_registration(zloop_t *loop, int timerid, void *args);
static void cb_regok(dd_client_t *self, zmsg_t *msg, zloop_t *loop);
static void cb_pong(dd_client_t *self, zmsg_t *msg, zloop_t *loop);
static void cb_chall(dd_client_t *self, zmsg_t *msg);
static void cb_data(dd_client_t *self, zmsg_t *msg);
static void cb_pub(dd_client_t *self, zmsg_t *msg);
static void cb_subok(dd_client_t *self, zmsg_t *msg);
static void cb_error(dd_client_t *self, zmsg_t *msg);
static int s_on_pipe_msg(zloop_t *loop, zsock_t *handle, void *args);
static int s_on_dealer_msg(zloop_t *loop, zsock_t *handle, void *args);
static void dd_keys_print(dd_keys_t *keys);

// update or add topic/scope/active to list
// TODO: should this be dd_sublist class?
void sublist_add(dd_client_t *self, char *topic, char *scope, char active);
int sublist_delete(dd_client_t *self, char *topic, char *scope);
void sublist_activate(dd_client_t *self, char *topic, char *scope);
zlistx_t *sublist_new();
void sublist_destroy(zlistx_t **self_p);
void sublist_deactivate_all(dd_client_t *self);
static void sublist_resubscribe(dd_client_t *self) {
    dd_topic_t *item;
    while ((item = zlistx_next((zlistx_t *) dd_client_get_subscriptions(self)))) {
        zsock_send(self->socket, "bbbss", &dd_version, 4, &dd_cmd_sub, 4,
                   &self->cookie, sizeof(self->cookie), dd_topic_get_topic(item),
                   dd_topic_get_scope(item));
    }
}

// ////////////////////////////////////////////////////
// // Commands for subscribe / publish / sendmessage //
// ////////////////////////////////////////////////////
const char *dd_get_version() {
    char *strp;
    asprintf(&strp, "verision %d-%d-%d", DD_VERSION_MAJOR, DD_VERSION_MINOR, DD_VERSION_PATCH);
    return strp;
}

int dd_client_get_state(dd_client_t *self) { return self->state; }

const char *dd_client_get_endpoint(dd_client_t *self) { return (const char *) self->endpoint; }

zsock_t *dd_client_get_pipe(dd_client_t *self) {
    return self->pipe;
}

const char *dd_client_get_keyfile(dd_client_t *self) { return (const char *) self->keyfile; }

char *dd_client_get_privkey(dd_client_t *self) {
    char *hex = malloc(100);
    sodium_bin2hex(hex, 100, dd_keys_priv(self->keys), crypto_box_SECRETKEYBYTES);
    return hex;
}

char * dd_client_get_pubkey(dd_client_t *self) {
    assert(self);
    char *hex = malloc(100);
    assert(hex);
    sodium_bin2hex(hex, 100, dd_keys_pub(self->keys), crypto_box_PUBLICKEYBYTES);
    return hex;
}

char *dd_client_get_publickey(dd_client_t *self) {
    assert(self);
    char *hex = malloc(100);
    assert(hex);
    sodium_bin2hex(hex, 100, dd_keys_publicpub(self->keys),
                   crypto_box_PUBLICKEYBYTES);
    return hex;
}

zlistx_t *dd_client_get_subscriptions(dd_client_t *self) { return self->sublist; }

int dd_client_subscribe(dd_client_t *self, const char *topic, const char *scope) {
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
        zsock_send(self->socket, "bbbss", &dd_version, 4, &dd_cmd_sub, 4,
                   &self->cookie, sizeof(self->cookie), topic, scopestr);
        return 0;
    }
    return -1;
}


int dd_client_unsubscribe(dd_client_t *self, const char *topic, const char *scope) {
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
    sublist_delete(self, topic, scopestr);
    if (self->state == DD_STATE_REGISTERED)
        zsock_send(self->socket, "bbbss", &dd_version, 4, &dd_cmd_unsub, 4,
                   &self->cookie, sizeof(self->cookie), topic, scopestr);
    return 0;
}

int dd_client_publish(dd_client_t *self, const char *topic, const byte *message, size_t mlen) {
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
            fprintf(stderr, "Public client cannot publish to tenants!\n");
            return -1;
        }
        *dot = '.';
    }
    if (!precalck && !dstpublic) {
        precalck = dd_keys_custboxk(self->keys);
    } else if (dstpublic) {
        precalck = dd_keys_pubboxk(self->keys);
    }

    size_t enclen = mlen + crypto_box_NONCEBYTES + crypto_box_MACBYTES;
    unsigned char *dest = calloc(1, enclen);
    unsigned char *ciphertext = dest; // dest+crypto_box_NONCEBYTES;

    // increment nonce
    dd_keys_nonce_increment(self->nonce, crypto_box_NONCEBYTES);
    memcpy(dest, self->nonce, crypto_box_NONCEBYTES);

    dest += crypto_box_NONCEBYTES;
    retval = crypto_box_easy_afternm(dest, (const unsigned char *) message, mlen,
                                     self->nonce, precalck);

    if (retval != 0) {
        fprintf(stderr, "DD: Unable to encrypt %zu bytes!\n", mlen);
        free(ciphertext);
        return -1;
    }
    if (self->state == DD_STATE_REGISTERED) {
        zsock_send(self->socket, "bbbszb", &dd_version, 4, &dd_cmd_pub, 4,
                   &self->cookie, sizeof(self->cookie), topic, ciphertext, enclen);
    }
    free(ciphertext);
    return 0;
}

int dd_client_notify(dd_client_t *self, const char *target, const byte *message, size_t mlen) {
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

    size_t enclen = mlen + crypto_box_NONCEBYTES + crypto_box_MACBYTES;
    unsigned char *dest = calloc(1, enclen);
    unsigned char *ciphertext = dest; // dest+crypto_box_NONCEBYTES;

    // increment nonce
    dd_keys_nonce_increment(self->nonce, crypto_box_NONCEBYTES);
    memcpy(dest, self->nonce, crypto_box_NONCEBYTES);

    dest += crypto_box_NONCEBYTES;
    retval = crypto_box_easy_afternm(dest, (const unsigned char *) message, mlen,
                                     self->nonce, precalck);
    /* char *hex = calloc (1, 1000); */
    /* sodium_bin2hex (hex, 1000, ciphertext, enclen); */
    /* printf ("ciphertext size %d: %s\n", enclen, hex); */
    /* free (hex); */

    if (retval == 0) {
    } else {
        fprintf(stderr, "DD: Unable to encrypt %zu bytes!\n", mlen);
        free(ciphertext);
        return -1;
    }
    if (self->state == DD_STATE_REGISTERED) {
        zsock_send(self->socket, "bbbsb", &dd_version, 4, &dd_cmd_send, 4,
                   &self->cookie, sizeof(self->cookie), target, ciphertext, enclen);
    }
    free(ciphertext);
    return 0;
}

// ////////////////////////
// callbacks from zloop //
// ////////////////////////

static int s_ping(zloop_t *loop, int timerid, void *args) {
    dd_client_t *self = (dd_client_t *) args;
    if (self->state == DD_STATE_REGISTERED)
        zsock_send(self->socket, "bbb", &dd_version, 4, &dd_cmd_ping, 4,
                   &self->cookie, sizeof(self->cookie));
    return 0;
}

static int s_heartbeat(zloop_t *loop, int timerid, void *args) {
    dd_client_t *self = (dd_client_t *) args;
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
    dd_client_t *self = (dd_client_t *) args;
    if (self->state == DD_STATE_UNREG) {
        zsock_set_linger(self->socket, 0);
        zloop_reader_end(loop, self->socket);
        zsock_destroy(&self->socket);
        self->socket = zsock_new_dealer(NULL);
        if (!self->socket) {
            fprintf(stderr, "DD: Error in zsock_new_dealer: %s\n",
                    zmq_strerror(errno));
            free(self);
            return -1;
        }
        int rc = zsock_connect(self->socket, "%s", (const char *) self->endpoint);
        if (rc != 0) {
            fprintf(stderr, "DD: Error in zmq_connect: %s\n", zmq_strerror(errno));
            free(self);
            return -1;
        }
        zloop_reader(loop, self->socket, s_on_dealer_msg, self);
        zsock_send(self->socket, "bbs", &dd_version, 4, &dd_cmd_addlcl, 4,
                   (char *) dd_keys_hash(self->keys));
    }
    return 0;
}

// /////////////////////////////////////
// / callbacks for different messages //
// ////////////////////////////////////
static void cb_regok(dd_client_t *self, zmsg_t *msg, zloop_t *loop) {
    zframe_t *cookie_frame;
    cookie_frame = zmsg_pop(msg);
    if (cookie_frame == NULL) {
        fprintf(stderr, "DD: Misformed REGOK message, missing COOKIE!\n");
        return;
    }
    uint64_t *cookie2 = (uint64_t *) zframe_data(cookie_frame);
    self->cookie = *cookie2;
    zframe_destroy(&cookie_frame);
    self->state = DD_STATE_REGISTERED;
    zsock_send(self->socket, "bbb", &dd_version, 4, &dd_cmd_ping, 4,
               &self->cookie, sizeof(self->cookie));

    self->heartbeat_loop = zloop_timer(loop, 1500, 0, s_heartbeat, self);
    zloop_timer_end(loop, self->registration_loop);
    // if this is re-registration, we should try to subscribe again
    sublist_resubscribe(self);
    self->on_reg(self);
}

static void cb_pong(dd_client_t *self, zmsg_t *msg, zloop_t *loop) {
    zloop_timer(loop, 1500, 1, s_ping, self);
}

static void cb_chall(dd_client_t *self, zmsg_t *msg) {
    int retval = 0;
    zframe_t *encrypted = zmsg_first(msg);
    unsigned char *data = zframe_data(encrypted);
    size_t enclen = zframe_size(encrypted);
    unsigned char *decrypted = calloc(1, enclen);

    retval = crypto_box_open_easy_afternm(decrypted, data + crypto_box_NONCEBYTES,
                                          enclen - crypto_box_NONCEBYTES, data,
                                          dd_keys_ddboxk(self->keys));
    if (retval != 0) {
        fprintf(stderr, "Unable to decrypt CHALLENGE from broker\n");
        return;
    }

    zframe_t *temp_frame = zframe_new(decrypted, enclen - crypto_box_NONCEBYTES -
                                                 crypto_box_MACBYTES);
    zsock_send(self->socket, "bbfss", &dd_version, 4, &dd_cmd_challok, 4,
               temp_frame, dd_keys_hash(self->keys), self->client_name);
    zframe_destroy(&temp_frame);
    free(decrypted);
}

static void cb_data(dd_client_t *self, zmsg_t *msg) {
    int retval;
    char *source = zmsg_popstr(msg);
    /* printf("cb_data: S: %s\n", source); */
    zframe_t *encrypted = zmsg_first(msg);
    unsigned char *data = zframe_data(encrypted);
    size_t enclen = zframe_size(encrypted);
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
        self->on_data(source, decrypted,
                      enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES, self);
    } else {
        fprintf(stderr, "DD: Unable to decrypt %zu bytes from %s\n",
                enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES, source);
    }
    free(decrypted);
    free(source);
}

// up to the user to free the memory!
static void cb_pub(dd_client_t *self, zmsg_t *msg) {
    int retval;
    char *source = zmsg_popstr(msg);
    char *topic = zmsg_popstr(msg);
    zframe_t *encrypted = zmsg_first(msg);
    unsigned char *data = zframe_data(encrypted);
    size_t enclen = zframe_size(encrypted);

    size_t mlen = enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES;
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
        self->on_pub(source, topic, decrypted, mlen, self);
    } else {
        fprintf(stderr, "DD: Unable to decrypt %zu bytes from %s, topic %s\n", mlen,
                source, topic);
    }
    free(decrypted);
    free(topic);
    free(source);
}

static void cb_subok(dd_client_t *self, zmsg_t *msg) {
    char *topic = zmsg_popstr(msg);
    char *scope = zmsg_popstr(msg);
    sublist_activate(self, topic, scope);
    free(topic);
    free(scope);
}

static void cb_error(dd_client_t *self, zmsg_t *msg) {
    zframe_t *code_frame;
    code_frame = zmsg_pop(msg);
    if (code_frame == NULL) {
        fprintf(stderr, "DD: Misformed ERROR message, missing ERROR_CODE!\n");
        return;
    }

    int32_t *error_code = (int32_t *) zframe_data(code_frame);
    char *error_msg = zmsg_popstr(msg);
    self->on_error(*error_code, error_msg, self);
    zframe_destroy(&code_frame);
    free(error_msg);
}


static int s_on_dealer_msg(zloop_t *loop, zsock_t *handle, void *args) {
    dd_client_t *self = (dd_client_t *) args;
    self->timeout = 0;
    zmsg_t *msg = zmsg_recv(handle);

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

    if (*((uint32_t *) zframe_data(proto_frame)) != DD_PRO_VERSION) {
        fprintf(stderr, "DD: Wrong version, expected 0x%x, got 0x%x\n", DD_PRO_VERSION,
                *zframe_data(proto_frame));
        zframe_destroy(&proto_frame);
        zmsg_destroy(&msg);
        return 0;
    }
    zframe_t *cmd_frame = zmsg_pop(msg);
    uint32_t cmd = *((uint32_t *) zframe_data(cmd_frame));
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
            fprintf(stderr, "DD: Got command DD_CMD_CHALLOK\n");
            break;
        case DD_CMD_PUB:
            cb_pub(self, msg);
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
            cb_subok(self, msg);
            break;
        default:
            fprintf(stderr, "DD: Unknown command, value: 0x%x\n", cmd);
            break;
    }
    zframe_destroy(&proto_frame);
    zframe_destroy(&cmd_frame);
    zmsg_destroy(&msg);
    return 0;
}


void dd_client_destroy(dd_client_t **self_p) {
    assert(self_p);
    if (*self_p) {
        dd_client_t *self = *self_p;

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
            free((void *) self->keyfile);
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

void dd_client_add_pipe(dd_client_t *self, zsock_t *socket, zloop_reader_fn handler){

    self->pipe = socket;
    int rc = zloop_reader(self->loop, socket, handler, self);
    assert(rc != -1);
}

void *dd_client_thread(dd_client_t *self) {
    zloop_start(self->loop);
    return self;
}

dd_client_t *dd_client_setup(const char *client_name, const char *endpoint, const char *keyfile, dd_client_on_con con,
                             dd_client_on_discon discon, dd_client_on_data data, dd_client_on_pub pub,
                             dd_client_on_error error) {
// Make sure that ZMQ doesn't affect main process signal handling
    zsys_init();
    zsys_handler_reset();
    dd_client_t *self = malloc(sizeof(dd_client_t));

    self->client_name = (unsigned char *) strdup(client_name);
    self->endpoint = (unsigned char *) strdup(endpoint);
    self->keyfile = strdup(keyfile);
    self->timeout = 0;
    self->state = DD_STATE_UNREG;
    randombytes_buf(self->nonce, crypto_box_NONCEBYTES);
    self->on_reg = con;
    self->on_discon = discon;
    self->on_data = data;
    self->on_pub = pub;
    self->on_error = error;

    int rc;

    self->socket = zsock_new_dealer(NULL);
    if (!self->socket) {
        fprintf(stderr, "DD: Error in zsock_new_dealer: %s\n", zmq_strerror(errno));
        free(self);
        return NULL;
    }

    rc = zsock_connect(self->socket,  "%s", (const char *) self->endpoint);
    if (rc != 0) {
        fprintf(stderr, "DD: Error in zmq_connect: %s\n", zmq_strerror(errno));
        free(self);
        return NULL;
    }

    self->keys = dd_keys_new(self->keyfile);
    if (self->keys == NULL) {
        fprintf(stderr, "DD: Error reading keyfile!\n");
        return NULL;
    }

    self->sublist = sublist_new();

    self->loop = zloop_new();
    assert(self->loop);
    self->registration_loop = zloop_timer(self->loop, 1000, 0, s_ask_registration, self);
    rc = zloop_reader(self->loop, self->socket, s_on_dealer_msg, self);
    if (rc == -1) {
        fprintf(stderr, "DD: Error in zmq_connect: %s\n", zmq_strerror(errno));
        free(self);
        return NULL;
    }

    return self;
}

dd_client_t * dd_client_new(const char *client_name, const char *endpoint, const char *keyfile,
                            dd_client_on_con con,
                            dd_client_on_discon discon, dd_client_on_data data, dd_client_on_pub pub,
                            dd_client_on_error error) {

    dd_client_t *self = dd_client_setup(client_name, endpoint, keyfile, con, discon, data, pub, error);
    zthread_new((void *(*)(void *)) dd_client_thread, self);
    return self;
}

// TODO: move to dd_keys instead,  a to_string version perhaps?

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

//  --------------------------------------------------------------------------
//  Self test of this class
void test_on_reg(dd_client_t *dd) {
    zsys_info("Registered with broker %s!\n", dd_client_get_endpoint(dd));
}

void test_on_discon(dd_client_t *dd) {
    zsys_info("Got disconnected from broker %s!\n", dd_client_get_endpoint(dd));
}

void test_on_pub(const char *source, const char *topic, const byte *data, size_t length,
                 dd_client_t *dd) {
    zsys_info("PUB S: %s T: %s L: %zu D: '%s'", source, topic, length, data);
}

void test_on_data(const char *source, const byte *data, size_t length, dd_client_t *dd) {
    zsys_info("\nDATA S: %s L: %zu D: '%s'", source, length, data);
}

void test_on_error(int error_code, const char *error_message, dd_client_t *dd) {
    switch (error_code) {
        case DD_ERROR_NODST:
            zsys_error("No destination: %s\n", error_message);
            break;
        case DD_ERROR_REGFAIL:
            zsys_error("Registration failed: %s\n", error_message);
            break;
        case DD_ERROR_VERSION:
            zsys_error("Version: %s\n", error_message);
            break;
        default:
            zsys_error("Unknown error!\n");
            break;
    }
}

void
dd_client_test(bool verbose) {
    printf(" * dd_client: ");

    //  @selftest
    //  Simple create/destroy test
    dd_client_t *self = dd_client_new("testcli","tcp://localhost:5555","keys/public-keys.json",
                                      test_on_reg, test_on_discon,test_on_data,test_on_pub,test_on_error);

    assert (self);
    printf("sleeping .. 5s");
    sleep(5);
    dd_client_destroy (&self);
    //  @end
    printf("OK\n");
}


// subscription list stuff


// - compare two items, for sorting
// typedef int (czmq_comparator) (const void *item1, const void *item2);
static int s_sublist_cmp(const void *item1, const void *item2) {
    dd_topic_t *i1, *i2;
    i1 = (dd_topic_t *) item1;
    i2 = (dd_topic_t *) item2;
    return strcmp(dd_topic_get_topic(i1), dd_topic_get_topic(i2));
}

// -- destroy an item
// typedef void (czmq_destructor) (void **item);
static void s_sublist_free(void **item) {
    // TODO
    // There's a bug lurking here.. the new_top pointer is not the same
    // as the the one passed to s_sublist_free..

    dd_topic_t *i;
    i = (dd_topic_t *) *item;
    //printf("s_sublist_free called p: %p t: %p s: %p \n", i, i->topic, i->scope);
    dd_topic_destroy(&i);

}

// -- duplicate an item
// typedef void *(czmq_duplicator) (const void *item);
static void *s_sublist_dup(const void *item) {
    dd_topic_t *new, *old;
    old = (dd_topic_t *) item;
    new = dd_topic_new();
    dd_topic_set_topic(new, strdup(dd_topic_get_topic(old)));
    dd_topic_set_scope(new, strdup(dd_topic_get_scope(old)));
    dd_topic_set_active(new, dd_topic_get_active(old));
    return new;
}

zlistx_t *sublist_new() {
    zlistx_t *n = zlistx_new();
    zlistx_set_destructor(n, s_sublist_free);
    zlistx_set_duplicator(n, s_sublist_dup);
    zlistx_set_comparator(n, s_sublist_cmp);
    return n;
}

void sublist_destroy(zlistx_t **self_p) {
    zlistx_destroy(self_p);
    *self_p = NULL;
}

// update or add topic/scope/active to list
void sublist_add(dd_client_t *self, char *topic, char *scope, char active) {
    dd_topic_t *item;
    int found = 0;

    while ((item = zlistx_next((zlistx_t *) dd_client_get_subscriptions(self)))) {
        if (streq(dd_topic_get_topic(item), topic) && streq(dd_topic_get_scope(item), scope)) {
            dd_topic_set_active(item, active);
            found = 1;
        }
    }

    // Otherwise, add new
    if (!found) {

        dd_topic_t *new_top = dd_topic_new();

        // TODO
        // There's a bug lurking here.. the new_top pointer is not the same
        // as the the one passed to s_sublist_free..
        /* printf("sublist_add, new %p (%d) t: %p s %p \n", new_top,
         * sizeof(dd_topic_t), */
        /*        topic, scope); */
        dd_topic_set_topic(new_top, topic);
        dd_topic_set_scope(new_top, scope);
        dd_topic_set_active(new_top, active);

        zlistx_add_start((zlistx_t *) dd_client_get_subscriptions(self), new_top);
    }
}

void sublist_delete_topic(dd_client_t *self, char *topic) {
    dd_topic_t *item = zlistx_first((zlistx_t *) dd_client_get_subscriptions(self));
    do {
        if (streq(dd_topic_get_topic(item), topic)) {
            zlistx_delete((zlistx_t *) dd_client_get_subscriptions(self), item);
        }
    } while ((item = zlistx_next((zlistx_t *) dd_client_get_subscriptions(self))));
}

void sublist_activate(dd_client_t *self, char *topic, char *scope) {
    dd_topic_t *item;
    while ((item = zlistx_next((zlistx_t *) dd_client_get_subscriptions(self)))) {
        if (strcmp(dd_topic_get_topic(item), topic) == 0 && strcmp(dd_topic_get_scope(item), scope) == 0) {
            dd_topic_set_active(item, 1);
        }
    }
}

void sublist_deactivate_all(dd_client_t *self) {
    dd_topic_t *item;
    while ((item = zlistx_next((zlistx_t *) dd_client_get_subscriptions(self)))) {
        dd_topic_set_active(item, 0);
    }
}

// TODO: This should be changed to return a copy of the sublist
// So that library clients can use it
void sublist_print(dd_client_t *self) {
    dd_topic_t *item;
    while ((item = zlistx_next((zlistx_t *) dd_client_get_subscriptions(self)))) {
        printf("Topic: %s Scope: %s Active: %d\n", dd_topic_get_topic(item), dd_topic_get_scope(item),
               dd_topic_get_active(item));
    }
}

int sublist_delete(dd_client_t *self, char *topic, char *scope) {
    dd_topic_t *del = dd_topic_new();
    dd_topic_set_topic(del, topic);
    dd_topic_set_scope(del, scope);

    dd_topic_t *item = zlistx_find((zlistx_t *) dd_client_get_subscriptions(self), del);
    dd_topic_destroy(&del);
    if (item)
        return zlistx_delete((zlistx_t *) dd_client_get_subscriptions(self), item);
    return -1;
}
