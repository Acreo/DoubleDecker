/*  =========================================================================
    dd_topic - class description

    Copyright (c) the Contributors as noted in the AUTHORS file.       
    This file is part of CZMQ, the high-level C binding for 0MQ:       
    http://czmq.zeromq.org.                                            
                                                                       
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
    =========================================================================
*/

#ifndef DD_TOPIC_H_INCLUDED
#define DD_TOPIC_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Create a new dd_topic
DD_EXPORT dd_topic_t *
dd_topic_new(void);

//  Destroy the dd_topic
DD_EXPORT void
dd_topic_destroy(dd_topic_t **self_p);

//  Self test of this class
DD_EXPORT void
dd_topic_test(bool verbose);

DD_EXPORT const char *
dd_topic_get_topic(dd_topic_t *);

DD_EXPORT const char *
dd_topic_get_scope(dd_topic_t *);

DD_EXPORT char
dd_topic_get_active(dd_topic_t *);


DD_EXPORT void
dd_topic_set_topic(dd_topic_t *, char *);

DD_EXPORT void
dd_topic_set_scope(dd_topic_t *, char *);

DD_EXPORT void
dd_topic_set_active(dd_topic_t *, char);




//  @end

#ifdef __cplusplus
}
#endif

#endif
