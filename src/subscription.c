/*  =========================================================================
    subscription - DoubleDecker subscription class

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
    subscription - DoubleDecker subscription class
@discuss
@end
*/

#include "dd_classes.h"

//  --------------------------------------------------------------------------
//  Create a new subscription
// Does not take ownership of sockid
subscription_t *subscription_new(zframe_t *sockid) {
  subscription_t *self = (subscription_t *)zmalloc(sizeof(subscription_t));
  assert(self);
  //  Initialize class properties here
  self->tag = SUBSCRIPTION_TAG;
  self->sockid = zframe_dup(sockid);
  self->topics = zlist_new();
  zlist_comparefn(self->topics, (zlist_compare_fn*) strcmp);
  zlist_autofree(self->topics);
  return self;
}

//  --------------------------------------------------------------------------
//  Destroy the subscription

void subscription_destroy(subscription_t **self_p) {
  assert(self_p);
  if (*self_p) {
    subscription_t *self = *self_p;
    zframe_destroy(&self->sockid);
    zlist_destroy(&self->topics);
    free(self);
    *self_p = NULL;
  }
}
//  Destroy a subscription list
void subscription_destroy(subscription_t **self_p);

//  Probe the supplied object, and report if it looks like a dd_dist_client_t.
bool subscription_is(void *self) {
  subscription_t *a = (subscription_t*) self;
  if (a->tag == SUBSCRIPTION_TAG)
    return true;
  return false;
}

//  Get the list of topics
zlist_t *subscription_get_topics(subscription_t *self) {
  return self->topics;
}

//  Add a subscribed topic
// Does not take ownership of topic
bool subscription_add_topic(subscription_t *self, const char *topic) {
  if(zlist_exists(self->topics,(void*) topic))
    return false;

  if (zlist_append(self->topics, (void*) topic) > -1)
    return true;

  return false;
}

//  Remove a topic
bool subscription_del_topic(subscription_t *self, const char *topic) {
  if(zlist_exists(self->topics,(void*) topic)){
    //zlist_remove also frees the item if autofree is enabled
    zlist_remove(self->topics,(void*) topic);
    return true;
  }
  return false;
}

void subscription_print(subscription_t *self){
  zframe_print(self->sockid, "sockid");
  char * topic = (char*) zlist_first(self->topics);
  while(topic){
    printf("topic: %s\n",topic);
    topic = (char*) zlist_next(self->topics);
  }
}

json_object * subscription_json(subscription_t *self){
  struct json_object *sub = json_object_new_object();
  char * sockid_str = zframe_strhex(self->sockid);
  json_object_object_add(sub,"sockid",json_object_new_string(sockid_str));
  free(sockid_str);
  struct json_object *topics = json_object_new_array();
  char * topic = (char*) zlist_first(self->topics);
  while(topic) {
    json_object_array_add(topics, json_object_new_string(topic));
    topic = (char *) zlist_next(self->topics);
  }
  json_object_object_add(sub,"topics",topics);
  return sub;
}

//  --------------------------------------------------------------------------
//  Self test of this class

void subscription_test(bool verbose) {
  printf(" * subscription: ");

  char id1[5] = { 0x21,0x22,0x23,0x24,0x25};
  //  @selftest
  //  Simple create/destroy test
  zframe_t *sockid = zframe_new(&id1[0],5);
  subscription_t *self = subscription_new(sockid);
  zframe_destroy(&sockid);
  printf("added 4 topics\n");
  subscription_add_topic(self,"topicA");
  subscription_add_topic(self,"topicB");
  subscription_add_topic(self,"topicC");
  subscription_add_topic(self,"topicD");
  subscription_print(self);
  subscription_del_topic(self,"topicB");
  subscription_del_topic(self,"topicC");
  printf("deleted 2 topics\n");
  subscription_print(self);
  json_object *subjson = subscription_json(self);
  printf("JSON: %s\n",json_object_to_json_string(subjson));
  json_object_put(subjson);
  assert(self);
  subscription_destroy(&self);
  //  @end
  printf("OK\n");
}
