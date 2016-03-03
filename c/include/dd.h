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
/* dd.h ---
 *
 * Filename: dd.h
 * Description:
 * Author: Pontus Sköldström <ponsko@acreo.se>
 * Created: fre mar 13 17:22:02 2015 (+0100)
 * Last-Updated:
 *           By:
 *
 */
#ifndef _DD_H_
#define _DD_H_

#include <czmq.h>
#include <sodium.h>
#include "ddkeys.h"
// state definitions
#define DD_STATE_UNREG 1
#define DD_STATE_ROOT 2
#define DD_STATE_EXIT 3
#define DD_STATE_CHALLENGED 4
#define DD_STATE_REGISTERED 5

// Commands and version
#define DD_VERSION 0x0d0d0003
#define DD_CMD_SEND 0
#define DD_CMD_FORWARD 1
#define DD_CMD_PING 2
#define DD_CMD_ADDLCL 3
#define DD_CMD_ADDDCL 4
#define DD_CMD_ADDBR 5
#define DD_CMD_UNREG 6
#define DD_CMD_UNREGDCLI 7
#define DD_CMD_UNREGBR 8
#define DD_CMD_DATA 9
#define DD_CMD_ERROR 10
#define DD_CMD_REGOK 11
#define DD_CMD_PONG 12
#define DD_CMD_CHALL 13
#define DD_CMD_CHALLOK 14
#define DD_CMD_PUB 15
#define DD_CMD_SUB 16
#define DD_CMD_UNSUB 17
#define DD_CMD_SENDPUBLIC 18
#define DD_CMD_PUBPUBLIC 19
#define DD_CMD_SENDPT 20
#define DD_CMD_FORWARDPT 21
#define DD_CMD_DATAPT 22
#define DD_CMD_SUBOK 23

// Error codes
#define DD_ERROR_REGFAIL 1
#define DD_ERROR_NODST 2
#define DD_ERROR_VERSION 3

#define DD_CALLBACK 1
#define DD_ACTOR 2
extern const uint32_t dd_cmd_send;
extern const uint32_t dd_cmd_forward;
extern const uint32_t dd_cmd_ping;
extern const uint32_t dd_cmd_addlcl;
extern const uint32_t dd_cmd_adddcl;
extern const uint32_t dd_cmd_addbr;
extern const uint32_t dd_cmd_unreg;
extern const uint32_t dd_cmd_unregdcli;
extern const uint32_t dd_cmd_unregbr;
extern const uint32_t dd_cmd_data;
extern const uint32_t dd_cmd_error;
extern const uint32_t dd_cmd_regok;
extern const uint32_t dd_cmd_pong;
extern const uint32_t dd_cmd_chall;
extern const uint32_t dd_cmd_challok;
extern const uint32_t dd_cmd_pub;
extern const uint32_t dd_cmd_sub;
extern const uint32_t dd_cmd_unsub;
extern const uint32_t dd_cmd_sendpublic;
extern const uint32_t dd_cmd_pubpublic;
extern const uint32_t dd_cmd_sendpt;
extern const uint32_t dd_cmd_forwardpt;
extern const uint32_t dd_cmd_datapt;
extern const uint32_t dd_cmd_subok;

extern const uint32_t dd_version;

extern const uint32_t dd_error_regfail;
extern const uint32_t dd_error_nodst;
extern const uint32_t dd_error_version;

// On connection
typedef void(dd_con)(void *);
// On disconnection
typedef void(dd_discon)(void *);
// On recieve DATA
typedef void(dd_data)(char *, unsigned char *, int, void *);
// On recieve PUB
typedef void(dd_pub)(char *, char *, unsigned char *, int, void *);
// On receive ERROR
typedef void(dd_error)(int, char*, void *);

typedef struct ddclient {
        void *socket;               //  Socket for clients & workers
        int verbose;                //  Print activity to stdout
        unsigned char *endpoint;    //  Broker binds to this endpoint
        unsigned char *customer;    //  Our customer id
        unsigned char *keyfile;     // JSON file with pub/priv keys
        unsigned char *client_name; // This client name
        int timeout;                // Incremental timeout (trigger > 3)
        int state;                  // Internal state
        int registration_loop;      // Timer ID for registration loop
        int heartbeat_loop;         // Timer ID for heartbeat loop
        uint64_t cookie;            // Cookie from authentication
        struct ddkeystate *keys;    // Encryption keys loaded from JSON file
        zlistx_t *sublist; // List of subscriptions, and if they're active
        zloop_t *loop;
        unsigned char nonce[crypto_box_NONCEBYTES];
        dd_con(*on_reg);
        dd_discon(*on_discon);
        dd_data(*on_data);
        dd_pub(*on_pub);
        dd_error(*on_error);
        void (*subscribe)(char *, char *, struct ddclient *);
        void (*unsubscribe)(char *, char *, struct ddclient *);
        void (*publish)(char *, char *, int, struct ddclient *);
        void (*notify)(char *, char *, int, struct ddclient *);
        void (*shutdown)(void *);
} ddclient_t;

ddclient_t *start_ddthread(int, char *, char *, char *, char *, dd_con,
                dd_discon, dd_data, dd_pub, dd_error);
zactor_t * start_ddactor(int, char *, char *, char *, char *);


#endif
