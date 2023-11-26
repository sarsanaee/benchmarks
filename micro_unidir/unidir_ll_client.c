/*
 * Copyright 2019 University of Washington, Max Planck Institute for
 * Software Systems, and The University of Texas at Austin
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

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
#include "unidir.h"

#define MAX_EVENTS 32

static inline uint64_t get_nanos(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

int main(int argc, char *argv[])
{
  void *buf;
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
  while (1) {
    num = flextcp_context_poll(ctx, MAX_EVENTS, evs);
    for (int i = 0;i < num; i++) {
      ev = &evs[i];
      switch (ev->event_type) {
        case FLEXTCP_EV_CONN_RECEIVED:
          int len = ev->ev.conn_received.len;
          if (flextcp_connection_rx_done(ctx, conn, len) != 0) {
            fprintf(stderr, "flextcp_connection_rx_done(%p, %d) failed\n", conn,
                len);
            exit(-1);
          }
          break;

        case FLEXTCP_EV_CONN_SENDBUF:
          break;

        default:
          fprintf(stderr, "loop_receive: unexpected event (%u)\n", ev->event_type);
          break;
      }
    }

    // send a message
    ret = flextcp_connection_tx_alloc(conn, 64, &buf);
    if (ret < 0) {
        fprintf(stderr, "flextcp_connection_tx_alloc failed\n");
        exit(-1);
    }

    if (ret < 64) {
        continue;
    }

    if (ret == 64) {
        memset(buf, 1, ret);
    } else {
        printf("I couldn't allocate\n");
    }

    if (flextcp_connection_tx_send(ctx, conn, ret) != 0) {\
        fprintf(stderr, "flextcp_connection_tx_send failed\n");
    } else {
        // printf("sent %d\n", ret);
    }
  }

  return 0;
}
