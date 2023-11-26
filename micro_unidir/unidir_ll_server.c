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
