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

/*
@header
    dd_broker_keys - 
@discuss
@end
*/

#include "dd_classes.h"


//  Structure of our class
struct _dd_broker_keys_t {
    // list of tenant names
    zlist_t *tenants;
    // broker private and public keys
    unsigned char *privkey;
    unsigned char *pubkey;
    // Broker pre-calc key
    unsigned char *ddboxk;
    // Precalculated tenant keys
    zhash_t *tenantkeys;
    // hash string
    char *hash;
    uint64_t cookie;
};



//  --------------------------------------------------------------------------
//  Create a new dd_broker_keys

dd_broker_keys_t *
dd_broker_keys_new(char *filename){
    FILE *fp;
    int retval;
    size_t retsize;
    dd_broker_keys_t *ddkeys = NULL;
    struct stat stats;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        zsys_error("Error opening file: %s, exiting..\n", filename);
        return NULL;
    }
    retval = stat(filename, &stats);
    if (retval != 0) {
        zsys_error( "Could not stat %s, exiting..\n", filename);
        fclose(fp);
        return NULL;
    }
    char *data = (char *)calloc(1, (size_t) (stats.st_size + 1));
    if (data == NULL) {
        zsys_error( "Error allocating memory\n");
        fclose(fp);
        return NULL;
    }
    retsize = fread(data, (size_t) stats.st_size, 1, fp);
    if (retsize < 1) {
        zsys_error( "Error reading file\n");
        fclose(fp);
        free(data);
        return NULL;
    }

    struct json_object *parse_result = json_tokener_parse((char *)data);
    free(data);
    base64_decodestate state_in;
    ddkeys = (dd_broker_keys_t *)calloc(1, sizeof(dd_broker_keys_t));
    assert(ddkeys);
    ddkeys->tenantkeys = zhash_new();
    ddkeys->tenants = zlist_new();
    // Find DD key first
    json_object_object_foreach(parse_result, key, val) {
        //enum json_type o_type = json_object_get_type(val);
        if (strcmp(key, "dd") == 0) {
            /* printf("Found ddkey, %s %s\n", key, json_object_get_string (val));
             */
            json_object_object_foreach(val, key2, val2) {
                if (strcmp(key2, "privkey") == 0) {
                    base64_init_decodestate(&state_in);
                    ddkeys->privkey = (unsigned char *)calloc(1, 33);
                    retval = base64_decode_block(json_object_get_string(val2),
                                                 json_object_get_string_len(val2),
                                                 (char *)ddkeys->privkey, &state_in);
                    if (retval != 32) {
                        zsys_error( "Error during base64_decode of %s\n", key2);
                        return NULL;
                    }
                } else if (strcmp(key2, "pubkey") == 0) {
                    ddkeys->hash = strdup(json_object_get_string(val2));
                    base64_init_decodestate(&state_in);
                    ddkeys->pubkey = (unsigned char *)calloc(1, 33);
                    retval = base64_decode_block(json_object_get_string(val2),
                                                 json_object_get_string_len(val2),
                                                 (char *)ddkeys->pubkey, &state_in);
                    if (retval != 32) {
                        zsys_error( "Error during base64_decode of %s\n", key2);
                        return NULL;
                    }
                } else if (strcmp(key2, "R") == 0) {
                    ddkeys->cookie = strtoull(json_object_get_string(val2), NULL, 10);
                }
            }
        }
    }

    // pre-calc the broker<->broker shared key
    ddkeys->ddboxk = (unsigned char *)calloc(1, crypto_box_BEFORENMBYTES);
    retval = crypto_box_beforenm(ddkeys->ddboxk, ddkeys->pubkey, ddkeys->privkey);

    // second pass to find tenant keys
    json_object_object_foreach(parse_result, key3, val3) {
        if (strcmp(key3, "dd") != 0) {
            ddtenant_t *ten = (ddtenant_t *)calloc(1, sizeof(ddtenant_t));
            char *hash = NULL;
            char pubkey[33];
//            unsigned long long int *cookie =
            //                   (long long unsigned int *)malloc(sizeof(unsigned long long int));
            hash = key3;
            json_object_object_foreach(val3, key4, val4) {
//                enum json_type o_type2 = json_object_get_type(val4);
                if (strcmp(key4, "pubkey") == 0) {
                    base64_init_decodestate(&state_in);
                    // pubkey = calloc (1, 33);
                    retval = base64_decode_block(json_object_get_string(val4),
                                                 json_object_get_string_len(val4), pubkey,
                                                 &state_in);
                    if (retval != 32) {
                        zsys_error( "Error during base64_decode of %s\n", key4);
                        return NULL;
                    }
                    ten->boxk = (char *)calloc(1, crypto_box_BEFORENMBYTES);
                    retval = crypto_box_beforenm((unsigned char *)ten->boxk,
                                                 (const unsigned char *)pubkey,
                                                 ddkeys->privkey);
                    if (retval != 0) {
                        zsys_error( "Error precalculating shared key!\n");
                        return NULL;
                    }

                } else if (strcmp(key4, "r") == 0) {
                    ten->name = strdup(json_object_get_string(val4));
                } else if (strcmp(key4, "R") == 0) {
                    ten->cookie = strtoull(json_object_get_string(val4), NULL, 10);
                }
            }

            //      dd_debug("added keys for tenant: %s\n", ten->name);
            // add to list of tenants
            zlist_append(ddkeys->tenants, ten->name);
            // add "hash" -> ddtenant_t
            //      dd_debug("hash: %s --> name: %s\n", hash, ten->name);
            zhash_insert(ddkeys->tenantkeys, hash, ten);
        }
    }
    fclose(fp);
    json_object_put(parse_result);
    return ddkeys;
}



//  --------------------------------------------------------------------------
//  Self test of this class

void
dd_broker_keys_test (bool verbose)
{
    printf (" * dd_broker_keys: ");

    //  @selftest
    //  Simple create/destroy test
    dd_broker_keys_t *self = dd_broker_keys_new ("keys/broker-keys.json");
    assert (self);
    dd_broker_keys_destroy (&self);
    //  @end
    printf ("OK\n");
}
void
dd_broker_keys_destroy(dd_broker_keys_t **self_p) {
    dd_broker_keys_t *self = *self_p;
    assert(self_p);
    if (*self_p) {
        if (self->privkey)
            free(self->privkey);
        if (self->pubkey)
            free(self->pubkey);
        if (self->ddboxk)
            free(self->ddboxk);
        if (self->hash)
            free(self->hash);

        ddtenant_t *ten = (ddtenant_t*) zhash_first(self->tenantkeys);
        while (ten) {
            free(ten->boxk);
            free(ten->name);
            free(ten);
            ten = (ddtenant_t*) zhash_next(self->tenantkeys);
        }
        zhash_destroy(&self->tenantkeys);

        zlist_destroy(&self->tenants);
        free(self);
        *self_p = NULL;
    }
}

const char * dd_broker_keys_get_hash(dd_broker_keys_t *keys){
    return keys->hash;
}
uint64_t dd_broker_keys_get_cookie(dd_broker_keys_t *keys){
    return keys->cookie;
}
zhash_t * dd_broker_keys_get_tenkeys(dd_broker_keys_t *keys){
    return keys->tenantkeys;
}
zlist_t * dd_broker_keys_get_tenlist(dd_broker_keys_t *keys){
    return keys->tenants;
}

const unsigned char * dd_broker_keys_get_ddboxk(dd_broker_keys_t * keys){
    return keys->ddboxk;
}
const unsigned char * dd_broker_keys_get_privkey(dd_broker_keys_t * keys){
    return keys->privkey;
}const unsigned char * dd_broker_keys_get_pubkey(dd_broker_keys_t * keys){
    return keys->pubkey;
}