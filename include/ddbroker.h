/*  =========================================================================
    ddbroker - class description

    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of CZMQ, the high-level C binding for 0MQ:       
    http://czmq.zeromq.org.                                            
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

#ifndef DDBROKER_H_INCLUDED
#define DDBROKER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Create a new ddbroker
DD_EXPORT ddbroker_t *
    ddbroker_new (void);

//  Destroy the ddbroker
DD_EXPORT void
    ddbroker_destroy (ddbroker_t **self_p);

//  Self test of this class
DD_EXPORT void
    ddbroker_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
