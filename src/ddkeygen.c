#include "../include/keys.h"
#include "cdecode.h"
#include "cencode.h"
#include <sodium.h>

ddbrokerkeys_t *broker_keys;
dd_keys_t *public_keys;

char *generate_broker_keys() { return "no"; }
char *generate_public_keys() { return "no"; }

void generate_client_key(char *name, unsigned char *broker_pubkey,
                         unsigned char *public_pubkey) {
  unsigned char alice_publickey[crypto_box_PUBLICKEYBYTES];
  unsigned char alice_secretkey[crypto_box_SECRETKEYBYTES];
  crypto_box_keypair(alice_publickey, alice_secretkey);

  char *hash = malloc(100);
  sodium_bin2hex(hash, 1000, alice_publickey, crypto_box_PUBLICKEYBYTES);

  base64_encodestate state_in;

  char *char_pubkey = calloc(1, 100);
  char *char_seckey = calloc(1, 100);
  char *char_pubpubkey = calloc(1, 100);
  char *char_bropubkey = calloc(1, 100);

  base64_init_encodestate(&state_in);
  base64_encode_block(alice_publickey, crypto_box_PUBLICKEYBYTES, char_pubkey,
                      &state_in);

  base64_init_encodestate(&state_in);
  base64_encode_block(alice_secretkey, crypto_box_SECRETKEYBYTES, char_seckey,
                      &state_in);
  base64_init_encodestate(&state_in);
  base64_encode_block(broker_pubkey, crypto_box_PUBLICKEYBYTES, char_bropubkey,
                      &state_in);
  base64_init_encodestate(&state_in);
  base64_encode_block(public_pubkey, crypto_box_PUBLICKEYBYTES, char_pubpubkey,
                      &state_in);

  struct json_object *jobj = json_object_new_object();

  json_object_object_add(jobj, "hash", json_object_new_string(hash));
  json_object_object_add(jobj, "privkey", json_object_new_string(char_seckey));
  json_object_object_add(jobj, "pubkey", json_object_new_string(char_pubkey));
  json_object_object_add(jobj, "publicpubkey",
                         json_object_new_string(char_pubpubkey));
  json_object_object_add(jobj, "ddpubkey",
                         json_object_new_string(char_bropubkey));

  char *ret = json_object_to_json_string(jobj);
  char *filename;
  asprintf(&filename, "%s-keys.json", name);
  FILE *fp = fopen(filename, "w");
  fwrite(ret, strlen(ret), 1, fp);
  printf("client: %s\n", ret);
}

int main(int argc, char **argv) {

  char *broker_key_file = NULL;
  char *public_key_file = NULL;
  char *tenant_names = NULL;
  bool generate_broker = false;
  bool generate_public = false;
  char c;
  while ((c = getopt(argc, argv, "C:c:B:b:P:p:")) != -1)
    switch (c) {
    case 'c':
      printf("Generate client keys\n");
      tenant_names = optarg;
      break;
    case 'b':
      printf("Generate broker keys\n");
      generate_broker = true;
      break;
    case 'p':
      printf("Generate public keys\n");
      generate_public = true;
      break;
    case 'B':
      printf("Read broker keys\n");
      broker_key_file = optarg;
      break;
    case 'P':
      printf("Read public keys\n");
      public_key_file = optarg;
      break;
    case '?':
      if (optopt == 'c' || optopt == 's')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint(optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      return 1;
    default:
      abort();
    }

  if (broker_key_file && !generate_broker) {
    broker_keys = read_ddbrokerkeys(broker_key_file);
  } else {
    broker_keys = generate_broker_keys();
  }

  if (public_key_file && !generate_public) {
    public_keys = dd_keys_new(public_key_file);
  } else {
    public_keys = generate_public_keys();
  }

  if (tenant_names && broker_key_file && public_key_file) {
    char *token;
    token = strtok(tenant_names, ",");
    while (token) {
      generate_client_key(token, broker_keys->pubkey, dd_keys_pub(public_keys));
      token = strtok(NULL, ",");
    }
  }
}
