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

uint64_t allocated = 0;

static inline uint64_t get_nanos(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

// how about creating a new function for preparing buffers? // TODO: ALireza


uint64_t send_tcp_message(struct flextcp_context* ctx, struct flextcp_connection *conn) {
    void *buf;
    uint64_t ret;

    ret = flextcp_connection_tx_alloc(conn, MSG_SIZE - allocated, &buf);
    if (ret < 0) {
        fprintf(stderr, "flextcp_connection_tx_alloc failed\n");
        exit(-1);
    }

    allocated += ret;

    memset(buf, 1, ret);

    if (allocated < MSG_SIZE) {
        // printf("allocated %lu bytes, %lu ret\n", allocated, ret);
        return 0;
    }

    // sending should be separated.
    if (flextcp_connection_tx_send(ctx, conn, allocated) != 0) {
        fprintf(stderr, "flextcp_connection_tx_send failed\n");
        exit(-1);
    } else {
        ret = allocated;
        allocated = 0;
        return ret;
    }
}

int main(int argc, char *argv[])
{
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

  if (flextcp_connection_open(ctx, conn,
    ntohl(inet_addr("10.0.255.100")),
    1234) != 0)
  {
    fprintf(stderr, "open_conns: flextcp_obj_connection_open failed\n");
    abort();
  }

  int state = 0;
  int ret, j;
  struct flextcp_event evs[MAX_EVENTS];

  // wait for it to establish
  while (state != 1) {
    /* get events */
    if ((ret = flextcp_context_poll(ctx, MAX_EVENTS, evs)) < 0) {
        fprintf(stderr, "open_conns: flextcp_context_poll "
                "failed\n");
        abort();
    }

    for (j = 0; j < ret; j++) {
        if (evs[j].event_type != FLEXTCP_EV_CONN_OPEN) {
            fprintf(stderr, "open_conns: unexpected event type "
                    "%u\n", evs[j].event_type);
            continue;
        }

        if (evs[j].ev.conn_open.status != 0) {
            fprintf(stderr, "open_conns: copen connection failed %d\n",
                    evs[j].event_type);
            abort();
        }

        state = 1;
    }
  }

  // CONNECTION ESTABLISH
  printf("Connection established\n");

  // start another for loop for events and then send messages.
  int num;
  struct flextcp_event *ev;
  uint64_t tx_bump = 0, rx_bump = 0;
  uint64_t sent_bytes;
//   uint64_t total_recv = 0, last_recv = 0;
  uint64_t end = get_nanos(), start = get_nanos();
//   uint8_t max_pending = BATCH_SIZE;
  while (1) {
    num = flextcp_context_poll(ctx, MAX_EVENTS, evs);
    for (int i = 0;i < num; i++) {
      ev = &evs[i];
      switch (ev->event_type) {
        case FLEXTCP_EV_CONN_RECEIVED:
          int len = ev->ev.conn_received.len;
          rx_bump += len;
          //   total_recv += len;
          //   if (total_recv - last_recv >= MSG_SIZE) {
          //     last_recv = total_recv;
          //     max_pending++;
          //   }
          //   assert (max_pending <= BATCH_SIZE);
          if (flextcp_connection_rx_done(ctx, conn, len) != 0) {
            fprintf(stderr, "flextcp_connection_rx_done(%p, %d) failed\n", conn,
                len);
            exit(-1);
          }
          break;

        case FLEXTCP_EV_CONN_SENDBUF:
        //   sent_bytes = send_tcp_message(ctx, conn);
        //   if (sent_bytes > 0) {
        //     tx_bump += sent_bytes;
        //   }
          break;

        default:
          fprintf(stderr, "loop_receive: unexpected event (%u)\n", ev->event_type);
          break;
      }
    }

    end = get_nanos();
    if (end - start > 1000000000) {
      printf("tx tput= %f Gbps; rx tput= %f Gbps\n", tx_bump*8.0/(end - start),
            rx_bump*8.0/(end - start));
      start = get_nanos();
      rx_bump = 0;
      tx_bump = 0;
    }

    // send batch of messages
    // for (int bi = 0; bi < max_pending; bi++) {
    // while (max_pending > 0) {
    //     sent_bytes = send_tcp_message(ctx, conn);
    //     if (sent_bytes == 0) {
    //         break;
    //     }
    //     tx_bump += sent_bytes;
    //     max_pending--;
    // }

    // for (int bi = 0; bi < BATCH_SIZE; bi++) {
    //     sent_bytes = send_tcp_message(ctx, conn);
    //     if (sent_bytes == 0) {
    //         break;
    //     }
    //     tx_bump += sent_bytes;
    // }
    sent_bytes = send_tcp_message(ctx, conn);
    if (sent_bytes > 0) {
        tx_bump += sent_bytes;
    }

  }

  return 0;
}
