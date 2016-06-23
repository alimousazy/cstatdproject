#include <assert.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/survey.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ev.h>
#include <sys/tree.h>
#include "stat_server.h"
#include "logger.h"


#define SERVER "server"
#define CLIENT "client"
#define DATE   "DATE"
#define MAX_GROUP_ID 1000
#define MAX_NODES_PER_PROXY 100
#define BUF_SIZE        4096


static char buffer[BUF_SIZE];
struct ev_periodic survey_periodic;
static int surv_sock;
static int pub_sock;
static int groups[MAX_NODES_PER_PROXY]; 
static int msg_count = 0;



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
      logger("ERROR", "HB timeout from connected statsd nodes.");
      break;
    }
    if (rc == -1) {
      logger("ERROR", "Error can't recive HB messages at moment.");
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
  printf("\nMsg counter %d\n", msg_count);
}
void msg_rcv(EV_P_ ev_io *w, int revents)
{
  struct sock_ev_serv *serv = (struct sock_ev_serv *) w;
  socklen_t addr_len = sizeof(serv->addr);
	socklen_t bytes = recvfrom(serv->fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*) &serv->addr, (socklen_t *) &addr_len);
  buffer[bytes] = '\0';
  msg_count++;
}
int main (const int argc, const char **argv)
{
  struct sock_ev_serv server;
  struct ev_loop *loop = EV_DEFAULT;
  initlize_sock(&surv_sock, "tcp://127.0.0.1:9999");
  initlize_sock(&pub_sock,  "tcp://127.0.0.1:8888");
  server_init(&server, 3561);
  add_ev_loop(&server, loop, msg_rcv);
  ev_periodic_init(&survey_periodic, survey_request, 0, 60, 0);
  ev_periodic_start(EV_A_ &survey_periodic);
  ev_run(loop, 0);
  nn_shutdown (surv_sock, 0);
	return 0;
}
