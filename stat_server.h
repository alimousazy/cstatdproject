#pragma once 

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <ev.h>
#include <netinet/in.h>


typedef void (* ev_cb)(EV_P_ ev_io *w, int revents);

struct sock_ev_serv {
  ev_io io;
  int fd;
  struct sockaddr_in addr;
};

int server_init(struct sock_ev_serv* server, int port);
int add_ev_loop(struct sock_ev_serv* server, struct ev_loop *loop, ev_cb cb);

