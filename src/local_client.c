/*  =========================================================================
    local_client - DoubleDecker local client class

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
    local_client - DoubleDecker local client class
@discuss
@end
*/
#define _GNU_SOURCE 1
#include <stdio.h>
#include "dd_classes.h"

//  --------------------------------------------------------------------------
//  Create a new local_client
// does not take ownership of parameters!
local_client_t *local_client_new(const char *name, const char *tenant,
                                 zframe_t *sockid, zframe_t *cookie) {
  local_client_t *self = (local_client_t *)zmalloc(sizeof(local_client_t));
  assert(self);
  self->tag = LOCAL_CLIENT_TAG;
  self->name =  strdup((char*) name);
  self->tenant = strdup((char*) tenant);
  self->sockid = zframe_dup(sockid);
  self->cookie = zframe_dup(cookie);
  assert(-1 != asprintf(&self->prefix_name,"%s.%s",self->tenant, self->name));
  self->timeout = 0;
  return self;
}

//  --------------------------------------------------------------------------
//  Destroy the local_client
void local_client_destroy(local_client_t **self_p) {
  assert(self_p);
  if (*self_p) {
    local_client_t *self = *self_p;
    self->tag = 0;
    free(self->name);
    free(self->tenant);
    free(self->prefix_name);
    zframe_destroy(&self->sockid);
    zframe_destroy(&self->cookie);
    free(self);
    *self_p = NULL;
  }
}


//  Probe the supplied object, and report if it looks like a dd_local_client_t.
bool local_client_is(void *self){
    local_client_t *a = (local_client_t*) self;
    if (a->tag == LOCAL_CLIENT_TAG)
        return true;
    return false;
}

//  Get the short name of this client
const char *local_client_get_name(local_client_t *self){
    return self->name;
}

//  Get the full name of this client
const char *local_client_get_prefix_name(local_client_t *self){
    return self->prefix_name;
}

//  Get the tenant name of this client
const char *local_client_get_tenant_name(local_client_t *self){
    return self->tenant;
}

//  Get the cookie of this client
uint64_t  local_client_get_cookie(local_client_t *self){
  uint64_t *cook;
  cook = (uint64_t *)zframe_data(self->cookie);
  return *cook;
}

zframe_t*  local_client_get_cookie_zframe(local_client_t *self){
 return self->cookie;
}
//  Get the socket id of this client
zframe_t *local_client_get_sockid(local_client_t *self){
    return self->sockid;
}

uint32_t local_client_string_hash(local_client_t *self){
   return util_string_hash(self->prefix_name);
}

//  Get the hash value of this distant client
uint32_t local_client_sockid_hash(local_client_t *self){
  return util_sockid_cookie_hash(self->sockid, self->cookie);
}

//  Set the short name of this client
void local_client_set_name(local_client_t *self, const char *name){
  free(self->name);
  self->name = strdup(name);
}

//  Set the full name of this client
void local_client_set_prefix_name(local_client_t *self,
                                  const char *prefix_name){
  free(self->prefix_name);
  self->prefix_name = strdup(prefix_name);
}

//  Set the tenant name of this client
void local_client_set_tenant_name(local_client_t *self,
                                  const char *tenant_name){
  free(self->tenant);
  self->tenant = strdup(tenant_name);
}

//  Set the cookie of this client
void local_client_set_cookie(local_client_t *self, uint64_t cookie){
  free(self->cookie);
  self->cookie = zframe_new(&cookie,sizeof(cookie));
}
//  Set the cookie of this client
void local_client_set_cookie_zframe(local_client_t *self, zframe_t * cookie){
  free(self->cookie);
  self->cookie = cookie;
}

//  Set the socket id of this client
void local_client_set_sockid(local_client_t *self, zframe_t *sockid){
  free(self->sockid);
  self->sockid = sockid;
}

//  Reset the timeout value for local client
void local_client_reset_timeout(local_client_t *self){
    self->timeout = 0;
}

//  Increment the timeout value for local client
int local_client_increment_timeout(local_client_t *self){
    self->timeout += 1;
  return self->timeout;
}

//  Check if the local client has timed out
bool local_client_timed_out(local_client_t *self){
    if(self->timeout >= 3)
        return true;
    return false;
}

json_object_t *local_client_json(local_client_t *self) {
  json_object *jobj = json_object_new_object();
  json_object_object_add(jobj, "tenant", json_object_new_string(self->tenant));
  json_object_object_add(jobj, "name", json_object_new_string(self->name));
  json_object_object_add(jobj, "prefix_name", json_object_new_string(self->prefix_name));
  json_object_object_add(jobj, "timeout", json_object_new_int(self->timeout));
  char *frame_str = zframe_strhex(self->sockid);
  json_object_object_add(jobj, "sockid", json_object_new_string(frame_str));
  free(frame_str);
  frame_str = zframe_strhex(self->cookie);
  json_object_object_add(jobj, "cookie", json_object_new_string(frame_str));
  free(frame_str);
  json_object_object_add(jobj, "strhash", json_object_new_int(local_client_string_hash(self)));
  json_object_object_add(jobj, "sockhash", json_object_new_int(local_client_sockid_hash(self)));

  return jobj;
}



//  --------------------------------------------------------------------------
//  Self test of this class

void local_client_test(bool verbose) {
  printf(" * local_client: ");

  //  @selftest
  //  Simple create/destroy test
  char *name = "testclient";
  char *tenant = "testtenant";
  uint64_t cookie = 94359873456834233L;
  zframe_t *cookie_frame = zframe_new(&cookie,sizeof(cookie));
  char id1[5] = { 0x21,0x22,0x23,0x24,0x25};
  zframe_t *sockid = zframe_new(&id1[0],5);
  local_client_t *self = local_client_new(name,tenant,sockid,cookie_frame);
  zframe_destroy(&sockid);
  zframe_destroy(&cookie_frame);
  json_object *subjson = local_client_json(self);
  printf("JSON: %s\n",json_object_to_json_string(subjson));
  json_object_put(subjson);

  printf("cookie = %lu\n", local_client_get_cookie(self));
  char *sockid_str = zframe_strhex(local_client_get_sockid(self));
  printf("sockid = %s\n",sockid_str );
  free(sockid_str);
  printf("name = %s\n", local_client_get_name(self));
  printf("prefix_name = %s\n", local_client_get_prefix_name(self) );
  printf("sockid hash = %u\n", local_client_sockid_hash(self));
  printf("string hash = %u\n", local_client_string_hash(self));

  assert(self);
  local_client_destroy(&self);
  //  @end
  printf("OK\n");
}