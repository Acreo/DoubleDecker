/*  =========================================================================
    dd_broker - DoubleDecker broker API

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
        see http://www.gnu.org/licenses/.                                   
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

//  @warning THE FOLLOWING @INTERFACE BLOCK IS AUTO-GENERATED BY ZPROJECT
//  @warning Please edit the model at "api/dd_broker.xml" to make changes.
//  @interface
//  This API is a draft, and may change without notice.
#ifdef DD_BUILD_DRAFT_API
//  *** Draft method, for development use, may change without warning ***
//  Create a new DoubleDecker broker
DD_EXPORT dd_broker_t *
    dd_broker_new (void);

//  *** Draft method, for development use, may change without warning ***
//  Destroy a DoubleDecker broker
DD_EXPORT void
    dd_broker_destroy (dd_broker_t **self_p);

//  *** Draft method, for development use, may change without warning ***
//  Start the broker, blocking until it is stopped.
DD_EXPORT int
    dd_broker_start (dd_broker_t *self);

//  *** Draft method, for development use, may change without warning ***
//  Start an actor version of the broker, returning a zactor_t object.                                             
//  The actor version runs in a separate thread, using the zactor_t object to communicate with the starting thread.
DD_EXPORT zactor_t *
    dd_broker_actor (dd_broker_t *self);

//  *** Draft method, for development use, may change without warning ***
//  Set the broker scope.                                                           
//  The scope is a numerical representation of the position in the broker hierarchy.
//  The hierarchy is currently set to 3 levels, "regionId/clusterId/nodeId".        
//  A broker in region 0, cluster 1, node 2, would have the scope string "0/1/2"    
DD_EXPORT int
    dd_broker_set_scope (dd_broker_t *self, const char *scope);

//  *** Draft method, for development use, may change without warning ***
//  Set the URI of the broker REST interface.                                                           
//  For example "tcp://127.0.0.1:8080" , instead of an IP address, "*:8008" allows you to listen to all.
DD_EXPORT int
    dd_broker_set_rest_uri (dd_broker_t *self, const char *resturi);

//  *** Draft method, for development use, may change without warning ***
//  Set the logging file of the broker
DD_EXPORT int
    dd_broker_set_logfile (dd_broker_t *self, const char *logfile);

//  *** Draft method, for development use, may change without warning ***
//  Set the broker loglevel, as a single character string.         
//  Where "e":error,"w":warning,"n":notice,"i":info, and "d":debug.
//  "q" will keep all logging quiet                                
DD_EXPORT int
    dd_broker_set_loglevel (dd_broker_t *self, const char *loglevel);

//  *** Draft method, for development use, may change without warning ***
//  Checks if the broker is ready to be started, i.e. if the necessary configuration has been done.
DD_EXPORT bool
    dd_broker_ready (dd_broker_t *self);

//  *** Draft method, for development use, may change without warning ***
//  Remove a broker URI
DD_EXPORT int
    dd_broker_del_router (dd_broker_t *self, const char *routeruri);

//  *** Draft method, for development use, may change without warning ***
//  Add a router URI, where the broker listens for new connections
DD_EXPORT int
    dd_broker_add_router (dd_broker_t *self, const char *routeruri);

//  *** Draft method, for development use, may change without warning ***
//  Return the current configuration of the routerURI
DD_EXPORT const char *
    dd_broker_get_router (dd_broker_t *self);

//  *** Draft method, for development use, may change without warning ***
//  Set the dealer URI for the broker to connect to, typically another brokers router address.
DD_EXPORT int
    dd_broker_set_dealer (dd_broker_t *self, const char *dealeruri);

//  *** Draft method, for development use, may change without warning ***
//  Set the configuration file for the broker, given in CZMQ style format
DD_EXPORT int
    dd_broker_set_config (dd_broker_t *self, const char *configfile);

//  *** Draft method, for development use, may change without warning ***
//  Set the key file for the broker, as generated by ddkeys.py
DD_EXPORT int
    dd_broker_set_keyfile (dd_broker_t *self, const char *keyfile);

//  *** Draft method, for development use, may change without warning ***
//  Self test of this class.
DD_EXPORT void
    dd_broker_test (bool verbose);

#endif // DD_BUILD_DRAFT_API
//  @end

#ifdef __cplusplus
}
#endif

#endif
