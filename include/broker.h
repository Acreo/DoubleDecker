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
   */
/* broker.h ---
 *
 * Filename: broker.h
 * Description:
 * Author: Pontus Sköldström <ponsko@acreo.se>
 * Created: fre mar 13 17:22:02 2015 (+0100)
 * Last-Updated:
 *           By:
 *
 */
#ifndef _BROKER_H_
#define _BROKER_H_
#include "dd_classes.h"
#include "keys.h"
struct _dd_broker_t {
  // Connection strings
  char *broker_scope;
  char *dealer_connect;
  char *router_bind;
  char *reststr;
  char *pub_bind;
  char *pub_connect;
  char *sub_bind;
  char *sub_connect;

  char *logfile;
  char *syslog_enabled;


  int state, timeout;

  // Timer IDs
  int br_timeout_loop, cli_timeout_loop, heartbeat_loop, reg_loop;

  // Tries
  struct nn_trie topics_trie;

  // Broker Identity, assigned by higher broker
  zframe_t *broker_id;
  zframe_t *broker_id_null;

  // Lists
  zlist_t *scope;
  zlist_t *rstrings;
  zlist_t *pub_strings, *sub_strings;

  // main loop
  zloop_t *loop;

  // Sockets
  zsock_t *pubN;
  zsock_t *subN;
  zsock_t *pubS;
  zsock_t *subS;
  zsock_t *rsock;
  zsock_t *dsock;
  zsock_t *http;

  // Hash tables
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

  // keys and crypto
  char nonce[crypto_box_NONCEBYTES];
  ddbrokerkeys_t *keys;

  // Logging
  FILE *logfp;
};
typedef struct _lcl_broker local_broker;
typedef struct _dist_node dist_client;
typedef struct _lcl_node local_client;
typedef struct _subscription_node subscribe_node;

void del_cli_up(dd_broker_t *self, char *prefix_name);
void add_cli_up(dd_broker_t *self, char *prefix_name, int distancoe);

void forward_locally(dd_broker_t *self, zframe_t *dest_sockid, char *src_string,
                     zmsg_t *msg);

void forward_down(dd_broker_t *self, char *src_string, char *dst_string,
                  zframe_t *br_sockid, zmsg_t *msg);
void forward_up(dd_broker_t *self, char *src_string, char *dst_string,
                zmsg_t *msg);

void dest_invalid_rsock(dd_broker_t *self, zframe_t *sockid, char *src_string,
                        char *dst_string);
void dest_invalid_dsock(dd_broker_t *self, char *src_string, char *dst_string);
void connect_pubsubN(dd_broker_t *self);
void unreg_cli(dd_broker_t *self, zframe_t *sockid, uint64_t cookie);
void unreg_broker(dd_broker_t *self, local_broker *np);
void bind_router(dd_broker_t *self);

#endif

