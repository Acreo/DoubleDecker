/*  =========================================================================
    dd_broker_keys - class description

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

#ifndef DD_BROKER_KEYS_H_INCLUDED
#define DD_BROKER_KEYS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tenantsinfo {
    char *name;
    uint64_t cookie;
    char *boxk;
} ddtenant_t;


typedef struct _dd_broker_keys_t dd_broker_keys_t;

//  @interface
//  Create a new dd_broker_keys
DD_EXPORT dd_broker_keys_t *
    dd_broker_keys_new (char *filename);

//  Destroy the dd_broker_keys
DD_EXPORT void
    dd_broker_keys_destroy (dd_broker_keys_t **self_p);

//  Self test of this class
DD_EXPORT void
    dd_broker_keys_test (bool verbose);


DD_EXPORT const char * dd_broker_keys_get_hash(dd_broker_keys_t *keys);
DD_EXPORT uint64_t dd_broker_keys_get_cookie(dd_broker_keys_t *keys);
DD_EXPORT zhash_t * dd_broker_keys_get_tenkeys(dd_broker_keys_t *keys);
DD_EXPORT zlist_t * dd_broker_keys_get_tenlist(dd_broker_keys_t *keys);
DD_EXPORT const unsigned char * dd_broker_keys_get_ddboxk(dd_broker_keys_t * keys);
DD_EXPORT const unsigned char * dd_broker_keys_get_privkey(dd_broker_keys_t * keys);
DD_EXPORT const unsigned char * dd_broker_keys_get_pubkey(dd_broker_keys_t * keys);
//  @end

#ifdef __cplusplus
}
#endif

#endif
