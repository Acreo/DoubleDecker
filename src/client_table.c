/*  =========================================================================
    client_table - DoubleDecker client table

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
    client_table - DoubleDecker client table
@discuss
@end
*/

#include "dd_classes.h"
#include "tommy.h"
#include "tommyhashlin.c"
#include <ddlog.h>
//  Structure of our class
#define CLIENT_TABLE_TAG 0x63564822
struct _client_table_t {
  uint32_t tag;
  // sockid + cookie
  tommy_hashlin clients;
  // prefix_name
  tommy_hashlin rev_clients;
};

//  --------------------------------------------------------------------------
//  Create a new client_table

client_table_t *client_table_new(void) {
  client_table_t *self = (client_table_t *)zmalloc(sizeof(client_table_t));
  assert(self);
  tommy_hashlin_init(&self->clients);
  tommy_hashlin_init(&self->rev_clients);
  self->tag = CLIENT_TABLE_TAG;
  return self;
}
bool client_table_is(void *self) {
  client_table_t *a = (client_table_t *)self;
  if (a->tag == CLIENT_TABLE_TAG)
    return true;

  return false;
}

void free_client(void *ptr) {
  if (local_client_is(ptr)) {
    local_client_destroy((local_client_t **)&ptr);
  } else if (dist_client_is(ptr)) {
    dist_client_destroy((dist_client_t **)&ptr);
  } else if (local_broker_is(ptr)) {
    local_broker_destroy((local_broker_t **)&ptr);
  }
}
//  --------------------------------------------------------------------------
//  Destroy the client_table
void client_table_destroy(client_table_t **self_p) {
  assert(self_p);
  if (*self_p) {
    client_table_t *self = *self_p;

    tommy_hashlin_foreach(&self->rev_clients, free_client);
    tommy_hashlin_foreach(&self->clients, free_client);
    tommy_hashlin_done(&self->rev_clients);
    tommy_hashlin_done(&self->clients);
    free(self);
    *self_p = NULL;
  }
}

static int match_sockid(const void *arg, const void *obj) {
  const zframe_t *sockid = (const zframe_t *)arg;
  const local_client_t *localClient = (const local_client_t *)obj;
  if (local_client_is((void *)localClient)) {
    if (zframe_eq((zframe_t *)sockid, localClient->sockid) == true)
      return 0;
  } else {
    const local_broker_t *localBroker = (const local_broker_t *)obj;
    if (local_broker_is((void *)localBroker)) {
      if (zframe_eq((zframe_t *)sockid, localBroker->sockid) == true)
        return 0;
    } else {
      dd_error("client_table clients contain objects neither local_client_t or "
               "local_broker_t");
    }
  }

  return -1;
}
//  Check whether there's an client with this name
bool client_table_has_node_hash(client_table_t *self, zframe_t *sockid,
                                zframe_t *cookie, bool update_timer) {

  local_client_t *found =
      client_table_get_node_hash(self, sockid, cookie, update_timer);

  if (found == NULL)
    return false;

  if (update_timer)
    local_client_reset_timeout(found);

  return true;
}

//  If a client with this sockid and cookie exists, return it, otherwise NULL.
local_client_t *client_table_get_node_hash(client_table_t *self,
                                           zframe_t *sockid, zframe_t *cookie,
                                           bool update_timer) {
  tommy_hash_t key = util_sockid_cookie_hash(sockid, cookie);
  local_client_t *found = (local_client_t *)tommy_hashlin_search(
      &self->clients, match_sockid, sockid, key);

  if (found == 0)
    return NULL;
  if (local_client_is(found)) {
    if (update_timer)
      local_client_reset_timeout(found);

    return found;
  }
  dd_error("client_table_get_client_hash found object that is not local_client_t");
  return NULL;
}

//  Deletes the client with this sockid and cookie
bool client_table_del_node_hash(client_table_t *self, zframe_t *sockid,
                                zframe_t *cookie) {
  void *found = client_table_get_node_hash(self, sockid, cookie, false);
  if (found == NULL) {
    dd_error("Could not find node to delete") return false;
  }

  if (local_client_is(found)) {
    local_client_t *lcl = (local_client_t*) found;
    tommy_hashlin_remove_existing(&self->clients, &lcl->sockid_node);
    tommy_hashlin_remove_existing(&self->rev_clients, &lcl->string_node);
    local_client_destroy(&lcl);
    return true;
  } else if (dist_client_is(found)) {
    dist_client_t *dst = (dist_client_t*) found;
    tommy_hashlin_remove_existing(&self->rev_clients, &dst->string_node);
    dist_client_destroy(&dst);
    return true;
  } else if (local_broker_is(found)) {
    local_broker_t *broker = (local_broker_t*) found;
    tommy_hashlin_remove_existing(&self->clients, &broker->node);
    return true;
  }
  dd_error("Found node to delete but can't match it's type");
  return false;
}

static int match_prefix_name(const void *arg, const void *obj) {
//  const char *name = (const char *)arg;
  if (local_client_is((void *)obj)) {
    const local_client_t *localClient = (const local_client_t *)obj;
    if (streq(arg, localClient->prefix_name))
      return 0;

    return -1;
  } else if (dist_client_is((void *)obj)) {
    const dist_client_t *distClient = (const dist_client_t *)obj;
    if (streq(arg, distClient->name))
      return 0;

    return -1;
  } else {
    dd_error("client_table clients contain objects neither local_client_t or "
             "dist_client_t");
    return -1;
  }
}

//  Check whether there's an client with this name
bool client_table_has_node_name(client_table_t *self, const char *name) {
  local_client_t *found = (local_client_t*) client_table_get_node_name(self, name);

  if (found == NULL)
    return false;

  return true;
}

//  If a client with this name exists, return it, otherwise NULL.
//  Can return local_client or dist_client, use _is() to figure out which it is.
//  Returns NULL if not found
void *client_table_get_node_name(client_table_t *self, const char *name) {
  tommy_hash_t key_hash = util_string_hash(name);
  void *found = tommy_hashlin_search(&self->rev_clients, match_prefix_name,
                                     name, key_hash);

  if (found == 0)
    return NULL;
  if (local_client_is(found))
    return found;

  if (dist_client_is(found))
    return found;

  dd_error("client_table_get_client_hash found object that is not "
           "local_client_t or dist_client_t");
  return NULL;
}

//  Deletes the client with this name
int client_table_del_node_name(client_table_t *self, const char *name) {
  void *found = client_table_get_node_name(self, name);
  if (found == NULL)
    return false;
  if (local_client_is(found)) {
    local_client_t *lcl = (local_client_t *)found;
    tommy_hashlin_remove_existing(&self->clients, &lcl->sockid_node);
    tommy_hashlin_remove_existing(&self->rev_clients, &lcl->string_node);
    local_client_destroy(&lcl);
    return true;
  } else if (dist_client_is(found)) {
    dist_client_t *dst = (dist_client_t *)found;
    tommy_hashlin_remove_existing(&self->rev_clients, &dst->string_node);
    dist_client_destroy(&dst);
    return true;
  } else {
    return false;
  }
}

//  Deletes the client with this name
bool client_table_del_node(client_table_t *self, void *node) {
  if (node == NULL || self == NULL)
    return false;
  if (local_client_is(node)) {
    local_client_t *lcl = (local_client_t *)node;
    tommy_hashlin_remove_existing(&self->clients, &lcl->sockid_node);
    tommy_hashlin_remove_existing(&self->rev_clients, &lcl->string_node);
    local_client_destroy(&lcl);
    return true;
  } else if (dist_client_is(node)) {
    dist_client_t *dst = (dist_client_t *)node;
    tommy_hashlin_remove_existing(&self->rev_clients, &dst->string_node);
    dist_client_destroy(&dst);
    return true;
  } else if (local_broker_is(node)) {
    local_broker_t *broker = (local_broker_t *)node;
    tommy_hashlin_remove_existing(&self->clients, &broker->node);
    local_broker_destroy(&broker);
    return true;
  } else {
    return false;
  }
}
//  Insert a local client to the table
bool client_table_insert_local(client_table_t *self, local_client_t *client) {

  if (client_table_has_node_name(self, client->prefix_name)) {
    dd_error("Client %s already in name table!", client->prefix_name);
    return false;
  }
  if (client_table_has_node_hash(self, client->sockid, client->cookie, false)) {
    dd_error("Client %s already in sockid table!", client->prefix_name);
    return false;
  }

  tommy_key_t sockid_hash = local_client_sockid_hash(client);
  tommy_key_t string_hash = local_client_string_hash(client);
  tommy_hashlin_insert(&self->clients, &client->sockid_node, client,
                       sockid_hash);
  tommy_hashlin_insert(&self->rev_clients, &client->string_node, client,
                       string_hash);
  return true;
}
//  Insert a broker to the table
bool client_table_insert_broker(client_table_t *self, local_broker_t *broker) {

  if (client_table_has_node_hash(self, broker->sockid, broker->cookie, false)) {
    dd_error("Broker %s already in name table!", zframe_strhex(broker->sockid));
    return false;
  }
  tommy_key_t sockid_hash = local_broker_hash(broker);

  tommy_hashlin_insert(&self->clients, &broker->node, broker, sockid_hash);
  return true;
}

//  Insert a distant client to the table
bool client_table_insert_dist(client_table_t *self, dist_client_t *client) {
  if (client_table_has_node_name(self, client->name)) {
    dd_error("Client %s already in name table!", client->name);
    return false;
  }
  tommy_key_t string_hash = dist_client_hash(client);
  tommy_hashlin_insert(&self->rev_clients, &client->string_node, client,
                       string_hash);
  return true;
}

void client_table_foreach(client_table_t *self, client_table_fn function) {
  tommy_hashlin_foreach(&self->rev_clients, (tommy_foreach_func *)function);
}

//  Call method on each local client, with supplied argument
void client_table_foreach_arg(client_table_t *self,
                              client_table_arg_fn function, void *arg) {
  tommy_hashlin_foreach_arg(&self->rev_clients,
                            (tommy_foreach_arg_func *)function, arg);
}
void client_table_local_foreach_arg(client_table_t *self,
                                    client_table_arg_fn function, void *arg) {
  tommy_hashlin_foreach_arg(&self->clients, (tommy_foreach_arg_func *)function,
                            arg);
}

/**
 * Foreach function with an argument.
 * \param arg Pointer to a generic argument.
 * \param obj Pointer to the object to iterate.
 */
void clients_to_json(void *arg, void *obj) {
  if (local_client_is(obj)) {
    json_object *jobj = local_client_json((local_client_t*)obj);
    json_object_array_add((json_object*)arg, jobj);
  } else if (dist_client_is(obj)) {
    json_object *jobj = dist_client_json((dist_client_t*)obj);
    json_object_array_add((json_object*)arg, jobj);
  } else if (local_broker_is(obj)) {
    json_object *jobj =  local_broker_json((local_broker_t*)obj);
    json_object_array_add((json_object*)arg, jobj);
  }
}

void rev_clients_to_json(void *arg, void *obj) {
  if (local_client_is(obj)) {
    json_object *jobj = local_client_json((local_client_t*)obj);
    json_object_array_add((json_object*)arg, jobj);
  } else if (dist_client_is(obj)) {
    json_object *jobj = dist_client_json((dist_client_t*)obj);
    json_object_array_add((json_object*)arg, jobj);
  } else if (local_broker_is(obj)) {
    json_object *jobj = local_broker_json((local_broker_t*)obj);
    json_object_array_add((json_object*)arg, jobj);
  }
}

json_object_t *client_table_json(client_table_t *self) {
  json_object *jobj = json_object_new_object();
  json_object_object_add(
      jobj, "client_size",
      json_object_new_int64(tommy_hashlin_memory_usage(&self->clients)));
  json_object_object_add(
      jobj, "rev_client_size",
      json_object_new_int64(tommy_hashlin_memory_usage(&self->rev_clients)));

  json_object *jclients_array = json_object_new_array();
  tommy_hashlin_foreach_arg(&self->clients, clients_to_json, jclients_array);
  json_object_object_add(jobj, "clients", jclients_array);

  json_object *jrev_clients_array = json_object_new_array();
  tommy_hashlin_foreach_arg(&self->rev_clients, rev_clients_to_json,
                            jrev_clients_array);
  json_object_object_add(jobj, "rev_clients", jrev_clients_array);

  /* json_object_object_add(jobj, "tenant",
  json_object_new_string(self->tenant));
  json_object_object_add(jobj, "name", json_object_new_string(self->name));
  json_object_object_add(jobj, "prefix_name",
  json_object_new_string(self->prefix_name));
  json_object_object_add(jobj, "timeout", json_object_new_int(self->timeout));
  char *frame_str = zframe_strhex(self->sockid);
  json_object_object_add(jobj, "sockid", json_object_new_string(frame_str));
  free(frame_str);
  frame_str = zframe_strhex(self->cookie);
  json_object_object_add(jobj, "cookie", json_object_new_string(frame_str));
  free(frame_str); */
  return jobj;
}

void client_table_test(bool verbose) {
  printf(" * client_table: ");

  //  @selftest
  //  Simple create/destroy test
  client_table_t *self = client_table_new();
  assert(self);

  char *name = "testclientA";
  char *tenant = "testtenantA";
  uint64_t cookie = 94359873456834233L;
  zframe_t *cookie_frame = zframe_new(&cookie, sizeof(cookie));
  char id1[5] = {0x21, 0x22, 0x23, 0x24, 0x25};
  zframe_t *sockid = zframe_new(&id1[0], 5);
  local_client_t *local_client1 =
      local_client_new(name, tenant, sockid, cookie_frame);
  zframe_destroy(&sockid);
  zframe_destroy(&cookie_frame);
  id1[0] = 0x55;
  name = "testclientB";
  cookie_frame = zframe_new(&cookie, sizeof(cookie));
  sockid = zframe_new(&id1[0], 5);
  local_client_t *local_client2 =
      local_client_new(name, tenant, sockid, cookie_frame);
  zframe_destroy(&sockid);
  zframe_destroy(&cookie_frame);
  id1[0] = 0x56;
  name = "testclientC";
  cookie = 94359873456834244L;
  tenant = "testtenantB";
  cookie_frame = zframe_new(&cookie, sizeof(cookie));
  sockid = zframe_new(&id1[0], 5);
  local_client_t *local_client3 =
      local_client_new(name, tenant, sockid, cookie_frame);
  zframe_destroy(&sockid);
  zframe_destroy(&cookie_frame);

  client_table_insert_local(self, local_client1);
  client_table_insert_local(self, local_client2);
  client_table_insert_local(self, local_client3);

  sockid = zframe_new(&id1[0], 5);
  dist_client_t *dist_client1 = dist_client_new("public.test1", sockid, 1);
  zframe_destroy(&sockid);
  sockid = zframe_new(&id1[0], 5);
  dist_client_t *dist_client2 = dist_client_new("public.test2", sockid, 1);
  zframe_destroy(&sockid);
  sockid = zframe_new(&id1[0], 5);
  dist_client_t *dist_client3 = dist_client_new("a.test3", sockid, 1);
  zframe_destroy(&sockid);

  client_table_insert_dist(self, dist_client1);
  client_table_insert_dist(self, dist_client2);
  client_table_insert_dist(self, dist_client3);

  json_object *subjson = client_table_json(self);
  printf("Initial JSON: %s\n",
         json_object_to_json_string_ext(subjson, JSON_C_TO_STRING_PRETTY));
  json_object_put(subjson);

  local_client_t *lcl = (local_client_t*) client_table_get_node_name(self, "testtenantA.testclientB");
  assert(lcl == local_client2);
  lcl = (local_client_t*) client_table_get_node_name(self, "fdds.testclientB");
  assert(lcl == NULL);

  dist_client_t *dst = (dist_client_t*) client_table_get_node_name(self, "a.test3");
  assert(dst == dist_client3);
  assert(client_table_has_node_name(self, "asdfaf") == false);
  assert(client_table_has_node_name(self, "testtenantA.testclientA") == true);
  assert(client_table_has_node_name(self, "public.test2") == true);

  client_table_del_node_name(self, "public.test2");
  assert(client_table_has_node_name(self, "public.test2") == false);

  cookie = 94359873456834233L;
  cookie_frame = zframe_new(&cookie, sizeof(cookie));
  char id7[5] = {0x21, 0x22, 0x23, 0x24, 0x25};
  sockid = zframe_new(&id7[0], 5);

  assert(client_table_has_node_hash(self, sockid, cookie_frame, false) == true);
  client_table_del_node_hash(self, sockid, cookie_frame);
  assert(client_table_has_node_hash(self, sockid, cookie_frame, false) ==
         false);
  zframe_destroy(&cookie_frame);
  zframe_destroy(&sockid);

  client_table_del_node_name(self, "testtenantB.testclientC");
  client_table_del_node_name(self, "a.test3");

  subjson = client_table_json(self);
  printf("Middle JSON: %s\n",
         json_object_to_json_string_ext(subjson, JSON_C_TO_STRING_PRETTY));
  json_object_put(subjson);

  client_table_del_node_name(self, "testtenantA.testclientB");
  client_table_del_node_name(self, "public.test1");

  subjson = client_table_json(self);
  printf("Final JSON: %s\n",
         json_object_to_json_string_ext(subjson, JSON_C_TO_STRING_PRETTY));
  json_object_put(subjson);

  client_table_destroy(&self);
  //  @end
  printf("OK\n");
}
