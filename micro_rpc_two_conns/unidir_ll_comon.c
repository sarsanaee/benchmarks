#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <tas_ll.h>
#include <utils.h>
#include <assert.h>
#include <stdatomic.h>

#define MSG_SIZE 64
#define BATCH_SIZE 1

#include "unidir_ll_simple.h"

uint32_t max_pending = 0;
uint64_t remaining_bytes = 0;
uint64_t start_experiment = 0;

// uint64_t sent_lat = 0;
// uint64_t recv_lat = 0;

// histogram
// uint32_t *hist;
// double fracs[6] = { 0.5, 0.9, 0.95, 0.99, 0.999, 0.9999 };
// size_t fracs_pos[sizeof(fracs) / sizeof(fracs[0])];


// static inline void hist_fract_buckets(uint32_t *hist, uint64_t total,
//         double *fracs, size_t *idxs, size_t num)
// {
//     size_t i, j;
//     uint64_t sum = 0, goals[num];
//     for (j = 0; j < num; j++) {
//         goals[j] = total * fracs[j];
//     }
//     for (i = 0, j = 0; i < HIST_BUCKETS && j < num; i++) {
//         sum += hist[i];
//         for (; j < num && sum >= goals[j]; j++) {
//             idxs[j] = i;
//         }
//     }
// }

// static inline void record_latency(uint64_t nanos)
// {
//     size_t bucket = ((nanos / 1000) - HIST_START_US) / HIST_BUCKET_US;
//     if (bucket >= HIST_BUCKETS) {
//         bucket = HIST_BUCKETS - 1;
//     }
//     __sync_fetch_and_add(&hist[bucket], 1);
// }

// static inline int hist_value(size_t i)
// {
//     if (i == HIST_BUCKETS - 1) {
//         return -1;
//     }

//     return i * HIST_BUCKET_US + HIST_START_US;
// }

static inline uint64_t get_nanos(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

uint64_t send_tcp_message(struct flextcp_context *ctx, struct flextcp_connection *conn)
{
  void *buf;
  uint64_t ret;
  uint32_t available = 0;

  ret = flextcp_conn_txbuf_available(conn);
  if (ret < 0)
  {
    fprintf(stderr, "flextcp_connection_tx_alloc failed\n");
    exit(-1);
  }

  available += ret;

  if (available < MSG_SIZE)
  {
    return 0;
  }

  ret = flextcp_connection_tx_alloc(conn, MSG_SIZE, &buf);
  if (ret < 0)
  {
    fprintf(stderr, "flextcp_connection_tx_alloc failed\n");
    exit(-1);
  }

  assert(ret == MSG_SIZE);
  memset(buf, 1, ret);

  if (flextcp_connection_tx_send(ctx, conn, ret) != 0)
  {
    fprintf(stderr, "flextcp_connection_tx_send failed\n");
    exit(-1);
  }
  else
  {
    return ret;
  }
}

int client(struct context *thread_ctx)
{
  struct flextcp_context *ctx = malloc(sizeof(*ctx));
  struct flextcp_connection *conn = malloc(sizeof(*conn));
  struct flextcp_listener *listen = malloc(sizeof(*listen));

  if (flextcp_context_create(ctx))
  {
    fprintf(stderr, "flextcp_context_create failed\n");
    exit(-1);
  }

  // if (thread_ctx->params->client && !thread_ctx->params->client_of_server) {
  //   if ((hist = calloc(HIST_BUCKETS, sizeof(*hist))) == NULL) {
  //       fprintf(stderr, "allocating total histogram failed\n");
  //       abort();
  //   }
  // }

  if (flextcp_connection_open(ctx, conn,
                              ntohl(inet_addr(thread_ctx->server_ip)),
                              thread_ctx->server_port) != 0)
  {
    fprintf(stderr, "open_conns: flextcp_obj_connection_open failed\n");
    abort();
  }

  if (start_experiment == 0)
    max_pending = BATCH_SIZE;

  int state = 0;
  int ret, j;
  struct flextcp_event evs[MAX_EVENTS];

  // wait for it to establish
  while (state != 1)
  {
    /* get events */
    if ((ret = flextcp_context_poll(ctx, MAX_EVENTS, evs)) < 0)
    {
      fprintf(stderr, "open_conns: flextcp_context_poll "
                      "failed\n");
      abort();
    }

    for (j = 0; j < ret; j++)
    {
      if (evs[j].event_type != FLEXTCP_EV_CONN_OPEN)
      {
        fprintf(stderr, "open_conns: unexpected event type "
                        "%u\n",
                evs[j].event_type);
        continue;
      }

      if (evs[j].ev.conn_open.status != 0)
      {
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
  uint64_t total_recv = 0;
  uint64_t total_sent = 0;
  uint64_t end = get_nanos(), start = get_nanos();
  uint64_t last_transmit = 0;
  while (1)
  {
    num = flextcp_context_poll(ctx, MAX_EVENTS, evs);
    for (int i = 0; i < num; i++)
    {
      ev = &evs[i];
      switch (ev->event_type)
      {
      case FLEXTCP_EV_CONN_RECEIVED:
        fprintf(stderr, "Should no receive on this connection (%u)\n", ev->event_type);
        abort();
        break;

      case FLEXTCP_EV_CONN_SENDBUF:
        // printf("More buffering available on the client side!\n");
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
    if (end - start > 1000000000)
    {
      printf("Client: tx tput= %f Gbps; rx tput= %f Gbps; Max pending= %d, sent_bytes= %lu, received_bytes= %lu; diff bytes: %lu\n", tx_bump * 8.0 / (end - start),
             rx_bump * 8.0 / (end - start), max_pending, total_sent, total_recv, total_sent - total_recv);
      start = get_nanos();
      rx_bump = 0;
      tx_bump = 0;

      // uint32_t hx = 0, msg_total = 0;
      // if (thread_ctx->params->client && !thread_ctx->params->client_of_server) {
      //   for (j = 0; j < HIST_BUCKETS; j++) {
      //     hx = hist[j];
      //     msg_total += hx;
      //     hist[j] += hx;
      //   }
      //   hist_fract_buckets(hist, msg_total, fracs, fracs_pos,
      //           sizeof(fracs) / sizeof(fracs[0]));
      //   printf("Client: histogram: ");
      //   for (j = 0; j < sizeof(fracs) / sizeof(fracs[0]); j++) {
      //     printf("%d=%d ",(int)fracs[j]*100, hist_value(fracs_pos[j]));
      //   }
      //   printf("\n");
      // }
    }

    // if (end - last_transmit < 100000)
    //   continue;

    // last_transmit = end;

    if (start_experiment != 1)
      continue;

    if (thread_ctx->params->response && !thread_ctx->params->client_of_server)
    {
      __sync_synchronize();
      while (max_pending > 0)
      {
        // printf("max_pending: %d\n", max_pending);
        sent_bytes = send_tcp_message(ctx, conn);
        tx_bump += sent_bytes;
        total_sent += sent_bytes;
        if (sent_bytes > 0)
        {
          // sent_lat = get_nanos();
          assert(sent_bytes == MSG_SIZE);
          __sync_fetch_and_sub(&max_pending, 1);

        }
      }
    }
    else if (thread_ctx->params->client_of_server) {
      // I just need to see my server has received anything or not.
      __sync_synchronize();
      if (remaining_bytes >= MSG_SIZE){
        sent_bytes = send_tcp_message(ctx, conn);
        tx_bump += sent_bytes;
        total_sent += sent_bytes;
        __sync_fetch_and_sub(&remaining_bytes, sent_bytes);
      }
    }
    else
    {
      // this is client for actual Client Machine.
      sent_bytes = send_tcp_message(ctx, conn);
      tx_bump += sent_bytes;
      total_sent += sent_bytes;
    }
  }
}

int server(struct context *thread_ctx)
{
  struct flextcp_context *ctx = malloc(sizeof(*ctx));
  struct flextcp_connection *conn = malloc(sizeof(*conn));
  struct flextcp_listener *listen = malloc(sizeof(*listen));

  if (flextcp_context_create(ctx))
  {
    fprintf(stderr, "flextcp_context_create failed\n");
    exit(-1);
  }

  printf("Server: context created %d\n", thread_ctx->client_port);
  // listen with port resue, that I don't think really need it.
  if (flextcp_listen_open(ctx, listen, thread_ctx->client_port,
                          1, FLEXTCP_LISTEN_REUSEPORT) != 0)
  {
    fprintf(stderr, "flextcp_listen_open failed\n");
    exit(-1);
  }

  // wait for the listen to be successful
  struct flextcp_event ev;
  int ret;

  printf("Waiting for listen to be successful\n");

  while (1)
  {
    if ((ret = flextcp_context_poll(ctx, 1, &ev)) < 0)
    {
      fprintf(stderr, "init_listen: flextcp_context_poll failed\n");
      return -1;
    }

    /* skip if no event */
    if (ret == 0)
    {
      continue;
    }

    if (ev.event_type != FLEXTCP_EV_LISTEN_OPEN)
    {
      fprintf(stderr, "init_listen: unexpected event type (%u)\n",
              ev.event_type);
      continue;
    }

    if (ev.ev.listen_open.status != 0)
    {
      fprintf(stderr, "init_listen: listen open request failed\n");
      return -1;
    }

    break;
  }

  printf("Listen successful\n");

  // here we should start polling on events to open connections and other stuff.
  struct flextcp_event *evs;

  if ((evs = calloc(MAX_EVENTS, sizeof(*evs))) == NULL)
  {
    fprintf(stderr, "Allocating event buffer failed\n");
    abort();
  }

  uint64_t tx_bump = 0, rx_bump = 0;
  uint64_t total_sent = 0, total_recv = 0;
  uint64_t end = 0, start = get_nanos();
  int len = 0;

  while (1)
  {
    int n = flextcp_context_poll(ctx, MAX_EVENTS, evs);
    if (n < 0)
    {
      fprintf(stderr, "flextcp_context_poll failed\n");
      abort();
    }

    // for loop on the events
    for (int i = 0; i < n; i++)
    {
      switch (evs[i].event_type)
      {
      case FLEXTCP_EV_LISTEN_NEWCONN:
        if (flextcp_listen_accept(ctx, listen, conn) != 0)
        {
          fprintf(stderr, "connection_new: flextcp_obj_listen_accept failed\n");
          abort();
        }
        printf("New connection\n");
        break;
      case FLEXTCP_EV_LISTEN_ACCEPT:
        printf("Connection accepted\n");
        start_experiment = 1;
        break;
      case FLEXTCP_EV_CONN_RECEIVED:
        if (!thread_ctx->params->response)
        {
          len = evs[i].ev.conn_received.len;
          total_recv += len;
          __sync_fetch_and_add(&remaining_bytes, len);
          rx_bump += len;

          if (flextcp_connection_rx_done(ctx, conn, len) != 0)
          {
            fprintf(stderr, "thread_event_rx: rx_done failed\n");
            abort();
          }
        }
        else {
          len = len + evs[i].ev.conn_received.len;
          total_recv += len;
          while (len >= MSG_SIZE)
          {
            if (flextcp_connection_rx_done(ctx, conn, MSG_SIZE) != 0)
            {
              fprintf(stderr, "thread_event_rx: rx_done failed\n");
              abort();
            }
            len -= MSG_SIZE;
            rx_bump += MSG_SIZE;
            //create a spin lock around max pending

            // if (!thread_ctx->params->client_of_server) {
            //   total_messages++;
            //   recv_lat = get_nanos();
            //   record_latency(recv_lat - sent_lat);
            // }

            __sync_fetch_and_add(&max_pending, 1);
            __sync_fetch_and_add(&remaining_bytes, MSG_SIZE);
          }
        }

        break;

      case FLEXTCP_EV_CONN_SENDBUF:
        printf("FLEXCONN_SENDBUF %lu\n", remaining_bytes);
        abort();
        break;
      default:
        printf("Unknown event type\n");
        break;
      }
    }

    end = get_nanos();
    if (end - start > 1000000000)
    {
      printf("Server: tx tput= %f Gbps; rx tput= %f Gbps; sent_bytes= %lu, received_bytes= %lu; diff bytes: %lu, remaining_bytes= %lu\n", tx_bump * 8.0 / (end - start),
             rx_bump * 8.0 / (end - start), total_sent, total_recv, total_recv - total_sent, remaining_bytes);
      start = get_nanos();
      rx_bump = 0;
      tx_bump = 0;
    }

    // if (thread_ctx->params->response)
    // {
    //   while (remaining_bytes >= MSG_SIZE)
    //   {
    //     // assert(1 == 12);
    //     uint32_t sent_bytes = send_tcp_message(ctx, conn);
    //     tx_bump += sent_bytes;
    //     remaining_bytes -= sent_bytes;
    //     if (sent_bytes == 0)
    //     {
    //       // printf("this should not fail %lu\n", remaining_bytes);
    //       break;
    //     }
    //   }
    // }
  }
}

