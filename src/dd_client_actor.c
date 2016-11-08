/*  =========================================================================
    dd_client_actor - DoubleDecker client actor

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
    dd_client_actor - DoubleDecker client actor
@discuss
@end
*/

//#include <sublist.h>
#include "dd_classes.h"

//  Structure of our actor

struct _dd_client_actor_t {
    zsock_t *pipe;              //  Actor command pipe
    dd_client_t *dd_client;
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?
};

static void actor_con(void *args);
static void actor_discon(void *args);
static void actor_pub(char *source, char *topic, unsigned char *data, size_t length, void *args);
static void actor_data(char *source, unsigned char *data, size_t length, void *args);
static void actor_error(int error_code, char *error_message, void *args);
static int s_on_pipe_msg(zloop_t *loop, zsock_t *handle, void *args);

//  --------------------------------------------------------------------------
//  Create a new dd_client_actor instance

static dd_client_actor_t *
dd_client_actor_new (char *client_name, char *endpoint, char *keyfile)
{
    dd_client_actor_t *self = (dd_client_actor_t *) zmalloc (sizeof (dd_client_actor_t));
    assert (self);

    self->dd_client = dd_client_setup(client_name,endpoint,keyfile,
                                      actor_con,actor_discon, actor_data,actor_pub,actor_error);
    self->terminated = false;

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the dd_client_actor instance

static void
dd_client_actor_destroy (dd_client_actor_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        dd_client_actor_t *self = *self_p;
        dd_client_destroy(&self->dd_client);
        free (self);
        *self_p = NULL;
    }
}



//  --------------------------------------------------------------------------
//  This is the actor which runs in its own thread.
void
dd_client_actor_actor (zsock_t *pipe, void *args)
{
    dd_client_actor_t * self = args;

    if (!self)
        return;          //  Interrupted
    self->pipe = pipe;
    dd_client_add_pipe(self->dd_client, pipe, s_on_pipe_msg);

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    // run the dd_client process
    dd_client_thread(self->dd_client);

    // destroy it if it returns for some reason
    dd_client_actor_destroy (&self);
}

//  --------------------------------------------------------------------------
//  Self test of this actor.

void
dd_client_actor_test (bool verbose)
{
    printf (" * dd_client_actor: ");
    //  @selftest
    //  Simple create/destroy test
    char *client_name = "testcli";
    char *endpoint = "tcp://127.0.0.1:5555";
    char *keyfile = "keys/public.keys.json";

    // create an actor with a embedded dd_client
    dd_client_actor_t *actor_conf = dd_client_actor_new("testcliactor","tcp://localhost:5555","keys/public-keys.json");
    // run it
    zactor_t *dd_client_actor = zactor_new (dd_client_actor_actor, actor_conf);
    sleep(5);
    zactor_destroy (&dd_client_actor);
    //  @end

    printf ("OK\n");
}



void actor_con(void *args) {
    dd_client_t *self = (dd_client_t *)args;
    zsock_send(dd_client_get_pipe(self), "ss", "reg", dd_client_get_endpoint(self));
}

void actor_discon(void *args) {
    dd_client_t *self = (dd_client_t *)args;
    zsock_send(dd_client_get_pipe(self), "ss", "discon", dd_client_get_endpoint(self));
}

void actor_pub(char *source, char *topic, unsigned char *data, size_t length, void *args) {
    dd_client_t *self = (dd_client_t *)args;
    zsock_send(dd_client_get_pipe(self), "sssbb", "pub", source, topic, &length, sizeof(length),
               data, length);
}

void actor_data(char *source, unsigned char *data, size_t length, void *args) {
    dd_client_t *self = (dd_client_t *)args;
    zsock_send(dd_client_get_pipe(self), "ssbb", "data", source, &length, sizeof(length), data,
               length);
}

void actor_error(int error_code, char *error_message, void *args) {
    dd_client_t *self = (dd_client_t *)args;
    zsock_send(dd_client_get_pipe(self), "ssb", "error", error_message, &error_code,
               sizeof(error_code));
}

int s_on_pipe_msg(zloop_t *loop, zsock_t *handle, void *args) {
    dd_client_t *self = (dd_client_t *)args;
    zmsg_t *msg = zmsg_recv(handle);
    char *command = zmsg_popstr(msg);
    //  All actors must handle $TERM in this way
    // returning -1 should stop zloop_start and terminate the actor
    if (streq(command, "$TERM")) {
        free(command);
        zmsg_destroy(&msg);
        return -1;

    } else if (streq(command, "subscribe")) {
        char *topic = zmsg_popstr(msg);
        char *scope = zmsg_popstr(msg);
        dd_client_subscribe(self, topic, scope);
        free(topic);
        free(scope);
        free(command);
        zmsg_destroy(&msg);
    } else if (streq(command, "unsubscribe")) {
        char *topic = zmsg_popstr(msg);
        char *scope = zmsg_popstr(msg);
        dd_client_unsubscribe(self, topic, scope);
        free(topic);
        free(scope);
        free(command);
        zmsg_destroy(&msg);
    } else if (streq(command, "publish")) {
        char *topic = zmsg_popstr(msg);
        char *message = zmsg_popstr(msg);
        zframe_t *mlen = zmsg_pop(msg);
        uint32_t len = *((uint32_t *)zframe_data(mlen));
        dd_client_publish(self, topic, message, len);
        zframe_destroy(&mlen);
        free(topic);
        free(message);
        free(command);
        zmsg_destroy(&msg);

    } else if (streq(command, "notify")) {
        char *target = zmsg_popstr(msg);
        char *message = zmsg_popstr(msg);
        zframe_t *mlen = zmsg_pop(msg);
        uint32_t len = *((uint32_t *)zframe_data(mlen));
        dd_client_notify(self, target, message, len);
        zframe_destroy(&mlen);
        free(target);
        free(message);
        free(command);
        zmsg_destroy(&msg);
    } else {
        fprintf(stderr, "s_on_pipe_msg, got unknown command: %s\n", command);
        free(command);
        zmsg_destroy(&msg);
    }
    return 0;
}


// main actor
// motsvarar dd_client_actor_actor
// och dd_client_thread
//void dd_actor(zsock_t *pipe, void *args) {
//    dd_client_t *self = (dd_client_t *)args;
//    int rc;
//
//    zsock_signal(pipe, 0);
//    self->pipe = pipe;
//
//    self->socket = zsock_new_dealer(NULL);
//    if (!self->socket) {
//        fprintf(stderr, "DD: Error in zsock_new_dealer: %s\n", zmq_strerror(errno));
//        zsock_send(self->pipe, "ss", "$TERM", "Error creating socket");
//        dd_client_destroy(&self);
//        return;
//    }
//    rc = zsock_connect(self->socket, (const char *)self->endpoint);
//    if (rc != 0) {
//        fprintf(stderr, "DD: Error in zmq_connect: %s\n", zmq_strerror(errno));
//        zsock_send(self->pipe, "ss", "$TERM", "Connection failed");
//        dd_client_destroy(&self);
//        return;
//    }
//
//    self->keys = dd_keys_new(self->keyfile);
//    if (self->keys == NULL) {
//        fprintf(stderr, "DD: Error reading keyfile!\n");
//        zsock_send(self->pipe, "ss", "$TERM", "Missing keyfile");
//        dd_client_destroy(&self);
//        return;
//    }
//
//    self->sublist = sublist_new();
//
//    self->loop = zloop_new();
//    assert(self->loop);
//    self->registration_loop =
//            zloop_timer(self->loop, 1000, 0, s_ask_registration, self);
//    rc = zloop_reader(self->loop, self->socket, s_on_dealer_msg, self);
//    assert(rc != -1);
//    rc = zloop_reader(self->loop, pipe, s_on_pipe_msg, self);
//    assert(rc != -1);
//    while (rc == 0){
//        rc = zloop_start(self->loop);
//        if(rc == 0) {
//            fprintf(stderr,"DD:dd_actor:zloop_start returned 0, interrupted! - terminating(might not be the best choice)\n");
//            // terminate, maybe not always a good choice here :(
//            rc = -1;
//        } else if (rc == -1) {
//            //fprintf(stderr,"DD:dd_actor:zloop_start returned -1, cancelled by handler!\n");
//        }
//    }
//    //fprintf(stderr, "DD:dd_actor - calling dd_destroy\n");
//    dd_client_destroy(&self);
//}

//
//zactor_t *ddactor_new(char *client_name, char *endpoint, char *keyfile) {
//    // Make sure that ZMQ doesn't affect main process signal handling
//
//
//    zsys_init();
//    zsys_handler_reset();
//    dd_client_t *self = malloc(sizeof(dd_client_t));
//
//
//    self->client_name = (unsigned char *)strdup(client_name);
//    self->endpoint = (unsigned char *)strdup(endpoint);
//    self->keyfile = strdup(keyfile);
//    self->timeout = 0;
//    self->state = DD_STATE_UNREG;
//
//    self->pipe = NULL;
//    self->sublist = NULL;
//    self->loop = NULL;
//
//    randombytes_buf(self->nonce, crypto_box_NONCEBYTES);
//    self->on_reg = actor_con;
//    self->on_discon = actor_discon;
//    self->on_data = actor_data;
//    self->on_pub = actor_pub;
//    self->on_error = actor_error;
//
//
//    // until here it's identical with dd_client_new
//
//    zactor_t *actor = zactor_new(dd_actor, self);
//    return actor;
//}

