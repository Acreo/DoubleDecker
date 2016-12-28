/*  =========================================================================
    dd_selftest.c - run selftests

    Runs all selftests.

    -------------------------------------------------------------------------
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

################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
    =========================================================================
*/

#include "dd_classes.h"

typedef struct {
    const char *testname;
    void (*test) (bool);
} test_item_t;

static test_item_t
all_tests [] = {
    { "dd_protocol", dd_protocol_test },
    { "dd_broker_keys", dd_broker_keys_test },
    { "base64", base64_test },
    { "dist_client", dist_client_test },
    { "local_client", local_client_test },
    { "local_broker", local_broker_test },
    { "subscription", subscription_test },
    { "client_table", client_table_test },
    { "util", util_test },
#ifdef DD_BUILD_DRAFT_API
    { "dd_broker", dd_broker_test },
    { "dd_client", dd_client_test },
    { "dd_keys", dd_keys_test },
    { "dd_topic", dd_topic_test },
    { "dd_client_actor", dd_client_actor_test },
#endif // DD_BUILD_DRAFT_API
    {0, 0}          //  Sentinel
};

//  -------------------------------------------------------------------------
//  Test whether a test is available.
//  Return a pointer to a test_item_t if available, NULL otherwise.
//

test_item_t *
test_available (const char *testname)
{
    test_item_t *item;
    for (item = all_tests; item->test; item++) {
        if (streq (testname, item->testname))
            return item;
    }
    return NULL;
}

//  -------------------------------------------------------------------------
//  Run all tests.
//

static void
test_runall (bool verbose)
{
    test_item_t *item;
    printf ("Running doubledecker selftests...\n");
    for (item = all_tests; item->test; item++)
        item->test (verbose);

    printf ("Tests passed OK\n");
}

int
main (int argc, char **argv)
{
    bool verbose = false;
    test_item_t *test = 0;
    int argn;
    for (argn = 1; argn < argc; argn++) {
        if (streq (argv [argn], "--help")
        ||  streq (argv [argn], "-h")) {
            puts ("dd_selftest.c [options] ...");
            puts ("  --verbose / -v         verbose test output");
            puts ("  --number / -n          report number of tests");
            puts ("  --list / -l            list all tests");
            puts ("  --test / -t [name]     run only test 'name'");
            puts ("  --continue / -c        continue on exception (on Windows)");
            return 0;
        }
        if (streq (argv [argn], "--verbose")
        ||  streq (argv [argn], "-v"))
            verbose = true;
        else
        if (streq (argv [argn], "--number")
        ||  streq (argv [argn], "-n")) {
            puts ("14");
            return 0;
        }
        else
        if (streq (argv [argn], "--list")
        ||  streq (argv [argn], "-l")) {
            puts ("Available tests:");
            puts ("    dd_broker");
            puts ("    dd_client");
            puts ("    dd_keys");
            puts ("    dd_topic");
            puts ("    dd_protocol");
            puts ("    dd_broker_keys");
            puts ("    base64");
            puts ("    dist_client");
            puts ("    local_client");
            puts ("    local_broker");
            puts ("    subscription");
            puts ("    client_table");
            puts ("    util");
            puts ("    dd_client_actor");
            return 0;
        }
        else
        if (streq (argv [argn], "--test")
        ||  streq (argv [argn], "-t")) {
            argn++;
            if (argn >= argc) {
                fprintf (stderr, "--test needs an argument\n");
                return 1;
            }
            test = test_available (argv [argn]);
            if (!test) {
                fprintf (stderr, "%s not valid, use --list to show tests\n", argv [argn]);
                return 1;
            }
        }
        else
        if (streq (argv [argn], "--continue")
        ||  streq (argv [argn], "-c")) {
#ifdef _MSC_VER
            //  When receiving an abort signal, only print to stderr (no dialog)
            _set_abort_behavior (0, _WRITE_ABORT_MSG);
#endif
        }
        else {
            printf ("Unknown option: %s\n", argv [argn]);
            return 1;
        }
    }
    if (test) {
        printf ("Running doubledecker test '%s'...\n", test->testname);
        test->test (verbose);
    }
    else
        test_runall (verbose);

    return 0;
}
/*
################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
*/
