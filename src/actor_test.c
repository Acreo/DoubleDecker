#include <czmq.h>
#include <dd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
zactor_t *actor;
int stay_alive = 1;

void my_handler(int s) {
  printf("sighandler: Caught signal %d\n", s);
  stay_alive = -1;
  printf("sighandler_Setting stay_alive to %d, calling zactor_destroy\n",
         stay_alive);
  zactor_destroy(&actor);
}

int main(int argc, char **argv) {
  struct sigaction sigIntHandler;
  // handle signals
  sigIntHandler.sa_handler = my_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // Create new actor
  actor = ddactor_new("testactor", "tcp://127.0.0.1:5555",
                      "/etc/doubledecker/a-keys.json");

  // zactor_t is a zeromq class representing a thread and a PAIR pipe socket
  // you can get and select()/poll() etc a fileno for it
  printf("filedescriptor for zactor : %d\n", zsock_fd(actor));

  // loop while stay alive is true
  stay_alive = 1;
  while (stay_alive > 0) {

    // recieve a message from the DD client actor thread
    zmsg_t *msg = zmsg_recv(actor);
    if (msg == NULL) {
      printf("zmsg_recv got interrupted, stay_alive = %d\n", stay_alive);
      continue;
    }

    // Get the type of message recieved (reg,pub,data..)
    char *event = zmsg_popstr(msg);

    // We successfully registered
    if (streq("reg", event)) {
      printf(" testactor - Registered\n");
      free(event);
      zsock_send(actor, "sss", "subscribe", "testtopic", "all");
    }

    // We got disconnected
    else if (streq("discon", event)) {
      printf("testactor - Unregistered\n");
      free(event);
    }

    // We recieived a publication
    else if (streq("pub", event)) {
      char *source = zmsg_popstr(msg);
      char *topic = zmsg_popstr(msg);
      zframe_t *mlen = zmsg_pop(msg);
      char *message = zmsg_popstr(msg);
      printf("Got publication from %s on %s : \"%s\"\n", source, topic,
             message);
      if (streq("die", message))
        stay_alive = -1;

      // Answer with a publication on  "topic"
      int len = strlen("hello");
      zsock_send(actor, "sssb", "publish", "topic", "hello", &len, sizeof(len));

      free(event);
      free(topic);
      free(message);
      zframe_destroy(&mlen);

    }

    // We received a notification
    else if (streq("data", event)) {
      char *source = zmsg_popstr(msg);
      zframe_t *mlen = zmsg_pop(msg);
      char *message = zmsg_popstr(msg);
      printf("Got notification from %s : \"%s\"\n", source, message);
      if (streq("die", message))
        stay_alive = -1;

      // Answer with a friendly "pong"
      int len = strlen("pong");
      zsock_send(actor, "sssb", "notify", source, "pong", &len, sizeof(len));

      free(event);
      free(message);
      zframe_destroy(&mlen);
    }

    // We had a failure
    else if (streq("$TERM", event)) {
      char *error = zmsg_popstr(msg);
      printf("Error initilizing DD client, %s\n", error);
      zactor_destroy(&actor);
      return EXIT_FAILURE;
    }
  }

  // Destroy the ddactor
  zactor_destroy(&actor);
  printf("Done!\n");
}
