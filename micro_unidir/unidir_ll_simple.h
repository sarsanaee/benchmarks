#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define MAX_EVENTS 32
#define BATCH_SIZE 8
#define MSG_SIZE 8192 // 1KB

struct params {
  uint32_t conns;
  uint32_t bytes;
  uint32_t ip;
  uint32_t threads;
  uint32_t tx_delay;
  uint32_t op_delay;
  int tx;
  uint16_t port;
  uint8_t response;
};


int client();
int server();

static inline int parse_params(int argc, char *argv[], struct params *p)
{
  // static const char *short_opts = "i:p:t:c:b:d:o:r";
  static const char *short_opts = "r";
  int c, done = 0;
  // char *end;

  p->response = 0;
  p->tx = 0;


  while (!done) {
    c = getopt(argc, argv, short_opts);
    switch (c) {
      case 'r':
        p->response = 1;
        break;
      case 'o':
        p->tx = 1;
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

