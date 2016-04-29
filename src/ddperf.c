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
/*
 * ddperf.c --- Filename: ddperf.c Description: Author: Pontus
 * Sköldström <ponsko@acreo.se> Created: fre mar 27 00:34:10 2015
 * (+0100) Last-Updated: By:
 */
#include <czmq.h>
#include <zmq.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "dd.h"

#define SERVER 1
#define CLIENT 2

#define THROUGHPUT 1
#define LATENCY 2
int state = DD_STATE_UNREG;
int latency = 0;
int mnum, msize;
int keep_going = 1;
int pps = 0;
int send_timer;
int burst = 1;
long int n_msg = 0, b_msg = 0, pn_msg = 0, pb_msg = 0;
long int tot_msg = 0;
long int tot_bytes = 0;
int timeout = 0;
int verbose = 0;

FILE *logfile;

char *keyfile = "/etc/doubledecker/public-keys.json";
void s_sendmsg(dd_t *dd);
void usage() {
  printf("ddperf - test DoubleDecker throughput\n");
  printf("  -c <address> - Act as client\n");
  printf("  -s <address> - Act as server\n");
  printf("  -m <int>     - Number of messages to send (client only)\n");
  printf("  -n <int>     - Size of messages sent (client only)\n");
  printf("  -l           - Enable latency measurements (server and client "
         "on same machine!)\n");
  printf("  -v           - Verbose\n");
}

void stop_program(int sig) {
  printf("Stop program called\n");
  fclose(logfile);
  exit(1);
}

void print_diff(struct timespec *start, struct timespec *end) {
  struct timespec temp;
  if ((end->tv_nsec - start->tv_nsec) < 0) {
    temp.tv_sec = end->tv_sec - start->tv_sec - 1;
    temp.tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
  } else {
    temp.tv_sec = end->tv_sec - start->tv_sec;
    temp.tv_nsec = end->tv_nsec - start->tv_nsec;
  }
  if (temp.tv_sec) {
    temp.tv_nsec += temp.tv_sec * 1000000000;
  }
  fprintf(logfile, "latency:  %ld nanosec\n", temp.tv_nsec);
}

static int s_print_throughput() {
  //  printf("s_print_throughpyut\n");
  long int msgs = n_msg - pn_msg;
  long int bytes = b_msg - pb_msg;
  char *bytesym = "b/s";
  char *bitsym = "bit/s";

  long int bits = bytes * 8;

  double dbyte, dbits;
  if (bytes > 1.0e12) {
    bytesym = "TB/s";
    dbyte = bytes / 1.0e12;
  } else if (bytes > 1.0e9) {
    bytesym = "GB/s";
    dbyte = bytes / 1.0e9;
  } else if (bytes > 1.0e6) {
    bytesym = "MB/s";
    dbyte = bytes / 1.0e6;
  } else if (bytes > 1.0e3) {
    bytesym = "KB/s";
    dbyte = bytes / 1.0e3;
  } else
    dbyte = (double)bytes;

  if (bits > 1.0e12) {
    bitsym = "Tbit/s";
    dbits = bits / 1.0e12;
  } else if (bits > 1.0e9) {
    bitsym = "Gbit/s";
    dbits = bits / 1.0e9;
  } else if (bits > 1.0e6) {
    bitsym = "Mbit/s";
    dbits = bits / 1.0e6;
  } else if (bits > 1.0e3) {
    bitsym = "Kbit/s";
    dbits = bits / 1.0e3;
  } else
    dbits = (double)bits;

  if (msgs > 0) {
    char *tstr = zclock_timestr();
    printf("%s -- %'ld MSG/s, %.2lf %s (%.2lf %s)\n", tstr, msgs, dbyte,
           bytesym, dbits, bitsym);
    zstr_free(&tstr);
  }
  tot_msg += n_msg;
  tot_bytes += b_msg;
  pn_msg = n_msg;
  pb_msg = b_msg;
  return 0;
}

static int s_print_stats() {
  printf("total messages: %'ld , bytes: %'ld\n", n_msg, b_msg);
  return 0;
}

void on_reg_server(void *args) {
  dd_t *dd = (dd_t *)args;
  printf("DDPerf server registered with broker %s!\n", dd_get_endpoint(dd));
}

static int s_interval_send(void *args) {
  int i = 0;
  dd_t *dd = (dd_t *)args;

  for (i = 0; i < burst; i++) {
    s_sendmsg(dd);
    if (zsys_interrupted)
      dd_destroy(&dd);
  }
  mnum -= burst;
  if (zsys_interrupted)
    dd_destroy(&dd);

  if (mnum <= 0) {
    printf("Test completed, waiting 2s before shutdown..\n");
    zclock_sleep(2000);
    dd_destroy(&dd);
  }
  return 1;
}

char *rndstr;
void s_sendmsg(dd_t *dd) {
  struct timespec sendt, recvt;
  if (latency) {
    clock_gettime(CLOCK_MONOTONIC, &sendt);
    if (verbose)
      printf("sending clock: s:%ld ns:%ld ", sendt.tv_sec, sendt.tv_nsec);

    dd_notify(dd, "ddperfsrv", (char *)&sendt, sizeof(struct timespec));
  } else {
    if (verbose)
      printf("sending message to ddperfsrv, size %d\n", msize);

    dd_notify(dd, "ddperfsrv", rndstr, msize);
  }
}
void on_reg_client(void *args) {
  dd_t *dd = (dd_t *)args;
  printf("DDPerf client registered with broker %s\n", dd_get_endpoint(dd));
  rndstr = malloc(msize + 1);
  int i;
  for (i = 0; i < msize; i++) {
    rndstr[i] = 'a';
  }
  rndstr[msize] = '\0';

  printf("Starting test..\n");
  if (pps == 0) {
    // try to get rid of everyting at once
    for (i = 0; i != mnum; i++) {
      s_sendmsg(dd);
      if (zsys_interrupted) {
        dd_destroy(&dd);
        break;
      }
    }

    // Shutdown down
    printf("Test completed, waiting 2s before shutdown..\n");
    zclock_sleep(2000);
    dd_destroy(&dd);

  } else {
    // start a timer with interval based on pps
    float t_interval = 1000.0 / (float)pps;
    int interval;
    // zloop_timer is per ms, add some burst logic to go above
    if (t_interval < 1.0) {
      interval = 1;
      burst = pps / 1000;
      printf("pps > 1000, setting interval 1ms, with %d burst\n", burst);
    } else {
      interval = t_interval;
      printf("setting interval %dms\n", interval);
    }
    while (!zsys_interrupted) {
      s_interval_send(dd);
      zclock_sleep(interval);
    }
  }
}

void on_discon(void *args) {
  dd_t *dd = (dd_t *)args;
  printf("\nGot disconnected from broker %s!\n", dd_get_endpoint(dd));
}

void on_pub(char *source, char *topic, unsigned char *data, int length,
            void *args) {
  dd_t *dd = (dd_t *)args;
  printf("\nPUB S: %s T: %s L: %d D: '%s'\n", source, topic, length, data);
}

void on_data_server(char *source, unsigned char *data, int length, void *args) {
  dd_t *dd = (dd_t *)args;
  struct timespec sendt, recvt;
  if (latency) {
    memcpy(&sendt, data, sizeof(struct timespec));
    clock_gettime(CLOCK_MONOTONIC, &recvt);

    uint64_t a = recvt.tv_sec - sendt.tv_sec;
    printf("%f ms\n",
           (((a * 1000000000) + recvt.tv_nsec) - sendt.tv_nsec) / 1e6);
  }
  //  free (source);
  b_msg += length;
  n_msg++;
  if (verbose)
    printf("DATA S: %s L: %d D: '%s'\n", source, length, data);
}

// void on_nodst(char *source, void *args) {
//   dd_t *dd = (dd_t *)args;
//   printf("\nNODST T: %s\n", source);
// }

void on_error(int error_code, char *error_message, void *args) {
  printf("Error %d : %s", error_code, error_message);
}

void start_server(char *address) {
  setlocale(LC_NUMERIC, "en_US.utf-8"); /* important */

  dd_t *client =
      dd_new("ddperfsrv", "public", address, keyfile, on_reg_server, on_discon,
             on_data_server, on_pub, on_error); // on_nodst);
  //  int timer_id = zloop_timer (client->loop, 1000, 0,
  //  s_print_throughput, NULL);
  //  int timer_id2 = zloop_timer (client->loop, 10000, 0, s_print_stat,
  //  NULL);

  int i = 0;
  while (dd_get_state(client) != DD_STATE_EXIT && !zsys_interrupted) {
    sleep(1);
    s_print_throughput();
    if (i == 10) {
      s_print_stats();
      i = 0;
    }
  }
  dd_destroy(&client);
}

void start_client(char *address, int message_num, int message_size) {
  mnum = message_num;
  msize = message_size;
  // uint64_t sendt;
  struct timespec sendt;
  setlocale(LC_NUMERIC, "en_US.utf-8"); /* important */

  printf("Starting ddperf client (num=%d, size=%d), registering at %s..\n",
         message_num, message_size, address);

  dd_t *client =
      dd_new("ddperfcli", "public", address, keyfile, on_reg_client, on_discon,
             on_data_server, on_pub, on_error); // on_nodst);
  while (dd_get_state(client) != DD_STATE_EXIT && !zsys_interrupted) {
    sleep(1);
  }
}

int main(int argc, char **argv) {

  int role = 0;
  char *address = NULL;
  int message_num = 1000;
  int message_size = 1000;
  int index;
  int c;

  opterr = 0;

  // these get replaced by zmq ctx?
  signal(SIGTERM, stop_program);
  signal(SIGINT, stop_program);

  logfile = fopen("ddperf.log", "w+");

  while ((c = getopt(argc, argv, "m:n:s:c:vlp:")) != -1)
    switch (c) {
    case 'm':
      message_num = atoi(optarg);
      break;
    case 'n':
      message_size = atoi(optarg);
      break;
    case 'c':
      if (role != 0) {
        usage();
        return 1;
      }
      address = optarg;
      role = CLIENT;
      break;
    case 's':
      if (role != 0) {
        usage();
        return 1;
      }
      address = optarg;
      role = SERVER;
      break;
    case 'v':
      verbose = 1;
      break;
    case 'l':
      latency = 1;
      break;
    case 'p':
      pps = atoi(optarg);
      break;
    case '?':
      if (optopt == 'c' || optopt == 's')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint(optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      return 1;
    default:
      abort();
    }

  if (address == NULL) {
    usage();
    return 1;
  }

  if (role == SERVER) {
    start_server(address);
  } else {
    start_client(address, message_num, message_size);
  }

  return 0;
}
