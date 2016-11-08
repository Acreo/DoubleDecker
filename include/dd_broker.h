/*  =========================================================================
    dd_broker - class description

    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of CZMQ, the high-level C binding for 0MQ:       
    http://czmq.zeromq.org.                                            
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

#ifndef DD_BROKER_H_INCLUDED
#define DD_BROKER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif
#define _GNU_SOURCE 1
#define __GNU_SOURCE 1
#define __USE_GNU 1

#include <err.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <urcu.h>
#include <urcu/rculfhash.h>

//  @interface
//  Create a new dd_broker
DD_EXPORT dd_broker_t *
    dd_broker_new (void);

//  Destroy the dd_broker
DD_EXPORT void
    dd_broker_destroy (dd_broker_t **self_p);

//  Self test of this class
DD_EXPORT void
    dd_broker_test (bool verbose);

//  @end
// DD broker api
/**
 * Create a new broker object
 *
 *
 * @return dd_broker_t object
 */
DD_EXPORT dd_broker_t *dd_broker_new();
/**
 * Start the broker, blocking until the broker is stopped
 *
 * @param self
 *
 * @return
 */
DD_EXPORT int dd_broker_start(dd_broker_t *self);
/**
 * Set the scope of the broker
 *
 * @param self
 * @param scope_string "regionId/clusterId/nodeId" e.g. "0/1/2"
 *
 * @return
 */
DD_EXPORT int dd_broker_set_scope(dd_broker_t *self, char *scope_string);
/**
 * Set the logfile of the broker
 *
 * @param self
 * @param logfile
 *
 * @return
 */
DD_EXPORT int dd_broker_set_logfile(dd_broker_t *self, char *logfile);
/**
 * Set URI for the REST interface
 *
 * @param self
 * @param reststr For example "tcp://127.0.0.1:8080" , instead of an IP address, * allows you to listen to all
 *
 * @return
 */
DD_EXPORT int dd_broker_set_rest(dd_broker_t *self, char *reststr);
/**
 * Set loglevel of the broker
 *
 * @param self
 * @param logstr single character, with
 * e=error,w=warning,n=notice,i=info,d=debug,q=quiet
 *
 * @return
 */
DD_EXPORT int dd_broker_set_loglevel(dd_broker_t *self, char *logstr);
/**
 * Set the keyfile of the broker
 *
 * @param self
 * @param key_file
 *
 * @return
 */
DD_EXPORT int dd_broker_set_keyfile(dd_broker_t *self, char *key_file);
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
DD_EXPORT int dd_broker_set_config(dd_broker_t *self, char *config_file);
/**
 * Set the dealer URI for the broker to connect to
 *
 * @param self
 * @param dealer_string  (tcp://address:port, ipc:///folder/file)
 *
 * @return
 */
DD_EXPORT int dd_broker_set_dealer(dd_broker_t *self, char *dealer_string);
/**
 * Add a router URI for the broker to listen to
 *
 * @param self
 * @param router_string
 *
 * @return
 */
DD_EXPORT int dd_broker_add_router(dd_broker_t *self, char *router_string);
/**
 * Remove a broker URI
 *
 * @param self
 * @param router_string
 *
 * @return
 */
DD_EXPORT int dd_broker_del_router(dd_broker_t *self, char *router_string);

/**
 * Create an actor version of the broker
 *
 * @param self
 *
 * @return
 */
DD_EXPORT zactor_t *dd_broker_actor(dd_broker_t *self);

#ifdef __cplusplus
}
#endif

#endif
