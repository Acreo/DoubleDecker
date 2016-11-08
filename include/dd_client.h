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

#ifndef DD_CLIENT_H_INCLUDED
#define DD_CLIENT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif



/**
 * Called upon disconnection
 * @param self DD client which issued the callback
 *
 * @return
 */
typedef void(dd_client_on_discon)(void *self);
/**
 * Called upon connection
 * @param self DD client which issued the callback
 * @return
 */

typedef void(dd_client_on_con)(void *self);
/**
 * Called upon recieving a notification
 * @param source Client that sent the message
 * @param data Message data
 * @param length Message length
 * @param self DD client which issued the callback
 * @return
 */

typedef void(dd_client_on_data)(char *source, unsigned char *data, size_t length,
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
typedef void(dd_client_on_pub)(char *source, char *topic, unsigned char *data,
                               size_t length, void *self);
/**
 * Called upon an error
 * @param error_code Error which occoured (DD_ERROR_*)
 * @param error_msg Message explaining the error
 * @param self DD client which issued the callback
 * @return
 */
typedef void(dd_client_on_error)(int error_code, char *error_msg, void *self);


//  @interface




//  Create a new dd_client
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
 * @return a dd_client_t object representing a DD client or NULL on failure
*/
DD_EXPORT dd_client_t *dd_client_new(char *client_name, char *endpoint, char *keyfile,
                                       dd_client_on_con con, dd_client_on_discon discon, dd_client_on_data data,
                                       dd_client_on_pub pub, dd_client_on_error error);

// For actor use only
DD_EXPORT dd_client_t *dd_client_setup(char *client_name, char *endpoint, char *keyfile, dd_client_on_con con,
                             dd_client_on_discon discon, dd_client_on_data data, dd_client_on_pub pub,
                             dd_client_on_error error);
// For actor use only
DD_EXPORT void dd_client_add_pipe(dd_client_t * self, zsock_t *pipe, zloop_reader_fn handler);
// For actor use only
DD_EXPORT void *dd_client_thread(void *args);

//  Destroy the dd_client
DD_EXPORT void dd_client_destroy (dd_client_t **self_p);

//  Self test of this class
DD_EXPORT void dd_client_test (bool verbose);


// ------------------------
// Callback based DD client


// Class definition for a DoubleDecker callback client
typedef struct _dd_t dd_t;
// Class definiotion for subscribed topics
typedef struct _ddtopic_t ddtopic_t;
// Class definition for internal key structure
typedef struct _dd_keys_t dd_keys_t;

/**
 * Subscribe to a topic
 *
 * @param self The DD client to add the subscription to
 * @param topic The topic to subscribe on
 * @param scope The scope of the subscription (all, region, cluster, node)
 *
 * @return 0 on success, -1 on failure
 */
DD_EXPORT int dd_client_subscribe(dd_client_t *self, char *topic, char *scope);
/**
 * Unsubscribe from a topic
 *
 * @param self The DD client to remove the subscription from
 * @param topic The topic to unsubscribe from
 * @param scope The scope of the topic
 *
 * @return 0 on success, -1 on failure
 */
DD_EXPORT int dd_client_unsubscribe(dd_client_t *self, char *topic, char *scope);
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
DD_EXPORT int dd_client_publish(dd_client_t *self, char *topic, char *message, unsigned long long int mlen);
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
DD_EXPORT int dd_client_notify(dd_client_t *self, char *target, char *message, unsigned long long int mlen);

/**
 * Destroy the DD client, sets the dd_client_t pointer to null.
 * Can be called multiple times on the same client.
 *
 * @param self_p The DD client to destroy
 */
DD_EXPORT void dd_client_destroy(dd_client_t **self_p);

/**
 * Return the library version
 *
 *
 * @return String containing the version
 */
DD_EXPORT const char *dd_client_get_version();

/**
 * Get the current state of the DD client
 *
 * @param self The DD client
 *
 * @return DD_STATE_UNREG, DD_STATE_EXIT, DD_STATE_CHALLENGED,
 *DD_STATE_REGISTERED
 */
DD_EXPORT int dd_client_get_state(dd_client_t *self);
/**
 * Get the configured endpoint for this DD client
 *
 * @param self The DD client
 *
 * @return String containing the endpoint
 */
DD_EXPORT const char *dd_client_get_endpoint(dd_client_t *self);
/**
 * Get the configured keyfile for this DD client
 *
 * @param self The DD client
 *
 * @return
 */
DD_EXPORT const char *dd_client_get_keyfile(dd_client_t *self);

/**
 * Get the private key used by this client
 *
 * @param self The DD client
 *
 * @return
 */
DD_EXPORT char *dd_client_get_privkey(dd_client_t *self);

/**
 * Get the public key used by this client
 *
 * @param self The DD client
 *
 * @return
 */
DD_EXPORT char *dd_client_get_pubkey(dd_client_t *self);

/**
 * Get the public broker key used by this client
 *
 * @param self The DD client
 *
 * @return
 */
DD_EXPORT char *dd_client_get_publickey(dd_client_t *self);

/**
 * Get a list of topics the client is subscribed to
 *
 * @param self The DD client
 *
 * @return a zlistx_t containing the topics as ddtopic_t objects
 */
DD_EXPORT const zlistx_t *dd_client_get_subscriptions(dd_client_t *self);

/**
 * Get the topic of this subscription
 *
 * @param sub ddtopic_t to extract the topic from
 *
 * @return String with the topic
 */
DD_EXPORT const char *dd_client_sub_get_topic(ddtopic_t *sub);

/**
 * Get the scope of this subscription
 *
 * @param sub ddtopic_t to extract the topic from
 *
 * @return String with the scope
 */
DD_EXPORT const char *dd_client_sub_get_scope(ddtopic_t *sub);

/**
 * Check whether the topic is active or not (i.e. sent to the broker)
 *
 * @param sub ddtopic_t to investigate
 *
 * @return
 */
DD_EXPORT char dd_client_sub_get_active(ddtopic_t *sub);

/**
 * Get the actor pipe associated with this dd_client_t instance
 * If no actor is associated with the dd_client_t, return null
 *
 * @param self ddclient_t instance
 *
 * @return zsockt_t pipe representation
 */
zsock_t *dd_client_get_pipe(dd_client_t *self);

//  @end

#ifdef __cplusplus
}
#endif

#endif


