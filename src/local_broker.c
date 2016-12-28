/*  =========================================================================
    local_broker - DoubleDecker local broker class

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
    local_broker - DoubleDecker local broker class
@discuss
@end
*/

#include "dd_classes.h"


//  --------------------------------------------------------------------------
//  Create a new local_broker

local_broker_t *
local_broker_new (zframe_t *sockid, zframe_t* cookie, int distance)
{
  local_broker_t *self = (local_broker_t *) zmalloc (sizeof (local_broker_t));
  assert (self);
  self->tag = LOCAL_BROKER_TAG;
  self->sockid = zframe_dup(sockid);
  self->cookie = zframe_dup(cookie);
  self->distance = distance;
  self->timeout = 0;
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the local_broker

void
local_broker_destroy (local_broker_t **self_p)
{
    assert (self_p);
    if (*self_p) {
      local_broker_t *self = *self_p;
      //  Free class properties here
      //  Free object itself
      zframe_destroy(&self->sockid);
      zframe_destroy(&self->cookie);
      self->tag = 0;
      free (self);
      *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

//  Probe the supplied object, and report if it looks like a dd_local_broker_t.
bool local_broker_is(void *self){
  local_broker_t *a = (local_broker_t*) self;
  if (a->tag == LOCAL_BROKER_TAG)
    return true;
  return false;
}

//  Get the cookie of this client
uint64_t  local_broker_get_cookie(local_broker_t *self){
  uint64_t *cook;
  cook = (uint64_t *)zframe_data(self->cookie);
  return *cook;
}

zframe_t*  local_broker_get_cookie_zframe(local_broker_t *self){
  return self->cookie;
}
//  Get the socket id of this client
zframe_t *local_broker_get_sockid(local_broker_t *self){
  return self->sockid;
}

//  Get the hash value of this distant client
uint32_t local_broker_hash(local_broker_t *self){
  return util_sockid_cookie_hash(self->sockid,self->cookie);
}

//  Set the cookie of this client
void local_broker_set_cookie(local_broker_t *self, uint64_t cookie){
  free(self->cookie);
  self->cookie = zframe_new(&cookie,sizeof(cookie));
}
//  Set the cookie of this client
void local_broker_set_cookie_zframe(local_broker_t *self, zframe_t * cookie){
  free(self->cookie);
  self->cookie = cookie;
}

//  Set the socket id of this client
void local_broker_set_sockid(local_broker_t *self, zframe_t *sockid){
  free(self->sockid);
  self->sockid = sockid;
}

//  Reset the timeout value for local client
void local_broker_reset_timeout(local_broker_t *self){
  self->timeout = 0;
}

//  Increment the timeout value for local client
int local_broker_increment_timeout(local_broker_t *self){
  self->timeout += 1;
  return self->timeout;
}

//  Check if the local client has timed out
bool local_broker_timed_out(local_broker_t *self){
  if(self->timeout >= 3)
    return true;
  return false;
}

json_object_t *local_broker_json(local_broker_t *self) {
  json_object *jobj = json_object_new_object();
  json_object_object_add(jobj, "timeout", json_object_new_int(self->timeout));
  char *frame_str = zframe_strhex(self->sockid);
  json_object_object_add(jobj, "sockid", json_object_new_string(frame_str));
  free(frame_str);
  frame_str = zframe_strhex(self->cookie);
  json_object_object_add(jobj, "cookie", json_object_new_string(frame_str));
  free(frame_str);
  json_object_object_add(jobj, "hash", json_object_new_int(local_broker_hash(self)));

  return jobj;
}


void
local_broker_test (bool verbose)
{
  printf (" * local_broker: ");

  uint64_t cookie = 94359873456834233L;
  zframe_t *cookie_frame = zframe_new(&cookie,sizeof(cookie));
  char id1[5] = { 0x21,0x22,0x23,0x24,0x25};
  zframe_t *sockid = zframe_new(&id1[0],5);
  local_broker_t *self = local_broker_new(sockid,cookie_frame,10);
  zframe_destroy(&sockid);
  zframe_destroy(&cookie_frame);
  json_object *subjson = local_broker_json(self);
  printf("JSON: %s\n",json_object_to_json_string(subjson));
  json_object_put(subjson);

  printf("cookie = %lu\n", local_broker_get_cookie(self));
  char *sockid_str = zframe_strhex(local_broker_get_sockid(self));
  printf("sockid = %s\n",sockid_str );
  free(sockid_str);
  printf("sockid hash = %u\n", local_broker_hash(self));
  local_broker_destroy (&self);
  //  @end
  printf ("OK\n");
}