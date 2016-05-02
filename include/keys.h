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

#ifndef _DDKEYS_H_
#define _DDKEYS_H_

#include "dd.h"

typedef struct ddbrokerkeys {
  // list of tenant names
  zlist_t *tenants;
  // broker private and public keys
  unsigned char *privkey;
  unsigned char *pubkey;
  // Broker pre-calc key
  unsigned char *ddboxk;
  // Precalculated tenant keys
  zhash_t *tenantkeys;
  // hash string
  char *hash;
  uint64_t cookie;
} ddbrokerkeys_t;

typedef struct tenantsinfo {
  char *name;
  uint64_t cookie;
  char *boxk;
} ddtenant_t;

dd_keys_t *dd_keys_new(const char *filename);
void dd_keys_destroy(dd_keys_t **self);
zhash_t *dd_keys_clients(dd_keys_t *self);
bool dd_keys_ispublic(dd_keys_t *self);
const uint8_t *dd_keys_custboxk(dd_keys_t *self);
const char *dd_keys_hash(dd_keys_t *self);
const uint8_t *dd_keys_pub(dd_keys_t *self);
const uint8_t *dd_keys_ddboxk(dd_keys_t *self);
const uint8_t *dd_keys_ddpub(dd_keys_t *self);
const uint8_t *dd_keys_pubboxk(dd_keys_t *self);
const uint8_t *dd_keys_publicpub(dd_keys_t *self);
const uint8_t *dd_keys_priv(dd_keys_t *self);

ddbrokerkeys_t *read_ddbrokerkeys(char *filename);
#endif





