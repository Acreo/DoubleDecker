/*  =========================================================================
    dd_topic - Holds topics and scopes defining different subscriptions

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
    dd_topic - 
@discuss
@end
*/

#include "dd_classes.h"

//  Structure of our class
struct _dd_topic_t {
    char *topic;
    char *scope;
    bool active;
};


//  --------------------------------------------------------------------------
//  Create a new dd_topic

dd_topic_t *
dd_topic_new (void)
{
    dd_topic_t *self = (dd_topic_t *) zmalloc (sizeof (dd_topic_t));
    assert (self);
    //  Initialize class properties here
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the dd_topic

void
dd_topic_destroy (dd_topic_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        dd_topic_t *self = *self_p;
        //  Free class properties here
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
dd_topic_test (bool verbose)
{
    printf (" * dd_topic: ");

    //  @selftest
    //  Simple create/destroy test
    dd_topic_t *self = dd_topic_new ();
    assert (self);
    dd_topic_destroy (&self);
    //  @end
    printf ("OK\n");
}

const char *dd_topic_get_topic(dd_topic_t *sub) {
    return (const char *)sub->topic;
}
const char *dd_topic_get_scope(dd_topic_t *sub) {
    return (const char *)sub->scope;
}
bool dd_topic_get_active(dd_topic_t *sub) {
    return sub->active; 
}

void dd_topic_set_topic(dd_topic_t *sub, const char *topic) {
    sub->topic = (char*) topic;
}
void dd_topic_set_scope(dd_topic_t *sub, const char *scope) {
    sub->scope = (char*) scope;

}
void dd_topic_set_active(dd_topic_t *sub, bool active) {
    sub->active = active;
}
