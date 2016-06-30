#include <assert.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/survey.h>
#include <nanomsg/pubsub.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ev.h>
#include <sys/tree.h>
#include "stat_server.h"
#include "logger.h"
#include <sys/queue.h>
#include <hash_ring.h>


#define SERVER "server"
#define CLIENT "client"
#define GREQUEST   "GROUP_REQUEST"
#define MAX_GROUP_ID 1000
#define MAX_NODES_PER_PROXY 100
#define BUF_SIZE        4096



hash_ring_t *ring;
static char buffer[BUF_SIZE];
struct ev_periodic survey_periodic;
static int surv_sock;
static int pub_sock;
static int groups[MAX_NODES_PER_PROXY]; 
static int groups_count = 0; 
static int msg_count = 0;
static int dcounter = 0;

struct statd_msg {
  char *c;
  TAILQ_ENTRY(statd_msg) entries;
};

TAILQ_HEAD(statd_msg_list_head, statd_msg) msg_list = TAILQ_HEAD_INITIALIZER(msg_list);

struct sock_ev_nanomsg {
  ev_io io;
  int fd;
  int nn_fd;
};

struct check_ev_nanomsg {
	struct ev_check c;
  struct sock_ev_nanomsg *sockev;
  int is_io_started;
};

static int compare_int(const void *lt, const void *lp) 
{
  return *((int *) lp) -  *((int *) lt);
}
static int initlize_sock(int *sock, char *url, int type) 
{
  *sock = nn_socket (AF_SP, type);
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
  printf("MSG count %d\n", dcounter);
  int sz_d = strlen(GREQUEST) + 1; 
  int bytes = nn_send (surv_sock, GREQUEST, sz_d, 0);
  groups_count = 0;
  hash_ring_free(ring);
  ring = hash_ring_create(100, HASH_FUNCTION_SHA1);
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
        if((hash_ring_add_node(ring, (uint8_t*)buf, bytes)) != HASH_RING_OK) {
         logger("Error", "Can't add to hash ring");
        } else {
          groups_count++;
        }
      }
      nn_freemsg (buf);
    }
  }
}
void idle_cb(EV_P_ ev_io *w, int revents) {
  sleep(1);
}
void push_cb(EV_P_ ev_io *w, int revents)
{
  struct statd_msg *np = TAILQ_LAST(&msg_list, statd_msg_list_head);
  if(np == NULL)
  {
    return;
  }
  hash_ring_node_t *node;
  char *spe = strstr(np->c, ":");
  size_t len;  
  if(spe) {
    len = spe - np->c; 
  } else {
    len = strlen(np->c);
  }
  node = hash_ring_find_node(ring, (uint8_t*) np->c, len);
  if(node == NULL)
  {
    logger("ERROR", "Can't process messsage");
    return;
  }
  struct sock_ev_nanomsg *s = (struct sock_ev_nanomsg *) w; 
  int metric_len = strlen(np->c) + node->nameLen + 2;
  void *msg = nn_allocmsg(metric_len, 0);
  snprintf(msg, metric_len, "%s>%s", node->name, np->c);
  int bytes = nn_send(s->nn_fd, &msg, NN_MSG, NN_DONTWAIT);
  if(bytes == -1) {
    logger("ERROR", "FAiled to send message\n");
    if(errno == ETIMEDOUT) {
      logger("ERROR", "timing out\n");
    }
    return;
  }
  TAILQ_REMOVE(&msg_list, np, entries);
  free(np->c);
  free(np);
  msg_count--;
}
void msg_rcv(EV_P_ ev_io *w, int revents)
{
  struct sock_ev_serv *serv = (struct sock_ev_serv *) w;
  socklen_t addr_len = sizeof(serv->addr);
	socklen_t bytes = recvfrom(serv->fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*) &serv->addr, (socklen_t *) &addr_len);
  buffer[bytes] = '\0';
  struct statd_msg *entry = malloc(sizeof(struct statd_msg));      /* Insert at the tail. */
  if(entry == NULL)
  {
    logger("ERROR", "Failed to allocate msg for news msgs");
  }
  entry->c = strdup(buffer);
  TAILQ_INSERT_TAIL(&msg_list, entry, entries);
  dcounter++;
  msg_count++;
}
void check_metric_array(struct ev_loop* loop, struct ev_check* instance, int revents)
{
	struct check_ev_nanomsg *ce = (struct check_ev_nanomsg *) instance;
  struct sock_ev_nanomsg *sock = ce->sockev;
  if(!ce->is_io_started && msg_count > 0 && groups_count > 0) {
    logger("INFO", "Enabling io");
    ev_io_start(loop, &sock->io);
    ce->is_io_started = 1;
  } else if(msg_count <= 0)  {
    ev_io_stop(loop, &sock->io);
    ce->is_io_started = 0;
  }
}

int main (const int argc, const char **argv)
{
  struct sock_ev_serv server;
  struct ev_loop *loop = EV_DEFAULT;
  struct sock_ev_nanomsg sevnano;
  static ev_idle idle_watcher;
  TAILQ_INIT(&msg_list); 
  initlize_sock(&surv_sock, "tcp://0.0.0.0:9999", NN_SURVEYOR);
  initlize_sock(&pub_sock,  "tcp://0.0.0.0:8888", NN_PUB);
  server_init(&server, 3561);
  sleep(10);

  add_ev_loop(&server, loop, msg_rcv);
  ev_periodic_init(&survey_periodic, survey_request, 0, 60, 0);
  ev_periodic_start(EV_A_ &survey_periodic);

  size_t len = sizeof(sevnano.fd);
  nn_getsockopt (pub_sock, NN_SOL_SOCKET, NN_SNDFD, &sevnano.fd, &len);
  sevnano.nn_fd = pub_sock; 
  ev_io_init(&sevnano.io, push_cb, sevnano.fd, EV_WRITE);

  ev_idle_init(&idle_watcher, idle_cb);
  ev_idle_start (loop, &idle_watcher);

	struct check_ev_nanomsg ce;
	ce.sockev = &sevnano;
  ce.is_io_started = 0;
	ev_check_init(&ce.c, check_metric_array);
	ev_check_start(loop, &ce.c);

  ev_run(loop, 0);
  nn_shutdown (surv_sock, 0);
	return 0;
}
