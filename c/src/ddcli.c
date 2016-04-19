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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "cparser.h"
#include "cparser_priv.h"
#include "cparser_token.h"
#include "cparser_tree.h"
#include "dd.h"

static ddclient_t *client;

cparser_result_t

cparser_cmd_show_subscriptions(cparser_context_t *context) {
  printf("List of subscriptions:\n");
  sublist_print(client);
  return CPARSER_OK;
}

cparser_result_t cparser_cmd_show_status(cparser_context_t *context) {
  if (client->state == DD_STATE_UNREG) {
    printf("DoubleDecker client: UNREGISTRED\n");
  } else if (client->state == DD_STATE_REGISTERED) {
    printf("DoubleDecker client: REGISTRED\n");
  } else if (client->state == DD_STATE_CHALLENGED) {
    printf("DoubleDecker client: AUTHENTICATING\n");
  } else {
    printf("DoubleDecker client: UNKNOWN!\n");
  }
  return CPARSER_OK;
}

cparser_result_t cparser_cmd_show_keys(cparser_context_t *context) {
  char hex[100];
  printf("Keys read from: %s\n", client->keyfile);

  printf("Private key: \t%s\n",
         sodium_bin2hex(hex, 100, client->keys->privkey,
                        crypto_box_SECRETKEYBYTES));
  printf("Public key: \t%s\n",
         sodium_bin2hex(hex, 100, client->keys->pubkey,
                        crypto_box_PUBLICKEYBYTES));
  printf("Pub public key: \t%s\n",
         sodium_bin2hex(hex, 100, client->keys->publicpubkey,
                        crypto_box_PUBLICKEYBYTES));

  printf("Pub shared key: \t%s\n",
         sodium_bin2hex(hex, 100, client->keys->pubboxk,
                        crypto_box_BEFORENMBYTES));
  printf("Broker shared key: \t%s\n",
         sodium_bin2hex(hex, 100, client->keys->ddboxk,
                        crypto_box_BEFORENMBYTES));
  printf("Tenant shared key: \t%s\n",
         sodium_bin2hex(hex, 100, client->keys->custboxk,
                        crypto_box_BEFORENMBYTES));

  zlist_t *precalc = zhash_keys(client->keys->clientkeys);
  unsigned char *sharedk;
  char *k = NULL;
  k = zlist_first(precalc);
  while (k) {
    sharedk = zhash_lookup(client->keys->clientkeys, k);
    if (sharedk) {
      printf("Pub-Ten %s shared key: \t%s\n", k,
             sodium_bin2hex(hex, 100, sharedk, crypto_box_BEFORENMBYTES));
    }
    k = zlist_next(precalc);
  }

  return CPARSER_OK;
}

cparser_result_t
cparser_cmd_subscribe_topic_scope(cparser_context_t *context,
                                  char **topic_ptr, char **scope_ptr) {
  char *topic;
  char *scope;
  if (topic_ptr)
    topic = *topic_ptr;
  else {
    printf("error: subscribe 'topic' 'ALL/REGION/CLUSTER/NODE/NOSCOPE, "
           "1/2/3'\n");
    return CPARSER_NOT_OK;
  }
  if (scope_ptr)
    scope = *scope_ptr;
  else {
    printf("error: subscribe 'topic' 'ALL/REGION/CLUSTER/NODE/NOSCOPE, "
           "1/2/3'\n");
    return CPARSER_NOT_OK;
  }
  client->subscribe(topic, scope, client);
  return CPARSER_OK;
}

cparser_result_t
cparser_cmd_no_subscribe_topic_scope(cparser_context_t *context,
                                     char **topic_ptr, char **scope_ptr) {
  char *topic;
  char *scope;
  if (topic_ptr)
    topic = *topic_ptr;
  else {
    printf("error: no subscribe 'topic' 'ALL/REGION/CLUSTER/NODE/NOSCOPE, "
           "1/2/3'\n");
    return CPARSER_NOT_OK;
  }
  if (scope_ptr)
    scope = *scope_ptr;
  else {
    printf("error: no subscribe 'topic' 'ALL/REGION/CLUSTER/NODE/NOSCOPE, "
           "1/2/3'\n");
    return CPARSER_NOT_OK;
  }
  client->unsubscribe(topic, scope, client);

  return CPARSER_OK;
}

cparser_result_t
cparser_cmd_publish_topic_message(cparser_context_t *context,
                                  char **topic_ptr, char **message_ptr) {
  char *topic;
  char *message;
  if (topic_ptr)
    topic = *topic_ptr;
  else {
    printf("error: publish 'topic' 'message'\n");
    return CPARSER_NOT_OK;
  }
  if (message_ptr)
    message = *message_ptr;
  else {
    printf("error: publish 'topic' 'message'\n");
    return CPARSER_NOT_OK;
  }
  // +1 for \0 in strlen
  client->publish(topic, message, strlen(message) , client);
  return CPARSER_OK;
}

cparser_result_t
cparser_cmd_notify_destination_message(cparser_context_t *context,
                                       char **destination_ptr,
                                       char **message_ptr) {
  char *destination;
  char *message;
  if (destination_ptr)
    destination = *destination_ptr;
  else {
    printf("error: notify 'destination' 'message'\n");
    return CPARSER_NOT_OK;
  }
  if (message_ptr)
    message = *message_ptr;
  else {
    printf("error: notify 'destination' 'message'\n");
    return CPARSER_NOT_OK;
  }
  // +1 for \0 in strlen
  client->notify(destination, message, strlen(message), client);

  return CPARSER_OK;
}

cparser_result_t cparser_cmd_quit(cparser_context_t *context) {
  client->shutdown(client);
  cparser_quit(context->parser);
  return CPARSER_OK;
}

cparser_result_t cparser_cmd_help(cparser_context_t *context) {
  return cparser_help_cmd(context->parser, NULL);
  return CPARSER_OK;
}

// callback functions
void on_reg(void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  printf("\nRegistered with broker %s!\n", dd->endpoint);
  fflush(stdout);
}

void on_discon(void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  printf("\nGot disconnected from broker %s!\n", dd->endpoint);
  fflush(stdout);
}

void on_pub(char *source, char *topic, unsigned char *data, int length,
            void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  printf("\nPUB S: %s T: %s L: %d D: '%s'", source, topic, length, data);
  fflush(stdout);
}

void on_data(char *source, unsigned char *data, int length, void *args) {
  ddclient_t *dd = (ddclient_t *)args;
  printf("\nDATA S: %s L: %d D: '%s'", source, length, data);
  fflush(stdout);
}

// void on_nodst(char *source, void *args) {
//         ddclient_t *dd = (ddclient_t *)args;
//         printf("\nNODST T: %s", source);
//         fflush(stdout);
// }

void on_error(int error_code, char* error_message, void* args){
  switch(error_code){
    case DD_ERROR_NODST:
      printf("Error - no destination: %s\n", error_message);
      break;
    case DD_ERROR_REGFAIL:
      printf("Error - registration failed: %s\n", error_message);
      break;
    case DD_ERROR_VERSION:
      printf("Error - version: %s\n", error_message);
      break;
    default:
      printf("Error - unknown error!\n");
      break;
  }
  fflush(stdout);
}

int main(int argc, char *argv[]) {
  cparser_t parser;
  cparser_result_t rc;
  int debug = 0;
  
  int i;
  char *rndstr;
  int index;
  int c;
  char *keyfile;
  char *connect_to;
  char *customer;
  char *client_name;
  
  opterr = 0;
  
  while ((c = getopt(argc, argv, "c:d:k:n:")) != -1) {
    switch (c) {
      case 'k':
        keyfile = optarg;
        break;
      case 'd':
        connect_to = optarg;
        break;
      case 'c':
        customer = optarg;
        break;
      case 'n':
        client_name = optarg;
        break;
      default:
        abort();
    }
  }
  if (client_name == NULL || customer == NULL || keyfile == NULL ||
      connect_to == NULL) {
    printf("usage: client -c <customer> -k <keyfile> -n <name> -d "
           "<tcp/ipc url>\n");
    return 1;
  }
  client = start_ddthread(1, client_name, customer, connect_to, keyfile,
                          on_reg, on_discon, on_data, on_pub, on_error);
  if (client == NULL) {
    printf("DD initialization failed!\n");
    return -1;
  } else {
    printf("DD initalization ok\n");
  }

  parser.cfg.root = &cparser_root;
  parser.cfg.ch_complete = '\t';

  parser.cfg.ch_erase = '\b';
  parser.cfg.ch_del = 127;
  parser.cfg.ch_help = '?';
  parser.cfg.flags = (debug ? CPARSER_FLAGS_DEBUG : 0);
  sprintf(&parser.cfg.prompt[0], "%s>> ", client_name);
  parser.cfg.fd = STDOUT_FILENO;
  cparser_io_config(&parser);

  if (CPARSER_OK != cparser_init(&parser.cfg, &parser)) {
    printf("Fail to initialize parser.\n");
    return -1;
  }
  cparser_run(&parser);
}
