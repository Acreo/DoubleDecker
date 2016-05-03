#include "../../include/dd.h"
#include "../../include/keys.h"
#include "../../include/dd_classes.h"

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
void dd_keys_destroy(dd_keys_t **self_p) {
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

zhash_t *dd_keys_clients(dd_keys_t *self) { return self->clientkeys; }
bool dd_keys_ispublic(dd_keys_t *self) { return self->ispublic; }
const unsigned char *dd_keys_custboxk(dd_keys_t *self) {
  return self->custboxk;
}
const char *dd_keys_hash(dd_keys_t *self) { return self->hash; }
const uint8_t *dd_keys_pub(dd_keys_t *self) { return self->pubkey; }
const uint8_t *dd_keys_ddboxk(dd_keys_t *self) { return self->ddboxk; }
const uint8_t *dd_keys_ddpub(dd_keys_t *self) { return self->ddpubkey; }
const uint8_t *dd_keys_pubboxk(dd_keys_t *self) { return self->pubboxk; }
const uint8_t *dd_keys_publicpub(dd_keys_t *self) {
  return self->publicpubkey;
}
const unsigned char *dd_keys_priv(dd_keys_t *self) { return self->privkey; }

// Read the Doubledecker keys from JSON file, for customer
// Returns a pointer to a struct ddkeystate of successful
// Or NULL if something fails.
dd_keys_t *dd_keys_new(const char *filename) {
  assert(filename);
  FILE *fp;
  int retval;
  int i = 0;
  int ispublic = 0;

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
  char *data = (char *)calloc(1,stats.st_size + 1);
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
        enum json_type o_type = json_object_get_type(val);
        json_object_object_foreach(val, key2, val2) {
          enum json_type o_type2 = json_object_get_type(val2);
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
      enum json_type o_type2 = json_object_get_type(val2);
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

void dd_broker_keys_destroy(ddbrokerkeys_t **self_p){
  ddbrokerkeys_t *self = *self_p;
  assert(self_p);
  if(*self_p) {
    if(self->privkey)
      free(self->privkey);
    if(self->pubkey)
      free(self->pubkey);
    if(self->ddboxk)
      free(self->ddboxk);
    if(self->hash)
      free(self->hash);

    ddtenant_t *ten = zhash_first (self->tenantkeys);
    while(ten){
      printf("dd_broker_keys_destroy, freeing %s\n",ten->name);
      free(ten->boxk);
      free(ten->name);
      free(ten);
      ten = zhash_next(self->tenantkeys);
    }
    zhash_destroy(&self->tenantkeys);

    

    zlist_destroy(&self->tenants);
    free(self);
    *self_p = NULL;
  }
}

ddbrokerkeys_t *read_ddbrokerkeys(char *filename) {
  FILE *fp;
  int retval;

  ddbrokerkeys_t *ddkeys = NULL;
  struct stat stats;

  fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error opening file: %s, exiting..\n", filename);
    return NULL;
  }
  retval = stat(filename, &stats);
  if (retval != 0) {
    fprintf(stderr, "Could not stat %s, exiting..\n", filename);
    fclose(fp);
    return NULL;
  }
  char *data = (char *)calloc(1,stats.st_size + 1);
  if (data == NULL) {
    fprintf(stderr, "Error allocating memory\n");
    fclose(fp);
    return NULL;
  }
  retval = fread(data, stats.st_size, 1, fp);
  if (retval < 1) {
    fprintf(stderr, "Error reading file\n");
    fclose(fp);
    free(data);
    return NULL;
  }

  struct json_object *parse_result = json_tokener_parse((char *)data);
  free(data);
  base64_decodestate state_in;
  ddkeys = (ddbrokerkeys_t *)calloc(1, sizeof(ddbrokerkeys_t));
  ddkeys->tenantkeys = zhash_new();
  ddkeys->tenants = zlist_new();
  // Find DD key first
  json_object_object_foreach(parse_result, key, val) {
    enum json_type o_type = json_object_get_type(val);
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
            fprintf(stderr, "Error during base64_decode of %s\n", key2);
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
            fprintf(stderr, "Error during base64_decode of %s\n", key2);
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
      unsigned long long int *cookie =
          (long long unsigned int *)malloc(sizeof(unsigned long long int));
      hash = key3;
      json_object_object_foreach(val3, key4, val4) {
        enum json_type o_type2 = json_object_get_type(val4);
        if (strcmp(key4, "pubkey") == 0) {
          base64_init_decodestate(&state_in);
          // pubkey = calloc (1, 33);
          retval = base64_decode_block(json_object_get_string(val4),
                                       json_object_get_string_len(val4), pubkey,
                                       &state_in);
          if (retval != 32) {
            fprintf(stderr, "Error during base64_decode of %s\n", key4);
            return NULL;
          }
          ten->boxk = (char *)calloc(1, crypto_box_BEFORENMBYTES);
          retval = crypto_box_beforenm((unsigned char *)ten->boxk,
                                       (const unsigned char *)pubkey,
                                       ddkeys->privkey);
          if (retval != 0) {
            fprintf(stderr, "Error precalculating shared key!\n");
            return NULL;
          }

        } else if (strcmp(key4, "r") == 0) {
          ten->name = strdup(json_object_get_string(val4));
        } else if (strcmp(key4, "R") == 0) {
          ten->cookie = strtoull(json_object_get_string(val4), NULL, 10);
          // ten->cookie = atoll(json_object_get_string(val4));
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






