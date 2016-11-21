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
Messages from the actor may be:

  On error:
    {string "error", string error_msg, byte error_code}

  On disconnection from broker:
    {string "discon", string broker endpoint}

  On connection to broker established:
    {string "reg", string broker endpoint}

  On notification received:
    {string "data", string source, size_t length, byte data}

  On publication received:
    {string "pub", string source, string topic, size_t length, byte data}

Messages to the actor may be:

  To subscribe:
    {string "subscribe", string topic, string scope}

  To unsubscribe:
    {string "unsubscribe", string topic, string scope}

  To publish:
    {string "publish", string topic, byte[] message}

  To notify:
    {string "notify", string target, byte [] message}

Sending messages can be simplified using the dd_client_actor_* methods.
@end
*/

//#include <sublist.h>
#include <doubledecker.h>
#include "dd_client.h"

//  Structure of our actor

struct _dd_client_actor_t {
    zsock_t *pipe;              //  Actor command pipe
    dd_client_t *dd_client;
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?
};

static void actor_con(dd_client_t *self);
static void actor_discon(dd_client_t *self);
static void actor_pub(const char *source, const char *topic, const byte *data, size_t length, dd_client_t *self);
static void actor_data(const char *source, const byte *data, size_t length, dd_client_t *self);
static void actor_error(int error_code, const char *error_message, dd_client_t *self);
static int s_on_pipe_msg(zloop_t *loop, zsock_t *handle, void *args);

//  --------------------------------------------------------------------------
//  Create a new dd_client_actor instance

dd_client_actor_t *
dd_client_actor_new (const char *client_name, const char *endpoint, const char *keyfile)
{
    dd_client_actor_t *self = (dd_client_actor_t *) malloc (sizeof (dd_client_actor_t));
    assert (self);

    self->dd_client = dd_client_setup(client_name,endpoint,keyfile,
                                      actor_con,
                                      actor_discon,
                                      actor_data,
                                      actor_pub,
                                      actor_error);
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
dd_client_actor (zsock_t *pipe, void *args)
{
    dd_client_actor_t * self = (dd_client_actor_t*) args;

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
    dd_client_actor_t *actor_conf = dd_client_actor_new(client_name,endpoint,keyfile);
    // run it
    zactor_t *actor = zactor_new (dd_client_actor, actor_conf);
    sleep(5);
    zactor_destroy (&actor);
    //  @end

    printf ("OK\n");
}

//  Subscribe to a topic.
int dd_client_actor_subscribe (zactor_t *actor, const char *topic, const char *scope){
    return zsock_send(actor, "sss", "subscribe", topic, scope);
}

//  Unsubscribe from a topic.
int dd_client_actor_unsubscribe (zactor_t *actor, const char *topic, const char *scope){
    return zsock_send(actor, "sss", "unsubscribe", topic, scope);
}

//  Publish a message on a topic. It's the callers responsibility to free the message.
int dd_client_actor_publish (zactor_t *actor, const char *topic, const byte *message, size_t length){
    return zsock_send(actor, "sb", topic, message, length);
}

//  Send a notification to another DD client. It's the callers responsibility to free the message.
int dd_client_actor_notify (zactor_t *actor, const char *target, const byte *message, size_t length){
    return zsock_send(actor, "sb", target, message, length);
}

int s_on_pipe_msg(zloop_t *loop, zsock_t *handle, void *args) {
    dd_client_t *self = (dd_client_t* ) args;
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
        zframe_t *data =  zmsg_pop(msg);
        byte *message =  zframe_data(data);
        zframe_t *mlen = zmsg_pop(msg);
        uint32_t len = *((uint32_t *)zframe_data(mlen));
        dd_client_publish(self, topic, message, len);
        zframe_destroy(&mlen);
        zframe_destroy(&data);
        free(topic);
        free(message);
        free(command);
        zmsg_destroy(&msg);

    } else if (streq(command, "notify")) {
        char *target = zmsg_popstr(msg);
        zframe_t *data =  zmsg_pop(msg);
        byte *message = zframe_data(data);
        zframe_t *mlen = zmsg_pop(msg);
        uint32_t len = *((uint32_t *)zframe_data(mlen));
        dd_client_notify(self, target, message, len);
        zframe_destroy(&mlen);
        zframe_destroy(&data);
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

void actor_error(int error_code, const char *error_message, dd_client_t *self) {

    zsock_send(dd_client_get_pipe(self), "ssb", "error", error_message, &error_code,
               sizeof(error_code));
}

void actor_data(const char *source, const byte *data, size_t length, dd_client_t *self) {

    zsock_send(dd_client_get_pipe(self), "ssbb", "data", source, &length, sizeof(length), data,
               length);
}

void actor_pub(const char *source, const char *topic, const byte *data, size_t length, dd_client_t *self) {
    zsock_send(dd_client_get_pipe(self), "sssbb", "pub", source, topic, &length, sizeof(length),
               data, length);
}

void actor_discon(dd_client_t *self) {
    zsock_send(dd_client_get_pipe(self), "ss", "discon", dd_client_get_endpoint(self));
}

void actor_con(dd_client_t *self) {
    zsock_send(dd_client_get_pipe(self), "ss", "reg", dd_client_get_endpoint(self));
}
