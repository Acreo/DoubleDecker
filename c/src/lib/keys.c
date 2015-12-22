#include "ddkeys.h"
// Read the Doubledecker keys from JSON file, for customer
// Returns a pointer to a struct ddkeystate of successful
// Or NULL if something fails.

ddbrokerkeys_t *read_ddbrokerkeys(char *filename) {
  FILE *fp;
  int retval;
  int i = 0;

  ddbrokerkeys_t *ddkeys;
  struct stat stats;

  fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error opening file: %s, exiting..\n", filename);
    return NULL;
  }
  retval = stat(filename, &stats);
  if (retval != 0) {
    fprintf(stderr, "Could not stat %s, exiting..\n", filename);
    return NULL;
  }
  char *data = malloc(stats.st_size + 1);
  if (data == NULL) {
    fprintf(stderr, "Error allocating memory\n");
    return NULL;
  }
  retval = fread(data, stats.st_size, 1, fp);
  if (retval < 1) {
    fprintf(stderr, "Error reading file\n");
    return NULL;
  }
  struct json_object *parse_result = json_tokener_parse((char *)data);
  free(data);
  base64_decodestate state_in;
  ddkeys = calloc(1, sizeof(ddbrokerkeys_t));
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
          ddkeys->privkey = calloc(1, 33);
          retval = base64_decode_block(json_object_get_string(val2),
                                       json_object_get_string_len(val2),
                                       ddkeys->privkey, &state_in);
          if (retval != 32) {
            fprintf(stderr, "Error during base64_decode of %s\n", key2);
            return NULL;
          }
        } else if (strcmp(key2, "pubkey") == 0) {
          ddkeys->hash = strdup(json_object_get_string(val2));
          base64_init_decodestate(&state_in);
          ddkeys->pubkey = calloc(1, 33);
          retval = base64_decode_block(json_object_get_string(val2),
                                       json_object_get_string_len(val2),
                                       ddkeys->pubkey, &state_in);
          if (retval != 32) {
            fprintf(stderr, "Error during base64_decode of %s\n", key2);
            return NULL;
          }
        } else if (strcmp(key2, "R") == 0) {
          ddkeys->cookie =
              strtoull(json_object_get_string(val2), NULL, 10);
          // ddkeys->cookie = atoll(json_object_get_string(val2));
        }
      }
    }
  }

  // pre-calc the broker<->broker shared key
  ddkeys->ddboxk = calloc(1, crypto_box_BEFORENMBYTES);
  retval =
      crypto_box_beforenm(ddkeys->ddboxk, ddkeys->pubkey, ddkeys->privkey);

  // second pass to find tenant keys
  json_object_object_foreach(parse_result, key3, val3) {
    if (strcmp(key3, "dd") != 0) {
      ddtenant_t *ten = calloc(1, sizeof(ddtenant_t));
      char *hash = NULL;
      char pubkey[33];
      unsigned long long int *cookie =
          malloc(sizeof(unsigned long long int));
      hash = strdup(key3);
      json_object_object_foreach(val3, key4, val4) {
        enum json_type o_type2 = json_object_get_type(val4);
        if (strcmp(key4, "pubkey") == 0) {
          base64_init_decodestate(&state_in);
          // pubkey = calloc (1, 33);
          retval = base64_decode_block(json_object_get_string(val4),
                                       json_object_get_string_len(val4),
                                       pubkey, &state_in);
          if (retval != 32) {
            fprintf(stderr, "Error during base64_decode of %s\n", key4);
            return NULL;
          }
          ten->boxk = calloc(1, crypto_box_BEFORENMBYTES);
          retval = crypto_box_beforenm(ten->boxk, pubkey, ddkeys->privkey);
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
  return ddkeys;
}

void print_ddbrokerkeys(ddbrokerkeys_t *keys) {
  char *hex = malloc(100);
  //  printf ("Hash value: \t%s\n", keys->hash);
  printf("Private key: \t%s", sodium_bin2hex(hex, 100, keys->privkey, 32));
  printf("Public key: \t%s", sodium_bin2hex(hex, 100, keys->pubkey, 32));
  printf("Cookie: \t%llu", keys->cookie);
  printf("Hash:\t\t%s", keys->hash);

  printf("Tenants:  %d : ", zlist_size(keys->tenants));

  char *k = NULL;
  k = zlist_first(keys->tenants);
  while (k) {
    printf("%s", k);
    k = zlist_next(keys->tenants);
    if (k != NULL)
      printf(", ");
  }
  printf("");

  printf("Tenant keys: ");
  zlist_t *precalc = zhash_keys(keys->tenantkeys);
  ddtenant_t *ten;
  k = zlist_first(precalc);
  while (k) {
    ten = zhash_lookup(keys->tenantkeys, k);
    sodium_bin2hex(hex, 100, ten->boxk, crypto_box_BEFORENMBYTES);
    //		printf("key for cust %s = %s\n",k,hex);
    printf("\t %s", k);
    printf("\t precalc: %s", hex);
    printf("\t name: %s cookie %llu", ten->name, ten->cookie);
    k = zlist_next(precalc);
  }
  free(hex);
}

struct ddkeystate *read_ddkeys(char *filename, char *customer) {
  FILE *fp;
  int retval;
  int i = 0;
  int ispublic = 0;

  if (strcmp("public", customer) == 0)
    ispublic = 1;

  struct ddkeystate *ddkeys;
  struct stat stats;

  fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error opening file: %s, exiting..\n", filename);
    return NULL;
  }
  retval = stat(filename, &stats);
  if (retval != 0) {
    fprintf(stderr, "Could not stat %s, exiting..\n", filename);
    return NULL;
  }
  char *data = malloc(stats.st_size + 1);
  if (data == NULL) {
    fprintf(stderr, "Error allocating memory\n");
    return NULL;
  }
  retval = fread(data, stats.st_size, 1, fp);
  if (retval < 1) {
    fprintf(stderr, "Error reading file\n");
    return NULL;
  }
  struct json_object *parse_result = json_tokener_parse((char *)data);
  free(data);
  const char *unjson = json_object_get_string(parse_result);

  base64_decodestate state_in;
  ddkeys = calloc(1, sizeof(struct ddkeystate));
  ddkeys->clientkeys = zhash_new();

  if (ispublic) {
    json_object_object_foreach(parse_result, key, val) {
      if (strcmp(key, customer) == 0) {
        enum json_type o_type = json_object_get_type(val);
        json_object_object_foreach(val, key2, val2) {
          enum json_type o_type2 = json_object_get_type(val2);
          if (strcmp(key2, "publicpubkey") == 0) {
            base64_init_decodestate(&state_in);
            ddkeys->publicpubkey = calloc(1, 33);
            retval = base64_decode_block(json_object_get_string(val2),
                                         json_object_get_string_len(val2),
                                         ddkeys->publicpubkey, &state_in);
            if (retval != 32) {
              fprintf(stderr, "Error during base64_decode of %s\n", key2);
              return NULL;
            }
          } else if (strcmp(key2, "ddpubkey") == 0) {
            base64_init_decodestate(&state_in);
            ddkeys->ddpubkey = calloc(1, 33);
            retval = base64_decode_block(json_object_get_string(val2),
                                         json_object_get_string_len(val2),
                                         ddkeys->ddpubkey, &state_in);
            if (retval != 32) {
              fprintf(stderr, "Error during base64_decode of %s\n", key2);
              return NULL;
            }
          } else if (strcmp(key2, "privkey") == 0) {
            base64_init_decodestate(&state_in);
            ddkeys->privkey = calloc(1, 33);
            retval = base64_decode_block(json_object_get_string(val2),
                                         json_object_get_string_len(val2),
                                         ddkeys->privkey, &state_in);
            if (retval != 32) {
              fprintf(stderr, "Error during base64_decode of %s\n", key2);
              return NULL;
            }
          } else if (strcmp(key2, "pubkey") == 0) {
            base64_init_decodestate(&state_in);
            ddkeys->pubkey = calloc(1, 33);
            retval = base64_decode_block(json_object_get_string(val2),
                                         json_object_get_string_len(val2),
                                         ddkeys->pubkey, &state_in);
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
      if (strcmp(key3, customer) != 0) {
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
            retval = base64_decode_block(customer_pubkey,
                                         strlen(customer_pubkey), pubkey,
                                         &state_in);
            unsigned char *precalck = calloc(1, crypto_box_BEFORENMBYTES);
            retval =
                crypto_box_beforenm(precalck, pubkey, ddkeys->privkey);
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
        ddkeys->publicpubkey = calloc(1, 33);
        retval = base64_decode_block(json_object_get_string(val2),
                                     json_object_get_string_len(val2),
                                     ddkeys->publicpubkey, &state_in);
        if (retval != 32) {
          fprintf(stderr, "Error during base64_decode of %s\n", key2);
          return NULL;
        }
      } else if (strcmp(key2, "ddpubkey") == 0) {
        base64_init_decodestate(&state_in);
        ddkeys->ddpubkey = calloc(1, 33);
        retval = base64_decode_block(json_object_get_string(val2),
                                     json_object_get_string_len(val2),
                                     ddkeys->ddpubkey, &state_in);
        if (retval != 32) {
          fprintf(stderr, "Error during base64_decode of %s\n", key2);
          return NULL;
        }
      } else if (strcmp(key2, "privkey") == 0) {
        base64_init_decodestate(&state_in);
        ddkeys->privkey = calloc(1, 33);
        retval = base64_decode_block(json_object_get_string(val2),
                                     json_object_get_string_len(val2),
                                     ddkeys->privkey, &state_in);
        if (retval != 32) {
          fprintf(stderr, "Error during base64_decode of %s\n", key2);
          return NULL;
        }
      } else if (strcmp(key2, "pubkey") == 0) {
        base64_init_decodestate(&state_in);
        ddkeys->pubkey = calloc(1, 33);
        retval = base64_decode_block(json_object_get_string(val2),
                                     json_object_get_string_len(val2),
                                     ddkeys->pubkey, &state_in);
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

  // pre-calculate the shared keys for the DD bus itself
  ddkeys->ddboxk = calloc(1, crypto_box_BEFORENMBYTES);
  retval = crypto_box_beforenm(ddkeys->ddboxk, ddkeys->ddpubkey,
                               ddkeys->privkey);

  // pre-calculate the shared keys for the particular customer
  ddkeys->custboxk = calloc(1, crypto_box_BEFORENMBYTES);
  retval = crypto_box_beforenm(ddkeys->custboxk, ddkeys->pubkey,
                               ddkeys->privkey);

  // pre-calculate the shared keys for the public customer
  ddkeys->pubboxk = calloc(1, crypto_box_BEFORENMBYTES);
  retval = crypto_box_beforenm(ddkeys->pubboxk, ddkeys->publicpubkey,
                               ddkeys->privkey);

  return ddkeys;
}

void free_ddkeystate(ddkeystate_t *keys) {
  free(keys->hash);
  free(keys->privkey);
  free(keys->pubkey);
  free(keys->ddpubkey);
  free(keys->publicpubkey);
  free(keys);
}
