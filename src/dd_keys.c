/*  =========================================================================
    dd_keys - class description

    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of CZMQ, the high-level C binding for 0MQ:       
    http://czmq.zeromq.org.                                            
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

/*
@header
    dd_keys - 
@discuss
@end
*/

#include "dd_classes.h"
//  Structure of our class

struct _dd_keys_t {
    // crypto_box_PUBLICKEYBYTES / crypto_box_SECRETKEYBYTES
    // 32U arrays
    // Public key of "public" tenant
    uint8_t *publicpubkey;
    // Broker public key
    uint8_t *ddpubkey;
    // Client private key
    uint8_t *privkey;
    // Client public key
    uint8_t *pubkey;

    // crypto_box_BEFORENMBYTES
    // 32U arrays
    // Broker symmetric key
    uint8_t *ddboxk;
    // Tenant symmetric key
    uint8_t *custboxk;
    // Tenant-Public symmetric key
    uint8_t *pubboxk;
    int ispublic;
    char *hash;
    zhash_t *clientkeys;
};




//  --------------------------------------------------------------------------
//  Destroy the dd_keys
void
dd_keys_destroy(dd_keys_t **self_p) {
    assert(self_p);
    if (*self_p) {
        dd_keys_t *self = *self_p;
        free(self->publicpubkey);
        free(self->ddpubkey);
        free(self->privkey);
        free(self->pubkey);
        free(self->ddboxk);
        free(self->custboxk);
        free(self->pubboxk);
        free(self->hash);
        zhash_destroy(&self->clientkeys);
        free(self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
dd_keys_test (bool verbose)
{
    printf (" * dd_keys: ");

    //  @selftest
    //  Simple create/destroy test
    dd_keys_t *self = dd_keys_new ("keys/public-keys.json");
    assert (self);
    dd_keys_destroy (&self);
    //  @end
    printf ("OK\n");
}

//  --------------------------------------------------------------------------
//  Create a new dd_keys from JSON file, for a customer
//  Returns NULL on failure



dd_keys_t *
dd_keys_new(const char *filename) {
    assert(filename);
    FILE *fp;
    int retval;

    dd_keys_t *ddkeys;
    struct stat stats;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error opening file: %s, exiting..\n", filename);
        assert(fp);
    }
    retval = stat(filename, &stats);
    if (retval != 0) {
        printf("Could not stat %s, exiting..\n", filename);
        fclose(fp);
        assert(retval);
        return NULL;
    }
    char *data = (char *)calloc(1, stats.st_size + 1);
    if (data == NULL) {
        printf("Error allocating memory\n");
        fclose(fp);
        return NULL;
    }
    retval = fread(data, stats.st_size, 1, fp);
    if (retval < 1) {
        printf("Error reading file\n");
        fclose(fp);
        free(data);
        return NULL;
    }
    struct json_object *parse_result = json_tokener_parse((char *)data);
    free(data);
    fclose(fp);
    //  const char *unjson = json_object_get_string(parse_result);

    base64_decodestate state_in;
    ddkeys = (dd_keys_t *)calloc(1, sizeof(dd_keys_t));
    ddkeys->clientkeys = zhash_new();

    json_object_object_foreach(parse_result, key, val) {
        if (streq(key, "public")) {
            ddkeys->ispublic = true;
        }
    }

    if (ddkeys->ispublic) {
        json_object_object_foreach(parse_result, key, val) {
            if (strcmp(key, "public") == 0) {
                //enum json_type o_type = json_object_get_type(val);
                json_object_object_foreach(val, key2, val2) {
//                    enum json_type o_type2 = json_object_get_type(val2);
                    if (strcmp(key2, "publicpubkey") == 0) {
                        base64_init_decodestate(&state_in);
                        ddkeys->publicpubkey = (unsigned char *)calloc(1, 33);
                        retval = base64_decode_block(
                                json_object_get_string(val2), json_object_get_string_len(val2),
                                (char *)ddkeys->publicpubkey, &state_in);
                        if (retval != 32) {
                            fprintf(stderr, "Error during base64_decode of %s\n", key2);
                            return NULL;
                        }
                    } else if (strcmp(key2, "ddpubkey") == 0) {
                        base64_init_decodestate(&state_in);
                        ddkeys->ddpubkey = (unsigned char *)calloc(1, 33);
                        retval = base64_decode_block(json_object_get_string(val2),
                                                     json_object_get_string_len(val2),
                                                     (char *)ddkeys->ddpubkey, &state_in);
                        if (retval != 32) {
                            fprintf(stderr, "Error during base64_decode of %s\n", key2);
                            return NULL;
                        }
                    } else if (strcmp(key2, "privkey") == 0) {
                        base64_init_decodestate(&state_in);
                        ddkeys->privkey = (unsigned char *)calloc(1, 33);
                        retval = base64_decode_block(json_object_get_string(val2),
                                                     json_object_get_string_len(val2),
                                                     (char *)ddkeys->privkey, &state_in);
                        if (retval != 32) {
                            fprintf(stderr, "Error during base64_decode of %s\n", key2);
                            return NULL;
                        }
                    } else if (strcmp(key2, "pubkey") == 0) {
                        base64_init_decodestate(&state_in);
                        ddkeys->pubkey = (unsigned char *)calloc(1, 33);
                        retval = base64_decode_block(json_object_get_string(val2),
                                                     json_object_get_string_len(val2),
                                                     (char *)ddkeys->pubkey, &state_in);
                        if (retval != 32) {
                            fprintf(stderr, "Error during base64_decode of %s\n", key2);
                            return NULL;
                        }

                    } else if (strcmp(key2, "hash") == 0) {
                        ddkeys->hash = strdup(json_object_get_string(val2));
                    } else {
                    }
                }
            }
        }

        // do a second pass to find other keys
        json_object_object_foreach(parse_result, key3, val3) {
            if (strcmp(key3, "public") != 0) {
                const char *customer_name = NULL;
                const char *customer_pubkey = NULL;
                json_object_object_foreach(val3, key4, val4) {
                    // printf("Key %s : val
                    // %s\n",key4,json_object_get_string(val4));
                    if (strcmp(key4, "r") == 0) {
                        customer_name = json_object_get_string(val4);
                    } else if (strcmp(key4, "pubkey") == 0) {
                        customer_pubkey = json_object_get_string(val4);
                    }
                    unsigned char pubkey[33];
                    // printf("customer name = %s, customer_pubkey =
                    // %s\n",customer_name,customer_pubkey);
                    if (customer_name && customer_pubkey) {
                        base64_init_decodestate(&state_in);
                        retval =
                                base64_decode_block(customer_pubkey, strlen(customer_pubkey),
                                                    (char *)pubkey, &state_in);
                        unsigned char *precalck =
                                (unsigned char *)calloc(1, crypto_box_BEFORENMBYTES);
                        retval = crypto_box_beforenm(precalck, pubkey, ddkeys->privkey);
                        zhash_insert(ddkeys->clientkeys, customer_name, precalck);
                        customer_pubkey = NULL;
                        customer_name = NULL;
                    }
                }
            }
        }
    } else { // Not public!
        json_object_object_foreach(parse_result, key2, val2) {

            if (strcmp(key2, "publicpubkey") == 0) {
                base64_init_decodestate(&state_in);
                ddkeys->publicpubkey = (unsigned char *)calloc(1, 33);
                retval = base64_decode_block(json_object_get_string(val2),
                                             json_object_get_string_len(val2),
                                             (char *)ddkeys->publicpubkey, &state_in);
                if (retval != 32) {
                    fprintf(stderr, "Error during base64_decode of %s\n", key2);
                    return NULL;
                }
            } else if (strcmp(key2, "ddpubkey") == 0) {
                base64_init_decodestate(&state_in);
                ddkeys->ddpubkey = (unsigned char *)calloc(1, 33);
                retval = base64_decode_block(json_object_get_string(val2),
                                             json_object_get_string_len(val2),
                                             (char *)ddkeys->ddpubkey, &state_in);
                if (retval != 32) {
                    fprintf(stderr, "Error during base64_decode of %s\n", key2);
                    return NULL;
                }
            } else if (strcmp(key2, "privkey") == 0) {
                base64_init_decodestate(&state_in);
                ddkeys->privkey = (unsigned char *)calloc(1, 33);
                retval = base64_decode_block(json_object_get_string(val2),
                                             json_object_get_string_len(val2),
                                             (char *)ddkeys->privkey, &state_in);
                if (retval != 32) {
                    fprintf(stderr, "Error during base64_decode of %s\n", key2);
                    return NULL;
                }
            } else if (strcmp(key2, "pubkey") == 0) {
                base64_init_decodestate(&state_in);
                ddkeys->pubkey = (unsigned char *)calloc(1, 33);
                retval = base64_decode_block(json_object_get_string(val2),
                                             json_object_get_string_len(val2),
                                             (char *)ddkeys->pubkey, &state_in);
                if (retval != 32) {
                    fprintf(stderr, "Error during base64_decode of %s\n", key2);
                    return NULL;
                }

            } else if (strcmp(key2, "hash") == 0) {
                ddkeys->hash = strdup(json_object_get_string(val2));
            }
        }
    }

    // json_object_delete(parse_result);
    // TODO: how to free the memory used by the json parser here?
    json_object_put(parse_result);
    // pre-calculate the shared keys for the DD bus itself
    ddkeys->ddboxk = (unsigned char *)calloc(1, crypto_box_BEFORENMBYTES);
    retval =
            crypto_box_beforenm(ddkeys->ddboxk, ddkeys->ddpubkey, ddkeys->privkey);

    // pre-calculate the shared keys for the particular customer
    ddkeys->custboxk = (unsigned char *)calloc(1, crypto_box_BEFORENMBYTES);
    retval =
            crypto_box_beforenm(ddkeys->custboxk, ddkeys->pubkey, ddkeys->privkey);

    // pre-calculate the shared keys for the public customer
    ddkeys->pubboxk = (unsigned char *)calloc(1, crypto_box_BEFORENMBYTES);
    retval = crypto_box_beforenm(ddkeys->pubboxk, ddkeys->publicpubkey,
                                 ddkeys->privkey);

    return ddkeys;
}
zhash_t *
dd_keys_clients(dd_keys_t *self) { return self->clientkeys; }
bool
dd_keys_ispublic(dd_keys_t *self) { return self->ispublic; }
const unsigned char *
dd_keys_custboxk(dd_keys_t *self) {
    return self->custboxk;
}
const char *
dd_keys_hash(dd_keys_t *self) { return self->hash; }
const uint8_t *
dd_keys_pub(dd_keys_t *self) { return self->pubkey; }
const uint8_t *
dd_keys_ddboxk(dd_keys_t *self) { return self->ddboxk; }
const uint8_t *
dd_keys_ddpub(dd_keys_t *self) { return self->ddpubkey; }
const uint8_t *
dd_keys_pubboxk(dd_keys_t *self) { return self->pubboxk; }
const uint8_t *
dd_keys_publicpub(dd_keys_t *self) { return self->publicpubkey; }
const unsigned char *
dd_keys_priv(dd_keys_t *self) { return self->privkey; }



void dd_keys_nonce_increment(byte *n, size_t nlen) {
    size_t i = 0U;
    uint_fast16_t c = 1U;

#ifdef HAVE_AMD64_ASM
    uint64_t t64, t64_2;
  uint32_t t32;

  if (nlen == 12U) {
    __asm__ __volatile__("xorq %[t64], %[t64] \n"
                         "xorl %[t32], %[t32] \n"
                         "stc \n"
                         "adcq %[t64], (%[out]) \n"
                         "adcl %[t32], 8(%[out]) \n"
                         : [t64] "=&r"(t64), [t32] "=&r"(t32)
                         : [out] "D"(n)
                         : "memory", "flags", "cc");
    return;
  } else if (nlen == 24U) {
    __asm__ __volatile__("movq $1, %[t64] \n"
                         "xorq %[t64_2], %[t64_2] \n"
                         "addq %[t64], (%[out]) \n"
                         "adcq %[t64_2], 8(%[out]) \n"
                         "adcq %[t64_2], 16(%[out]) \n"
                         : [t64] "=&r"(t64), [t64_2] "=&r"(t64_2)
                         : [out] "D"(n)
                         : "memory", "flags", "cc");
    return;
  } else if (nlen == 8U) {
    __asm__ __volatile__("incq (%[out]) \n"
                         :
                         : [out] "D"(n)
                         : "memory", "flags", "cc");
    return;
  }
#endif
    for (; i < nlen; i++) {
        c += (uint_fast16_t)n[i];
        n[i] = (unsigned char)c;
        c >>= 8;
    }
}

/* TODO, API for key generation


  generate broker keys
   tenants = null
   privkey = something
   pubkey = something
   tenantkeys = null
   hash = something
   cookie = something
   dd_broker_keys_t *broker_keys = dd_broker_keys_new();


   generate public tenant and add to broker_keys
   return the public tenant keys

   dd_keys_t* public_key = dd_keys_add_public(
    broker_keys);

   generate private tenant and add to broker keys
   return the private tenant keys

   dd_keys_t* private_key = dd_keys_add_private(
   broker_keys, public_key, "tenant");

   char * dd_keys_json(public_key);
   fprintf(.. dd_keys_json());
   fprintf(.. dd_keys_json());
   fprintf(.. dd_broker_keys_json());
*/
