/*  =========================================================================
    dd_keys - class description

    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of CZMQ, the high-level C binding for 0MQ:       
    http://czmq.zeromq.org.                                            
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

#ifndef DD_KEYS_H_INCLUDED
#define DD_KEYS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Create a new dd_keys
dd_keys_t *dd_keys_new(const char *filename);

//  Destroy the dd_keys
DD_EXPORT void
    dd_keys_destroy (dd_keys_t **self_p);

//  Self test of this class
DD_EXPORT void
    dd_keys_test (bool verbose);

//  @end
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
void dd_keys_nonce_increment(unsigned char *, const size_t );
#ifdef __cplusplus
}
#endif

#endif
