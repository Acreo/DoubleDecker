/*  =========================================================================
    ddbroker - DoubleDecker broker program

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
    =========================================================================
*/

/*
@interface
Run a command line version of the DoubleDecker client

@header

In the CLI interface use the menu to select actions, e.g. to subscribe,
 publish, and send notifications.

@discuss
 Required options are -c, -n and -d.

  -d [ADDR] - Which broker to connect to
     Where [ADDR] can be e.g. tcp://127.0.0.1:5555 or ipc:///file

  -k [KEYFILE] - where to find the keys
     Where [KEYFILE] is the path to a JSON file containing the client keys
     These have to be generated with ddkeys.py

  -n [NAME] - set the name of the client, this has to be unique
     E.g. -n client2

@end
*/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "cparser.h"
#include "cparser_tree.h"
// hack to get around bug (?) in zproject
// <extra> seems not to allow .c files to be added to build
#include "cparser.c"
#include "cparser_fsm.c"
#include "cparser_io_unix.c"
#include "cparser_line.c"
#include "cparser_token.c"
#include "cparser_token_tbl.c"
#include "cparser_tree.c"


#include "doubledecker.h"

static dd_client_t *client;
cparser_t parser;

cparser_result_t
cparser_cmd_show_subscriptions(cparser_context_t *context) {
    printf("List of subscriptions:\n");
    const zlistx_t *subs = dd_client_get_subscriptions(client);
    dd_topic_t *item;
    while ((item = (dd_topic_t *) zlistx_next((zlistx_t *) subs))) {
        printf("Topic: %s Scope: %s Active: %d\n", dd_topic_get_topic(item), dd_topic_get_scope(item),
               dd_topic_get_active(item));
    }

    return CPARSER_OK;
}

cparser_result_t cparser_cmd_show_status(cparser_context_t *context) {
    if (dd_client_get_state(client) == DD_STATE_UNREG) {
        printf("DoubleDecker client: UNREGISTRED\n");
    } else if (dd_client_get_state(client) == DD_STATE_REGISTERED) {
        printf("DoubleDecker client: REGISTRED\n");
    } else if (dd_client_get_state(client) == DD_STATE_CHALLENGED) {
        printf("DoubleDecker client: AUTHENTICATING\n");
    } else {
        printf("DoubleDecker client: UNKNOWN!\n");
    }
    return CPARSER_OK;
}

cparser_result_t cparser_cmd_show_keys(cparser_context_t *context) {
    printf("Keys read from: %s\n", dd_client_get_keyfile(client));
    char *privkey = dd_client_get_privkey(client);
    printf("Private key: \t%s\n", privkey);
    free(privkey);
    char *pubkey = dd_client_get_pubkey(client);
    printf("Public key: \t%s\n", pubkey);
    free(pubkey);
    char *publickey = dd_client_get_publickey(client);
    printf("Pub public key: \t%s\n", publickey);
    free(pubkey);

    return CPARSER_OK;
}

cparser_result_t cparser_cmd_subscribe_topic_scope(cparser_context_t *context,
                                                   char **topic_ptr,
                                                   char **scope_ptr) {
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
    dd_client_subscribe(client, topic, scope);
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
    dd_client_unsubscribe(client, topic, scope);

    return CPARSER_OK;
}

cparser_result_t cparser_cmd_publish_topic_message(cparser_context_t *context,
                                                   char **topic_ptr,
                                                   char **message_ptr) {
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
    dd_client_publish(client, topic, (byte *) message, strlen(message));
    return CPARSER_OK;
}

cparser_result_t cparser_cmd_notify_destination_message(
        cparser_context_t *context, char **destination_ptr, char **message_ptr) {
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
    dd_client_notify(client, destination, (byte *) message, strlen(message));

    return CPARSER_OK;
}

cparser_result_t cparser_cmd_quit(cparser_context_t *context) {
    dd_client_destroy(&client);
    cparser_quit(context->parser);
    return CPARSER_OK;
}

cparser_result_t cparser_cmd_help(cparser_context_t *context) {
    return cparser_help_cmd(context->parser, NULL);
    return CPARSER_OK;
}

// callback functions
void on_reg(const char *client_name, dd_client_t *dd) {
    printf("\nRegistered with broker %s with name %s!\n", dd_client_get_endpoint(dd), client_name);
    snprintf(parser.prompt[0], sizeof(parser.prompt[0]), "%s>>",
             client_name);
    free((char*)client_name);
    fflush(stdout);
}

void on_discon(dd_client_t *dd) {
    printf("\nGot disconnected from broker %s!\n", dd_client_get_endpoint(dd));
    fflush(stdout);
}

void on_pub(const char *source, const char *topic, const byte *data, size_t length,
            dd_client_t *args) {
    printf("\nPUB S: %s T: %s L: %zu D: '%s'", source, topic, length, data);
    fflush(stdout);
}

void on_data(const char *source, const byte *data, size_t length, dd_client_t *args) {
    printf("\nDATA S: %s L: %zu D: '%s'", source, length, data);
    fflush(stdout);
}

void on_error(int error_code, const char *error_message, dd_client_t *args) {
    switch (error_code) {
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
    int debug = 0;
    int c;
    char *keyfile = NULL;
    char *connect_to = NULL;
    //  char *customer = NULL;
    char *client_name = NULL;

    opterr = 0;

    while ((c = getopt(argc, argv, "d:k:n:")) != -1) {
        switch (c) {
            case 'k':
                keyfile = optarg;
                break;
            case 'd':
                connect_to = optarg;
                break;
                /*    case 'c':
                customer = optarg;
                break; */
            case 'n':
                client_name = optarg;
                break;
            default:
                abort();
        }
    }
    if (client_name == NULL || keyfile == NULL ||
        connect_to == NULL) {
        printf("usage: ddclient -k <keyfile> -n <name> -d "
                       "<tcp/ipc url>\n");
        return 1;
    }
    client = dd_client_new(client_name, connect_to, keyfile, on_reg, on_discon,
                           on_data, on_pub, on_error);
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





