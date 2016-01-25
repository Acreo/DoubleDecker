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
#include <urcu.h>
#include <zmq.h>
#include <czmq.h>
#include "dd.h"
#include "../include/broker.h"
#include "sodium.h"
#include "../include/ddhtable.h"
#include "../include/ddlog.h"
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <err.h>
#include "../include/trie.h"

char *broker_scope;

char *dealer_connect = NULL;
char *router_bind = "tcp://*:5555";
char *monitor_name = NULL;
char *pub_bind = NULL, *pub_connect = NULL;
char *sub_bind = NULL, *sub_connect = NULL;
char nonce[crypto_box_NONCEBYTES];
ddbrokerkeys_t *keys;

// timer IDs
int br_timeout_loop, cli_timeout_loop, heartbeat_loop, reg_loop;

int state = DD_STATE_UNREG, timeout = 0, verbose = 0;
struct nn_trie topics_trie;
// Broker Identity, assigned by higher broker
zframe_t *broker_id = NULL, *broker_id_null;
zlist_t *scope;
zloop_t *loop;
zsock_t *pubN = NULL, *subN = NULL;
zsock_t *pubS = NULL, *subS = NULL;
zsock_t *rsock = NULL, *dsock = NULL, *msock = NULL;

void print_ddbrokerkeys(ddbrokerkeys_t *keys);
void dest_invalid_rsock(zframe_t *sockid, char *src_string,
                char *dst_string);
void dest_invalid_dsock(char *src_string, char *dst_string);

int loglevel = DD_LOG_INFO;
void handler(int sig) {
        void *array[10];
        size_t size;

        // get void*'s for all entries on the stack
        size = backtrace(array, 10);

        // print out all the frames to stderr
        dd_error("Error: signal %d:\n", sig);
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(EXIT_FAILURE);
}

int is_int(char *s) {
        while (*s) {
                if (isdigit(*s++) == 0)
                        return 0;
        }

        return 1;
}

void delete_dist_clients(local_broker *br) {
        struct cds_lfht_iter iter;
        dist_client *mp;

        cds_lfht_first(dist_cli_ht, &iter);
        struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
        dd_debug("delete_dist_clients:");
        //  zframe_print(br->sockid, "broker");
        while (ht_node != NULL) {
                mp = caa_container_of(ht_node, dist_client, node);
                dd_debug("Distclient %s", mp->name);
                //    zframe_print(mp->broker, "client");
                if (zframe_eq(mp->broker, br->sockid)) {
                        char buf[256] = "";
                        dd_debug("Was under missing broker %s",
                                        zframe_tostr(br->sockid, buf));
                        del_cli_up(mp->name);
                        rcu_read_lock();
                        int ret = cds_lfht_del(dist_cli_ht, ht_node);
                        rcu_read_unlock();
                }
                cds_lfht_next(dist_cli_ht, &iter);
                ht_node = cds_lfht_iter_get_node(&iter);
        }
}

/** Functions for handling incoming messages */

void cmd_cb_addbr(zframe_t *sockid, zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_addbr called");
        zframe_print(sockid, "sockid");
        zmsg_print(msg);
#endif
        char *hash = zmsg_popstr(msg);
        if (hash == NULL) {
                dd_error("Error, got ADDBR without hash!");
                return;
        }
        //  printf("comparing hash %s with keys->hash %s\n", hash, keys->hash);
        if (strcmp(hash, keys->hash) != 0) {
                // TODO send error
                dd_error("Error, got ADDBR with wrong hash!");
                return;
        }

        int enclen =
                sizeof(uint64_t) + crypto_box_NONCEBYTES + crypto_box_MACBYTES;
        unsigned char *dest = calloc(1, enclen);
        unsigned char *ciphertext = dest; // dest+crypto_box_NONCEBYTES;

        // increment nonce
        sodium_increment(nonce, crypto_box_NONCEBYTES);
        memcpy(dest, nonce, crypto_box_NONCEBYTES);

        dest += crypto_box_NONCEBYTES;
        int retval =
                crypto_box_easy_afternm(dest, (unsigned char *)&keys->cookie,
                                sizeof(keys->cookie), nonce, keys->ddboxk);

        retval = zsock_send(rsock, "fbbbf", sockid, &dd_version, 4,
                        &dd_cmd_chall, 4, ciphertext, enclen, sockid);
        if (retval != 0) {
                dd_error("Error sending challenge!");
        }
}

void cmd_cb_addlcl(zframe_t *sockid, zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_addlcl called");
        zframe_print(sockid, "sockid");
        zmsg_print(msg);
#endif

        char *hash = zmsg_popstr(msg);
        if (hash == NULL) {
                dd_error("Error, got ADDLCL without hash!");
                return;
        }
        ddtenant_t *ten;

        ten = zhash_lookup(keys->tenantkeys, hash);
        free(hash);
        if (ten == NULL) {
                dd_error("Could not find key for client");
                zsock_send(rsock, "fbbbs", sockid, &dd_version, 4, &dd_cmd_error, 4,
                                &dd_error_regfail, 4, "Authentication failed!");
                return;
        }

        int enclen =
                sizeof(uint64_t) + crypto_box_NONCEBYTES + crypto_box_MACBYTES;

        unsigned char *dest = calloc(1, enclen);
        unsigned char *ciphertext = dest; // dest+crypto_box_NONCEBYTES;

        // increment nonce
        sodium_increment(nonce, crypto_box_NONCEBYTES);
        memcpy(dest, nonce, crypto_box_NONCEBYTES);

        dest += crypto_box_NONCEBYTES;
        int retval =
                crypto_box_easy_afternm(dest, (unsigned char *)&ten->cookie,
                                sizeof(ten->cookie), nonce, ten->boxk);

        retval = zsock_send(rsock, "fbbb", sockid, &dd_version, 4, &dd_cmd_chall,
                        4, ciphertext, enclen);
        free(ciphertext);
        if (retval != 0) {
                dd_error("Error sending challenge!");
        }
}

void cmd_cb_adddcl(zframe_t *sockid, zframe_t *cookie_frame, zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_adddcl called");
        zframe_print(sockid, "sockid");
        zframe_print(cookie_frame, "cookie");
        zmsg_print(msg);
#endif
        uint64_t *cookie = (uint64_t *)zframe_data(cookie_frame);
        if (hashtable_has_local_broker(sockid, *cookie, 0) == NULL) {
                dd_debug("Got ADDDCL from unregistered broker...");
                return;
        }

        struct dist_node *dn;
        char *name = zmsg_popstr(msg);
        zframe_t *dist_frame = zmsg_pop(msg);
        int *dist = (int *)zframe_data(dist_frame);

        if (dn = hashtable_has_dist_node(name)) {
                add_cli_up(name, *dist);
                zframe_destroy(&dn->broker);
                dn->broker = sockid;
                free(name);
        } else {
                hashtable_insert_dist_node(name, sockid, *dist);
                dd_info(" + Dist client added: %s (%d)", name, *dist);
                // zframe_print(sockid, "brsockid");
                add_cli_up(name, *dist);
        }

        zframe_destroy(&dist_frame);
}

void cmd_cb_chall(zmsg_t *msg) {
        int retval = 0;
#ifdef DEBUG
        dd_debug("cmd_cb_chall called");
        zmsg_print(msg);
#endif
        zframe_t *encrypted = zmsg_pop(msg);
        if (broker_id)
                zframe_destroy(&broker_id);
        broker_id = zmsg_pop(msg);
        unsigned char *data = zframe_data(encrypted);

        int enclen = zframe_size(encrypted);
        unsigned char *decrypted = calloc(1, enclen);

        retval = crypto_box_open_easy_afternm(
                        decrypted, data + crypto_box_NONCEBYTES,
                        enclen - crypto_box_NONCEBYTES, data, keys->ddboxk);
        if (retval != 0) {
                dd_error("Unable to decrypt CHALLENGE from broker");
                goto cleanup;
        }

        zsock_send(dsock, "bbfss", &dd_version, 4, &dd_cmd_challok, 4,
                        zframe_new(decrypted, enclen - crypto_box_NONCEBYTES -
                                crypto_box_MACBYTES),
                        keys->hash, "broker");
cleanup:

        zframe_destroy(&encrypted);
        free(decrypted);
}

void cmd_cb_challok(zframe_t *sockid, zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_challok called");
        zframe_print(sockid, "sockid");
        zmsg_print(msg);
#endif

        int retval;
        zframe_t *cook = zmsg_pop(msg);
        uint64_t *cookie = (uint64_t *)zframe_data(cook);
        char *hash = zmsg_popstr(msg);
        char *client_name = zmsg_popstr(msg);

        if (cook == NULL || cookie == NULL || hash == NULL ||
                        client_name == NULL) {

                dd_error("DD_CMD_CHALLOK: misformed message!");
                goto cleanup;
        }
        // broker <-> broker authentication
        if (strcmp(hash, keys->hash) == 0) {
                if (keys->cookie != *cookie) {
                        dd_warning("DD_CHALL_OK: authentication error!");
                        // TODO: send error message
                        goto cleanup;
                }
                dd_debug("Authentication of broker %s successful!", client_name);
                if (NULL == hashtable_has_local_broker(sockid, *cookie, 0)) {
                        hashtable_insert_local_broker(sockid, *cookie);

                        const char *pubs_endpoint = zsock_endpoint(pubS);
                        const char *subs_endpoint = zsock_endpoint(subS);

                        dd_info("pubs_endpoint %s", pubs_endpoint);
                        dd_info("subs_endpoint %s", subs_endpoint);

                        zsock_send(rsock, "fbbbss", sockid, &dd_version, 4, &dd_cmd_regok, 4,
                                        &keys->cookie, sizeof(keys->cookie), pubs_endpoint,
                                        subs_endpoint);
                        char buf[256];
                        dd_info(" + Added broker: %s", zframe_tostr(sockid, buf));
                        goto cleanup;
                }
                goto cleanup;
        }
        // tenant <-> broker authentication
        ddtenant_t *ten;
        ten = zhash_lookup(keys->tenantkeys, hash);
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

        retval = insert_local_client(sockid, ten, client_name);
        if (retval == -1) {
                // TODO: send error message
                dd_error("DD_CMD_CHALLOK: Couldn't insert local client!");
                goto cleanup;
                return;
        }
        zsock_send(rsock, "fbbb", sockid, &dd_version, 4, &dd_cmd_regok, 4,
                        &ten->cookie, sizeof(ten->cookie));
        dd_info(" + Added local client: %s.%s", ten->name, client_name);
        char prefix_name[MAXTENANTNAME];
        int prelen = snprintf(prefix_name, 200, "%s.%s", ten->name, client_name);
        if (state != DD_STATE_ROOT)
                add_cli_up(prefix_name, 0);

cleanup:
        if (hash)
                free(hash);
        if (client_name)
                free(client_name);
        if (cook)
                zframe_destroy(&cook);
}

void cmd_cb_forward_dsock(zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_forward_dsock called");
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

        struct dist_node *dn;
        local_client *ln;

        if (ln = hashtable_has_rev_local_node(dst, 0)) {
                if ((srcpublic && !dstpublic) || (!srcpublic && dstpublic)) {
                        dd_debug("Forward_dsock, not stripping tenant %s", src);
                        forward_locally(ln->sockid, src, msg);
                } else {
                        dd_debug("Forward_dsock, stripping tenant %s", src);
                        char *dot = strchr(src, '.');
                        forward_locally(ln->sockid, dot + 1, msg);
                }
        } else if (dn = hashtable_has_dist_node(dst)) {
                forward_down(src, dst, dn->broker, msg);
        } else if (state == DD_STATE_ROOT) {
                dest_invalid_dsock(src, dst);
        } else {
                forward_up(src, dst, msg);
        }
        free(src);
        free(dst);
}

void cmd_cb_forward_rsock(zframe_t *sockid, zframe_t *cookie_frame,
                zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_forward_rsock called");
        zframe_print(sockid, "sockid");
        zframe_print(cookie_frame, "cookie");
        zmsg_print(msg);
#endif

        uint64_t *cookie;
        cookie = (uint64_t *)zframe_data(cookie_frame);
        if (!hashtable_has_local_broker(sockid, *cookie, 1)) {
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
        struct dist_node *dn;
        local_client *ln;
        if ((ln = hashtable_has_rev_local_node(dst_string, 0))) {
                if ((srcpublic && !dstpublic) || (!srcpublic && dstpublic)) {
                        dd_debug("Forward_rsock, not stripping tenant %s", src_string);
                        forward_locally(ln->sockid, src_string, msg);
                } else {
                        dd_debug("Forward_dsock, stripping tenant %s", src_string);
                        char *dot = strchr(src_string, '.');
                        forward_locally(ln->sockid, dot + 1, msg);
                }
        } else if (dn = hashtable_has_dist_node(dst_string)) {
                forward_down(src_string, dst_string, dn->broker, msg);
        } else if (state == DD_STATE_ROOT) {
                dest_invalid_rsock(sockid, src_string, dst_string);
        } else {
                forward_up(src_string, dst_string, msg);
        }
        free(src_string);
        free(dst_string);
}

/*
 * TODO: Add a lookup for dist_cli here as well!
 */
void cmd_cb_nodst_dsock(zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_nodst_dsock called");
        zmsg_print(msg);
#endif

        local_client *ln;
        char *dst_string = zmsg_popstr(msg);
        char *src_string = zmsg_popstr(msg);
        dd_debug("cmd_cb_nodst_dsock called!)");

        if ((ln = hashtable_has_rev_local_node(src_string, 0))) {
                zsock_send(rsock, "fbbbs", ln->sockid, &dd_version, 4, &dd_cmd_error, 4,
                                &dd_error_nodst, 4,dst_string);
        } else {
                dd_error("Could not forward NODST message downwards");
        }
}

void cmd_cb_nodst_rsock(zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_nodst_rsock called");
        zmsg_print(msg);
#endif
        dd_error("cmd_cb_nodst_rsock called, not implemented!");
}

void cmd_cb_pub(zframe_t *sockid, zframe_t *cookie, zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_pub called");
        zframe_print(sockid, "sockid");
        zframe_print(cookie, "cookie");
        zmsg_print(msg);
#endif

        char *topic = zmsg_popstr(msg);
        zframe_t *pathv = zmsg_pop(msg);

        local_client *ln;
        ln = hashtable_has_local_node(sockid, cookie, 1);
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
                snprintf(&newtopic[0], 256, "%s%s", topic, broker_scope);
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

        if (pubN) {
                dd_debug("publishing north %s %s ", pubtopic, name);
                zsock_send(pubN, "ssfm", pubtopic, name, broker_id, msg);
        }

        if (pubS) {
                dd_debug("publishing south %s %s", pubtopic, name);

                zsock_send(pubS, "ssfm", pubtopic, name, broker_id_null, msg);
        }

        zlist_t *socks = nn_trie_tree(&topics_trie, pubtopic, strlen(pubtopic));

        if (socks != NULL) {
                zframe_t *s = zlist_first(socks);
                dd_debug("Local sockids to send to: ");
                while (s) {
                        print_zframe(s);
                        zsock_send(rsock, "fbbssm", s, &dd_version, 4, &dd_cmd_pub, 4, name,
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

void cmd_cb_ping(zframe_t *sockid, zframe_t *cookie) {
#ifdef DEBUG
        dd_debug("cmd_cb_ping called");
        zframe_print(sockid, "sockid");
        zframe_print(cookie, "cookie");
        zmsg_print(msg);
#endif

        if (hashtable_has_local_node(sockid, cookie, 1)) {
                zsock_send(rsock, "fbb", sockid, &dd_version, 4, &dd_cmd_pong, 4);
                return;
        }

        uint64_t *cook;
        cook = (uint64_t *)zframe_data(cookie);
        if (hashtable_has_local_broker(sockid, *cook, 1)) {
                zsock_send(rsock, "fbb", sockid, &dd_version, 4, &dd_cmd_pong, 4);
                return;
        }
        dd_warning("Ping from unregistered client/broker: ");
        zframe_print(sockid, NULL);
}

void cmd_cb_regok(zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_regok called");
        zmsg_print(msg);
#endif

        state = DD_STATE_REGISTERED;

        // stop trying to register
        zloop_timer_end(loop, reg_loop);
        heartbeat_loop = zloop_timer(loop, 1000, 0, s_heartbeat, dsock);

        // iterate through local clients and add_cli_up to transmit to next
        // broker
        struct cds_lfht_iter iter;
        local_client *np;
        rcu_read_lock();
        cds_lfht_first(lcl_cli_ht, &iter);
        struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
        while (ht_node != NULL) {
                rcu_read_unlock();
                np = caa_container_of(ht_node, local_client, lcl_node);
                dd_debug("Registering, found local client: %s", np->name);
                add_cli_up(np->prefix_name, 0);
                rcu_read_lock();
                cds_lfht_next(lcl_cli_ht, &iter);
                ht_node = cds_lfht_iter_get_node(&iter);
        };
        rcu_read_unlock();
        // iterate through dist clients and add_cli_up to transmit to next
        // broker

        struct dist_node *nd;
        rcu_read_lock();
        cds_lfht_first(dist_cli_ht, &iter);
        ht_node = cds_lfht_iter_get_node(&iter);
        while (ht_node != NULL) {
                rcu_read_unlock();
                nd = caa_container_of(ht_node, struct dist_node, node);
                dd_debug("Registering, found distant client: %s", nd->name);
                add_cli_up(nd->name, nd->distance);
                rcu_read_lock();
                cds_lfht_next(dist_cli_ht, &iter);
                ht_node = cds_lfht_iter_get_node(&iter);
        };
        rcu_read_unlock();

        if (3 == zmsg_size(msg)) {
                zframe_t *cook = zmsg_pop(msg);
                char *puburl = zmsg_popstr(msg);
                char *suburl = zmsg_popstr(msg);
                connect_pubsubN(puburl, suburl);
                free(puburl);
                free(suburl);
                zframe_destroy(&cook);
        } else {
                dd_warning("No PUB/SUB interface configured");
        }
}

void cmd_cb_send(zframe_t *sockid, zframe_t *cookie, zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_send called");
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
        ln = hashtable_has_local_node(sockid, cookie, 1);
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

        dd_debug("cmd_cb_send, srcpublic %d, dstpublic %d", srcpublic,
                        dstpublic);

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
                        k = zlist_first(keys->tenants);
                        while (k) {
                                if (strncmp(dest, k, strlen(k)) == 0) {
                                        dd_debug("found matching tenant: %s, not adding prefix!", k);
                                        add_prefix = 0;
                                        break;
                                }
                                k = zlist_next(keys->tenants);
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

        dd_debug("cmd_cb_send: src \"%s\", dst \"%s\"", src_string, dst_string);

        struct dist_node *dn;
        if ((ln = hashtable_has_rev_local_node(dst_string, 0))) {
                if ((!srcpublic && !dstpublic) || (srcpublic && dstpublic)) {
                        char *dot = strchr(src_string, '.');
                        forward_locally(ln->sockid, dot + 1, msg);
                } else {
                        forward_locally(ln->sockid, src_string, msg);
                }
        } else if (dn = hashtable_has_dist_node(dst_string)) {
                dd_debug("calling forward down");
                forward_down(src_string, dst_string, dn->broker, msg);
        } else if (state == DD_STATE_ROOT) {
                if ((!srcpublic && !dstpublic) || (srcpublic && dstpublic)) {
                        char *src_dot = strchr(src_string, '.');
                        char *dst_dot = strchr(dst_string, '.');
                        dest_invalid_rsock(sockid, src_dot + 1, dst_dot + 1);
                } else {
                        dest_invalid_rsock(sockid, src_string, dst_string);
                }
        } else {
                forward_up(src_string, dst_string, msg);
        }

        free(dest);
}

void cmd_cb_sub(zframe_t *sockid, zframe_t *cookie, zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_sub called");
        zframe_print(sockid, "sockid");
        zframe_print(cookie, "cookie");
        zmsg_print(msg);
#endif

        char *topic = zmsg_popstr(msg);
        char *scopestr = zmsg_popstr(msg);

        local_client *ln;
        ln = hashtable_has_local_node(sockid, cookie, 1);
        if (!ln) {
                dd_warning("DD: Unregistered client trying to send!");
                free(topic);
                free(scopestr);
                return;
        }

        if (strcmp(topic, "public") == 0) {
                zsock_send(rsock, "fbbss", sockid, &dd_version, 4, &dd_cmd_data, 4,
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
        char *t = zlist_first(scope);
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
                                t = zlist_next(scope);
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
        zsock_send(rsock, "fbbss", sockid, &dd_version, 4, &dd_cmd_subok, 4,
                        topic, scopedup);
        free(scopedup);

        retval = snprintf(ntptr, 256, "%s.%s%s", ln->tenant, topic,
                        (char *)&newscope[0]);
        dd_info("newtopic = %s, len = %d\n", ntptr, retval);

        int new = 0;
        // Hashtable
        // subscriptions[sockid(5byte array)] = [topic,topic,topic]
        retval = insert_subscription(sockid, ntptr);

        dd_info("insert_subscription() returned %d", retval);
        if (retval != 0)
                new += 1;

        print_sub_ht();

        // Trie
        // topics_trie[newtopic(char*)] = [sockid, sockid, sockid]
        retval =
                nn_trie_subscribe(&topics_trie, ntptr, strlen(ntptr), sockid, 1);
        // doesn't really matter
        if (retval == 0) {
                dd_info("topic %s already in trie!", ntptr);
        } else if (retval == 1) {
                dd_info("new topic %s", ntptr);
        } else if (retval == 2) {
                dd_info("inserted new sockid on topic %s", ntptr);
        }

        free(scopestr);
        free(topic);
#ifdef DEBUG
        nn_trie_dump(&topics_trie);
#endif
        // refcount -> integrate in the topic_trie as refcount_s and refcount_n
        // topic_north[newtopic(char*)] = int
        // topic_south[newtopic(char*)] = int

        if (retval != 2)
                return;

        // add subscription to the north and south sub sockets
        newtopic[0] = 1;
        ntptr = &newtopic[0];
        if (subN) {
                dd_debug("adding subscription for %s to north SUB", &newtopic[1]);
                retval = zsock_send(subN, "b", &newtopic[0], 1 + strlen(&newtopic[1]));
        }
        if (subS) {
                dd_debug("adding subscription for %s to south SUB", &newtopic[1]);
                retval = zsock_send(subS, "b", &newtopic[0], 1 + strlen(&newtopic[1]));
        }
}

/*
 * TODO fix this
 */
void cmd_cb_unreg_br(char *name, zmsg_t *msg) {
        dd_debug("cmd_cb_unreg_br called, not implemented");
        /*
         * tmp_to_del = [] print('unregistering', name) for cli in
         * self.dist_cli: print(cli, self.dist_cli[cli][0]) if
         * self.dist_cli[cli][0] == name: tmp_to_del.append(cli)
         *
         * for i in tmp_to_del: self.unreg_dist_cli(name, [i])
         * self.local_br.pop(name)
         */
}

void cmd_cb_unreg_cli(zframe_t *sockid, zframe_t *cookie, zmsg_t *msg) {

#ifdef DEBUG
        dd_debug("cmd_cb_unreg_cli called");
        zframe_print(sockid, "sockid");
        zframe_print(cookie, "cookie");
        zmsg_print(msg);
#endif

        local_client *ln;
        if ((ln = hashtable_has_local_node(sockid, cookie, 0))) {
                dd_info(" - Removed local client: %s", ln->prefix_name);
                del_cli_up(ln->prefix_name);
                int a = remove_subscriptions(sockid);
                dd_info("   - Removed %d subscriptions", a);
                hashtable_unlink_local_node(ln->sockid, ln->cookie);
                hashtable_unlink_rev_local_node(ln->prefix_name);
                zframe_destroy(&ln->sockid);
                free(ln->prefix_name);
                free(ln->name);
                free(ln);
                print_local_ht();
        } else {
                dd_warning("Request to remove unknown client");
        }
}

void cmd_cb_unreg_dist_cli(zframe_t *sockid, zframe_t *cookie_frame,
                zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_unreg_dist_cli called");
        zframe_print(sockid, "sockid");
        zframe_print(cookie, "cookie");
        zmsg_print(msg);
#endif

        uint64_t *cook = (uint64_t *)zframe_data(cookie_frame);
        if (!hashtable_has_local_broker(sockid, *cook, 0)) {
                dd_error("Unregistered broker trying to remove clients!");
                return;
        }

        struct dist_node *dn;
        char *name = zmsg_popstr(msg);
        dd_debug("trying to remove distant client: %s", name);

        if (dn = hashtable_has_dist_node(name)) {
                dd_info(" - Removed distant client: %s", name);
                hashtable_remove_dist_node(name);
                del_cli_up(name);
        }
        free(name);
}

void cmd_cb_unsub(zframe_t *sockid, zframe_t *cookie, zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("cmd_cb_unsub called");
        zframe_print(sockid, "sockid");
        zframe_print(cookie, "cookie");
        zmsg_print(msg);
#endif

        char *topic = zmsg_popstr(msg);
        char *scopestr = zmsg_popstr(msg);

        local_client *ln;
        ln = hashtable_has_local_node(sockid, cookie, 1);
        if (!ln) {
                dd_warning("Unregistered client trying to send!\n");
                free(topic);
                free(scopestr);
                return;
        }

        if (strcmp(topic, "public") == 0) {
                zsock_send(rsock, "fbbss", sockid, &dd_version, 4, &dd_cmd_data, 4,
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
        char *t = zlist_first(scope);
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
                                t = zlist_next(scope);
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

        retval = snprintf(ntptr, 256, "%s.%s%s", ln->tenant, topic,
                        (char *)&newscope[0]);
        dd_info("deltopic = %s, len = %d\n", ntptr, retval);

        int new = 0;
        retval = remove_subscription(sockid, ntptr);

        // only delete a subscription if something was actually removed
        // from the trie and/or hashtable Otherwise multiple unsub from
        // a single client will f up for the  others

        if (retval == 0)
                return;

        newtopic[0] = 0;
        ntptr = &newtopic[0];
        if (subN) {
                dd_debug("deleting 1 subscription for %s to north SUB", &newtopic[1]);
                retval = zsock_send(subN, "b", &newtopic[0], 1 + strlen(&newtopic[1]));
        }
        if (subS) {
                dd_debug("deleting 1 subscription for %s to south SUB", &newtopic[1]);
                retval = zsock_send(subS, "b", &newtopic[0], 1 + strlen(&newtopic[1]));
        }
}

/* Functions called from zloop on timers or when message recieved */

int s_on_subN_msg(zloop_t *loop, zsock_t *handle, void *arg) {
        zmsg_t *msg = zmsg_recv(handle);

#ifdef DEBUG
        dd_debug("s_on_subN_msg called");
        zmsg_print(msg);
#endif

        char *pubtopic = zmsg_popstr(msg);
        char *name = zmsg_popstr(msg);
        zframe_t *pathv = zmsg_pop(msg);

        if (zframe_eq(pathv, broker_id)) {
                goto cleanup;
        }

        dd_debug("pubtopic: %s source: %s", pubtopic, name);
        // zframe_print(pathv, "pathv: ");
        zlist_t *socks = nn_trie_tree(&topics_trie, pubtopic, strlen(pubtopic));

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
                        zsock_send(rsock, "fbbssm", s, &dd_version, 4, &dd_cmd_pub, 4, name,
                                        dot, msg);
                        s = zlist_next(socks);
                }
                *slash = '/';
                zlist_destroy(&socks);
        } else {
                dd_debug("No matching nodes found by nn_trie_tree");
        }

        // If from north, only send south (only one reciever in the north)
        if (pubS)
                zsock_send(pubS, "ssfm", pubtopic, name, broker_id_null, msg);

cleanup:
        free(pubtopic);
        free(name);
        zframe_destroy(&pathv);
        zmsg_destroy(&msg);
        return 0;
}

int s_on_subS_msg(zloop_t *loop, zsock_t *handle, void *arg) {
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
        zlist_t *socks = nn_trie_tree(&topics_trie, pubtopic, strlen(pubtopic));

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
                        zsock_send(rsock, "fbbssm", s, &dd_version, 4, &dd_cmd_pub, 4, name,
                                        dot, msg);
                        s = zlist_next(socks);
                }
                *slash = '/';
                zlist_destroy(&socks);
        } else {
                dd_debug("No matching nodes found by nn_trie_tree");
        }

        // if from the south, send north & south, multiple recievers south
        if (pubN)
                zsock_send(pubN, "ssfm", pubtopic, name, broker_id, msg);
        if (pubS)
                zsock_send(pubS, "ssfm", pubtopic, name, pathv, msg);

cleanup:
        free(pubtopic);
        free(name);
        zframe_destroy(&pathv);
        zmsg_destroy(&msg);
        return 0;
}

int s_on_pubN_msg(zloop_t *loop, zsock_t *handle, void *arg) {
        zmsg_t *msg = zmsg_recv(handle);
#ifdef DEBUG
        dd_debug("s_on_pubN_msg called");
        zmsg_print(msg);
#endif

        zframe_t *topic_frame = zmsg_pop(msg);
        char *topic = (char *)zframe_data(topic_frame);

        if (topic[0] == 1) {
                dd_info(" + Got subscription for: %s", &topic[1]);
                nn_trie_add_sub_north(&topics_trie, &topic[1],
                                zframe_size(topic_frame) - 1);
        }
        if (topic[0] == 0) {
                dd_info(" - Got unsubscription for: %s", &topic[1]);
                nn_trie_del_sub_north(&topics_trie, &topic[1],
                                zframe_size(topic_frame) - 1);
        }

        // subs from north should continue down
        if (subS)
                zsock_send(subS, "f", topic_frame);

        zframe_destroy(&topic_frame);
        zmsg_destroy(&msg);
        return 0;
}

int s_on_pubS_msg(zloop_t *loop, zsock_t *handle, void *arg) {
        zmsg_t *msg = zmsg_recv(handle);
#ifdef DEBUG
        dd_debug("s_on_pubS_msg called");
        zmsg_print(msg);
#endif

        zframe_t *topic_frame = zmsg_pop(msg);
        char *topic = (char *)zframe_data(topic_frame);

        if (topic[0] == 1) {
                dd_info(" + Got subscription for: %s", &topic[1]);
                nn_trie_add_sub_south(&topics_trie, &topic[1],
                                zframe_size(topic_frame) - 1);
        }
        if (topic[0] == 0) {
                dd_info(" - Got unsubscription for: %s", &topic[1]);
                nn_trie_del_sub_south(&topics_trie, &topic[1],
                                zframe_size(topic_frame) - 1);
        }

        // subs from north should continue down
        if (subS)
                zsock_send(subS, "f", topic_frame);
        if (subN)
                zsock_send(subN, "f", topic_frame);

        zframe_destroy(&topic_frame);
        zmsg_destroy(&msg);
        return 0;
}

int s_on_monitor_msg(zloop_t *loop, zsock_t *handle, void *arg) {
        zmsg_t *msg = zmsg_recv(handle);
#ifdef DEBUG
        dd_debug("s_on_monitor_msg called");
        zmsg_print(msg);
#endif

        if (msg == NULL) {
                dd_error("zmsg_recv returned NULL");
                return 0;
        }

        if (zmsg_size(msg) < 1) {
                dd_error("monitor message less than 1, error!");
                zmsg_destroy(&msg);
                return 0;
        }
        char *cmd = zmsg_popstr(msg);

        if (strcmp("subs", cmd) == 0) {
                dd_debug("############## SUBSCRIPTIONS ###########");
                print_sub_ht();
                dd_debug("########################################");
        } else if (strcmp("trie", cmd) == 0) {
                dd_debug("############### TRIE ###################");
                nn_trie_dump(&topics_trie);
                dd_debug("########################################");
        } else if (strcmp("clients", cmd) == 0) {
                dd_debug("############## CLIENTS #################");
                print_local_ht();
                print_dist_ht();
                dd_debug("########################################");
        } else if (strcmp("brokers", cmd) == 0) {
                dd_debug("############## BROKERS #################");
                char buf[256];
                dd_debug("My broker id: %s", zframe_tostr(broker_id, buf));
                print_broker_ht();
                dd_debug("########################################");
        }

        zsock_send(msock, "z");
        free(cmd);
        zmsg_destroy(&msg);
}

int s_on_router_msg(zloop_t *loop, zsock_t *handle, void *arg) {
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
                zsock_send(rsock, "fbbbs", source_frame, pver, 4, &dd_cmd_error, 4, &dd_error_version, 4,
                                "Different versions in use");
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
                        cmd_cb_send(source_frame, cookie_frame, msg);
                        break;

                case DD_CMD_FORWARD:
                        cookie_frame = zmsg_pop(msg);
                        if (cookie_frame == NULL) {
                                dd_error("Malformed FORWARD, missing COOKIE");
                                goto cleanup;
                        }
                        cmd_cb_forward_rsock(source_frame, cookie_frame, msg);
                        break;

                case DD_CMD_PING:
                        cookie_frame = zmsg_pop(msg);
                        if (cookie_frame == NULL) {
                                dd_error("Malformed PING, missing COOKIE");
                                goto cleanup;
                        }
                        cmd_cb_ping(source_frame, cookie_frame);
                        break;

                case DD_CMD_SUB:
                        cookie_frame = zmsg_pop(msg);
                        if (cookie_frame == NULL) {
                                dd_error("Malformed SUB, missing COOKIE");
                                goto cleanup;
                        }
                        cmd_cb_sub(source_frame, cookie_frame, msg);
                        break;

                case DD_CMD_UNSUB:
                        cookie_frame = zmsg_pop(msg);
                        if (cookie_frame == NULL) {
                                dd_error("Malformed UNSUB, missing COOKIE");
                                goto cleanup;
                        }
                        cmd_cb_unsub(source_frame, cookie_frame, msg);
                        break;

                case DD_CMD_PUB:
                        cookie_frame = zmsg_pop(msg);
                        if (cookie_frame == NULL) {
                                dd_error("Malformed PUB, missing COOKIE");
                                goto cleanup;
                        }
                        cmd_cb_pub(source_frame, cookie_frame, msg);
                        break;

                case DD_CMD_ADDLCL:
                        cmd_cb_addlcl(source_frame, msg);
                        break;

                case DD_CMD_ADDDCL:
                        cookie_frame = zmsg_pop(msg);
                        if (cookie_frame == NULL) {
                                dd_error("Malformed ADDDCL, missing COOKIE");
                                goto cleanup;
                        }
                        cmd_cb_adddcl(source_frame, cookie_frame, msg);
                        break;

                case DD_CMD_ADDBR:
                        cmd_cb_addbr(source_frame, msg);
                        break;

                case DD_CMD_UNREG:
                        cookie_frame = zmsg_pop(msg);
                        if (cookie_frame == NULL) {
                                dd_error("Malformed ADDBR, missing COOKIE");
                                goto cleanup;
                        }
                        cmd_cb_unreg_cli(source_frame, cookie_frame, msg);
                        break;

                case DD_CMD_UNREGDCLI:
                        cookie_frame = zmsg_pop(msg);
                        if (cookie_frame == NULL) {
                                dd_error("Malformed UNREGDCLI, missing COOKIE");
                                goto cleanup;
                        }
                        cmd_cb_unreg_dist_cli(source_frame, cookie_frame, msg);
                        break;

                case DD_CMD_UNREGBR:
                        cmd_cb_unreg_br(NULL, msg);
                        break;

                case DD_CMD_CHALLOK:
                        cmd_cb_challok(source_frame, msg);
                        break;

                case DD_CMD_ERROR:
                        //TODO implment
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
        timeout = 0;
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
                        cmd_cb_regok(msg);
                        break;
                case DD_CMD_FORWARD:
                        cmd_cb_forward_dsock(msg);
                        break;
                case DD_CMD_CHALL:
                        cmd_cb_chall(msg);
                        break;
                case DD_CMD_PONG:
                        break;
                case DD_CMD_ERROR:
                        // TODO implement regfail and nodst
                        dd_error("Recived error from higher-layer broker, not implemented!");
                        break;
                default:
                        dd_error("Unknown command, value: 0x%x", cmd);
                        break;
        }
        zmsg_destroy(&msg);
        return 0;
}

int s_register(zloop_t *loop, int timer_id, void *arg) {
        dd_debug("trying to register..");

        if (state == DD_STATE_UNREG || state == DD_STATE_ROOT) {
                zsock_set_linger(dsock, 0);
                zloop_reader_end(loop, dsock);
                zsock_destroy((zsock_t **)&dsock);
                dsock = zsock_new_dealer(NULL);
                if (!dsock) {
                        dd_error("Error in zsock_new_dealer: %s", zmq_strerror(errno));
                        return -1;
                }

                int rc = zsock_connect(dsock, dealer_connect);
                if (rc != 0) {
                        dd_error("Error in zmq_connect: %s", zmq_strerror(errno));
                        return -1;
                }
                zloop_reader(loop, dsock, s_on_dealer_msg, NULL);

                zsock_send(dsock, "bbs", &dd_version, 4, &dd_cmd_addbr, 4, keys->hash);
        }
        return 0;
}

int s_heartbeat(zloop_t *loop, int timer_id, void *arg) {
        timeout += 1;
        zsock_t *socket = arg;
        if (timeout > 3) {
                state = DD_STATE_ROOT;
                zloop_timer_end(loop, heartbeat_loop);
                reg_loop = zloop_timer(loop, 1000, 0, s_register, dsock);
        }
        zsock_send(socket, "bbb", &dd_version, sizeof(dd_version), &dd_cmd_ping,
                        sizeof(dd_cmd_ping), &keys->cookie, sizeof(keys->cookie));
        return 0;
}

int s_check_cli_timeout(zloop_t *loop, int timer_fd, void *arg) {
        // iterate through local clients and check if they should time out
        struct cds_lfht_iter iter;
        local_client *np;
        rcu_read_lock();
        cds_lfht_first(lcl_cli_ht, &iter);
        struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
        while (ht_node != NULL) {
                rcu_read_unlock();
                np = caa_container_of(ht_node, local_client, lcl_node);
                if (np->timeout < 3) {
                        np->timeout += 1;
                } else {
                        dd_debug("deleting local client %s", np->prefix_name);
                        unreg_cli(np->sockid, np->cookie);
                }
                rcu_read_lock();
                cds_lfht_next(lcl_cli_ht, &iter);
                ht_node = cds_lfht_iter_get_node(&iter);
        };
        rcu_read_unlock();
}

int s_check_br_timeout(zloop_t *loop, int timer_fd, void *arg) {
        // iterate through local brokers and check if they should time out
        struct cds_lfht_iter iter;
        local_broker *np;
        rcu_read_lock();
        cds_lfht_first(lcl_br_ht, &iter);
        struct cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);
        rcu_read_unlock();
        while (ht_node != NULL) {
                np = caa_container_of(ht_node, local_broker, node);
                if (np->timeout < 3) {
                        np->timeout += 1;
                } else {
                        char buf[256];
                        dd_debug("Deleting local broker %s", zframe_tostr(np->sockid, buf));

                        delete_dist_clients(np);

                        rcu_read_lock();
                        int ret = cds_lfht_del(lcl_br_ht, ht_node);
                        rcu_read_unlock();
                        if (ret) {
                                dd_info(" - Local broker %s removed (concurrently)",
                                                zframe_tostr(np->sockid, buf));
                                free(np);
                        } else {
                                synchronize_rcu();
                                dd_info(" - Local broker %s removed",
                                                zframe_tostr(np->sockid, buf));
                                free(np);
                        }
                }
                rcu_read_lock();
                cds_lfht_next(lcl_cli_ht, &iter);
                ht_node = cds_lfht_iter_get_node(&iter);
                rcu_read_unlock();
        }
}

/* helper functions */

void add_cli_up(char *prefix_name, int distance) {
        if (state == DD_STATE_ROOT)
                return;

        dd_debug("add_cli_up(%s,%d), state = %d", prefix_name, distance, state);
        zsock_send(dsock, "bbbsb", &dd_version, 4, &dd_cmd_adddcl, 4,
                        &keys->cookie, sizeof(keys->cookie), prefix_name, &distance,
                        sizeof(distance));
}

void del_cli_up(char *prefix_name) {
        if (state != DD_STATE_ROOT) {
                dd_debug("del_cli_up %s", prefix_name);
                zsock_send(dsock, "bbbs", &dd_version, 4, &dd_cmd_unregdcli, 4,
                                &keys->cookie, sizeof(keys->cookie), prefix_name);
        }
}

void forward_locally(zframe_t *dest_sockid, char *src_string,
                zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("forward_locally: src: %s", src_string);
        zframe_print(dest_sockid, "dest_sockid");
        zmsg_print(msg);
#endif

        zsock_send(rsock, "fbbsm", dest_sockid, &dd_version, 4, &dd_cmd_data, 4,
                        src_string, msg);
}

void forward_down(char *src_string, char *dst_string, zframe_t *br_sockid,
                zmsg_t *msg) {
#ifdef DEBUG
        dd_info("Sending CMD_FORWARD to broker with sockid");
        print_zframe(br_sockid);
#endif
        zsock_send(rsock, "fbbssm", br_sockid, &dd_version, 4, &dd_cmd_forward,
                        4, src_string, dst_string, msg);
}

void forward_up(char *src_string, char *dst_string, zmsg_t *msg) {
#ifdef DEBUG
        dd_debug("forward_up called s: %s d: %s", src_string, dst_string);
        zmsg_print(msg);
#endif
        if (state == DD_STATE_REGISTERED)
                zsock_send(dsock, "bbbssm", &dd_version, 4, &dd_cmd_forward, 4,
                                &keys->cookie, sizeof(keys->cookie), src_string, dst_string,
                                msg);
}

void dest_invalid_rsock(zframe_t *sockid, char *src_string,
                char *dst_string) {
        zsock_send(rsock, "fbbbs", sockid, &dd_version, 4, &dd_cmd_error, 4,
                        &dd_error_nodst, 4, dst_string);
}

void dest_invalid_dsock(char *src_string, char *dst_string) {
        zsock_send(dsock, "bbss", &dd_version, 4, &dd_cmd_error, 4,
                        &dd_error_nodst, 4, dst_string);
}

void unreg_cli(zframe_t *sockid, uint64_t cookie) {
        zframe_t *cookie_frame = zframe_new(&cookie, sizeof cookie);
        cmd_cb_unreg_cli(sockid, cookie_frame, NULL);
        zframe_destroy(&cookie_frame);
}

void unreg_broker(local_broker *np) {
        dd_warning("unreg_broker called, unimplemented!\n");
}

void connect_pubsubN_tcp(char *puburl, char *suburl) {
        // extract
        int dealport = -1;
        int pubport = -1;
        int subport = -1;

        if (strstr(dealer_connect, "tcp")) {
                char *token, *string, *tofree;
                tofree = string = strdup(dealer_connect);
                assert(string != NULL);

                while ((token = strsep(&string, ":")) != NULL) {
                        sscanf(token, "%d", &dealport);
                }
                dd_debug("dealport found %d", dealport);
                free(tofree);
        }
        if (strstr(puburl, "tcp")) {
                char *token, *string, *tofree;
                tofree = string = strdup(puburl);
                assert(string != NULL);

                while ((token = strsep(&string, ":")) != NULL) {
                        sscanf(token, "%d", &pubport);
                }
                dd_debug("pubport found %d", pubport);
                free(tofree);
        }
        if (strstr(suburl, "tcp")) {
                char *token, *string, *tofree;
                tofree = string = strdup(suburl);
                assert(string != NULL);

                while ((token = strsep(&string, ":")) != NULL) {
                        sscanf(token, "%d", &subport);
                }
                dd_debug("subport found %d", subport);
                free(tofree);
        }
        char orig[10], prep[10], srep[10];
        sprintf(orig, "%d", dealport);
        sprintf(prep, "%d", pubport);
        sprintf(srep, "%d", subport);
        sub_connect = str_replace(dealer_connect, orig, prep);
        pub_connect = str_replace(dealer_connect, orig, srep);
}

void connect_pubsubN(char *puburl, char *suburl) {
        dd_debug("Connect pubsubN (%s, %s)", puburl, suburl);

        if (strstr(dealer_connect, "tcp")) {
                connect_pubsubN_tcp(puburl, suburl);
        }
        if (strstr(dealer_connect, "ipc")) {
                pub_connect = puburl;
                sub_connect = suburl;
        }

        dd_info("pub_connect: %s sub_connect: %s", pub_connect, sub_connect);
        pubN = zsock_new(ZMQ_XPUB);
        subN = zsock_new(ZMQ_XSUB);
        int rc = zsock_connect(pubN, pub_connect);
        if (rc < 0) {
                dd_error("Unable to connect pubN to %s", pub_connect);
                perror("Error: ");
                exit(EXIT_FAILURE);
        }

        rc = zsock_connect(subN, sub_connect);
        if (rc < 0) {
                dd_error("Unable to connect subN to %s", sub_connect);
                perror("Error: ");
                exit(EXIT_FAILURE);
        }
        rc = zloop_reader(loop, pubN, s_on_pubN_msg, NULL);
        assert(rc == 0);
        zloop_reader_set_tolerant(loop, pubN);

        rc = zloop_reader(loop, subN, s_on_subN_msg, NULL);
        assert(rc == 0);
        zloop_reader_set_tolerant(loop, subN);
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
                newstr =
                        malloc(strlen(oldstr) - strlen(substr) + strlen(replacement) + 1);
                /*
                 * failed to alloc mem, free old string and return NULL
                 */
                if (newstr == NULL) {
                        free(oldstr);
                        return NULL;
                }
                memcpy(newstr, oldstr, tok - oldstr);
                memcpy(newstr + (tok - oldstr), replacement, strlen(replacement));
                memcpy(newstr + (tok - oldstr) + strlen(replacement),
                                tok + strlen(substr),
                                strlen(oldstr) - strlen(substr) - (tok - oldstr));
                memset(newstr + strlen(oldstr) - strlen(substr) + strlen(replacement),
                                0, 1);
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

void stop_program(int sig) {
        dd_debug("Stop program called");
        zsock_destroy(&rsock);
        zsock_destroy(&dsock);
        zsock_destroy(&msock);
        zsock_destroy(&pubS);
        zsock_destroy(&pubN);
        zsock_destroy(&subS);
        zsock_destroy(&subN);

        exit(EXIT_SUCCESS);
}

void usage() {
        printf(
                        "broker -m <name> -d <dealer addr> -r <router addr> -v [verbose] -h "
                        "[help]\n"
                        "REQUIRED OPTIONS\n"
                        "-r <router addr> e.g. tcp://127.0.0.1:5555\n"
                        "   Multiple addresses with comma tcp://127.0.0.1:5555,ipc:///file\n"
                        "   Router is where clients connect\n"
                        "-k <keyfile>\n"
                        "   JSON file containing the broker keys\n"
                        "-s <scope>\n"
                        "   scope ~ \"1/2/3\"\n"
                        "OPTIONAL\n"
                        "-d <dealer addr> e.g tcp://1.2.3.4:5555\n"
                        "   Multiple addresses with comma tcp://127.0.0.1:5555,ipc:///file\n"
                        "   Dealer should be connected to Router of another broker\n"
                        "-l <loglevel> e:ERROR,w:WARNING,n:NOTICE,i:INFO,d:DEBUG,q:QUIET\n"
                        "-m <name> open a monitoring socket at /tmp/<name>.debug\n"
                        "-v [verbose]\n"
                        "-h [help]\n");
}

void start_pubsub_tcp(char *rstr, int port) {
        char orig[10], prep[10], srep[10];

        sprintf(orig, "%d", port);
        sprintf(prep, "%d", port + 1);
        sprintf(srep, "%d", port + 2);
        pub_bind = str_replace(rstr, orig, prep);
        sub_bind = str_replace(rstr, orig, srep);
        dd_debug("puburl: %s suburl: %s", pub_bind, sub_bind);

        pubS = zsock_new(ZMQ_XPUB);
        subS = zsock_new(ZMQ_XSUB);
        int rc = zsock_bind(pubS, pub_bind);
        if (rc < 0) {
                dd_error("Unable to bind pubS to %s", pub_bind);
                perror("Error: ");
                exit(EXIT_FAILURE);
        }
        rc = zsock_bind(subS, sub_bind);
        if (rc < 0) {
                dd_error("Unable to bind subS to %s", sub_bind);
                perror("Error: ");
                exit(EXIT_FAILURE);
        }

        rc = zloop_reader(loop, pubS, s_on_pubS_msg, NULL);
        assert(rc == 0);
        zloop_reader_set_tolerant(loop, pubS);

        rc = zloop_reader(loop, subS, s_on_subS_msg, NULL);
        assert(rc == 0);
        zloop_reader_set_tolerant(loop, subS);
}

int start_broker(char *router_bind, char *dealer_connect, char *keyfile,
                int verbose, char *monitor_name) {
        dd_info("Starting broker, router at %s, dealer at %s", router_bind,
                        dealer_connect);
        keys = read_ddbrokerkeys(keyfile);
        if (keys == NULL) {
                exit(EXIT_FAILURE);
        }

        broker_id = zframe_new("root", 4);
        broker_id_null = zframe_new("", 0);

        print_ddbrokerkeys(keys);
        randombytes_buf(nonce, crypto_box_NONCEBYTES);

        zframe_t *f;
        signal(SIGTERM, stop_program);
        signal(SIGINT, stop_program);
        // needs to be called for each thread using RCU lib
        rcu_register_thread();

        init_hashtables();

        loop = zloop_new();
        assert(loop);
        rsock = zsock_new(ZMQ_ROUTER);
        int rc = zsock_bind(rsock, router_bind);
        if (rc < 0) {
                dd_error("Couldn't bind router socket to %s", router_bind);
                perror("Error: ");
                exit(EXIT_FAILURE);
        }
        rc = zloop_reader(loop, rsock, s_on_router_msg, NULL);
        assert(rc == 0);
        zloop_reader_set_tolerant(loop, rsock);

        if (dealer_connect != NULL) {
                dsock = zsock_new(ZMQ_DEALER);
                zsock_connect(dsock, dealer_connect);
                if (dsock == NULL) {
                        dd_error("Couldn't connect dealer socket to %s", dealer_connect);
                        perror("Error: ");
                        exit(EXIT_FAILURE);
                }
                rc = zloop_reader(loop, dsock, s_on_dealer_msg, NULL);
                assert(rc == 0);
                zloop_reader_set_tolerant(loop, dsock);
                reg_loop = zloop_timer(loop, 1000, 0, s_register, dsock);
        } else {
                dd_info("No dealer defined, the broker will act as the root");
                state = DD_STATE_ROOT;
        }

        if (monitor_name) {
                msock = zsock_new(ZMQ_REP);
                char endpoint[512];
                snprintf(&endpoint[0], 512, "ipc:///tmp/%s.debug", monitor_name);
                zsock_bind(msock, &endpoint[0]);
                if (msock == NULL) {
                        dd_error("Couldn't connect monitor socket to %s", &endpoint[0]);
                        perror("Error: ");
                        exit(EXIT_FAILURE);
                }
                rc = zloop_reader(loop, msock, s_on_monitor_msg, NULL);
                assert(rc == 0);
        }

        cli_timeout_loop = zloop_timer(loop, 3000, 0, s_check_cli_timeout, NULL);
        br_timeout_loop = zloop_timer(loop, 1000, 0, s_check_br_timeout, NULL);

        /*
         * check if router_bind is tcp, if so, start pubS & subS
         */
        int port = 0;
        if (strstr(router_bind, "tcp")) {
                char *token, *string, *tofree;
                tofree = string = strdup(router_bind);
                assert(string != NULL);

                while ((token = strsep(&string, ":")) != NULL) {
                        sscanf(token, "%d", &port);
                }
                dd_debug("port found %d", port);
                free(tofree);
                if (port == 0) {
                        dd_info("couldn't find tcp port in router_bind %s", router_bind);
                        dd_info("wont start pub/sub");
                } else {
                        start_pubsub_tcp(router_bind, port);
                }
        }

        if (strstr(router_bind, "ipc")) {

                int len = strlen(router_bind) + strlen(".pub") + 1;
                pub_bind = malloc(len);
                sub_bind = malloc(len);
                snprintf(pub_bind, len, "%s.pub", router_bind);
                snprintf(sub_bind, len, "%s.pub", router_bind);

                pubS = zsock_new(ZMQ_XPUB);
                subS = zsock_new(ZMQ_XSUB);
                int rc = zsock_bind(pubS, pub_bind);
                if (rc < 0) {
                        dd_error("Unable to bind pubS to %s", pub_bind);
                        perror("Error: ");
                        exit(EXIT_FAILURE);
                }
                rc = zsock_bind(subS, sub_bind);
                if (rc < 0) {
                        dd_error("Unable to bind subS to %s", sub_bind);
                        perror("Error: ");
                        exit(EXIT_FAILURE);
                }

                rc = zloop_reader(loop, pubS, s_on_pubS_msg, NULL);
                assert(rc == 0);
                zloop_reader_set_tolerant(loop, pubS);

                rc = zloop_reader(loop, subS, s_on_subS_msg, NULL);
                assert(rc == 0);
                zloop_reader_set_tolerant(loop, subS);
        }

        zloop_start(loop);
        if (monitor_name)
                zsock_destroy(&msock);

        zsock_destroy(&dsock);
        zsock_destroy(&rsock);
        zsock_destroy(&pubS);
        zsock_destroy(&pubN);
        zsock_destroy(&subS);
        zsock_destroy(&subN);
}

int main(int argc, char **argv) {
        int c;
        char *keyfile = NULL;
        char *scopestr = NULL;
        void *ctx = zsys_init();
        zsys_set_logident("DD");
        nn_trie_init(&topics_trie);
        char *log = "i";
        opterr = 0;
        while ((c = getopt(argc, argv, "d:r:l:k:s:vhm:")) != -1)
                switch (c) {
                        case 'd':
                                dealer_connect = optarg;
                                break;
                        case 'r':
                                router_bind = optarg;
                                break;
                        case 'v':
                                verbose = 1;
                                break;
                        case 'h':
                                usage();
                                exit(EXIT_FAILURE);
                                break;
                        case 'k':
                                keyfile = optarg;
                                break;
                        case 'l':
                                log = optarg;
                                int i;
                                for (i = 0; log[i]; i++) {
                                        log[i] = tolower(log[i]);
                                }

                                break;
                        case 's':
                                scopestr = optarg;
                                break;
                        case 'm':
                                monitor_name = optarg;
                                break;
                        case '?':
                                if (optopt == 'c' || optopt == 's') {
                                        dd_error("Option -%c requires an argument.", optopt);
                                } else if (isprint(optopt)) {
                                        dd_error("Unknown option `-%c'.", optopt);
                                } else {
                                        dd_error("Unknown option character `\\x%x'.", optopt);
                                }
                                return 1;
                        default:
                                abort();
                }

        if (strncmp(log, "e", 1) == 0) {
                printf("setting loglevel error\n");
                loglevel = DD_LOG_ERROR;
        } else if (strncmp(log, "w", 1) == 0) {
                printf("setting loglevel WARNING\n");
                loglevel = DD_LOG_WARNING;
        } else if (strncmp(log, "n", 1) == 0) {
                printf("setting loglevel NOTICE\n");
                loglevel = DD_LOG_NOTICE;
        } else if (strncmp(log, "i", 1) == 0) {
                printf("setting loglevel INFO\n");
                loglevel = DD_LOG_INFO;
        } else if (strncmp(log, "d", 1) == 0) {
                printf("setting loglevel DEBUG\n");
                loglevel = DD_LOG_DEBUG;
        } else if (strncmp(log, "q", 1) == 0) {
                printf("setting loglevel NONE\n");
                loglevel = DD_LOG_NONE;
        }

        if (keyfile == NULL) {
                dd_error("Keyfile required (-k <file>>)\n");
                usage();
                exit(EXIT_FAILURE);
        }
        if (scopestr == NULL) {
                dd_error("Scope required (-s <scopestr>>)\n");
                usage();
                exit(EXIT_FAILURE);
        }

        scope = zlist_new();
        char *str1, *str2, *token, *subtoken;
        char *saveptr1, *saveptr2;
        int j;

        for (j = 1, str1 = scopestr;; j++, str1 = NULL) {
                token = strtok_r(str1, "/", &saveptr1);
                if (token == NULL)
                        break;
                if (!is_int(token)) {
                        dd_error("Only '/' and digits in scope, %s is not!", token);
                        exit(EXIT_FAILURE);
                }
                zlist_append(scope, token);
        }

        char brokerscope[256];
        broker_scope = &brokerscope[0];
        int len = 256;
        int retval = snprintf(broker_scope, len, "/");
        broker_scope += retval;
        len -= retval;

        char *t = zlist_first(scope);
        while (t != NULL) {
                retval = snprintf(broker_scope, len, "%s/", t);
                broker_scope += retval;
                len -= retval;
                t = zlist_next(scope);
        }
        broker_scope = &brokerscope[0];
        dd_debug("broker scope set to: %s", broker_scope);

        signal(SIGSEGV, handler); // install our handler
        signal(SIGINT, handler);  // install our handler
        signal(SIGABRT, handler); // install our handler

        start_broker(router_bind, dealer_connect, keyfile, verbose,
                        monitor_name);
}
