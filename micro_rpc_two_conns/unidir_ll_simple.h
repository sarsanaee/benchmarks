
// add propper header define for avoid multiple inclusion
#ifndef UNIDIR_LL_SIMPLE_H
#define UNIDIR_LL_SIMPLE_H

#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_EVENTS 32
#define BATCH_SIZE 8
#define MSG_SIZE 2048 // 1KB

struct params {
  uint8_t client;
  uint8_t server;
  uint8_t response;
};

struct context {
  char *server_ip;
  uint16_t server_port;
  uint16_t client_port;
  struct params *params;
};

int client(struct context *);
int server(struct context *);

static inline int parse_params(int argc, char *argv[], struct params *p)
{
  // static const char *short_opts = "i:p:t:c:b:d:o:r";
  static const char *short_opts = "r:c:s";
  int c, done = 0;
  // char *end;

  p->response = 0;
  p->client = 0;
  p->server = 0;

  if (argc < 2 || argc > 7) {
      fprintf(stderr, "Usage: ./unidir_ll_server -c [1|0] -s [1|0] -r [1|0]\n");
      return EXIT_FAILURE;
  }

  while (!done) {
    c = getopt(argc, argv, short_opts);
    switch (c) {
      case 'r':
        p->response = 1;
        break;
      case 'c':
        p->client = 1;
        break;
      case 's':
        p->server = 1;
        break;

      case -1:
        done = 1;
        break;

      case '?':
        goto failed;

      default:
        abort();
    }
  }

  if (p->client && p->server) {
    fprintf(stderr, "Cannot be both client and server\n");
    goto failed;
  }

  return 0;

failed:
  fprintf(stderr,
      "Usage: %s OPTIONS\n"
      "Options:\n"
      "  -i IP       IP address for client mode       [default server mode]\n"
      "  -p PORT     Port to listen on/connect to     [default 1234]\n"
      "  -t THREADS  Number of threads to use         [default 1]\n"
      "  -c CONNS    Number of connections per thread [default 1]\n"
      "  -b BYTES    Message size in bytes            [default 1]\n"
      "  -r          Receive mode                     [default transmit]\n"
      "  -d DELAY    Seconds before sending starts    [default 0]\n"
      "  -o DELAY    Cycles to artificially delay/op  [default 0]\n",
      argv[0]);
  return -1;
}

#endif // UNIDIR_LL_SIMPLE_H
