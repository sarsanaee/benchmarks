#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <tas_ll.h>
#include <utils.h>
#include <assert.h>

#include "../common/microbench.h"
#include "unidir_ll_simple.h"

static inline uint64_t get_nanos(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

uint64_t send_tcp_message(struct flextcp_context* ctx, struct flextcp_connection *conn) {
    void *buf;
    uint64_t ret;

    ret = flextcp_connection_tx_alloc(conn, MSG_SIZE, &buf);
    if (ret < 0) {
        fprintf(stderr, "flextcp_connection_tx_alloc failed\n");
        exit(-1);
    }

    if (ret > 0) {
        memset(buf, 1, ret);
    } else {
        return 0;
    }

    if (flextcp_connection_tx_send(ctx, conn, ret) != 0) {
        fprintf(stderr, "flextcp_connection_tx_send failed\n");
        exit(-1);
    } else {
        return ret;
    }
}

int main(int argc, char *argv[])
{

  // uint64_t total_recv = 0;
  // uint64_t last_recv = 0;

  struct flextcp_context *ctx = malloc(sizeof(*ctx));
  struct flextcp_connection *conn = malloc(sizeof(*conn));
  struct flextcp_listener *listen = malloc(sizeof(*listen));



  if (flextcp_init()) {
    fprintf(stderr, "flextcp_init failed\n");
    return -1;
  }

  if (flextcp_context_create(ctx)) {
    fprintf(stderr, "flextcp_context_create failed\n");
    exit(-1);
  }

  // listen with port resue, that I don't think really need it.
  if (flextcp_listen_open(ctx, listen, 1234,
          1, FLEXTCP_LISTEN_REUSEPORT) != 0)
  {
      fprintf(stderr, "flextcp_listen_open failed\n");
      exit(-1);
  }

  // wait for the listen to be successful
  struct flextcp_event ev;
  int ret;

  printf("Waiting for listen to be successful\n");

  while (1) {
      if ((ret = flextcp_context_poll(ctx, 1, &ev)) < 0) {
          fprintf(stderr, "init_listen: flextcp_context_poll failed\n");
          return -1;
      }

      /* skip if no event */
      if (ret == 0)  {
          continue;
      }

      if (ev.event_type != FLEXTCP_EV_LISTEN_OPEN) {
          fprintf(stderr, "init_listen: unexpected event type (%u)\n",
                  ev.event_type);
          continue;
      }

      if (ev.ev.listen_open.status != 0) {
          fprintf(stderr, "init_listen: listen open request failed\n");
          return -1;
      }

      break;
  }

  printf("Listen successful\n");

  // here we should start polling on events to open connections and other stuff.
  struct flextcp_event *evs;

  if ((evs = calloc(MAX_EVENTS, sizeof(*evs))) == NULL) {
    fprintf(stderr, "Allocating event buffer failed\n");
    abort();
  }

  while (1) {
    int n = flextcp_context_poll(ctx, MAX_EVENTS, evs);
    if (n < 0) {
        fprintf(stderr, "flextcp_context_poll failed\n");
        abort();
    }

    // for loop on the events
    for (int i = 0; i < n; i++) {
      switch (evs[i].event_type) {
        case FLEXTCP_EV_LISTEN_NEWCONN:
          if (flextcp_listen_accept(ctx, listen, conn) != 0) {
            fprintf(stderr, "connection_new: flextcp_obj_listen_accept failed\n");
            abort();
          }
          printf("New connection\n");
          break;
        case FLEXTCP_EV_LISTEN_ACCEPT:
          printf("Connection accepted\n");
          break;
        case FLEXTCP_EV_CONN_RECEIVED:
          // printf("Received data\n");
          uint32_t len = evs[i].ev.conn_received.len;

          // total_recv += len;

          // if (total_recv - last_recv >= MSG_SIZE) {
          //   send_tcp_message(ctx, conn);
          //   last_recv = total_recv;
          // }

          if (flextcp_connection_rx_done(ctx, conn, len) != 0) {
	          fprintf(stderr, "thread_event_rx: rx_done failed\n");
	          abort();
	        }

          break;
        default:
          printf("Unknown event type\n");
          break;
      }
    }
  }

  return 0;
}
