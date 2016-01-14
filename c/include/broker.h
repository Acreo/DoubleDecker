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
#include <czmq.h>
#include <stdint.h>
#include "ddhtable.h"


// particular message types
void cmd_cb_addlcl(zframe_t *source, zmsg_t *msg);
void cmd_cb_adddcl(zframe_t *sockid, zframe_t *cookie_frame, zmsg_t *msg);
void cmd_cb_addbr(zframe_t *source, zmsg_t *msg);
void cmd_cb_unreg_cli(zframe_t *source_frame, zframe_t *cookie_frame,
                zmsg_t *msg);
void cmd_cb_chall(zmsg_t *msg);
void cmd_cb_challok(zframe_t *source, zmsg_t *msg);
void cmd_cb_unreg_dist_cli(zframe_t *sockid, zframe_t *cookie_frame,
                zmsg_t *msg);
void cmd_cb_unreg_br(char *name, zmsg_t *msg);
void cmd_cb_forward(zframe_t *sockid, zframe_t *cookie_frame, zmsg_t *msg);

void cmd_cb_ping(zframe_t *source, zframe_t *cookie);
//void cmd_cb_nodst(zmsg_t *msg);
void cmd_cb_error(zmsg_t *msg);
void cmd_cb_regok(zmsg_t *msg);
void cmd_cb_send(zframe_t *source, zframe_t *cookie, zmsg_t *msg);
// socket callbacks
int s_on_subS_msg(zloop_t *loop, zsock_t *handle, void *arg);
int s_on_router_msg(zloop_t *loop, zsock_t *handle, void *arg);
int s_on_dealer_msg(zloop_t *loop, zsock_t *handle, void *arg);
int s_on_pubN_msg(zloop_t *loop, zsock_t *handle, void *arg);
int s_on_subN_msg(zloop_t *loop, zsock_t *handle, void *arg);
int s_on_pubS_msg(zloop_t *loop, zsock_t *handle, void *arg);

// timer callbacks
int s_register(zloop_t *loop, int timer_id, void *arg);
int s_heartbeat(zloop_t *loop, int timer_id, void *arg);
int s_check_cli_timeout(zloop_t *loop, int timer_fd, void *arg);
int s_check_br_timeout(zloop_t *loop, int timer_fd, void *arg);
// other stuff
zmsg_t *add_me(zmsg_t *msg);
int already_passed(zmsg_t *msg);
void add_cli_up(char *prefix_name, int distance);
void del_cli_up(char *prefix_name);
void forward_locally(zframe_t *dest_sockid, char *src_string, zmsg_t *msg);
void forward_down(char *, char *, zframe_t *, zmsg_t *);
void forward_up(char *source, char *dest, zmsg_t *msg);
void dest_invalid(zframe_t *source, zframe_t *cookie, char *dest);
void unreg_cli(zframe_t *source, uint64_t cookie);
void unreg_broker(local_broker *np);
void connect_pubsubN(char *puburl, char *suburl);
void stop_program(int sig);
void usage(void);
extern int verbose;
void start_pubsub(char *rstr, int port);
int start_broker(char *, char *, char *, int, char *);
char *str_replace(const char *string, const char *substr,
                const char *replacement);

#endif
