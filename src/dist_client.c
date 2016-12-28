/*  =========================================================================
    dist_client - DoubleDecker distant client class

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
    dist_client - DoubleDecker distant client class
@discuss
@end
*/

#include "dd_classes.h"

//  --------------------------------------------------------------------------
//  Create a new dist_client
//  Does not take ownership of name or broker (i.e., you gotta free em)
dist_client_t *dist_client_new(const char *name, zframe_t *broker,
                               int distance) {
  dist_client_t *self = (dist_client_t *)zmalloc(sizeof(dist_client_t));
  assert(self);
  self->tag = DIST_CLIENT_TAG;
  self->name = strdup(name);
  self->distance = distance;
  self->broker = zframe_dup(broker);
  return self;
}

//  --------------------------------------------------------------------------
//  Destroy the dist_client

void dist_client_destroy(dist_client_t **self_p) {
  assert(self_p);
  if (*self_p) {
    dist_client_t *self = *self_p;
    self->tag = 0;
    free(self->name);
    free(self->broker);
    free(self);
    *self_p = NULL;
  }
}

//  --------------------------------------------------------------------------
//  Self test of this class

void dist_client_test(bool verbose) {
  printf(" * dist_client: ");

  //  @selftest
  //  Simple create/destroy test
  dist_client_t *self = dist_client_new("test", NULL, 0);
  assert(self);
  dist_client_destroy(&self);
  //  @end
  printf("OK\n");
}

//  Probe the supplied object, and report if it looks like a dd_dist_client_t.
bool dist_client_is(void *self) {
  dist_client_t *a =  (dist_client_t*) self;
  if (a->tag == DIST_CLIENT_TAG)
    return true;
  return false;
}

//  Get the full name of this client
const char *dist_client_get_name(dist_client_t *self) { return self->name; }

//  Get the broker sockid
zframe_t *dist_client_get_broker(dist_client_t *self) { return self->broker; }

//  Get the distance of this client
int dist_client_get_distance(dist_client_t *self) { return self->distance; }

//  Get the hash value of this distant client
uint32_t dist_client_hash(dist_client_t *self) {
  return util_string_hash(self->name);
}

//  Set the full name of this client
//  Takes ownership of the pointer!
void dist_client_set_name(dist_client_t *self, const char *name) {
  self->name = (char*) name;
}

//  Set the broker sockid
// Takes ownership of the pointer!
void dist_client_set_broker(dist_client_t *self, zframe_t *sockid) {
  self->broker = sockid;
}

//  Set the distance of this client
void dist_client_set_distance(dist_client_t *self, int distance) {
  self->distance = distance;
}
json_object_t *dist_client_json(dist_client_t *self) {
  json_object *jobj = json_object_new_object();
  json_object_object_add(jobj, "name", json_object_new_string(self->name));
  char *frame_str = zframe_strhex(self->broker);
  json_object_object_add(jobj, "broker", json_object_new_string(frame_str));
  free(frame_str);
  json_object_object_add(jobj, "hash", json_object_new_int(dist_client_hash(self)));
  return jobj;
}
