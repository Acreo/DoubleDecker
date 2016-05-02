/* Local Variables:  */
/* flycheck-gcc-include-path:
 * "/home/eponsko/double-decker/c-version/include/" */
/* End:              */
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

 * broker.c --- Filename: broker.c Description: Initial idea for a C
 * implementation of double-decker based around czmq and cds_lfht cds_lfht
 * is a high-performance multi-thread supporting hashtable Idea is to
 * have one thread (main) recieving all messages which are pushed using
 * inproc threads to processing threads.  Processing threads then perform
 * lookups in the shared hashtables and forward to the zmqsockets (they
 * are threadsafe I hope..?) Hashtable and usersparce RCU library
 * implementation at: git://git.lttng.org/userspace-rcu.git
 * http://lwn.net/Articles/573431/ Author: Pontus Sköldström
 * <ponsko@acreo.se> Created: tis mar 10 22:31:03 2015 (+0100)
 * Last-Updated: By:
 */
#define _GNU_SOURCE
#include "dd.h"
void usage() {
  printf("broker -m <name> -d <dealer addr> -r <router addr>  -h "
         "[help]\n"
         "REQUIRED OPTIONS\n"
         "-r <router addr> e.g. tcp://127.0.0.1:5555\n"
         "   Multiple addresses with comma tcp://127.0.0.1:5555,ipc:///file\n"
         "   Router is where clients connect\n"
         "-k <keyfile>\n"
         "   JSON file containing the broker keys\n"
         "-s <scope>\n"
         "   scope ~ \"1/2/3\"\n"
         "OPTIONAL\n"
         "-d <dealer addr> e.g tcp://1.2.3.4:5555\n"
         "   Multiple addresses with comma tcp://127.0.0.1:5555,ipc:///file\n"
         "   Dealer should be connected to Router of another broker\n"
         "-l <loglevel> e:ERROR,w:WARNING,n:NOTICE,i:INFO,d:DEBUG,q:QUIET\n"
         "-w <rest addr> open a REST socket at <rest addr>, eg tcp://*:8080\n"
         "-f <config file> read config from config file\n"
         "-D  daemonize\n"
         "-v [verbose]\n"
         "-h [help]\n");
}

int main(int argc, char **argv) {

  int c;
  char *configfile = NULL;

  void *ctx = zsys_init();
  int daemonize = 0;
  zsys_set_logident("DD");
  dd_broker_t *broker = dd_broker_new();
  opterr = 0;
  while ((c = getopt(argc, argv, "d:r:l:k:s:hm:f:w:D")) != -1)
    switch (c) {
    case 'd':
      dd_broker_set_dealer(broker, optarg);
      break;
    case 'r':
      dd_broker_set_router(broker, optarg);
      break;
    case 'D':
      daemonize = 1;
      break;
    case 'h':
      usage();
      exit(EXIT_FAILURE);
      break;
    case 'k':
      dd_broker_set_keyfile(broker, optarg);
      break;
    case 'l':
      dd_broker_set_loglevel(broker, optarg);
      break;
    case 's':
      dd_broker_set_scope(broker, optarg);
      break;
    case 'w':
      dd_broker_set_rest(broker, optarg);
      break;
    case 'f':
      dd_broker_set_config(broker, optarg);
      break;
    case '?':
      if (optopt == 'c' || optopt == 's') {
        printf("Option -%c requires an argument.", optopt);
      } else if (isprint(optopt)) {
        printf("Unknown option `-%c'.", optopt);
      } else {
        printf("Unknown option character `\\x%x'.", optopt);
      }
      return 1;
    default:
      abort();
    }

  if (daemonize == 1) {
    daemon(0, 0);
  }

  // if no router in config or as cli, set default
  
  dd_broker_start(broker);
}

