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
/* dd.h --- Public API definition
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
// -----------------

// Client/broker not registered
#define DD_STATE_UNREG 1
// Broker is the root broker
#define DD_STATE_ROOT 2
// In the process of exiting
#define DD_STATE_EXIT 3
// In the process of authentication with a broker
#define DD_STATE_CHALLENGED 4
// Registered with a broker
#define DD_STATE_REGISTERED 5

// Error codes
// -----------

// Registration failed
#define DD_ERROR_REGFAIL 1
// Message destination doesn't exist
#define DD_ERROR_NODST 2
// Wrong protocol version
#define DD_ERROR_VERSION 3

// ------------------------
// Callback based DD client

/**
 * Called upon disconnection
 * @param self DD client which issued the callback
 *
 * @return
 */
typedef void(dd_on_discon)(void *self);
/**
 * Called upon connection
 * @param self DD client which issued the callback
 * @return
 */

typedef void(dd_on_con)(void *self);
/**
 * Called upon recieving a notification
 * @param source Client that sent the message
 * @param data Message data
 * @param length Message length
 * @param self DD client which issued the callback
 * @return
 */

typedef void(dd_on_data)(char *source, unsigned char *data, int length,
                         void *self);
/**
 * Called upon recieving a publication
 * @param source Client that sent the message
 * @param topic Topic the message was published on
 * @param data Message data
 * @param length Message length
 * @param self DD client which issued the callback
 * @return
 */
typedef void(dd_on_pub)(char *source, char *topic, unsigned char *data,
                        int length, void *self);
/**
 * Called upon an error
 * @param error_code Error which occoured (DD_ERROR_*)
 * @param error_msg Message explaining the error
 * @param self DD client which issued the callback
 * @return
 */
typedef void(dd_on_error)(int error_code, char *error_msg, void *self);

// Class definition for a DoubleDecker callback client
typedef stdd_ruct _dd_t dd_t;
// Class definiotion for subscribed topics
typedef struct _ddtopic_t ddtopic_t;
// Class definition for internal key structure
typedef struct _dd_keys_t dd_keys_t;

/**
 * Called to create a callback style DoubleDecker client
 *
 * @param client_name Name of the client
 * @param endpoint Address of the broker (tcp://a.com:77, ipc:///folder/file)
 * @param keyfile Location of the keys
 * @param con Callback for successful connection establishment
 * @param discon Callback for connection loss
 * @param data Callback for notification recieved
 * @param pub Callback for publication recieved
 * @param error Callback for errors
 *
 * @return a dd_t object representing a DD client or NULL on failure
*/
CZMQ_EXPORT dd_t *dd_new(char *client_name, char *endpoint, char *keyfile,
                         dd_on_con con, dd_on_discon discon, dd_on_data data,
                         dd_on_pub pub, dd_on_error error);
/**
 * Subscribe to a topic
 *
 * @param self The DD client to add the subscription to
 * @param topic The topic to subscribe on
 * @param scope The scope of the subscription (all, region, cluster, node)
 *
 * @return 0 on success, -1 on failure
 */
CZMQ_EXPORT int dd_subscribe(dd_t *self, char *topic, char *scope);
/**
 * Unsubscribe from a topic
 *
 * @param self The DD client to remove the subscription from
 * @param topic The topic to unsubscribe from
 * @param scope The scope of the topic
 *
 * @return 0 on success, -1 on failure
 */
CZMQ_EXPORT int dd_unsubscribe(dd_t *self, char *topic, char *scope);
/**
 * Publish a message on a topic
 *
 * @param self The DD client to publish on
 * @param topic The topic of the message
 * @param message The message to publish (opaque data)
 * @param mlen Length of the data
 *
 * @return 0 on success, -1 on failure
 */
CZMQ_EXPORT int dd_publish(dd_t *self, char *topic, char *message, int mlen);
/**
 * Send a notification to another DD client
 *
 * @param self The DD client to use
 * @param target The destination of the message
 * @param message The message to send (opaque data)
 * @param mlen The length of the message
 *
 * @return 0 on success, -1 on failure
 */
CZMQ_EXPORT int dd_notify(dd_t *self, char *target, char *message, int mlen);

/**
 * Destroy the DD client, sets the dd_t pointer to null.
 * Can be called multiple times on the same client.
 *
 * @param self_p The DD client to destroy
 */
CZMQ_EXPORT void dd_destroy(dd_t **self_p);

/**
 * Return the library version
 *
 *
 * @return String containing the version
 */
CZMQ_EXPORT const char *dd_get_version();

/**
 * Get the current state of the DD client
 *
 * @param self The DD client
 *
 * @return DD_STATE_UNREG, DD_STATE_EXIT, DD_STATE_CHALLENGED,
 *DD_STATE_REGISTERED
 */
CZMQ_EXPORT int dd_get_state(dd_t *self);
/**
 * Get the configured endpoint for this DD client
 *
 * @param self The DD client
 *
 * @return String containing the endpoint
 */
CZMQ_EXPORT const char *dd_get_endpoint(dd_t *self);
/**
 * Get the configured keyfile for this DD client
 *
 * @param self The DD client
 *
 * @return
 */
CZMQ_EXPORT const char *dd_get_keyfile(dd_t *self);

/**
 * Get the private key used by this client
 *
 * @param self The DD client
 *
 * @return
 */
CZMQ_EXPORT char *dd_get_privkey(dd_t *self);

/**
 * Get the public key used by this client
 *
 * @param self The DD client
 *
 * @return
 */
CZMQ_EXPORT char *dd_get_pubkey(dd_t *self);

/**
 * Get the public broker key used by this client
 *
 * @param self The DD client
 *
 * @return
 */
CZMQ_EXPORT char *dd_get_publickey(dd_t *self);

/**
 * Get a list of topics the client is subscribed to
 *
 * @param self The DD client
 *
 * @return a zlistx_t containing the topics as ddtopic_t objects
 */
CZMQ_EXPORT const zlistx_t *dd_get_subscriptions(dd_t *self);

/**
 * Get the topic of this subscription
 *
 * @param sub ddtopic_t to extract the topic from
 *
 * @return String with the topic
 */
CZMQ_EXPORT const char *dd_sub_get_topic(ddtopic_t *sub);

/**
 * Get the scope of this subscription
 *
 * @param sub ddtopic_t to extract the topic from
 *
 * @return String with the scope
 */
CZMQ_EXPORT const char *dd_sub_get_scope(ddtopic_t *sub);

/**
 * Check whether the topic is active or not (i.e. sent to the broker)
 *
 * @param sub ddtopic_t to investigate
 *
 * @return
 */
CZMQ_EXPORT char dd_sub_get_active(ddtopic_t *sub);

/**
 * Create a new DD client, actor style
 *
 * @param client_name Name of the client
 * @param endpoint Address of the broker (tcp://a.com:77, ipc:///folder/file)
 * @param keyfile Location of the keys
 *
 * @return zactor_t object representing a dd client
 */
CZMQ_EXPORT zactor_t *ddactor_new(char *client_name, char *endpoint,
                                  char *keyfile);

// Class definitions for a DoubleDecker broker
typedef struct _dd_broker_t dd_broker_t;
typedef struct _dd_keys_t dd_keys_t;

// DD broker api
/**
 * Create a new broker object
 *
 *
 * @return dd_broker_t object
 */
CZMQ_EXPORT dd_broker_t *dd_broker_new();
/**
 * Start the broker, blocking until the broker is stopped
 *
 * @param self
 *
 * @return
 */
CZMQ_EXPORT int dd_broker_start(dd_broker_t *self);
/**
 * Set the scope of the broker
 *
 * @param self
 * @param scope_string "regionId/clusterId/nodeId" e.g. "0/1/2"
 *
 * @return
 */
CZMQ_EXPORT int dd_broker_set_scope(dd_broker_t *self, char *scope_string);
/**
 * Set the logfile of the broker
 *
 * @param self
 * @param logfile
 *
 * @return
 */
CZMQ_EXPORT int dd_broker_set_logfile(dd_broker_t *self, char *logfile);
/**
 * Set URI for the REST interface
 *
 * @param self
 * @param reststr For example "tcp://*:8080"
 *
 * @return
 */
CZMQ_EXPORT int dd_broker_set_rest(dd_broker_t *self, char *reststr);
/**
 * Set loglevel of the broker
 *
 * @param self
 * @param logstr single character, with
 *e=error,w=warning,n=notice,i=info,d=debug,q=quiet
 *
 * @return
 */
CZMQ_EXPORT int dd_broker_set_loglevel(dd_broker_t *self, char *logstr);
/**
 * Set the keyfile of the broker
 *
 * @param self
 * @param key_file
 *
 * @return
 */
CZMQ_EXPORT int dd_broker_set_keyfile(dd_broker_t *self, char *key_file);
/**
 * Set configuration file of the broker, given in CZMQ style configuration
 *format
 * See br0.cfg for example
 *
 * @param self
 * @param config_file
 *
 * @return
 */
CZMQ_EXPORT int dd_broker_set_config(dd_broker_t *self, char *config_file);
/**
 * Set the dealer URI for the broker to connect to
 *
 * @param self
 * @param dealer_string  (tcp://address:port, ipc:///folder/file)
 *
 * @return
 */
CZMQ_EXPORT int dd_broker_set_dealer(dd_broker_t *self, char *dealer_string);
/**
 * Add a router URI for the broker to listen to
 *
 * @param self
 * @param router_string
 *
 * @return
 */
CZMQ_EXPORT int dd_broker_add_router(dd_broker_t *self, char *router_string);
/**
 * Remove a broker URI
 *
 * @param self
 * @param router_string
 *
 * @return
 */
CZMQ_EXPORT int dd_broker_del_router(dd_broker_t *self, char *router_string);

/**
 * Create an actor version of the broker
 *
 * @param self
 *
 * @return
 */
CZMQ_EXPORT zactor_t *dd_broker_actor(dd_broker_t *self);

#endif
#ifdef __cplusplus
}
#endif
