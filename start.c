#include <assert.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/survey.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ev.h>
#include "logger.h"
#include <sys/tree.h>


#define SERVER "server"
#define CLIENT "client"
#define DATE   "DATE"
#define MAX_GROUP_ID 1000
#define MAX_NODES_PER_PROXY 100


static ev_timer timeout_watcher;
static int surv_sock;
static int pub_sock;
static int groups[MAX_NODES_PER_PROXY]; 



static int compare_int(const void *lt, const void *lp) 
{
  return *((int *) lp) -  *((int *) lt);
}
static int initlize_sock(int *sock, char *url) 
{
  *sock = nn_socket (AF_SP, NN_SURVEYOR);
  if(*sock < 0)
  {
    logger("ERROR", "Can't create socket");
    return 1;
  }
  if(nn_bind (*sock, url) < 0)
  {
    logger("ERROR", "Can't bind service");
    return 1;
  }
}
void survey_request(EV_P_ ev_timer *w, int revents) {
  char *group_request = "GROUP_REQUEST";
  int sz_d = strlen(group_request) + 1; // '\0' too
  int bytes = nn_send (surv_sock, DATE, sz_d, 0);
  int trail = 0;
  int counter = 0;
  int recv_group[MAX_NODES_PER_PROXY];
  memset(recv_group, 0, sizeof(recv_group[0]) * MAX_NODES_PER_PROXY);

  if(bytes != sz_d){
    logger("WARN", "Group request partially sent");
  }
  while (1)
  {
    struct nn_pollfd pfd[1] = {{.fd = surv_sock, .events = NN_POLLIN}};
    int rc = nn_poll(pfd, 1, 2000);
    if (rc == 0) {
      logger("HB timeout from connected statsd nodes.");
      break;
    }
    if (rc == -1) {
      logger("Error can't recive HB messages at moment.");
      break;
    }
    if (pfd [0].revents & NN_POLLIN) {
      char *buf = NULL;
      int bytes = nn_recv(surv_sock, &buf, NN_MSG, 0);
      if(errno == ETIMEDOUT) {
        break;
      }
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
        else if (group_id > MAX_GROUP_ID) {
          char *error_msg; 
          asprintf(&error_msg, "%s More than %d recived %d", "Group ID should be an integer", MAX_GROUP_ID, group_id);
          logger("ERROR", error_msg);
          free(error_msg);
        }
        else {
          if ( counter < MAX_NODES_PER_PROXY)
          {
            recv_group[counter++] = group_id;
          }
        }
        nn_freemsg (buf);
      }
    }
  }
  recv_group[counter] = 0;
  qsort(recv_group, MAX_NODES_PER_PROXY, sizeof(recv_group[0]), compare_int);
  int pre = 0;
  for(int x = 0, l = 0; recv_group[x] != 0; x++)
  {
    if(pre != recv_group[x])
    {
      groups[l++] = recv_group[x];
    }
    pre = recv_group[x];
  }
  ev_timer_again(EV_A_ w);
}
int main (const int argc, const char **argv)
{
  struct ev_loop *loop = EV_DEFAULT;
  initlize_sock(&surv_sock, "tcp://127.0.0.1:9999");
  initlize_sock(&pub_sock,  "tcp://127.0.0.1:8888");
	ev_init(&timeout_watcher, survey_request);
	timeout_watcher.repeat = 10;
	ev_timer_again(loop, &timeout_watcher);
  ev_run(loop, 0);
  nn_shutdown (surv_sock, 0);
	return 0;
}
