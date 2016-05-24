#include <dd.h>
zactor_t *actor_1, *actor_2, *actor_3, *cli1_act, *cli2_act, *cli3_act;
dd_broker_t *rootbr, *br_1, *br_2;
zlist_t *test_list;
int test_case = 0;

int s_ddbroker_msg(zloop_t *loop, zsock_t *handle, void *arg) {
  dd_broker_t *self = (dd_broker_t *)arg;
  zmsg_t *msg = zmsg_recv(handle);
  printf("s_ddbroker_msg message %p !\n", msg);
  if (msg != NULL) {
    zmsg_print(msg);
    zframe_t *fr = zmsg_pop(msg);
    printf("s_ddbroker: %s\n", zframe_data(fr));
    zmsg_destroy(&msg);
    zloop_reader_end(loop, actor_1);
    zactor_destroy(&actor_1);
    return -1;
  }

  return 0;
}
int s_ddclient_msg(zloop_t *loop, zsock_t *handle, void *arg) {
  zactor_t *self = (zactor_t *)arg;
  char *cliname;
  if (self == cli1_act)
    cliname = "Client1";
  if (self == cli2_act)
    cliname = "Client2";
  if (self == cli3_act)
    cliname = "Client3";

  zmsg_t *msg = zmsg_recv(handle);
  char *event = zmsg_popstr(msg);
  // if successfully registered
  if (streq("reg", event)) {
    printf("tc: %d %s - Registered\n", test_case, cliname);
    free(event);
    if (self == cli1_act) {
      zsock_send(self, "sss", "subscribe", "testtopic1", "all");
      zsock_send(self, "sss", "subscribe", "testtopic2", "all");
      zsock_send(self, "sss", "subscribe", "testtopic3", "all");
    }
    if (self == cli2_act) {
      zsock_send(self, "sss", "subscribe", "testtopic2", "all");
      zsock_send(self, "sss", "subscribe", "testtopic3", "all");
    }
    if (self == cli3_act) {
      zsock_send(self, "sss", "subscribe", "testtopic3", "all");
    }

  } // if disconnected
  else if (streq("discon", event)) {
    printf("WARNING: tc: %d %s - Unregistered\n", test_case, cliname);
    free(event);
  } // if we recieived a publication
  else if (streq("pub", event)) {
    char *source = zmsg_popstr(msg);
    char *topic = zmsg_popstr(msg);
    zframe_t *mlen = zmsg_pop(msg);
    zframe_t *message = zmsg_pop(msg);
    uint64_t stop = zclock_usecs();

    uint64_t *start = zframe_data(message);
    // printf("%s - Got publication from %s on %s\n", cliname, source, topic);
    // printf("time: %d microsec\n", stop - *start);
    char *test_str;

    asprintf(&test_str, "tc: %d %s - %s:publish(%s) %lu", test_case, cliname,
             source, topic, stop - *start);
    zlist_append(test_list, test_str);
    free(test_str);
    free(event);
    free(topic);
    free(source);
    zframe_destroy(&message);
    zframe_destroy(&mlen);
  } else if (streq("data", event)) {
    char *source = zmsg_popstr(msg);
    zframe_t *mlen = zmsg_pop(msg);
    zframe_t *message = zmsg_pop(msg);
    uint64_t stop = zclock_usecs();

    uint64_t *start = zframe_data(message);
    //    printf("%s - Got notification from %s \n", cliname, source);
    //  printf("time: %d microsec\n", stop - *start);
    char *test_str;
    asprintf(&test_str, "tc: %d %s - %s:notify() %lu", test_case, cliname,
             source, stop - *start);
    zlist_append(test_list, test_str);
    free(test_str);
    free(event);
    free(message);
    free(source);
    zframe_destroy(&mlen);
  } else if (streq("$TERM", event)) {
    char *error = zmsg_popstr(msg);
    printf("tc: %d Terminating DD client %s\n", test_case, cliname);
    free(error);
    zmsg_destroy(&msg);
    free(event);
    return -1;
  }
  zmsg_destroy(&msg);
  return 0;
}

int s_do_action_actor(zloop_t *loop, int timer, void *arg) {
  test_case = arg;

  if (test_case == 1) {
    printf("######################\n");
    printf(" test one, PUBLISH from cli1\n");
    printf("######################\n");
    uint64_t clock = zclock_usecs();
    int len = sizeof(clock);
    int i;
    for (i = 0; i < 10; i++) {
      clock = zclock_usecs();
      zsock_send(cli1_act, "ssbb", "publish", "testtopic1", &clock,
                 sizeof(clock), &len, sizeof(len));
    }
    zsock_send(cli1_act, "ssbb", "publish", "testtopic2", &clock, sizeof(clock),
               &len, sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli1_act, "ssbb", "publish", "testtopic3", &clock, sizeof(clock),
               &len, sizeof(len));
  }
  if (test_case == 2) {
    printf("######################\n");
    printf(" test two, PUBLISH from cli2\n");
    printf("######################\n");
    uint64_t clock = zclock_usecs();
    int len = sizeof(clock);
    zsock_send(cli2_act, "ssbb", "publish", "testtopic1", &clock, sizeof(clock),
               &len, sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli2_act, "ssbb", "publish", "testtopic2", &clock, sizeof(clock),
               &len, sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli2_act, "ssbb", "publish", "testtopic3", &clock, sizeof(clock),
               &len, sizeof(len));
  }
  if (test_case == 3) {
    printf("######################\n");
    printf(" test three, PUBLISH from cli3\n");
    printf("######################\n");
    uint64_t clock = zclock_usecs();
    int len = sizeof(clock);
    zsock_send(cli3_act, "ssbb", "publish", "testtopic1", &clock, sizeof(clock),
               &len, sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli3_act, "ssbb", "publish", "testtopic2", &clock, sizeof(clock),
               &len, sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli3_act, "ssbb", "publish", "testtopic3", &clock, sizeof(clock),
               &len, sizeof(len));
  }

  if (test_case == 4) {
    printf("######################\n");
    printf(" test four, NOTIFY from cli1\n");
    printf("######################\n");
    uint64_t clock = zclock_usecs();
    int len = sizeof(clock);
    zsock_send(cli1_act, "ssbb", "notify", "cli1", &clock, sizeof(clock), &len,
               sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli1_act, "ssbb", "notify", "cli2", &clock, sizeof(clock), &len,
               sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli1_act, "ssbb", "notify", "cli3", &clock, sizeof(clock), &len,
               sizeof(len));
  }
  if (test_case == 5) {
    printf("######################\n");
    printf(" test five, NOTIFY from cli2\n");
    printf("######################\n");
    uint64_t clock = zclock_usecs();
    int len = sizeof(clock);
    zsock_send(cli2_act, "ssbb", "notify", "cli1", &clock, sizeof(clock), &len,
               sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli2_act, "ssbb", "notify", "cli2", &clock, sizeof(clock), &len,
               sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli2_act, "ssbb", "notify", "cli3", &clock, sizeof(clock), &len,
               sizeof(len));
  }
  if (test_case == 6) {
    printf("######################\n");
    printf(" test six, NOTIFY from cli3\n");
    printf("######################\n");
    uint64_t clock = zclock_usecs();
    int len = sizeof(clock);
    zsock_send(cli3_act, "ssbb", "notify", "cli1", &clock, sizeof(clock), &len,
               sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli3_act, "ssbb", "notify", "cli2", &clock, sizeof(clock), &len,
               sizeof(len));
    clock = zclock_usecs();
    zsock_send(cli3_act, "ssbb", "notify", "cli3", &clock, sizeof(clock), &len,
               sizeof(len));
  }
  if (test_case == 7) {
    printf("######################\n");
    printf(" test seven, random unsub from cli3\n");
    printf("######################\n");
    zsock_send(cli1_act, "sss", "subscribe", "testtopic1", "all");
    zsock_send(cli1_act, "sss", "subscribe", "testtopic2", "all");
    zsock_send(cli1_act, "sss", "subscribe", "testtopic3", "all");
    zsock_send(cli2_act, "sss", "subscribe", "testtopic1", "all");
    zsock_send(cli2_act, "sss", "subscribe", "testtopic2", "all");
    zsock_send(cli2_act, "sss", "subscribe", "testtopic3", "all");
    zsock_send(cli3_act, "sss", "subscribe", "testtopic1", "all");
    zsock_send(cli3_act, "sss", "subscribe", "testtopic2", "all");
    zsock_send(cli3_act, "sss", "subscribe", "testtopic3", "all");

    zsock_send(cli1_act, "sss", "subscribe", "fdasf", "all");
    zsock_send(cli1_act, "sss", "subscribe", "f", "all");
    zsock_send(cli1_act, "sss", "subscribe", "testtopic3", "region");
    zsock_send(cli2_act, "sss", "subscribe", "adsf", "all");
    zsock_send(cli2_act, "sss", "subscribe", "testtopic2", "cluster");
    zsock_send(cli2_act, "sss", "subscribe", "testsdftopic3", "all");
    zsock_send(cli3_act, "sss", "subscribe", "adsf", "all");
    zsock_send(cli3_act, "sss", "subscribe", "asdf", "all");
    zsock_send(cli3_act, "sss", "subscribe", "adff", "node");
  }

  if (test_case == 8) {
    printf("######################\n");
    printf(" test eight, dead root broker\n");
    printf("######################\n");
    zloop_reader_end(loop, actor_1);
    zactor_destroy(&actor_1);
  }
  if (test_case == 99) {
    printf("######################\n");
    printf("     TEST OVER     \n");
    printf("######################\n");
    char *t = zlist_first(test_list);
    while (t) {
      printf("%s\n", t);
      t = zlist_next(test_list);
    }
    zlist_autofree(test_list);
    zlist_destroy(&test_list);

    // interrupt ourselves
    kill(getpid(), SIGTERM);
  }
}

int main(int argc, char **argv) {
  int c;
  char *configfile = NULL;
  int rc;
  void *ctx = zsys_init();
  zsys_set_logident("testDD");
  zloop_t *loop = zloop_new();
  /* cli1_act = ddactor_new("cli1", "tcp://127.0.0.1:5555", */
  /*                        "/etc/doubledecker/a-keys.json"); */

  /* cli2_act = ddactor_new("cli2", "tcp://127.0.0.1:5566", */
  /*                        "/etc/doubledecker/a-keys.json"); */

  /* cli3_act = ddactor_new("cli3", "tcp://127.0.0.1:5577", */
  /*                        "/etc/doubledecker/a-keys.json"); */
  test_list = zlist_new();
  zlist_autofree(test_list);

  /* rc = zloop_reader(loop, cli1_act, s_ddclient_msg, cli1_act); */
  /* rc = zloop_reader(loop, cli2_act, s_ddclient_msg, cli2_act); */
  /* rc = zloop_reader(loop, cli3_act, s_ddclient_msg, cli3_act); */

  rootbr = dd_broker_new();
  dd_broker_add_router(rootbr, "tcp://*:5555");
  dd_broker_set_keyfile(rootbr, "/etc/doubledecker/broker-keys.json");
  dd_broker_set_scope(rootbr, "0/0/0");
  dd_broker_set_rest(rootbr, "tcp://*:9080");

  actor_1 = dd_broker_actor(rootbr);
  rc = zloop_reader(loop, actor_1, s_ddbroker_msg, rootbr);

  /* br_1 = dd_broker_new(); */
  /* dd_broker_add_router(br_1, "tcp://\*:5566"); */
  /* dd_broker_set_dealer(br_1, "tcp://127.0.0.1:5555"); */
  /* dd_broker_set_keyfile(br_1, "/etc/doubledecker/broker-keys.json"); */
  /* dd_broker_set_scope(br_1, "0/1/0"); */

  /* actor_2 = dd_broker_actor(br_1); */
  /* rc = zloop_reader(loop, actor_2, s_ddbroker_msg, br_1); */

  /* br_2 = dd_broker_new(); */
  /* dd_broker_add_router(br_2, "tcp://\*:5577"); */
  /* dd_broker_set_dealer(br_2, "tcp://127.0.0.1:5555"); */
  /* dd_broker_set_keyfile(br_2, "/etc/doubledecker/broker-keys.json"); */
  /* dd_broker_set_scope(br_2, "0/1/0"); */

  /* actor_3 = dd_broker_actor(br_2); */
  /* rc = zloop_reader(loop, actor_3, s_ddbroker_msg, br_2); */

  // Setup tests
  /* rc = zloop_timer(loop, 5000, 1, s_do_action_actor, 1); */
  /* rc = zloop_timer(loop, 6000, 1, s_do_action_actor, 2); */
  /* rc = zloop_timer(loop, 7000, 1, s_do_action_actor, 3); */
  /* rc = zloop_timer(loop, 8000, 1, s_do_action_actor, 4); */
  /* rc = zloop_timer(loop, 9000, 1, s_do_action_actor, 5); */
  /* rc = zloop_timer(loop, 10000, 1, s_do_action_actor, 6); */
  /* rc = zloop_timer(loop, 11000, 1, s_do_action_actor, 7); */
  /* rc = zloop_timer(loop, 12000, 1, s_do_action_actor, 1); */
  /* rc = zloop_timer(loop, 13000, 1, s_do_action_actor, 2); */
  /* rc = zloop_timer(loop, 14000, 1, s_do_action_actor, 3); */
  /* rc = zloop_timer(loop, 15000, 1, s_do_action_actor, 4); */
  /* rc = zloop_timer(loop, 16000, 1, s_do_action_actor, 5); */
  /* rc = zloop_timer(loop, 17000, 1, s_do_action_actor, 6); */
  /* rc = zloop_timer(loop, 18000, 1, s_do_action_actor, 8); */
  /* rc = zloop_timer(loop, 19000, 1, s_do_action_actor, 1); */
  /* rc = zloop_timer(loop, 20000, 1, s_do_action_actor, 2); */
  /* rc = zloop_timer(loop, 21000, 1, s_do_action_actor, 3); */
  /* rc = zloop_timer(loop, 22000, 1, s_do_action_actor, 99); */

  rc = zloop_start(loop);

  /* printf("broker_test.c: zloop_start %d\n", rc); */
  /* zloop_reader_end(loop, cli3_act); */
  /* zactor_destroy(&cli3_act); */

  /* zloop_reader_end(loop, cli1_act); */
  /* zactor_destroy(&cli1_act); */

  /* zloop_reader_end(loop, actor_2); */
  /* zactor_destroy(&actor_2); */

  /* zloop_reader_end(loop, cli2_act); */
  /* zactor_destroy(&cli2_act); */

  /* zloop_reader_end(loop, actor_3); */
  /* zactor_destroy(&actor_3); */


  printf("broker_test.c: waiting for cli1..\n");
  zclock_sleep(1000);
  zloop_destroy(&loop);
  printf("Waiting before zsys_shutdown..\n");

  printf("actors: %p %p %p\n", actor_1, actor_2, actor_3);
  printf("actors: %p %p %p\n", cli1_act, cli2_act, cli3_act);

  zclock_sleep(5000);
  zsys_shutdown();
  return 1;
}

void test_zrex() {
  zrex_t *rexscope = zrex_new("^/*(\\d+)/(\\d+)/(\\d+)/*$");
  // zrex_t *rexscope = zrex_new("((\\d+)/)");
  int i = 0;
  assert(zrex_valid(rexscope));
  run_zrex(rexscope, "1/2/3");
  run_zrex(rexscope, "/1/2/3");
  run_zrex(rexscope, "/1/2/3/");
  run_zrex(rexscope, "/133/3222/553/");
  run_zrex(rexscope, "/133aa/3222/553/");

  zrex_destroy(&rexscope);

  rexscope = zrex_new("^\\s*(\\S+)\\s+(\\S+)\\s+HTTP/(\\d)\\.(\\d)");
  run_zrex(rexscope, "GET / HTTP/1.0\r\n\r\n");
  run_zrex(rexscope, "GET /tesgf HTTP/1.0\r\n\r\n");
  run_zrex(rexscope, "GET /dsafaadf HTTP/1.0\r\n\r\n");
  run_zrex(rexscope, "GET /fa/adf/adf/adf/dfadf HTTP/1.0\r\n\r\n");
  run_zrex(rexscope, "GET /fa/asfd//adf/adf/adf/dfadf HTTP/1.0\r\n\r\n");
  run_zrex(rexscope, "GET /fa/adf/a////df/adf/dfadf!#¤!¤# HTTP/1.0\r\n\r\n");
  run_zrex(rexscope, "POST /fa/adf/a////df/adf/dfadf!#¤!¤# HTTP/1.0\r\n\r\n");
  run_zrex(rexscope, "UPDATE /fa/adf/a////df/adf/dfadf!#¤!¤# HTTP/1.0\r\n\r\n");
  run_zrex(rexscope, "DELETE /fa/adf/a////df/adf/dfadf!#¤!¤# HTTP/1.0\r\n\r\n");

  zrex_destroy(&rexscope);
  exit(EXIT_SUCCESS);
}
void run_zrex(zrex_t *rex, char *string) {
  assert(zrex_valid(rex));
  int i;
  printf("matching %s\n", string);
  if (zrex_matches(rex, string)) {
    printf("got %d hits\n", zrex_hits(rex));
    for (i = 0; i < zrex_hits(rex); i++) {
      printf("match %d %s\n", i, zrex_hit(rex, i));
    }
  } else {
    printf("zrex_match \"%s\" failed\n", string);
  }
}
