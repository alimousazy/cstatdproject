#include "stat_server.h"

// Simply adds O_NONBLOCK to the file descriptor of choice
static int setnonblock(int fd)
{
  int flags;
  flags = fcntl(fd, F_GETFL);
  flags |= O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags);
}

static int udp_sock_init(struct sockaddr_in* socket_un) {
  int fd;
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (-1 == fd) {
    perror("echo server socket");
    exit(EXIT_FAILURE);
  }
  if (-1 == setnonblock(fd)) {
    perror("echo server socket nonblock");
    exit(EXIT_FAILURE);
  }
  return fd;
}

static int bind_sock(struct sock_ev_serv* server, int port) {
    memset((char *)&server->addr, 0, sizeof(server->addr));
    server->addr.sin_family = AF_INET;
    server->addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server->addr.sin_port = htons(port);
    return bind(server->fd, (struct sockaddr*)  &server->addr, sizeof(server->addr));
}


int add_ev_loop(struct sock_ev_serv* server, struct ev_loop *loop, ev_cb cb)
{
  ev_io_init(&server->io, cb, server->fd, EV_READ);
  ev_io_start(loop, &server->io);
}

int server_init(struct sock_ev_serv* server, int port) {
    server->fd = udp_sock_init(&server->addr);
    return bind_sock(server, port);
}

