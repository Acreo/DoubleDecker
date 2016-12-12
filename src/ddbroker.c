/*  =========================================================================
    ddbroker - DoubleDecker broker program

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
    =========================================================================
*/


/*
@interface
 Run a standalone DoubleDecker broker

@header
    Starts DoubleDecker broker
@discuss
 Required options are -r, -s and -k.

  -r [ADDR] - where to listen for clients and other brokers
     Where [ADDR] can be e.g. tcp://127.0.0.1:5555
     Multiple addresses with comma tcp://127.0.0.1:5555,ipc:///file

  -k [KEYFILE] - where to find the keys
     Where [KEYFILE] is the path to a JSON file containing the broker keys
     These have to be generated with ddkeys.py

  -s [SCOPE] - set the scope of the broker
      Scope of the broker, e.g. for region 1, cluster 2, node 3 it is "1/2/3"

 Optional options are -d, -k, -w, -f, -L , -D, and -S

  -d [ADDR] - set the dealer URI to connect to
     For example tcp://1.2.3.4:5555
     Dealer should be connected to Router of another broker

  -l [CHAR] - set the log level, e.g. "-l w"
     Where CHAR is "e" for ERROR,w:WARNING,n:NOTICE,i:INFO,d:DEBUG,q:QUIET

  -w [ADDR]
     Open a REST socket for debugging, [ADDR] can be e.g. tcp://127.0.0.1:8080

  -f [FILE]
     Read configuration [FILE]

  -L [FILE]
     Log to [FILE]

  -D - daemonize the broker

  -S - log to syslog

@end
*/

#define __USE_GNU 1
#define GNU_SOURCE 1
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include <dlfcn.h>

#include "doubledecker.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>

#ifdef PROFILE 
#define PTRACE_PIPENAME "TRACE"
#define PTRACE_REFERENCE_FUNCTION printf
#define REFERENCE_OFFSET "REFERENCE:"
#define FUNCTION_ENTRY "enter"
#define FUNCTION_EXIT "exit"
#define END_TRACE "EXIT"
#define __NON_INSTRUMENT_FUNCTION__ __attribute__((__no_instrument_function__))
#define PTRACE_OFF __NON_INSTRUMENT_FUNCTION__
#define STR(_x) #_x
#define DEF(_x) _x
#define GET(_x, _y) _x(_y)
#define TRACE __GNU_PTRACE_FILE__
FILE *trace_out;

#endif

FILE *logfp;
int daemonize = 0;

void usage() {
    char *t = "Usage: broker [OPTIONS] ...\n"
            "REQUIRED OPTIONS\n"
            "-r [ADDR]\n"
            "       For example tcp://127.0.0.1:5555\n"
            "       Multiple addresses with comma tcp://127.0.0.1:5555,ipc:///file\n"
            "       Router is where clients connect\n"
            "-k [FILE]\n"
            "       JSON file containing the broker keys\n"
            "-s [SCOPE]\n"
            "       Scope of the broker, e.g. for region 1, cluster 2, node 3\n"
            "       \"1/2/3\"\n"
            "       Can be set to \"auto\", in this case the broker will ask a higher level\n"
            "       to generate a scope based on it's own scope. If no higher layer brokers\n"
            "       exists, it will default to 0/0/0. For the assigned scope to be persisted\n"
            "       you need to use a config file (which is updated to hold the assigned scope)\n"
            "\n"
            "OPTIONAL OPTIONS\n"
            "-d [ADDR]\n"
            "       For example tcp://1.2.3.4:5555\n"
            "       Dealer should be connected to Router of another broker\n"
            "-l [CHAR]\n"
            "       e:ERROR,w:WARNING,n:NOTICE,i:INFO,d:DEBUG,q:QUIET\n"
            "-w [ADDR]\n"
            "       Open a REST socket, eg tcp://*:8080\n"
            "-f [FILE]\n"
            "       Read configuration file\n"
            "-L [FILE]\n"
            "       Log to file\n"
            "-D\n"
            "       Daemonize\n"
            "-S\n"
            "       Log to system log\n";

    printf("%s",t);
}

int s_ddactor_msg(zloop_t *loop, zsock_t *handle, void *arg) {
    zmsg_t *msg = zmsg_recv(handle);
    printf("s_ddactor_msg message %p from dd_broker_actor!\n", (void *) msg);
    if (msg != NULL) {
        zmsg_print(msg);
        zmsg_destroy(&msg);
    }
    return 0;
}

int get_config(dd_broker_t *self, char *conffile) {
    zconfig_t *root = zconfig_load(conffile);
    if (root == NULL) {
        fprintf(stderr, "Could not read configuration file \"%s\"\n", conffile);
        exit(EXIT_FAILURE);
    }
    zconfig_t *child = zconfig_child(root);
    while (child != NULL) {
        if (streq(zconfig_name(child), "dealer")) {
            dd_broker_set_dealer(self, zconfig_value(child));
        } else if (streq(zconfig_name(child), "scope")) {
            dd_broker_set_scope(self, zconfig_value(child));
        } else if (streq(zconfig_name(child), "router")) {
            dd_broker_add_router(self, zconfig_value(child));
        } else if (streq(zconfig_name(child), "rest")) {
            dd_broker_set_rest_uri(self, zconfig_value(child));
        } else if (streq(zconfig_name(child), "loglevel")) {
            dd_broker_set_loglevel(self, zconfig_value(child));
        } else if (streq(zconfig_name(child), "keyfile")) {
            dd_broker_set_keyfile(self, zconfig_value(child));
        } else if (streq(zconfig_name(child), "syslog")) {
            zsys_set_logsystem(true);
        } else if (streq(zconfig_name(child), "daemonize")) {
            daemonize = 1;
        } else if (streq(zconfig_name(child), "logfile")) {
            if (streq(zconfig_value(child), "off")) {
                logfp = NULL;
            } else {
                logfp = fopen(zconfig_value(child), "w");
                if (logfp == NULL) {
                    fprintf(stderr, "Cannot open logfile %s\n", zconfig_value(child));
                    perror("Logfile open");
                    exit(EXIT_FAILURE);
                }
            }
            zsys_set_logstream(logfp);
        } else {
            fprintf(stderr, "Unknown key in configuration file, \"%s\"",
                    zconfig_name(child));
        }
        child = zconfig_next(child);
    }
    zconfig_destroy(&root);
    dd_broker_set_config(self,conffile);
    return 0;
}

int main(int argc, char **argv) {

#ifdef PROFILE
    if ((trace_out = fopen("tracefile", "w+")) == NULL) {
          char *msg = strerror(errno);
          perror(msg);
          printf("[gnu_ptrace error]\n");
          return 0;
      }
#endif
    int c;
    //  char *configfile = NULL;
    zsys_init();
    zsys_set_logident("DD");
    dd_broker_t *broker = dd_broker_new();
    opterr = 0;

    for(int i = 0; i < argc; i++){
        if(streq(argv[i],"-D")){
            zsys_daemonize("/");
        }
    }
    while ((c = getopt(argc, argv, "d:r:l:k:s:h:f:w:DSL:")) != -1)
        switch (c) {
            case 'r':
                dd_broker_add_router(broker, optarg);
                break;
            case 's':
                dd_broker_set_scope(broker, optarg);
                break;
            case 'k':
                dd_broker_set_keyfile(broker, optarg);
                break;
            case 'h':
                usage();
                exit(EXIT_FAILURE);
                break;
            case 'd':
                dd_broker_set_dealer(broker, optarg);
                break;
            case 'l':
                dd_broker_set_loglevel(broker, optarg);
                break;
            case 'w':
                dd_broker_set_rest_uri(broker, optarg);
                break;
            case 'f':
                get_config(broker, optarg);
                break;
            case 'S':
                zsys_set_logsystem(true);
                break;
            case 'L':
                logfp = fopen(optarg, "w");
                if (logfp == NULL) {
                    fprintf(stderr, "Cannot open logfile %s\n", optarg);
                    perror("Logfile open");
                    exit(EXIT_FAILURE);
                }
                zsys_set_logstream(logfp);
                break;
            case 'D':
                daemonize = 1;
                break;
            case '?':
                if (optopt == 'c' || optopt == 's') {
                    printf("Option -%c requires an argument.\n", optopt);
                    usage();
                    exit(EXIT_FAILURE);
                } else if (isprint(optopt)) {
                    printf("Unknown option `-%c'.\n", optopt);
                    usage();
                    exit(EXIT_FAILURE);

                } else {
                    printf("Unknown option character `\\x%x'.\n", optopt);
                }
                return 1;
            default:
                printf("unknown argument %c\n", optopt);
        }



    zactor_t *actor = dd_broker_actor(broker);
    if (actor == NULL) {
        usage();
        exit(EXIT_FAILURE);
    }
    zloop_t *loop = zloop_new();
    int rc = zloop_reader(loop, (zsock_t *) actor, s_ddactor_msg, NULL);
    if (rc == -1) {
        fprintf(stderr, "Error adding reader to zloop!\n");
        exit(EXIT_FAILURE);
    }
    rc = zloop_start(loop);
    if (rc == -1) {
        fprintf(stderr, "Error starting zloop!\n");
        exit(EXIT_FAILURE);
    }

    while (!zsys_interrupted) {
        zmsg_t *msg = zmsg_recv(actor);
        if (msg != NULL) {
            zmsg_print(msg);
            zmsg_destroy(&msg);
        }
    }
    zactor_destroy(&actor);
    printf("Done, quitting!\n");
    return 1;
}

void ddbroker_test() {
    return;
}

//#define PROFILE
#ifdef PROFILE
void __cyg_profile_func_enter (void *this_fn, void *call_site) __attribute__((no_instrument_function));
void __cyg_profile_func_exit  (void *this_fn, void *call_site) __attribute__((no_instrument_function));





/** Final trace close */
static void __attribute__((__no_instrument_function__)) gnu_ptrace_close(void) {
    fprintf(stderr, "gnu_ptrace_close\n");
    fprintf(TRACE, END_TRACE " %ld\n", (long)getpid());

    if (TRACE != NULL)
        fclose(TRACE);
    return;
}

/** Trace initialization */
static int  __attribute__((__no_instrument_function__)) gnu_ptrace_init(void) {
    fprintf(stderr, "gnu_ptrace_init\n");
    struct stat sta;
    __GNU_PTRACE_FILE__ = NULL;

    /* See if a trace file exists */
    if (stat(PTRACE_PIPENAME, &sta) != 0) {
        /* No trace file: do not trace at all */
        return 0;
    } else {
        /* trace file: open up trace file */
        if ((TRACE = fopen(PTRACE_PIPENAME, "a")) == NULL) {
            char *msg = strerror(errno);
            perror(msg);
            printf("[gnu_ptrace error]\n");
            return 0;
        }

        fprintf(stderr, "%s %s %p\n", REFERENCE_OFFSET,
                GET(STR, PTRACE_REFERENCE_FUNCTION),
                (void *)GET(DEF, PTRACE_REFERENCE_FUNCTION));

        /* Tracing requested: a trace file was found */
        atexit(gnu_ptrace_close);
        return 1;
    }
}

/** Function called by every function event */
void  __attribute__((__no_instrument_function__)) gnu_ptrace(char *what, void *p) {
    static int first = 1;
    static int active = 1;
  fprintf(stderr, "gnu_ptrace %s %p\n", what, p);
    if (active == 0)
        return;

    if (first) {
        active = gnu_ptrace_init();
        first = 0;

        if (active == 0)
            return;
    }

    fprintf(TRACE, "%s %p\n", what, p);
    fflush(TRACE);
    return;
}
int call_depth = 0;
/** According to gcc documentation: called upon function entry */
struct timespec timers[100];
struct timespec temp;
__attribute__((__no_instrument_function__)) struct timespec diff(struct timespec start, struct timespec end)
{
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}
__attribute__((__no_instrument_function__)) void __cyg_profile_func_enter(void *this_fn, void *call_site) {
    Dl_info finfo;

    if(this_fn == fopen)
        return;
    if(this_fn == main)
        return;
    if(this_fn == printf)
        return;
    if(this_fn == stat)
        return;
    if(this_fn == fprintf)
        return;
    if(this_fn == dladdr)
        return;
    if(this_fn == __cyg_profile_func_enter)
        return;
    if(this_fn == __cyg_profile_func_exit)
        return;
    call_depth++;
    dladdr(this_fn, &finfo);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timers[call_depth]);

   if(finfo.dli_sname != NULL) {
       // print csv
       //fprintf(trace_out,"%2d,exit,%s, %li, %li\n", call_depth, finfo.dli_sname,tot.tv_sec, tot.tv_nsec);
        //fprintf(trace_out,"[%2d] \t->%*s%s \n", call_depth, call_depth*2," ",finfo.dli_sname);
    }

    //gnu_ptrace(FUNCTION_ENTRY, this_fn);
    (void)call_site;
}

/** According to gcc documentation: called upon function exit */
__attribute__((__no_instrument_function__)) void
__cyg_profile_func_exit(void *this_fn, void *call_site)  {
    Dl_info finfo;
    if(this_fn == fopen)
        return;
    if(this_fn == main)
        return;
    if(this_fn == printf)
        return;
    if(this_fn == fprintf)
        return;
    if(this_fn == dladdr)
        return;
    if(this_fn == stat)
        return;
    if(this_fn == __cyg_profile_func_enter)
        return;
    if(this_fn == __cyg_profile_func_exit)
        return;
    if(this_fn == diff)
        return;
    if(this_fn == clock_gettime)
        return;


    dladdr(this_fn, &finfo);
    struct timespec stop_t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop_t);
    struct timespec tot = diff(timers[call_depth],stop_t);


    if(finfo.dli_sname != NULL) {
        // print CSV
        fprintf(trace_out,"%2d,exit,%s, %li, %li\n", call_depth, finfo.dli_sname,tot.tv_sec, tot.tv_nsec);
        //fprintf(trace_out,"[%2d] \t<-%*s%s %'lis %'lins \n", call_depth, call_depth*2," ", finfo.dli_sname,tot.tv_sec, tot.tv_nsec);
   }
    call_depth--;
    //gnu_ptrace(FUNCTION_EXIT, this_fn);
    (void)call_site;
}

#endif // ifdef PROFILE

