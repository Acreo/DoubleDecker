/*  =========================================================================
    util - DoubleDecker utils class

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
    util - DoubleDecker utils class
@discuss
@end
*/

#include "dd_classes.h"

//  Structure of our class

struct _util_t {
    int filler;     //  Declare class properties here
};


//  --------------------------------------------------------------------------
//  Create a new util

util_t *
util_new (void)
{
    util_t *self = (util_t *) zmalloc (sizeof (util_t));
    assert (self);
    //  Initialize class properties here
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the util

void
util_destroy (util_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        util_t *self = *self_p;
        //  Free class properties here
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
util_test (bool verbose)
{
    printf (" * util: ");

    //  @selftest
    //  Simple create/destroy test
    util_t *self = util_new ();
    assert (self);
    util_destroy (&self);
    //  @end
    printf ("OK\n");
}
uint32_t  util_string_hash (const char *data){
  uint32_t key_hash = 0;
  while (*data)
    key_hash = 33 * key_hash ^ *data++;

  return key_hash;
}


//  Calculate the hash of a sockid cookie combintion
uint32_t util_sockid_cookie_hash (zframe_t *sockid, zframe_t *cookie) {
    uint32_t key_hash = 0;
    size_t i = zframe_size(sockid);
    byte *data = zframe_data(sockid);
    while (i > 0) {
        i--;
        key_hash = 33 * key_hash ^ *data++;
    }
    i = zframe_size(cookie);
    data = zframe_data(cookie);
    while (i > 0) {
        i--;
        key_hash = 33 * key_hash ^ *data++;
    }
    return key_hash;
}