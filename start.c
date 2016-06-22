#include <assert.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/survey.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "logger.h"


#define SERVER "server"
#define CLIENT "client"
#define DATE   "DATE"
#define MAG_GROUP_ID 1000

char *date ()
{
  time_t raw = time (&raw);
  struct tm *info = localtime (&raw);
  char *text = asctime (info);
  text[strlen(text)-1] = '\0'; // remove '\n'
  return text;
}

int server (const char *url)
{
  int sock = nn_socket (AF_SP, NN_SURVEYOR);
  if(sock < 0)
  {
    logger("ERROR", "Can't create socket");
    return 1;
  }

  if(nn_bind (sock, url) < 0)
  {
    logger("ERROR", "Can't bind service");
    return 1;
  }
  sleep(1); // wait for connections
  char *group_request = "GROUP_REQUEST";
  int sz_d = strlen(group_request) + 1; // '\0' too
  int bytes = nn_send (sock, DATE, sz_d, 0);
  if(bytes != sz_d){
    logger("WARN", "Group request partially sent");
  }
  while (1)
  {
    char *buf = NULL;
    int bytes = nn_recv (sock, &buf, NN_MSG, 0);
    int group_id = -1;
    char *error = NULL;
    if (bytes == ETIMEDOUT) break;
    if (bytes >= 0)
    {
      printf ("SERVER: RECEIVED \"%s\" SURVEY RESPONSE\n", buf);
      group_id = strtol(buf, &error,  10);
      if(!error) {
        char *error_msg; 
        asprintf(&error_msg, "%s msg(%s)", "Group ID should be an integer", buf);
        logger("ERROR", error_msg);
        free(error_msg);
      }
      else if (group_id > MAG_GROUP_ID) {
        char *error_msg; 
        asprintf(&error_msg, "%s More than %d recived %d", "Group ID should be an integer", MAG_GROUP_ID, group_id);
        logger("ERROR", error_msg);
        free(error_msg);
      }
      nn_freemsg (buf);
    }
  }
  return nn_shutdown (sock, 0);
}

int main (const int argc, const char **argv)
{
    return server ("tcp://127.0.0.1:9999");
}
