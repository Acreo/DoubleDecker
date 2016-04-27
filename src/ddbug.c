#include <stdio.h>
#include <czmq.h>

// Simple client to push debug messages into a running ddbroker
int main(int argc, char **argv) {
  char endpoint[512];

  if (argc < 3) {
    printf("Send debugging commands to a ddbroker\n");
    printf("%s <broker-name> <command> [arg1] .. [argN]\n", argv[0]);
    return 0;
  }

  snprintf(&endpoint[0], 512, "ipc:///tmp/%s.debug", argv[1]);
  printf("connecting to %s\n", &endpoint[0]);
  zsock_t *sock = zsock_new_req(&endpoint[0]);
  int i;
  zmsg_t *msg = zmsg_new();
  for (i = 2; i < argc; i++) {
    zframe_t *frame = zframe_new(argv[i], strlen(argv[i]));
    zmsg_append(msg, &frame);
  }
  zmsg_print(msg);
  zsock_send(sock, "m", msg);

  zsock_recv(sock, "m", msg);

  zsock_destroy(&sock);
  return 1;
}
