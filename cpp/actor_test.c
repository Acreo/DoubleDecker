#include <czmq.h>
#include <dd.h>
#include <stdio.h>

void main (int argc, char **argv)
{
  printf ("test libdd zactor model \n"); 
  zactor_t *actor =
      start_ddactor(1, "testactor", "a", "tcp://127.0.0.1:5555",
                  "/etc/doubledecker/ape-keys.json");

  printf("actor = %p\n", actor);
  
  // zactor_t is a zeromq thingy representing a thread
  // you can get and select()/poll() etc a fileno for it
  printf("filedescriptor for zactor : %d\n", zsock_fd(actor));

  // loop while stay alive..
  int stay_alive = 1; 
  while(stay_alive){
    // recieve a message from the DD client actor thread
    zmsg_t *msg = zmsg_recv (actor);
    zmsg_print(msg);
    char *event = zmsg_popstr(msg);

    // if successfully registered
    if (streq("reg",event)){
      printf(" testactor - Registered\n");
      free(event);
      zsock_send(actor, "sss","subscribe", "testtopic", "all");
    } // if disconnected
    else if (streq("discon",event)){
      printf("testactor - Unregistered\n");
      free(event);
    } // if we recieived a publication
    else if (streq("pub",event)){
      char * source = zmsg_popstr(msg);
      char * topic = zmsg_popstr(msg);
      zframe_t * mlen = zmsg_pop(msg);
      char * message = zmsg_popstr(msg);
      printf("Got publication from %s on %s : \"%s\"\n",source, topic, message);
      if(streq("die",message))
        stay_alive = -1;
      
      // Answer with a publication on  "topic"
      int len = strlen("hello");
      zsock_send(actor,"sssb", "publish", "topic", "hello",&len, sizeof(len));
            
      free(event);
      free(topic);
      free(message);
      zframe_destroy(&mlen);
      
    } // if we received a notification
    else if (streq("data",event)){
      char * source = zmsg_popstr(msg);
      zframe_t * mlen = zmsg_pop(msg);
      char * message = zmsg_popstr(msg);
      printf("Got notification from %s : \"%s\"\n",source, message);
      if(streq("die",message))
        stay_alive = -1;
      
      // Answer with a friendly "pong"
      int len = strlen("pong");
      zsock_send(actor,"sssb", "notify", source, "pong", &len, sizeof(len));

            
      free(event);
      free(message);
      zframe_destroy(&mlen);
    }
    else if (streq("$TERM",event)){
      char * error = zmsg_popstr(msg);
      printf("Error initilizing DD client, %s\n", error);
      zactor_destroy(&actor);
      return EXIT_FAILURE;
    }
  }
  zsock_send(actor, "s", "$TERM");
  zactor_destroy (&actor);
  printf ("Done!\n");
}








