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

#ifdef __cplusplus
extern "C" {
#endif
#ifndef _DD_H_
#define _DD_H_

#include <czmq.h>
#include <sodium.h>

// state definitions
#define DD_STATE_UNREG 1
#define DD_STATE_ROOT 2
#define DD_STATE_EXIT 3
#define DD_STATE_CHALLENGED 4
#define DD_STATE_REGISTERED 5

// Error codes
#define DD_ERROR_REGFAIL 1
#define DD_ERROR_NODST 2
#define DD_ERROR_VERSION 3

// On connection
typedef void(dd_con)(void *);
// On disconnection
typedef void(dd_discon)(void *);
// On recieve DATA
typedef void(dd_data)(char *, unsigned char *, int, void *);
// On recieve PUB
typedef void(dd_pub)(char *, char *, unsigned char *, int, void *);
// On receive ERROR
typedef void(dd_error)(int, char *, void *);

// class definition for a DoubleDecker client
typedef struct _dd_t dd_t;
// subscribed topics
typedef struct _ddtopic_t ddtopic_t;

CZMQ_EXPORT dd_t *dd_new(char *client_name, char *customer, char *endpoint,
                         char *keyfile, dd_con con, dd_discon discon,
                         dd_data data, dd_pub pub, dd_error error);
CZMQ_EXPORT zactor_t *ddactor_new(char *client_name, char *customer,
                                  char *endpoint, char *keyfile);

CZMQ_EXPORT const char *dd_get_version();
CZMQ_EXPORT int dd_get_state(dd_t *self);
CZMQ_EXPORT const char *dd_get_endpoint(dd_t *self);
CZMQ_EXPORT const char *dd_get_keyfile(dd_t *self);
CZMQ_EXPORT char *dd_get_privkey(dd_t *self);
CZMQ_EXPORT char *dd_get_pubkey(dd_t *self);
CZMQ_EXPORT char *dd_get_publickey(dd_t *self);
CZMQ_EXPORT int dd_subscribe(dd_t *self, char *topic, char *scope);
CZMQ_EXPORT int dd_unsubscribe(dd_t *self, char *topic, char *scope);
CZMQ_EXPORT int dd_publish(dd_t *self, char *topic, char *message, int mlen);
CZMQ_EXPORT int dd_notify(dd_t *self, char *target, char *message, int mlen);
CZMQ_EXPORT int dd_destroy(dd_t **self);

CZMQ_EXPORT const zlistx_t *dd_get_subscriptions(dd_t *self);
CZMQ_EXPORT const char* dd_sub_get_topic(ddtopic_t *sub);
CZMQ_EXPORT const char* dd_sub_get_scope(ddtopic_t *sub);
CZMQ_EXPORT char dd_sub_get_active(ddtopic_t *sub);

#endif
#ifdef __cplusplus
}
#endif
