/*  =========================================================================
    doubledecker - Project

    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of CZMQ, the high-level C binding for 0MQ:       
    http://czmq.zeromq.org.                                            
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

#ifndef DOUBLEDECKER_H_H_INCLUDED
#define DOUBLEDECKER_H_H_INCLUDED

//  Include the project library file
#include "dd_library.h"
#ifndef __USE_GNU
#define __USE_GNU
#endif


// state definitions
// -----------------

// Client/broker not registered
#define DD_STATE_UNREG 1
// Broker is the root broker
#define DD_STATE_ROOT 2
// In the process of exiting
#define DD_STATE_EXIT 3
// In the process of authentication with a broker
#define DD_STATE_CHALLENGED 4
// Registered with a broker
#define DD_STATE_REGISTERED 5

// Error codes
// -----------

// Registration failed
#define DD_ERROR_REGFAIL 1
// Message destination doesn't exist
#define DD_ERROR_NODST 2
// Wrong protocol version
#define DD_ERROR_VERSION 3


#endif
